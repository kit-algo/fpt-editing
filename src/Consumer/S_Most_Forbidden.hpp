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
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length, bool pruned = false>
	class Most_Impl : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "Most";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;

		struct State {
			Value_Matrix<size_t> use_count;
			size_t num_subgraphs;

			State(VertexID graph_size) : use_count(graph_size), num_subgraphs(0) {}
                };

        private:
		Finder_impl finder;

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
		Most_Impl(VertexID graph_size) : finder(graph_size) {;}

		State initialize(size_t, Graph const &graph, Graph_Edits const &edited)
		{
			State state(graph.size());

			finder.find(graph, [&](const subgraph_t& path)
			{
				++state.num_subgraphs;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					++state.use_count.at(*uit, *vit);
					return false;
				});

				return false;
			});

			return state;
		}

		void before_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				--state.num_subgraphs;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit)
				{
					--state.use_count.at(*uit, *vit);
					return false;
				});

				return false;
			});

			assert(state.use_count.at(u, v) == 0);
		}

		void after_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				++state.num_subgraphs;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					++state.use_count.at(*uit, *vit);
					return false;
				});

				return false;
			});

			assert(state.use_count.at(u, v) == 0);
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State& state, Graph const &/* graph */, Graph_Edits const &/*edited*/, VertexID u, VertexID v)
		{
			state.use_count.at(u, v) = 0;
		}

		ProblemSet result(State& state, size_t k, Graph const &graph, Graph const &edited, Options::Tag::Selector)
		{
			ProblemSet problem;
			problem.found_solution = (state.num_subgraphs == 0);
			problem.needs_no_edit_branch = false;

			// We only need to return an actual set of vertex pairs
			// if we have not found the solution and k > 0, i.e., there is
			// actually still something to do.
			if (!problem.found_solution && k > 0)
			{
				std::vector<forbidden_count> best_pairs, current_pairs;

				size_t max_subgraphs = 0;
				VertexID maxu = 0, maxv = 0;
				state.use_count.forAllNodePairs([&](VertexID u, VertexID v, size_t& num_fbs) {
					if (num_fbs > max_subgraphs) {
						max_subgraphs = num_fbs;
						maxu = u;
						maxv = v;
					}
				});

				// TODO: do this for multiple node pairs?

				finder.find_near(graph, maxu, maxv, [&](const subgraph_t& fs)
				{
					current_pairs.clear();
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit) {
						current_pairs.emplace_back(std::make_pair(*uit, *vit), state.use_count.at(*uit, *vit));
						return false;
					});

					if (current_pairs.empty())
					{
						best_pairs.clear();
						return true;
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

					return false;
				});

				for (size_t i = 0; i < best_pairs.size(); ++i)
				{
					const forbidden_count& pair_count = best_pairs[i];
					problem.vertex_pairs.emplace_back(pair_count.node_pair, (pruned && i > 0 && i + 1 < best_pairs.size() && best_pairs[i-1].num_forbidden > 1));
				}
			}

			return problem;
		}
	};

	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Most: public Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, false> {
	public:
		static constexpr char const *name = "Most";
		using Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, false>::Most_Impl;
	};

	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Most_Pruned : public Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, true> {
	public:
		static constexpr char const *name = "Most_Pruned";
		using Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, true>::Most_Impl;
	};
}

#endif
