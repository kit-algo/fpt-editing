#ifndef FINDER_CENTER_HPP
#define FINDER_CENTER_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits, size_t length>
	class Center
	{
		static_assert(length > 3, "Can only detect path/cycles with at least 4 vertices");

	public:
		static constexpr char const *name = "Center";

	private:
		std::vector<Packed> forbidden;

	public:
		Center(Graph const &graph) : forbidden(graph.alloc_rows(length / 2 - 1)) {;}

		template<typename Feeder>
		void find(Graph const &graph, Graph_Edits const &edited, Feeder &feeder)
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);
			std::vector<VertexID> path(length);

			Packed *f = forbidden.data() + (length / 2 - 2) * graph.get_row_length();
			if(length & 1U)
			{
				// does not work for length == 3
				for(VertexID u = 0; u < graph.size(); u++)
				{
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						f[i] = graph.get_row(u)[i];
					}
					f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
					path[length / 2] = u;
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(u)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
						{
							VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
							path[length / 2 - 1] = vf;
							for(size_t j = i; j < graph.get_row_length(); j++)
							{
								for(Packed curb = j == i? curf & ~(Packed(1) << __builtin_ctzll(curf)) : graph.get_row(u)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
								{
									VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
									if(graph.has_edge(vf, vb)) {continue;}
									path[length / 2 + 1] = vb;
									if(Find_Rec<Feeder, length / 2 - 1, length / 2 + 1, false>::find_rec(graph, edited, path, forbidden, feeder)) {return;}
								}
							}
						}
					}
				}
			}
			else
			{
				for(VertexID u = 0; u < graph.size(); u++)
				{
					f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
					path[length / 2 - 1] = u;
					for(size_t i = u / Packed_Bits; i < graph.get_row_length(); i++)// double exploration: i = 0  cur = graph.get_row(u)[i]
					{
						for(Packed cur = i == u / Packed_Bits? graph.get_row(u)[i] & ~((Packed(2) << (u % Packed_Bits)) - 1) : graph.get_row(u)[i]; cur; cur &= ~(Packed(1) << __builtin_ctzll(cur)))
						{
							VertexID v = __builtin_ctzll(cur) + i * Packed_Bits;
							f[v / Packed_Bits] |= Packed(1) << (v % Packed_Bits);
							path[length / 2] = v;
							if(Find_Rec<Feeder, length / 2 - 1, length / 2, false>::find_rec(graph, edited, path, forbidden, feeder)) {return;}
							f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
						}
					}
					f[u / Packed_Bits] = 0;
				}
			}
		}

		template<typename Feeder>
		void find_near(Graph const &graph, Graph_Edits const &edited, VertexID uu, VertexID vv, Feeder &feeder)
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);
			std::vector<VertexID> path(length);

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
					for(Packed cur = area[i] & ~included[i]; cur; cur &= ~(Packed(1) << __builtin_ctzll(cur)))
					{
						VertexID add = __builtin_ctzll(cur) + i * Packed_Bits;
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
					for(Packed cur = area[ii]; cur; cur &= ~(Packed(1) << __builtin_ctzll(cur)))
					{
						VertexID u = __builtin_ctzll(cur) + ii * Packed_Bits;
						for(size_t i = 0; i < graph.get_row_length(); i++)
						{
							f[i] = graph.get_row(u)[i];
						}
						f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
						path[length / 2] = u;
						for(size_t i = 0; i < graph.get_row_length(); i++)
						{
							for(Packed curf = graph.get_row(u)[i] & area[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
							{
								VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
								path[length / 2 - 1] = vf;
								for(size_t j = i; j < graph.get_row_length(); j++)
								{
									for(Packed curb = j == i? curf & ~(Packed(1) << __builtin_ctzll(curf)) : graph.get_row(u)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
									{
										VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
										if(graph.has_edge(vf, vb)) {continue;}
										path[length / 2 + 1] = vb;
										if(Find_Rec<Feeder, length / 2 - 1, length / 2 + 1, true>::find_rec(graph, edited, path, forbidden, feeder)) {return;}
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
					for(Packed curs = area[ii]; curs; curs &= ~(Packed(1) << __builtin_ctzll(curs)))
					{
						VertexID u = __builtin_ctzll(curs) + ii * Packed_Bits;
						f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
						path[length / 2 - 1] = u;
						for(size_t i = u / Packed_Bits; i < graph.get_row_length(); i++)// double exploration: i = 0  cur = graph.get_row(u)[i]
						{
							for(Packed cur = i == u / Packed_Bits? graph.get_row(u)[i] & ~area[i] & ~((Packed(2) << (u % Packed_Bits)) - 1) : graph.get_row(u)[i] & ~area[i]; cur; cur &= ~(Packed(1) << __builtin_ctzll(cur)))
							{
								VertexID v = __builtin_ctzll(cur) + i * Packed_Bits;
								f[v / Packed_Bits] |= Packed(1) << (v % Packed_Bits);
								path[length / 2] = v;
								if(Find_Rec<Feeder, length / 2 - 1, length / 2, true>::find_rec(graph, edited, path, forbidden, feeder)) {return;}
								f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
							}
						}
						f[u / Packed_Bits] = 0;
					}
				}
			}
		}

	private:

		template<typename Feeder, size_t lf, size_t lb, bool near>
		class Find_Rec
		{
		public:
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder)
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
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
						{
							VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
							path[lf - 1] = vf;
							for(size_t j = 0; j < graph.get_row_length(); j++)
							{
								for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
								{
									VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
									if(vf == vb || graph.has_edge(vf, vb)) {continue;}
									path[lb + 1] = vb;
									if(Find_Rec<Feeder, lf - 1, lb + 1, near>::find_rec(graph, edited, path, forbidden, feeder))
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

		template<typename Feeder, size_t lb>
		class Find_Rec<Feeder, 1, lb, false>
		{
		public:
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder)
			{
				constexpr size_t lf = 1;
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				//Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				if(lf == 1)
				{
					/* last vertices */
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
						{
							VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
							path[lf - 1] = vf;
							for(size_t j = 0; j < graph.get_row_length(); j++)
							{
								for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
								{
									VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
									if(vf == vb) {continue;}
									path[lb + 1] = vb;
									if(feeder.callback(graph, edited, path.cbegin(), path.cend())) {return true;}
								}
							}
						}
					}
				}
				return false;
			}
		};

		template<typename Feeder, size_t lb>
		class Find_Rec<Feeder, 1, lb, true>
		{
		public:
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder)
			{
				constexpr size_t lf = 1;
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				//Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				if(lf == 1)
				{
					/* last vertices */
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
						{
							VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
							path[lf - 1] = vf;
							for(size_t j = 0; j < graph.get_row_length(); j++)
							{
								for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
								{
									VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
									if(vf == vb) {continue;}
									path[lb + 1] = vb;
									if(feeder.callback_near(graph, edited, path.cbegin(), path.cend())) {return true;}
								}
							}
						}
					}
				}
				return false;
			}
		};
	};

	template<typename Graph, typename Graph_Edits>
	class Center_4 : public Center<Graph, Graph_Edits, 4>
	{
	private: using Parent = Center<Graph, Graph_Edits, 4>;
	public:
		static constexpr char const *name = "Center_4";
		Center_4(Graph const &graph) : Parent(graph) {;}
	};

	template<typename Graph, typename Graph_Edits>
	class Center_5 : public Center<Graph, Graph_Edits, 5>
	{
	private: using Parent = Center<Graph, Graph_Edits, 5>;
	public:
		static constexpr char const *name = "Center_5";
		Center_5(Graph const &graph) : Parent(graph) {;}
	};

}

#endif
