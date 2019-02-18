#ifndef CONSUMER_SELECTOR_MOST_HPP
#define CONSUMER_SELECTOR_MOST_HPP

#include <algorithm>
#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../util.hpp"
#include "../ProblemSet.hpp"

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

		struct forbidden_count
		{
			std::pair<VertexID, VertexID> node_pair;
			size_t num_forbidden;

			forbidden_count(std::pair<VertexID, VertexID> pair, size_t num_forbidden) : node_pair(pair), num_forbidden(num_forbidden) {}

			bool operator<(const forbidden_count& other) const
			{
				return this->num_forbidden > other.num_forbidden;
			}

			operator std::pair<VertexID, VertexID> () const
			{
				return node_pair;
			}
		};
	public:
		Most(VertexID graph_size) : use_count(graph_size) {;}

		void prepare(size_t, const Lower_Bound_Storage_type&)
		{
			use_count.forAllNodePairs([](VertexID, VertexID, size_t& v) { v = 0; });
			forbidden_subgraphs.clear();
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

		ProblemSet result(size_t k, Graph const &graph, Graph const &edited, Options::Tag::Selector)
		{
			ProblemSet problem;
			problem.found_solution = forbidden_subgraphs.empty();
			problem.needs_no_edit_branch = false;

			// We only need to return an actual set of vertex pairs
			// if we have not found the solution and k > 0, i.e., there is
			// actually still something to do.
			if (!problem.found_solution && k > 0)
			{
				std::vector<forbidden_count> best_pairs, current_pairs;

				for (const auto& fs : forbidden_subgraphs)
				{
					current_pairs.clear();
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit) {
						current_pairs.emplace_back(std::make_pair(*uit, *vit), use_count.at(*uit, *vit));
						return false;
					});

					if (current_pairs.empty())
					{
						best_pairs.clear();
						break;
					}

					if (best_pairs.empty() || (current_pairs.size() == 1 && (best_pairs.size() > 1 || best_pairs.front().num_forbidden < current_pairs.front().num_forbidden)))
					{
						best_pairs = current_pairs;
					}
					else
					{
						std::sort(current_pairs.begin(), current_pairs.end());

						auto bit = best_pairs.begin();
						auto cit = current_pairs.begin();

						while (bit != best_pairs.end() && cit != current_pairs.end() && bit->num_forbidden == cit->num_forbidden)
						{
							++bit;
							++cit;
						}

						if (cit == current_pairs.end() || (bit != best_pairs.end() && bit->num_forbidden < cit->num_forbidden))
						{
							best_pairs = current_pairs;
						}
					}
				}

				for (size_t i = 0; i < best_pairs.size(); ++i)
				{
					const forbidden_count& pair_count = best_pairs[i];
					problem.vertex_pairs.emplace_back(pair_count.node_pair, (i > 0 && i + 1 < best_pairs.size() && best_pairs[i-1].num_forbidden > 1));
				}
			}

			return problem;
		}
	};
}

#endif
