#ifndef CONSUMER_SELECTOR_MOST_HPP
#define CONSUMER_SELECTOR_MOST_HPP

#include <algorithm>
#include <utility>
#include <vector>
#include <limits>

#include <iostream>

#include "../util.hpp"

#include "../config.hpp"

#include "../Options.hpp"

#include "../Finder/Finder.hpp"
#include "../Finder/SubgraphStats.hpp"
#include "../Finder/Center.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../ProblemSet.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length, bool use_single, bool pruned>
	class Most_Impl : Options::Tag::Selector
	{
	public:
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using Subgraph_Stats_type = ::Finder::Subgraph_Stats<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;

		static constexpr bool needs_subgraph_stats = true;

		struct State
		{
			int64_t num_single_left;

			std::vector<subgraph_t> one_left_subgraphs;
			bool impossible_to_solve;

			State() : num_single_left(0), impossible_to_solve(false) {}
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
			State state;

			state.num_single_left = length;

			if (edited.count_edges() > 0)
			{
				finder.find(graph, [&](const subgraph_t& path)
				{
					size_t free = 0;

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto, auto) {
						++free;
						return false;
					});

					if (free == 1)
					{
						state.one_left_subgraphs.push_back(path);
					}
					else if (free == 0)
					{
						state.impossible_to_solve = true;
					}

					if(free == 0) {return true;} // completly edited subgraph -- impossible to solve graph

					return false;
				});
			}

			return state;
		}

		void set_initial_k(size_t, Graph const&, Graph_Edits const&) {}

		void before_mark_and_edit(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				size_t free = 0;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto, auto) {
					++free;
					return false;
				});

				if (free == 1)
				{
					state.one_left_subgraphs.push_back(path);
				}
				else if (free == 0)
				{
					state.impossible_to_solve = true;
				}

				return false;
			});

			state.num_single_left = length;
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			assert(edited.has_edge(u, v));

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				size_t free = 0;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto, auto) {
					++free;
					return false;
				});

				if (free == 1)
				{
					state.one_left_subgraphs.push_back(path);
				}
				else if (free == 0)
				{
					state.impossible_to_solve = true;
				}

				return false;
			});

			--state.num_single_left;
		}

		ProblemSet result(State& state, const Subgraph_Stats_type& subgraph_stats, size_t k, Graph const &graph, Graph_Edits const &edited, Options::Tag::Selector)
		{
			ProblemSet problem;
			problem.found_solution = (subgraph_stats.num_subgraphs == 0);
			problem.needs_no_edit_branch = false;
			if (!problem.found_solution && k > 0 && !state.impossible_to_solve) {
				while (!state.one_left_subgraphs.empty())
				{
					const subgraph_t sg = state.one_left_subgraphs.back();
					state.one_left_subgraphs.pop_back();

					if (finder.is_subgraph_valid(graph, sg))
					{
						Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, sg.begin(), sg.end(), [&](auto uit, auto vit) {
							problem.vertex_pairs.emplace_back(*uit, *vit);
							return false;
						});

						assert(problem.vertex_pairs.size() == 1);

						return problem;
					}
				}

				size_t max_subgraphs = 0;
				std::vector<std::pair<VertexID, VertexID>> node_pairs;
				subgraph_stats.num_subgraphs_per_edge.forAllNodePairs([&](VertexID u, VertexID v, size_t num_fbs) {
					if (num_fbs > max_subgraphs)
					{
						max_subgraphs = num_fbs;
						node_pairs.clear();
					}
					if (num_fbs == max_subgraphs && (num_fbs > 1 || node_pairs.empty()))
					{
						node_pairs.emplace_back(u, v);
					}
				});

				std::vector<forbidden_count> best_pairs, current_pairs;
				for (std::pair<VertexID, VertexID> node_pair : node_pairs)
				{
					// TODO: restrict listing to exclude already listed subgraphs?
					finder.find_near(graph, node_pair.first, node_pair.second, [&](const subgraph_t& fs)
					{
						current_pairs.clear();
						Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit) {
							current_pairs.emplace_back(std::make_pair(*uit, *vit), subgraph_stats.num_subgraphs_per_edge.at(*uit, *vit));
							return false;
						});

						assert(current_pairs.size() > 1);

						std::sort(current_pairs.begin(), current_pairs.end());

						if (best_pairs.empty())
						{
							best_pairs = current_pairs;
						}
						else
						{
							size_t bi = 0, ci = 0;

							while (bi + 1 < best_pairs.size() && ci + 1 < current_pairs.size() && best_pairs[bi].num_forbidden == current_pairs[ci].num_forbidden)
							{
								++bi;
								++ci;
							}

							if (ci + 1 == current_pairs.size() || (bi + 1 != best_pairs.size() && best_pairs[bi].num_forbidden < current_pairs[ci].num_forbidden))
							{
								best_pairs = current_pairs;
							}
						}

						return false;
					});
				}

				if (!use_single || state.num_single_left <= 0 || max_subgraphs == 1 || best_pairs.size() == 2) // only use single editing if there is a node pair that is part of multiple forbidden subgraphs
				{

					for (size_t i = 0; i < best_pairs.size(); ++i)
					{
						const forbidden_count& pair_count = best_pairs[i];
						problem.vertex_pairs.emplace_back(pair_count.node_pair, (pruned && i > 0 && i + 1 < best_pairs.size() && best_pairs[i-1].num_forbidden > 1));
					}
				}
				else
				{
					problem.vertex_pairs.push_back(node_pairs.front());
					problem.needs_no_edit_branch = true;
				}
			}

			return problem;
		}
	};

	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Single_Most : public Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, true, true>
	{
	public:
		static constexpr char const *name = "Single_Most";
		using Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, true, true>::Most_Impl;
	};

	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Most : public Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, false, false>
	{
	public:
		static constexpr char const *name = "Most";
		using Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, false, false>::Most_Impl;
	};

	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Most_Pruned : public Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, false, true>
	{
	public:
		static constexpr char const *name = "Most_Pruned";
		using Most_Impl<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length, false, true>::Most_Impl;
	};

}

#endif
