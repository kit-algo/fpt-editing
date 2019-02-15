#ifndef CONSUMER_SELECTOR_SINGLE_INDEPENDENT_SET_HPP
#define CONSUMER_SELECTOR_SINGLE_INDEPENDENT_SET_HPP

#include <algorithm>
#include <utility>
#include <vector>
#include <limits>

#include <iostream>

#include "../util.hpp"

#include "../config.hpp"

#include "../Options.hpp"

#include "../Finder/Finder.hpp"
#include "../Finder/Center.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../ProblemSet.hpp"

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
		ProblemSet problem;
		bool use_single;
	public:
		Single_Most(VertexID graph_size) : num_subgraphs_per_edge(graph_size), use_single(false) {;}

		void prepare(size_t no_edits_left, const Lower_Bound_Storage_type&)
		{
			use_single = (no_edits_left > 0);
			num_subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, size_t& v) { v = 0; });
			problem.vertex_pairs.clear();
			problem.found_solution = true;
			problem.needs_no_edit_branch = false;
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
			if(free < problem.vertex_pairs.size() || problem.empty())
			{
				problem.vertex_pairs.clear();

				Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
					problem.vertex_pairs.emplace_back(*uit, *vit);
					return false;
				});

				problem.found_solution = false;

				if(free == 0) {return true;} // completly edited subgraph -- impossible to solve graph
			}

			return false;
		}


		ProblemSet result(size_t, Graph const &, Graph_Edits const &, Options::Tag::Selector)
		{
			// Do not use single vertex pair editing if we have a solved graph or we found a completely edited/marked subgraph or one with just a single branch.
			if (problem.vertex_pairs.size() > 1 && use_single) {
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
					problem.vertex_pairs.clear();
					problem.vertex_pairs.push_back(node_pair);
					problem.needs_no_edit_branch = true;
				}
			}

			return problem;
		}
	};
}

#endif
