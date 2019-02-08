#ifndef CONSUMER_SELECTOR_MOST_HPP
#define CONSUMER_SELECTOR_MOST_HPP

#include <algorithm>
#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../util.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Most : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "Most";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		Value_Matrix<size_t> use_count;
		std::vector<typename Lower_Bound_Storage_type::subgraph_t> forbidden_subgraphs;
		std::vector<VertexID> problem;
	public:
		Most(VertexID graph_size) : use_count(graph_size) {;}

		void prepare(size_t, const Lower_Bound_Storage_type&)
		{
			use_count.forAllNodePairs([](VertexID, VertexID, size_t& v) { v = 0; });
			forbidden_subgraphs.clear();
			problem.clear();
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				++use_count.at(*uit, *vit);
				return false;
			});

			forbidden_subgraphs.emplace_back(Util::to_array<VertexID, length>(b));

			return false;
		}

		std::vector<VertexID> const& result(size_t, Graph const &graph, Graph const &edited, Options::Tag::Selector)
		{
			size_t max_used = 0;
			size_t min_pairs = std::numeric_limits<size_t>::max();

			for (const auto& fs : forbidden_subgraphs)
			{
				size_t num_used = 0, num_pairs = 0;
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					num_used += use_count.at(*uit, *vit);
					++num_pairs;
					return false;
				});

				if ((num_pairs <= 1 && num_pairs < min_pairs) || (min_pairs > 1 && (problem.empty() || num_pairs <= 1 || num_used / num_pairs > max_used)))
				{
					problem = {fs.begin(), fs.end()};
					if (num_pairs == 0)
					{
						return problem;
					}
					max_used = num_used / num_pairs;
					min_pairs = num_pairs;
				}
			}


			return problem;
		}
	};
}

#endif
