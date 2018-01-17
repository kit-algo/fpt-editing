#ifndef FINDER_CENTER_MATRIX_HPP
#define FINDER_CENTER_MATRIX_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits>
	class Center_Matrix
	{
	public:
		static constexpr char const *name = "Center_Matrix";

	private:
		size_t const length;

		std::vector<Packed> forbidden;

	public:
		Center_Matrix(Graph const &graph, size_t const length) : length(length), forbidden(graph.alloc_rows(length / 2 - 1)) {;}

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
									if(find_rec(graph, edited, path, length / 2 - 1, length / 2 + 1, feeder)) {return;}
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
							if(find_rec(graph, edited, path, length / 2 - 1, length / 2, feeder)) {return;}
							f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
						}
					}
					f[u / Packed_Bits] = 0;
				}
			}
		}

	private:
		template<typename Feeder>
		bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, size_t lf, size_t lb, Feeder &feeder)
		{
			Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
			Packed *nf = f - graph.get_row_length();
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
			else
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
								if(find_rec(graph, edited, path, lf - 1, lb + 1, feeder))
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
}

#endif
