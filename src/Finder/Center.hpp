#ifndef FINDER_CENTER_HPP
#define FINDER_CENTER_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t _length>
	class Center
	{
		static_assert(_length > 3, "Can only detect path/cycles with at least 4 vertices");

	public:
		static constexpr char const *name = "Center";
		static constexpr size_t length = _length;

	private:
		std::vector<Packed> forbidden;

	public:
		Center(VertexID graph_size) : forbidden(Graph::alloc_rows(graph_size, length / 2 - 1)) {;}

		template<typename Feeder>
		void find(Graph const &graph, Graph_Edits const &edited, Feeder &feeder)
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);
			std::vector<VertexID> path(length);

			auto callback = [&feeder, &graph, &edited](std::vector<VertexID>::const_iterator uit, std::vector<VertexID>::const_iterator vit) -> bool
					{
						return feeder.callback(graph, edited, uit, vit);
					};

			Packed *f = forbidden.data() + (length / 2 - 2) * graph.get_row_length();
			if(length & 1U) // uneven length
			{
				// does not work for length == 3
				for(VertexID u = 0; u < graph.size(); u++)
				{
					// Mark u and neighbors of u in f
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						f[i] = graph.get_row(u)[i];
					}
					f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
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
					f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
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
							f[v / Packed_Bits] |= Packed(1) << (v % Packed_Bits);
							path[length / 2] = v;
							// Path now contains the two node u and v
							if(Find_Rec<decltype(callback), length / 2 - 1, length / 2>::find_rec(graph, path, forbidden, callback)) {return;}
							// Unset v in f
							f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
						}
					}
					f[u / Packed_Bits] = 0;
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
			for(size_t rounds = 0; rounds < length / 2; rounds++)
			{
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(Packed cur = area[i] & ~included[i]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
					{
						VertexID add = PACKED_CTZ(cur) + i * Packed_Bits;
						included[add / Packed_Bits] |= Packed(1) << (add % Packed_Bits);
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

			if(length & 1U)
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
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				{
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						nf[i] = graph.get_row(uf)[i] | graph.get_row(ub)[i] | f[i];
					}
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i]; curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
						{
							VertexID vf = PACKED_CTZ(curf) + i * Packed_Bits;
							path[lf - 1] = vf;
							for(size_t j = 0; j < graph.get_row_length(); j++)
							{
								for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j]; curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
								{
									VertexID vb = PACKED_CTZ(curb) + j * Packed_Bits;
									if(vf == vb || graph.has_edge(vf, vb)) {continue;}
									path[lb + 1] = vb;
									if(Find_Rec<F, lf - 1, lb + 1>::find_rec(graph, path, forbidden, callback))
									{
										return true;
									}
								}
							}
						}
					}
				}
				return false;
			}
		};

		template<typename F, size_t lb>
		class Find_Rec<F, 1, lb>
		{
		public:
			static bool find_rec(Graph const &graph, std::vector<VertexID> &path, std::vector<Packed> &forbidden, F &callback)
			{
				constexpr size_t lf = 1;
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				//Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				if(lf == 1)
				{
					/* last vertices */
					// Find two vertices vf/vb that are adjacent to uf/ub but not adjacent to ub/uf and that are not marked in f
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i]; curf;)
						{
							const VertexID lzcurf = PACKED_CTZ(curf);
							// Unset bit of vf in curf to advance loop
							curf &= ~(Packed(1) << lzcurf);
							const VertexID vf = lzcurf + i * Packed_Bits;

							path[lf - 1] = vf;
							for(size_t j = 0; j < graph.get_row_length(); j++)
							{
								for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j]; curb;)
								{
									const VertexID lzcurb = PACKED_CTZ(curb);
									// Unset bit of vb in curb to advance loop
									curb &= ~(Packed(1) << lzcurb);
									const VertexID vb = lzcurb + j * Packed_Bits;
									// Due to the exclusion of the neighborhood of ub/uf, the two vertices cannot be adjacent
									assert (vf != vb);
									path[lb + 1] = vb;
									if(callback(path.cbegin(), path.cend())) {return true;}
								}
							}
						}
					}
				}
				return false;
			}
		};
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_4 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 4>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 4>;
	public:
		static constexpr char const *name = "Center_4";
		Center_4(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_5 : public Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 5>
	{
	private: using Parent = Center<Graph, Graph_Edits, Mode, Restriction, Conversion, 5>;
	public:
		static constexpr char const *name = "Center_5";
		Center_5(VertexID graph_size) : Parent(graph_size) {;}
	};

}

#endif
