#ifndef FINDER_CENTER_HPP
#define FINDER_CENTER_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"
#include "../Options.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t _length, bool _with_cycles = (_length > 3)>
	class Center
	{
		static_assert(_length > 1, "Can only detect path/cycles with at least 2 vertices");
		static_assert(_length > 3 || !_with_cycles, "Cycles are only supported with length at least 4");
		static_assert(_with_cycles || !std::is_same<Conversion, Options::Conversions::Skip>::value, "Without cycles, there is no conversion and thus nothing must be skipped");

	public:
		static constexpr char const *name = "Center";
		static constexpr size_t length = _length;
		static constexpr bool with_cycles = _with_cycles;

	private:
		std::vector<Packed> forbidden;

	public:
		Center(VertexID graph_size) : forbidden(Graph::alloc_rows(graph_size, length)) {;}

		template<typename Feeder>
		void find(Graph const &graph, Graph_Edits const &edited, Feeder &feeder)
		{
			assert(forbidden.size() / graph.get_row_length() == length);
			std::vector<VertexID> path(length);

			auto callback = [&feeder, &graph, &edited](std::vector<VertexID>::const_iterator uit, std::vector<VertexID>::const_iterator vit) -> bool
					{
						return feeder.callback(graph, edited, uit, vit);
					};

			constexpr bool length_even = (length % 2 == 0);
			Packed *f = forbidden.data() + (length_even ? 1 : 2) * graph.get_row_length();
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
									if(Find_Rec<decltype(callback), length / 2 - 1, length / 2 + 1>::find_rec(graph, path, forbidden, callback)) {return;}
								}
							}
						}
					}
				}
			}
			else
			{
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
							if(Find_Rec<decltype(callback), length / 2 - 1, length / 2>::find_rec(graph, path, forbidden, callback)) {return;}
							// Unset v in f
							if constexpr (length > 2) f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
						}
					}
					if constexpr (length > 2) f[u / Packed_Bits] = 0;
				}
			}
		}

		template<typename Feeder>
		void find_near(Graph const &graph, Graph_Edits const &edited, VertexID uu, VertexID vv, Feeder &feeder, Graph_Edits const *)
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);
			std::vector<VertexID> path(length);

			auto callback = [&feeder, &graph, &edited](std::vector<VertexID>::const_iterator uit, std::vector<VertexID>::const_iterator vit) -> bool
					{
						return feeder.callback_near(graph, edited, uit, vit);
					};


			Packed *f = forbidden.data() + (length / 2 - 2) * graph.get_row_length();
			/* only search for forbidden subgraphs near u-v
			 * -> both vertices of starting edge must be within length / 2 of u or v
			 */

			std::vector<Packed> start = graph.alloc_rows(3);//start vertices and neighbourhoods included
			Packed *area = start.data();
			Packed *included = start.data() + graph.get_row_length();
			Packed *adding = start.data() + 2 * graph.get_row_length();
			area[uu / Packed_Bits] |= Packed(1) << (uu % Packed_Bits);
			area[vv / Packed_Bits] |= Packed(1) << (vv % Packed_Bits);

			// BFS of depth	length/2 starting at uu/vv. All visited nodes are marked in area.
			// included contains all vertices already seen.
			// adding contains the next layer(s)
			for(size_t rounds = 0; rounds < length / 2; rounds++)
			{
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(Packed cur = area[i] & ~included[i]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
					{
						VertexID add = PACKED_CTZ(cur) + i * Packed_Bits;
						included[add / Packed_Bits] |= Packed(1) << (add % Packed_Bits);

						// Explore neighbors  of "add" for adding
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							adding[j] |= graph.get_row(add)[j];
						}
					}
				}
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					area[i] |= adding[i];
				}
			}

			if constexpr (length & 1U)
			{
				// does not work for length == 3
				for(size_t ii = 0; ii < graph.get_row_length(); ii++)
				{
					for(Packed cur = area[ii]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
					{
						VertexID u = PACKED_CTZ(cur) + ii * Packed_Bits;
						for(size_t i = 0; i < graph.get_row_length(); i++)
						{
							f[i] = graph.get_row(u)[i];
						}
						f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
						path[length / 2] = u;
						for(size_t i = 0; i < graph.get_row_length(); i++)
						{
							for(Packed curf = graph.get_row(u)[i] & area[i]; curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
							{
								VertexID vf = PACKED_CTZ(curf) + i * Packed_Bits;
								path[length / 2 - 1] = vf;
								for(size_t j = i; j < graph.get_row_length(); j++)
								{
									for(Packed curb = j == i? curf & ~(Packed(1) << PACKED_CTZ(curf)) : graph.get_row(u)[j]; curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
									{
										VertexID vb = PACKED_CTZ(curb) + j * Packed_Bits;
										if(graph.has_edge(vf, vb)) {continue;}
										path[length / 2 + 1] = vb;
										if(Find_Rec<decltype(callback), length / 2 - 1, length / 2 + 1>::find_rec(graph, path, forbidden, callback)) {return;}
									}
								}
							}
						}
					}
				}
			}
			else
			{
				for(size_t ii = 0; ii < graph.get_row_length(); ii++)
				{
					for(Packed curs = area[ii]; curs; curs &= ~(Packed(1) << PACKED_CTZ(curs)))
					{
						VertexID u = PACKED_CTZ(curs) + ii * Packed_Bits;
						f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
						path[length / 2 - 1] = u;
						for(size_t i = u / Packed_Bits; i < graph.get_row_length(); i++)// double exploration: i = 0  cur = graph.get_row(u)[i]
						{
							for(Packed cur = i == u / Packed_Bits? graph.get_row(u)[i] & ~area[i] & ~((Packed(2) << (u % Packed_Bits)) - 1) : graph.get_row(u)[i] & ~area[i]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
							{
								VertexID v = PACKED_CTZ(cur) + i * Packed_Bits;
								f[v / Packed_Bits] |= Packed(1) << (v % Packed_Bits);
								path[length / 2] = v;
								if(Find_Rec<decltype(callback), length / 2 - 1, length / 2>::find_rec(graph, path, forbidden, callback)) {return;}
								f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
							}
						}
						f[u / Packed_Bits] = 0;
					}
				}
			}
		}

	private:

		template<typename F, size_t lf, size_t lb>
		class Find_Rec
		{
		public:
			static bool find_rec(Graph const &graph, std::vector<VertexID> &path, std::vector<Packed> &forbidden, F &callback)
			{
				static_assert(lf > 0 || lb < length - 1);
				constexpr size_t remaining_length = length - (lb - lf) - 1;
				static_assert(remaining_length  > 0);
				constexpr size_t depth = lb - lf;
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

							if(Find_Rec<F, next_lf, next_lb>::find_rec(graph, path, forbidden, callback))
							{
								return true;
							}
						}
					}
				}
				return false;
			}
		};

		template<typename F>
		class Find_Rec<F, 0, length - 1>
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
