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
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t _length>
	class Linear
	{
		static_assert(_length == 4, "Can only detect P4/C4");

	public:
		static constexpr char const *name = "Linear";
		static constexpr size_t length = _length;

	public:
		Linear(VertexID) {;}

		bool is_quasi_threshold(Graph const &graph)
		{
			VertexID n = graph.size();
			std::vector<VertexID> sortedNodes(n);

			// counting sort by degree in decreasing order
			{
				std::vector<VertexID> nodePos(n + 1, 0);

				for (VertexID u = 0; u < n; ++u) {
					sortedNodes[u] = n - graph.degree(u);
					++nodePos[sortedNodes[u]];
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
					VertexID deg = sortedNodes[u];
					sortedNodes[nodePos[deg]++] = u;
				}
			}

			std::vector<VertexID> parent(n, n);
			std::vector<bool> processed(n, false);

			for (VertexID u : sortedNodes) {
				processed[u] = true;

				bool found_problem = graph.for_neighbors(u, [&](VertexID v) {
					// node v has already been removed
					if (processed[v]) return false;

					if (parent[v] == parent[u]) {
						parent[v] = u;
						return false;
					} else {
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

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Linear_4 : public Linear<Graph, Graph_Edits, Mode, Restriction, Conversion, 4>
	{
	private: using Parent = Linear<Graph, Graph_Edits, Mode, Restriction, Conversion, 4>;
	public:
		static constexpr char const *name = "Linear_4";
		Linear_4(VertexID graph_size) : Parent(graph_size) {;}
	};
}

#endif
