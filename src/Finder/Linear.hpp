#ifndef FINDER_LINEAR_HPP
#define FINDER_LINEAR_HPP

#include <assert.h>

#include <unordered_set>
#include <vector>

#include "../config.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits, size_t length>
	class Linear
	{
		static_assert(length == 4, "Can only detect P4/C4");

	public:
		static constexpr char const *name = "Linear";

	public:
		Linear(Graph const &) {;}

		template<typename Feeder>
		void find(Graph const &graph, Graph_Edits const &edited, Feeder &feeder)
		{
			// sort vertices by degree, decreasing
			std::vector<std::vector<Packed>> L;
			for(VertexID u = 0; u < graph.size(); u++)
			{
				size_t deg = graph.degree(u);
				if(L.size() <= deg) {L.resize(deg + 1, graph.alloc_rows(1));}
				L[deg][u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
			}

			std::vector<Packed> out = graph.alloc_rows(1);

			for(VertexID i = 0; i < graph.size(); i++)
			{
				VertexID x;
				for(size_t j = 0; j < graph.get_row_length(); j++)
				{
					for(Packed px = L.back()[j]; px; px &= ~(Packed(1) << __builtin_ctzll(px)))
					{
						x = __builtin_ctzll(px) + j * Packed_Bits;
						goto found_x;
					}
				}
				i--;
				L.pop_back();
				continue;

				found_x:;
				out[x / Packed_Bits] |= Packed(1) << (x % Packed_Bits);

				for(size_t idx = L.size(); idx > 0; idx--)
				{
					auto &val = L[idx - 1];
					std::vector<Packed> p = graph.alloc_rows(1);
					size_t count = 0;
					for(size_t j = 0; j < graph.get_row_length(); j++)
					{
						p[j] = val[j] & graph.get_row(x)[j];
						count += __builtin_popcount(p[j]);
					}

					if(count == 0)
					{
						VertexID y = ~0;
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(Packed py = p[j]; py; py &= ~(Packed(1) << __builtin_ctzll(py)))
							{
								y = __builtin_ctzll(py) + j * Packed_Bits;
								goto found_y;
							}
						}
						found_y:;
						std::vector<Packed> S = graph.alloc_rows(1);
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							S[j] = out[j] & graph.get_row(x)[j] & ~graph.get_row(y)[j];
						}
						VertexID w = ~0;
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(Packed pw = S[j]; pw; pw &= ~(Packed(1) << __builtin_ctzll(pw)))
							{
								w = __builtin_ctzll(pw) + j * Packed_Bits;
								goto found_w;
							}
						}
						found_w:;
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(Packed pz = graph.get_row(w)[j] & ~graph.get_row(x)[j]; pz; pz &= ~(Packed(1) << __builtin_ctzll(pz)))
							{
								VertexID z = __builtin_ctzll(pz) + j * Packed_Bits;
								std::vector<VertexID> problem{z, w, x, y};
								feeder.callback(graph, edited, problem.begin(), problem.end());
								return;
							}
						}
					}
					for(size_t j = 0; j < graph.get_row_length(); j++)
					{
						val[j] &= ~p[j];
					}
					L.insert(L.begin() + idx, p);
				}
			}
		}
	};

	template<typename Graph, typename Graph_Edits>
	class Linear_4 : public Linear<Graph, Graph_Edits, 4>
	{
	private: using Parent = Linear<Graph, Graph_Edits, 4>;
	public:
		static constexpr char const *name = "Linear_4";
		Linear_4(Graph const &graph) : Parent(graph) {;}
	};
}

#endif
