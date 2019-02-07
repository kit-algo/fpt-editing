#ifndef CONSUMER_SELECTOR_SINGLE_INDEPENDENT_SET_HPP
#define CONSUMER_SELECTOR_SINGLE_INDEPENDENT_SET_HPP

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include <iostream>

#include "../util.hpp"

#include "../config.hpp"

#include "../Options.hpp"

#include "../Finder/Finder.hpp"
#include "../Finder/Center.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Graph/ValueMatrix.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Single_Most : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "Single_Most";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		Value_Matrix<size_t> num_subgraphs_per_edge;
		std::vector<VertexID> fallback;
		size_t fallback_free = 0;
		bool use_single;
	public:
		Single_Most(VertexID graph_size) : num_subgraphs_per_edge(graph_size) {;}

		void prepare(size_t no_edits_left, const Lower_Bound_Storage_type&)
		{
			use_single = (no_edits_left > 0);
			num_subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, size_t& v) { v = 0; });
			fallback.clear();
			fallback_free = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			size_t free = 0;
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				++num_subgraphs_per_edge.at(*uit, *vit);
				free++;
				return false;
			});

			// fallback handling
			if(free < fallback_free || fallback_free == 0)
			{
				fallback_free = free;
				fallback = std::vector<VertexID>{b, e};
				if(free == 0) {return true;} // completly edited subgraph -- impossible to solve graph
			}

			return false;
		}


		std::vector<VertexID> result(size_t, Graph const &, Graph_Edits const &, Options::Tag::Selector)
		{
			if(fallback.empty() || fallback_free == 0)
			{
				// a solved graph?! or we found a completly edited/marked subgraph
				return fallback;
			}

			if (use_single) {
				size_t max_subgraphs = 0;
				std::pair<VertexID, VertexID> node_pair;
				num_subgraphs_per_edge.forAllNodePairs([&](VertexID u, VertexID v, size_t& num_fbs) {
					if (num_fbs > max_subgraphs) {
						max_subgraphs = num_fbs;
						node_pair.first = u;
						node_pair.second = v;
					}
				});

				if (max_subgraphs > 1) { // only use single editing if there is a node pair that is part of multiple forbidden subgraphs
					return std::vector<VertexID>{node_pair.first, node_pair.second};
				}
			}

			return fallback;
		}
	};
}

#endif
