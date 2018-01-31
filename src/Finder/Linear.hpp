#ifndef FINDER_LINEAR_HPP
#define FINDER_LINEAR_HPP

#include <assert.h>

#include <iomanip>
#include <iostream>

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
			//urgh... this needs to be done differently
			// sort vertices by degree, decreasing
			std::vector<std::pair<std::vector<Packed>, size_t>> L;
			for(VertexID u = 0; u < graph.size(); u++)
			{
				size_t deg = graph.degree(u);
				if(L.size() <= deg) {L.resize(deg + 1, {graph.alloc_rows(1), 0});}
				L[deg].first[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
				L[deg].second++;
			}
			for(size_t i = L.size(); i > 0; i--)
			{
				if(L[i - 1].second == 0) {L.erase(L.begin() + (i - 1));}
			}

/*			std::cout << "deg asc:\n";
			for(size_t i = 0; i < L.size(); i++)
			{
				std::cout << std::setw(3) << +i << ':';
				for(auto const &x: L[i].first) {std::cout << ' ' << std::hex << std::setw(16) << std::setfill('0') << x;}
				std::cout << std::setfill(' ') << std::dec << " (" << +L[i].second << ")\n";
			}
*/
			std::vector<Packed> out = graph.alloc_rows(1);

			for(VertexID i = 0; i < graph.size(); i++)
			{
				VertexID x;
				for(size_t j = 0; j < graph.get_row_length(); j++)
				{
					for(Packed px = L.back().first[j]; px; px &= ~(Packed(1) << __builtin_ctzll(px)))
					{
						x = __builtin_ctzll(px) + j * Packed_Bits;
						L.back().first[j] &= ~(Packed(1) << __builtin_ctzll(px));
						L.back().second--;

//						std::cout << "cur: " << +x << " (L[" << L.size() - 1 << "])\n";

						goto found_x;
					}
				}
				i--;
				L.pop_back();
				continue;

				found_x:;
				out[x / Packed_Bits] |= Packed(1) << (x % Packed_Bits);

/*				std::cout << "done:";
				for(auto const &x: out) {std::cout << ' ' << std::hex << std::setw(16) << std::setfill('0') << x;}
				std::cout << std::setfill(' ') << std::dec << '\n';
*/
				for(size_t idx = L.size(); idx > 0; idx--)
				{
					auto &val = L[idx - 1];
					std::vector<Packed> p = graph.alloc_rows(1);
					size_t count = 0;
					for(size_t j = 0; j < graph.get_row_length(); j++)
					{
						p[j] = val.first[j] & graph.get_row(x)[j];
						count += __builtin_popcount(p[j]);
					}

					if(count != 0)
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
								if(z == x) {continue;}
								std::vector<VertexID> problem{z, w, x, y};
								//std::cout << "found " << +z << ' ' << +w << ' ' << +x << ' ' << +y << '\n';
								feeder.callback(graph, edited, problem.begin(), problem.end());
								return;
							}
						}
					}
					/*else
					{
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							val.first[j] &= ~p[j];
							val.second -= count;
						}
						L.insert(L.begin() + idx, {p, count});
					}*/
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
