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

	private:
		Finder &finder;
		std::tuple<Consumer &...> consumer;
		Graph &graph;
		Graph_Edits edited;

		bool found_soulution;
		std::function<bool(Graph const &, Graph_Edits const &)> write;

#ifdef STATS
		std::vector<size_t> calls;
		std::vector<size_t> prunes;
		std::vector<size_t> fallbacks;
		std::vector<size_t> single;
		std::vector<size_t> extra_lbs;
#endif

	public:
		ST(Finder &finder, Graph &graph, std::tuple<Consumer &...> consumer, size_t) : finder(finder), consumer(consumer), graph(graph), edited(graph.size())
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
			found_soulution = false;

			Util::for_<sizeof...(Consumer)>([&](auto i){
				std::get<i.value>(consumer).initialize(k, graph, edited);
			});

			edit_rec(k);
			return found_soulution;
		}

#ifdef STATS
		std::map<std::string, std::vector<size_t> const &> stats() const
		{
			return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}, {"extra_lbs", extra_lbs}};
		}
#endif

	private:

		bool edit_rec(size_t k)
		{
#ifdef STATS
			calls[k]++;
#endif
			if (k < std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound()))
			{
#ifdef STATS
				prunes[k]++;
#endif
				// lower bound too high
				return false;
			}

			// graph solved?
			ProblemSet problem = std::get<selector>(consumer).result(k, graph, edited, Options::Tag::Selector());

			if(problem.found_solution)
			{
				found_soulution = true;
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

					if (k < std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound()))
					{
						break;
					}
				}

				Util::for_<sizeof...(Consumer)>([&](auto i)
				{
					std::get<i.value>(consumer).before_mark_and_edit(graph, edited, vertex_pair.first, vertex_pair.second);
				});

				if(!std::is_same<Restriction, Options::Restrictions::None>::value)
				{
					edited.set_edge(vertex_pair.first, vertex_pair.second);
				}

				graph.toggle_edge(vertex_pair.first, vertex_pair.second);

				Util::for_<sizeof...(Consumer)>([&](auto i)
				{
					std::get<i.value>(consumer).after_mark_and_edit(k, graph, edited, vertex_pair.first, vertex_pair.second);
				});

				if(edit_rec(k - 1)) {return true;}

				Util::for_<sizeof...(Consumer)>([&](auto i)
				{
					std::get<i.value>(consumer).before_undo_edit(graph, edited, vertex_pair.first, vertex_pair.second);
				});

				graph.toggle_edge(vertex_pair.first, vertex_pair.second);

				Util::for_<sizeof...(Consumer)>([&](auto i)
				{
					std::get<i.value>(consumer).after_undo_edit(k, graph, edited, vertex_pair.first, vertex_pair.second);
				});

				if constexpr (std::is_same<Restriction, Options::Restrictions::Undo>::value)
				{
					Util::for_<sizeof...(Consumer)>([&](auto i)
					{
						std::get<i.value>(consumer).before_undo_mark(graph, edited, vertex_pair.first, vertex_pair.second);
					});

					edited.clear_edge(vertex_pair.first, vertex_pair.second);

					Util::for_<sizeof...(Consumer)>([&](auto i)
					{
						std::get<i.value>(consumer).after_undo_mark(k, graph, edited, vertex_pair.first, vertex_pair.second);
					});
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

				if(edit_rec(k)) {return true;}
			}
#ifdef STATS
			else
			{
				fallbacks[k]++;
			}
#endif

			if constexpr (std::is_same<Restriction, Options::Restrictions::Redundant>::value)
			{
				for (ProblemSet::VertexPair vertex_pair : problem.vertex_pairs)
				{
					if (edited.has_edge(vertex_pair.first, vertex_pair.second))
					{
						Util::for_<sizeof...(Consumer)>([&](auto i)
						{
							std::get<i.value>(consumer).before_undo_mark(graph, edited, vertex_pair.first, vertex_pair.second);
						});

						edited.clear_edge(vertex_pair.first, vertex_pair.second);

						Util::for_<sizeof...(Consumer)>([&](auto i)
						{
							std::get<i.value>(consumer).after_undo_mark(k, graph, edited, vertex_pair.first, vertex_pair.second);
						});
					}
				}
			}

			return false;
		}
	};
}

#endif
