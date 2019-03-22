#ifndef FINDER_CENTER_HPP
#define FINDER_CENTER_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"
#include "../Options.hpp"
#include "../util.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits, typename _Mode, typename _Restriction, typename _Conversion, size_t _length, bool _with_cycles>
	class Center
	{
	public:
		static constexpr char const *name = "Center";
		static constexpr size_t length = _length;
		static constexpr bool with_cycles = _with_cycles;
		using Mode = _Mode;
		using Restriction = _Restriction;
		using Conversion = _Conversion;

		static_assert(_length > 1, "Can only detect path/cycles with at least 2 vertices");
		static_assert(_length > 3 || !_with_cycles, "Cycles are only supported with length at least 4");
		static_assert(_with_cycles || !std::is_same<Conversion, Options::Conversions::Skip>::value, "Without cycles, there is no conversion and thus nothing must be skipped");

	private:
		std::vector<Packed> forbidden;

	public:
		Center(VertexID graph_size) : forbidden(Graph::alloc_rows(graph_size, length)) {;}

		template<typename Feeder>
		void find(Graph const &graph, Graph_Edits const &edited, Feeder &feeder)
		{
			auto callback = [&feeder, &graph, &edited](std::vector<VertexID>::const_iterator uit, std::vector<VertexID>::const_iterator vit) -> bool
					{
						return feeder.callback(graph, edited, uit, vit);
					};

			find(graph, callback);
		}

		template <typename F>
		void find(Graph const &graph, F& callback)
		{
			assert(forbidden.size() / graph.get_row_length() == length);
			std::vector<VertexID> path(length);

			constexpr bool length_even = (length % 2 == 0);
			Packed *f = forbidden.data();
			// Note: if constexpr here avoids instantiation of the wrong recursion with wrong base case
			if constexpr (!length_even) // uneven length
			{
				// does not work for length == 3
				for(VertexID u = 0; u < graph.size(); u++)
				{
					if constexpr (length > 3)
					{
						// Mark u and neighbors of u in f
						for(size_t i = 0; i < graph.get_row_length(); i++)
						{
							f[i] = graph.get_row(u)[i];
						}
						f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
					}
					// Store u as central node
					path[length / 2] = u;
					// For all neighbors vf of u
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(u)[i]; curf;)
						{
							const VertexID lzcurf = PACKED_CTZ(curf);
							const VertexID vf = lzcurf + i * Packed_Bits;
							// Remove the bit of vf from curf
							curf &= ~(Packed(1) << lzcurf);

							path[length / 2 - 1] = vf;
							// For all neighbors vb of u with vf < vb and (vf, vb) is not an edge
							// Start iteration at block j = i
							for(size_t j = i; j < graph.get_row_length(); j++)
							{
								// For the first block, we can use curf (vf is already removed), otherwise get the block
								Packed curb = j == i? curf : graph.get_row(u)[j];

								// Exclude neighbors of vf
								curb &= ~(graph.get_row(vf)[j]);

								while (curb)
								{
									const VertexID lzcurb = PACKED_CTZ(curb);
									const VertexID vb = lzcurb + j * Packed_Bits;
									// Remove the bit of vb from curb
									curb &= ~(Packed(1) << lzcurb);

									path[length / 2 + 1] = vb;
									if(Find_Rec<decltype(callback), length / 2 - 1, length / 2 + 1, 0>::find_rec(graph, path, forbidden, callback)) {return;}
								}
							}
						}
					}
				}
			}
			else
			{
				if constexpr (length > 2) {
					for (size_t i = 0; i < graph.get_row_length(); ++i)
					{
						f[i] = 0;
					}
				}

				for(VertexID u = 0; u < graph.size(); u++) // outer loop: first node u
				{
					// Set bit u in f
					if constexpr (length > 2) f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
					path[length / 2 - 1] = u;
					// Second loop: second node v with u < v
					// First half of outer loop: explore packed neighbors >= u
					for(size_t i = u / Packed_Bits; i < graph.get_row_length(); i++)// double exploration: i = 0  cur = graph.get_row(u)[i]
					{
						// Second half: explore actual neighbors v.
						// For first Packed item (i == u / Packed_Bits) (that contains u), mask all bits up to (and including) position u (% Packed_Bits)
						// So basically these two loops are graph.next_neighbor(u, v), where v is initially u.
						for(Packed cur = i == u / Packed_Bits? graph.get_row(u)[i] & ~((Packed(2) << (u % Packed_Bits)) - 1) : graph.get_row(u)[i]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
						{
							VertexID v = PACKED_CTZ(cur) + i * Packed_Bits;
							// Set bit v in f
							if constexpr (length > 2) f[v / Packed_Bits] |= Packed(1) << (v % Packed_Bits);
							path[length / 2] = v;
							// Path now contains the two node u and v
							if(Find_Rec<decltype(callback), length / 2 - 1, length / 2, 0>::find_rec(graph, path, forbidden, callback)) {return;}
							// Unset v in f
							if constexpr (length > 2) f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
						}
					}
					if constexpr (length > 2) f[u / Packed_Bits] = 0;
				}
			}
		}

		/**
		 * List all forbidden subgraphs that contain the given node @a uu and @a vv.
		 * The algorithm first completes the inner part between the two nodes (if there is no edge) and then completes the outer part and calls the given @a callback for all found subgraphs.
		 * The algorithm lists each path that contains the two nodes exactly once and each cycles that contains the two nodes exactly length times.
		 *
		 * TODO: Add the possibility to specify a matrix where node pairs are marked that must not be used. Whenever a neighborhood is explored, all set edges must be excluded, i.e., neighbors[u] & ~excluded[u].
		 * Further, whenever a neighborhood of a graph is excluded, all set non-edges must be treated as edges, i.e., forbidden |= neighbors[u] | (excluded[u] & ~neighbors[u]).
		 */
		template<typename F>
		void find_near(Graph const &graph, VertexID uu, VertexID vv, F &callback)
		{
			std::vector<VertexID> path(length);

			if constexpr (length > 2)
			{
				Packed *f = forbidden.data();

				for (size_t i = 0; i < graph.get_row_length(); ++i)
				{
					f[i] = 0;
				}

				f[uu / Packed_Bits] |= (Packed(1) << (uu % Packed_Bits));
				f[vv / Packed_Bits] |= (Packed(1) << (vv % Packed_Bits));
			}

			bool shall_return = false;
			if (graph.has_edge(uu, vv))
			{
				// The easy case: the node pair is an edge. Just place uu, vv as initial node pair at all positions.
				Util::for_<length - 1>([&](auto i)
				{
					if (shall_return) return;

					path[i.value] = uu;
					path[i.value + 1] = vv;

					if (Find_Rec<decltype(callback), i.value, i.value + 1, 0>::find_rec(graph, path, forbidden, callback))
					{
						shall_return = true;
					}
				});

				// TODO: this is only necessary because we list all Cs length times.
				// Eliminate the extra listings from the regular listing!
				if constexpr (with_cycles)
				{
					if (!shall_return)
					{
						path[0] = uu;
						path[length - 1] = vv;

						auto cb = [&]() -> bool
							{
								return callback(path.cbegin(), path.cend());
							};

						Find_Inner_Rec<decltype(cb), 0, length - 1, 0>::find_inner_rec(graph, path, forbidden, cb);
					}
				}
			}
			else if constexpr (length > 2)
			{
				Util::for_<length - 2>([&](auto i)
				{
					if (shall_return) return;

					constexpr size_t distance = i.value + 2;

					if constexpr (std::is_same<Conversion, Options::Conversions::Skip>::value && distance == length - 1)
					{
						return;
					}

					constexpr size_t lf = 0;
					constexpr size_t lb = lf + distance;
					path[lf] = uu;
					path[lb] = vv;

					auto cb = [&forbidden = forbidden, &graph, &path, &callback]() -> bool
					{
						// Mark all nodes in the found part + all neighbors of all inner nodes as forbidden
						// First: inner nodes
						Packed *f = forbidden.data() + (lb - lf) * graph.get_row_length();
						for (size_t i = 0; i < graph.get_row_length(); ++i)
						{
							f[i] = 0;

							for (size_t j = lf + 1; j < lb; ++j)
							{
								f[i] |= graph.get_row(path[j])[i];
							}
						}

						// All nodes in the found part
						for (size_t i = lf; i <= lb; ++i)
						{
							VertexID x = path[i];
							f[x / Packed_Bits] |= (Packed(1) << (x % Packed_Bits));
						}

						bool shall_return = false;

						// Move the found inner part at every possible position in the path and complete outer part
						Util::for_<length - distance>([&](auto j)
						{
							if (shall_return) return;

							constexpr size_t inner_lf = lf + j.value;
							constexpr size_t inner_lb = lb + j.value;

							// Move to the right (right to left to avoid overriding the nodes still to copy
							if constexpr (j.value > 0)
							{
								for (size_t x = lb + j.value; x >= lf + j.value; --x)
								{
									path[x] = path[x - j.value];
								}
							}

							// Recursion to find the outer part
							shall_return = Find_Rec<decltype(callback), inner_lf, inner_lb, (lb - lf)>::find_rec(graph, path, forbidden, callback);

							// Move back to the start (left to right)
							if constexpr (j.value > 0)
							{
								for (size_t x = lf + j.value; x <= lb + j.value; ++x)
								{
									path[x - j.value] = path[x];
								}
							}
						});

						return shall_return;
					};

					if (Find_Inner_Rec<decltype(cb), lf, lb, 0>::find_inner_rec(graph, path, forbidden, cb))
					{
						shall_return = true;
					}
				});
			}

			if constexpr (length > 2)
			{
				Packed *f = forbidden.data();
				f[uu / Packed_Bits] = 0;
				f[vv / Packed_Bits] = 0;
			}
		}

		/**
		 * List all forbidden subgraphs that contain the given node @a uu and @a vv.
		 * The algorithm first completes the inner part between the two nodes (if there is no edge) and then completes the outer part and calls the feeder for all node pairs.
		 *
		 * TODO: Add the possibility to specify a matrix where node pairs are marked that must not be used. Whenever a neighborhood is explored, all set edges must be excluded, i.e., neighbors[u] & ~excluded[u].
		 * Further, whenever a neighborhood of a graph is excluded, all set non-edges must be treated as edges, i.e., forbidden |= neighbors[u] | (excluded[u] & ~neighbors[u]).
		 */
		template<typename Feeder>
		void find_near(Graph const &graph, Graph_Edits const &edited, VertexID uu, VertexID vv, Feeder &feeder, Graph_Edits const *)
		{
			auto callback = [&feeder, &graph, &edited](std::vector<VertexID>::const_iterator uit, std::vector<VertexID>::const_iterator vit) -> bool
					{
						return feeder.callback_near(graph, edited, uit, vit);
					};

			find_near(graph, uu, vv, callback);
		}

	private:

		template <typename F, size_t lf, size_t lb, size_t depth>
		class Find_Inner_Rec
		{
		public:
			/**
			 * Extend a subgraph from two (unconnected) boundary nodes into the center, i.e., find all paths with a certain number of inner nodes but no shortcuts between two given nodes.
			 * Currently the path is only expanded from the node at the front, i.e., lf.
			 * The algorithm needs to maintain an exclude list for all nodes that are already there apart from the innermost two nodes.
			 * For the expansion from one side, the innermost node of the other side needs to be excluded.
			 * In the last step, if there is only a single node left, common neighbors are enumerated.
			 * The given callback @a callback is called without parameters.
			 */
			static bool find_inner_rec(Graph const &graph, std::vector<VertexID> &path, std::vector<Packed> &forbidden, F &callback)
			{
				constexpr size_t remaining_length = lb - lf;

				static_assert(remaining_length > 1, "find_inner_rec called with no space left in path");
				Packed *f = forbidden.data() + depth * graph.get_row_length();


				// Base case: find a common neighbor
				if constexpr (lb - lf == 2)
				{
					for (size_t i = 0; i < graph.get_row_length(); ++i)
					{
						Packed cur = graph.get_row(path[lf])[i] & graph.get_row(path[lb])[i] & ~f[i];

						while (cur)
						{
							const VertexID lzcur = PACKED_CTZ(cur);
							// Unset bit of vf in curf to advance loop
							cur &= ~(Packed(1) << lzcur);
							const VertexID v = lzcur + i * Packed_Bits;

							path[lf + 1] = v;

							if(callback())
							{
								return true;
							}
						}

					}

				}
				else
				{
					// TODO: this generically goes lf -> lf+1. However, similar to bidirectional dijkstra, it would be better to recurse also with lb -> lb + 1 (note that this is only useful for length > 4 though).
					Packed *nf = f + graph.get_row_length();

					for (size_t i = 0; i < graph.get_row_length(); ++i)
					{
						nf[i] = f[i] | graph.get_row(path[lf])[i];
					}

					for (size_t i = 0; i < graph.get_row_length(); ++i)
					{
						Packed cur = graph.get_row(path[lf])[i] & ~graph.get_row(path[lb])[i] & ~f[i];

						while (cur)
						{
							const VertexID lzcur = PACKED_CTZ(cur);
							// Unset bit of vf in curf to advance loop
							cur &= ~(Packed(1) << lzcur);
							const VertexID v = lzcur + i * Packed_Bits;

							path[lf + 1] = v;

							if (Find_Inner_Rec<F, lf + 1, lb, depth + 1>::find_inner_rec(graph, path, forbidden, callback))
							{
								return true;
							}
						}
					}
				}

				return false;
			}

		};

		template<typename F, size_t lf, size_t lb, size_t depth>
		class Find_Rec
		{
		public:
			/*
			 * Expand the inner part between path[lf]..path[lb] into all possible full forbidden subgraphs path[0]..path[length-1].
			 * This works for arbitrary lf and lb, only a single initial edge is required.
			 * Every recursive call extends the path in one direction by a single node, the expansion always happens in the direction
			 * where more nodes are missing. The function assumes that all nodes that shall be excluded, in particular the nodes
			 * already in the path path[lf]..path[lb] and all neighbors of all inner nodes are marked in forbidden in the row at
			 * depth (template parameter). The function uses the next row in forbidden for the next recursive call.
			 */
			static bool find_rec(Graph const &graph, std::vector<VertexID> &path, std::vector<Packed> &forbidden, F &callback)
			{
				static_assert(lf > 0 || lb < length - 1);
				constexpr size_t remaining_length = length - (lb - lf) - 1;
				static_assert(remaining_length  > 0);
				constexpr bool expand_front = (length - lb - 1 <= lf);
				constexpr size_t next_lf = expand_front ? lf - 1 : lf;
				constexpr size_t next_lb = expand_front ? lb : lb + 1;
				constexpr size_t base_index = expand_front ? lf : lb;
				constexpr size_t opposed_index = expand_front ? lb : lf;
				constexpr size_t next_index = expand_front ? next_lf : next_lb;
				const VertexID base_node = path[base_index];
				const VertexID opposed_node = path[opposed_index];


				Packed *f = forbidden.data() + depth * graph.get_row_length();

				{
					if constexpr (remaining_length > 1)
					{
						Packed *nf = f + graph.get_row_length();
						for(size_t i = 0; i < graph.get_row_length(); i++)
						{
							nf[i] = graph.get_row(base_node)[i] | f[i];
						}
					}

					// Find the next node that is adjacent to base_node but not opposed_node unless this is the last node and we allow cycles
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						Packed curf = graph.get_row(base_node)[i] & ~f[i];
						if constexpr (remaining_length > 1 || !with_cycles)
						{
							// ensure there is no edge vf, opposed_node by excluding neighbors of opposed_node
							curf &= ~graph.get_row(opposed_node)[i];
						}

						(void)opposed_node;

						while(curf)
						{
							const VertexID lzcurf = PACKED_CTZ(curf);
							// Unset bit of vf in curf to advance loop
							curf &= ~(Packed(1) << lzcurf);
							const VertexID vf = lzcurf + i * Packed_Bits;

							path[next_index] = vf;

							// Due to the exclusion of f, and f containing all already selected nodes, the two nodes cannot be the same
							assert(vf != opposed_node);
							// Further, we have ensured that there is no edge between uf and opposed_node if lf > 1.
							assert(remaining_length == 1 || !graph.has_edge(vf, opposed_node));

							if(Find_Rec<F, next_lf, next_lb, depth + 1>::find_rec(graph, path, forbidden, callback))
							{
								return true;
							}
						}
					}
				}
				return false;
			}
		};

		template<typename F, size_t depth>
		class Find_Rec<F, 0, length - 1, depth>
		{
		public:
			static bool find_rec(Graph const &g, std::vector<VertexID> &path, std::vector<Packed> &, F &callback)
			{
				(void)g;

				for (VertexID u = 0; u < length - 1; ++u)
				{
					assert(g.has_edge(path[u], path[u+1]));
					for (VertexID v = u + 2; v < length; ++v)
					{
						assert((u == 0 && v == length -1 && with_cycles) || !g.has_edge(path[u], path[v]));
					}
				}

				return callback(path.cbegin(), path.cend());
			}
		};

	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_P2 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 2, false>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 2, false>;
	public:
		static constexpr char const *name = "Center_P2";
		Center_P2(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_P3 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 3, false>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 3, false>;
	public:
		static constexpr char const *name = "Center_P3";
		Center_P3(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_4 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 4, true>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 4, true>;
	public:
		static constexpr char const *name = "Center_4";
		Center_4(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_P4 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 4, false>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 4, false>;
	public:
		static constexpr char const *name = "Center_P4";
		Center_P4(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_5 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 5, true>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 5, true>;
	public:
		static constexpr char const *name = "Center_5";
		Center_5(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_P5 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 5, false>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 5, false>;
	public:
		static constexpr char const *name = "Center_P5";
		Center_P5(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_6 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 6, true>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 6, true>;
	public:
		static constexpr char const *name = "Center_6";
		Center_6(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_P6 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 6, false>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 6, false>;
	public:
		static constexpr char const *name = "Center_P6";
		Center_P6(VertexID graph_size) : Parent(graph_size) {;}
	};
}

#endif
