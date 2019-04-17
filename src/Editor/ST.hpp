#ifndef EDITOR_ST_HPP
#define EDITOR_ST_HPP

#include <assert.h>

#include <functional>
#include <iostream>
#include <map>
#include <typeinfo>
#include <vector>

#include "../config.hpp"

#include "Editor.hpp"
#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../ProblemSet.hpp"
#include "../Finder/SubgraphStats.hpp"

namespace Editor
{
	template<typename Finder, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, typename... Consumer>
	class ST
	{
	public:
		static constexpr char const *name = "ST";
		static_assert(Consumer_valid<Consumer...>::value, "Missing Selector and/or Lower_Bound");

		static constexpr size_t selector = Options::get_tagged_consumer<Options::Tag::Selector, Consumer...>::value;
		static constexpr size_t lb = Options::get_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value;
		using Selector_type = typename std::tuple_element<selector, std::tuple<Consumer ...>>::type;
		using Lower_Bound_type = typename std::tuple_element<lb, std::tuple<Consumer ...>>::type;
		using Lower_Bound_Storage_type = Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, Finder::length>;

		using State_Tuple_type = std::tuple<typename Consumer::State...>;

	private:
		Finder &finder;
		std::tuple<Consumer &...> consumer;
		Graph &graph;
		Graph_Edits edited;

		::Finder::Subgraph_Stats<Finder, Graph, Graph_Edits, Mode, Restriction, Conversion, Finder::length> subgraph_stats;

		static constexpr bool needs_subgraph_stats = (Consumer::needs_subgraph_stats || ...);

		bool found_solution;
		std::function<bool(Graph const &, Graph_Edits const &)> write;

#ifdef STATS
		std::vector<size_t> calls;
		std::vector<size_t> prunes;
		std::vector<size_t> fallbacks;
		std::vector<size_t> single;
		std::vector<size_t> extra_lbs;
#endif

	public:
		ST(Finder &finder, Graph &graph, std::tuple<Consumer &...> consumer, size_t) : finder(finder), consumer(consumer), graph(graph), edited(graph.size()), subgraph_stats(needs_subgraph_stats ? graph.size() : 0), found_solution(false)
		{
			;
		}

		bool edit(size_t k, decltype(write) const &writegraph)
		{
			// lock?
			write = writegraph;
#ifdef STATS
			calls = decltype(calls)(k + 1, 0);
			prunes = decltype(prunes)(k + 1, 0);
			fallbacks = decltype(fallbacks)(k + 1, 0);
			single = decltype(single)(k + 1, 0);
			extra_lbs = decltype(extra_lbs)(k + 1, 0);
#endif
			found_solution = false;

			subgraph_stats.initialize(graph, edited);

			State_Tuple_type state = Util::for_make_tuple<sizeof...(Consumer)>([&](auto i){
				return std::get<i.value>(consumer).initialize(k, graph, edited);
			});

			edit_rec(k, std::move(state));
			return found_solution;
		}

#ifdef STATS
		std::map<std::string, std::vector<size_t> const &> stats() const
		{
			return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}, {"extra_lbs", extra_lbs}};
		}
#endif

	private:

		bool edit_rec(size_t k, State_Tuple_type state)
		{
#ifdef STATS
			calls[k]++;
#endif
			if (k < std::get<lb>(consumer).result(std::get<lb>(state), subgraph_stats, k, graph, edited, Options::Tag::Lower_Bound()))
			{
#ifdef STATS
				prunes[k]++;
#endif
				// lower bound too high
				return false;
			}

			// graph solved?
			ProblemSet problem = std::get<selector>(consumer).result(std::get<selector>(state), subgraph_stats, k, graph, edited, Options::Tag::Selector());

			if(problem.found_solution)
			{
				found_solution = true;
				return !write(graph, edited);
			}
			else if(k == 0)
			{
#ifdef STATS
				prunes[k]++;
#endif
				// used all edits but graph still unsolved
				return false;
			}

			for (ProblemSet::VertexPair vertex_pair : problem.vertex_pairs)
			{
				if(edited.has_edge(vertex_pair.first, vertex_pair.second))
				{
					abort();
				}

				if (std::is_same<Restriction, Options::Restrictions::Redundant>::value && vertex_pair.updateLB)
				{
#ifdef STATS
					++calls[k];
					++extra_lbs[k];
#endif

					if (k < std::get<lb>(consumer).result(std::get<lb>(state), subgraph_stats, k, graph, edited, Options::Tag::Lower_Bound()))
					{
						break;
					}
				}

				State_Tuple_type next_state(state);

				Util::for_<sizeof...(Consumer)>([&](auto i)
				{
					std::get<i.value>(consumer).before_mark_and_edit(std::get<i.value>(next_state), graph, edited, vertex_pair.first, vertex_pair.second);
				});

				if(!std::is_same<Restriction, Options::Restrictions::None>::value)
				{
					Util::for_<sizeof...(Consumer)>([&](auto i)
					{
						std::get<i.value>(consumer).before_mark(std::get<i.value>(state), graph, edited, vertex_pair.first, vertex_pair.second);
					});

					edited.set_edge(vertex_pair.first, vertex_pair.second);

					Util::for_<sizeof...(Consumer)>([&](auto i)
					{
						std::get<i.value>(consumer).after_mark(std::get<i.value>(state), graph, edited, vertex_pair.first, vertex_pair.second);
					});

					subgraph_stats.after_mark(graph, edited, vertex_pair.first, vertex_pair.second);
				}

				subgraph_stats.before_edit(graph, edited, vertex_pair.first, vertex_pair.second);

				graph.toggle_edge(vertex_pair.first, vertex_pair.second);

				Util::for_<sizeof...(Consumer)>([&](auto i)
				{
					std::get<i.value>(consumer).after_mark_and_edit(std::get<i.value>(next_state), graph, edited, vertex_pair.first, vertex_pair.second);
				});

				subgraph_stats.after_edit(graph, edited, vertex_pair.first, vertex_pair.second);

				if(edit_rec(k - 1, std::move(next_state))) {return true;}

				subgraph_stats.before_edit(graph, edited, vertex_pair.first, vertex_pair.second);

				graph.toggle_edge(vertex_pair.first, vertex_pair.second);

				subgraph_stats.after_edit(graph, edited, vertex_pair.first, vertex_pair.second);

				if constexpr (std::is_same<Restriction, Options::Restrictions::Undo>::value)
				{
					edited.clear_edge(vertex_pair.first, vertex_pair.second);
					subgraph_stats.after_unmark(graph, edited, vertex_pair.first, vertex_pair.second);
				}
			}

			if (problem.needs_no_edit_branch)
			{
				if (!std::is_same<Restriction, Options::Restrictions::Redundant>::value)
				{
					throw std::runtime_error("No edit branches are only possible with restriction Redundant");
				}

#ifdef STATS
				single[k]++;
#endif

				if(edit_rec(k, std::move(state))) {return true;}
			}
#ifdef STATS
			else
			{
				fallbacks[k]++;
			}
#endif

			if constexpr (std::is_same<Restriction, Options::Restrictions::Redundant>::value)
			{

				for (auto it = problem.vertex_pairs.rbegin(); it != problem.vertex_pairs.rend(); ++it)
				{
					if (edited.has_edge(it->first, it->second))
					{
						edited.clear_edge(it->first, it->second);
						subgraph_stats.after_unmark(graph, edited, it->first, it->second);
					}
				}
			}

			return false;
		}
	};
}

#endif
