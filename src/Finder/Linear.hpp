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
	template<typename Graph>
	class Linear
	{
	public:
		using subgraph_t = std::array<VertexID, 4>;
		static constexpr char const *name = "Linear";
		static constexpr size_t length = 4;

	public:
		bool is_quasi_threshold(Graph const &graph, subgraph_t& certificate)
		{
			VertexID n = graph.size();
			std::vector<VertexID> sortedNodes(n);

			// counting sort by degree in decreasing order
			{
				std::vector<VertexID> nodePos(n + 1, 0);
				std::vector<VertexID> key(n);

				for (VertexID u = 0; u < n; ++u) {
					key[u] = n - graph.degree(u);
					++nodePos[key[u]];
				}

				// exclusive prefix sum
				VertexID tmp = nodePos[0];
				VertexID sum = tmp;
				nodePos[0] = 0;

				for (VertexID i = 1; i < nodePos.size(); ++i) {
					tmp = nodePos[i];
					nodePos[i] = sum;
					sum += tmp;
				}

				for (VertexID u = 0; u < n; ++u) {
					sortedNodes[nodePos[key[u]]++] = u;
				}

				assert(std::is_sorted(sortedNodes.begin(), sortedNodes.end(), [&](VertexID u, VertexID v) {
					return graph.degree(u) > graph.degree(v);
				}));
			}

			std::vector<VertexID> parent(n, n);
			std::vector<bool> processed(n, false);

			for (VertexID u : sortedNodes) {
				processed[u] = true;

				bool found_problem = graph.for_neighbours(u, [&](VertexID v) {
					// node v has already been removed
					if (processed[v]) return false;

					if (parent[v] == parent[u]) {
						parent[v] = u;
						return false;
					} else {
						if (parent[u] == n || graph.has_edge(parent[u], v)) {
							certificate[1] = parent[v];
							certificate[2] = v;
							certificate[3] = u;
							assert(certificate[1] != certificate[3]);
						} else {
							certificate[1] = parent[u];
							certificate[2] = u;
							certificate[3] = v;
							assert(certificate[1] != certificate[3]);
						}

						graph.for_neighbours(certificate[1], [&](VertexID z) {
							if (z != certificate[2] && !graph.has_edge(z, certificate[2])) {
								certificate[0] = z;
								return true;
							}

							return false;
						});

						return true;
					}
				});

				if (found_problem) {
					return false;
				}
			}

			return true;
		}
	};
}

#endif
