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

		::Finder::Feeder<Finder, Graph, Graph_Edits, Consumer...> feeder;
		bool found_soulution;
		std::function<bool(Graph const &, Graph_Edits const &)> write;

#ifdef STATS
		std::vector<size_t> calls;
		std::vector<size_t> prunes;
		std::vector<size_t> fallbacks;
		std::vector<size_t> single;
#endif

	public:
		ST(Finder &finder, Graph &graph, std::tuple<Consumer &...> consumer, size_t) : finder(finder), consumer(consumer), graph(graph), edited(graph.size()), feeder(finder, consumer)
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
#endif
			found_soulution = false;
			edit_rec(k, k, Lower_Bound_Storage_type());
			return found_soulution;
		}

#ifdef STATS
		std::map<std::string, std::vector<size_t> const &> stats() const
		{
			return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}};
		}
#endif

	private:

		bool edit_rec(size_t k, size_t no_edits_left, const Lower_Bound_Storage_type& lower_bound)
		{
#ifdef STATS
			calls[k]++;
#endif
			// start finder and feed into selector and lb
			feeder.feed(k, graph, edited, no_edits_left, lower_bound);

			// graph solved?
			auto problem = std::get<selector>(consumer).result(k, graph, edited, Options::Tag::Selector());
			if(problem.empty())
			{
				found_soulution = true;
				return !write(graph, edited);
			}
			else if(k < std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound()))
			{
				// lower bound too high
#ifdef STATS
				prunes[k]++;
#endif
				return false;
			}
			else if(k == 0 && !problem.empty())
			{
				// used all edits but graph still unsolved
				return false;
			}

			Lower_Bound_Storage_type updated_lower_bound(std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound_Update()));


			if(problem.size() == 2)
			{
#ifdef STATS
				single[k]++;
#endif
				// single edge editing
				if(edited.has_edge(problem.front(), problem.back()))
				{
					abort();
				}

				//edit
				{
					Lower_Bound_Storage_type next_lower_bound(updated_lower_bound);
					next_lower_bound.remove(graph, edited, problem.front(), problem.back());
					graph.toggle_edge(problem.front(), problem.back());
					edited.set_edge(problem.front(), problem.back());
					if(edit_rec(k - 1, no_edits_left, next_lower_bound)) {return true;}
				}

				//unedit, mark
				{
					graph.toggle_edge(problem.front(), problem.back());
					if(edit_rec(k, no_edits_left - 1, updated_lower_bound)) {return true;}
				}

				//unmark
				edited.clear_edge(problem.front(), problem.back());
			}
			else
			{
				// normal editing
#ifdef STATS
				fallbacks[k]++;
#endif
				std::vector<std::pair<size_t, size_t>> marked;
				bool done = ::Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, problem.begin(), problem.end(), [&](auto uit, auto vit){
					Lower_Bound_Storage_type next_lower_bound(updated_lower_bound);
					next_lower_bound.remove(graph, edited, *uit, *vit);
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						edited.set_edge(*uit, *vit);
					}
					graph.toggle_edge(*uit, *vit);
					if(edit_rec(k - 1, no_edits_left, next_lower_bound)) {return true;}
					graph.toggle_edge(*uit, *vit);
					if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
					else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
					return false;
				});
				if(done) {return true;}

				if(std::is_same<Restriction, Options::Restrictions::Redundant>::value)
				{
					for(auto it = marked.rbegin(); it != marked.rend(); it++)
					{
						edited.clear_edge(it->first, it->second);
					}
				}
			}
			return false;
		}
	};
}

#endif
