#ifndef CONSUMER_SELECTOR_SINGLE_SIMPLE_HPP
#define CONSUMER_SELECTOR_SINGLE_SIMPLE_HPP

#include <algorithm>
#include <set>
#include <tuple>
#include <unordered_map>
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
#include "LB_Updated.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Single_Simple : Options::Tag::Selector, Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Single_Simple";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		Updated<Graph, Graph_Edits, Mode, Restriction, Conversion, length> updated_lb;
		Value_Matrix<size_t> subgraphs_per_pair;
		std::vector<VertexID> fallback;
		size_t fallback_free = 0;
		size_t no_edits_left;
	public:
		Single_Simple(VertexID graph_size) : updated_lb(graph_size), subgraphs_per_pair(graph_size) {;}

		void prepare(size_t no_edits_left, const Lower_Bound_Storage_type& lower_bound)
		{
			updated_lb.prepare(no_edits_left, lower_bound);
			this->no_edits_left = no_edits_left;
			fallback.clear();
			fallback_free = 0;
			subgraphs_per_pair.forAllNodePairs([&](VertexID, VertexID, size_t& v) { v = 0; });
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			updated_lb.next(graph, edited, b, e);

			size_t free = 0;
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				++subgraphs_per_pair.at(*uit, *vit);
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

		std::vector<VertexID> result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Selector)
		{
			const Lower_Bound_Storage_type& lower_bound = updated_lb.result(k, g, e, Options::Tag::Lower_Bound_Update());

			if(lower_bound.empty())
			{
				// a solved graph?!
				fallback.clear();
				return fallback;
			}
			if(lower_bound.size() > k) {return lower_bound.as_vector(0);} // fast return since lb will prune anyway

			if(fallback_free == 0 && !fallback.empty())
			{
				// we found a completly edited/marked subgraph
				return fallback;
			}
			else if(no_edits_left)
			{
				std::vector<VertexID> single_pair = {0, 0};
				size_t max_subgraphs = 0;

				subgraphs_per_pair.forAllNodePairs([&](VertexID u, VertexID v, size_t& c) {
					if (c > max_subgraphs) {
						max_subgraphs = c;
						single_pair[0] = u;
						single_pair[1] = v;
					}
				});

				assert(single_pair[0] != single_pair[1]);
				return single_pair;
			}
			else
			{
				// fallback
				// pick least editable forbidden subgraph
				return fallback;
			}
		}

		size_t result(size_t k, Graph const & graph, Graph_Edits const & edited, Options::Tag::Lower_Bound)
		{
			return updated_lb.result(k, graph, edited, Options::Tag::Lower_Bound());
		}

		const Lower_Bound_Storage_type& result(size_t k, Graph const& graph, Graph_Edits const & edited, Options::Tag::Lower_Bound_Update)
		{
			return updated_lb.result(k, graph, edited, Options::Tag::Lower_Bound_Update());
		}
	};
}

#endif
