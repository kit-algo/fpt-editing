#ifndef EDITOR_HPP
#define EDITOR_HPP

#include <functional>
#include <map>
#include <typeinfo>
#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"

namespace Editor
{
	template<typename Finder, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, typename... Consumer>
	class Editor
	{
	public:
		static constexpr char const *name = "Editor";
		static constexpr bool valid = Options::has_tagged_consumer<Options::Tag::Selector, Consumer...>::value && Options::has_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value;

	private:
		Finder &finder;
		std::tuple<Consumer &...> consumer;
		static constexpr size_t selector = Options::get_tagged_consumer<Options::Tag::Selector, Consumer...>::value();
		static constexpr size_t lb = Options::get_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value();
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
		Editor(Finder &finder, Graph &graph, Consumer &... consumer) : finder(finder), consumer(consumer...), graph(graph), edited(graph.size()), feeder(finder, consumer...)
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
			single = decltype(fallbacks)(k + 1, 0);
#endif
			found_soulution = false;
			edit_rec(k);
			return found_soulution;
		}

#ifdef STATS
		std::map<std::string, std::vector<size_t> const &> stats() const
		{
			return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}};
		}
#endif

	private:

		bool edit_rec(size_t k)
		{
#ifdef STATS
			calls[k]++;
#endif
			// start finder and feed into selector and lb
			feeder.feed(graph, edited);

			// graph solved?
			auto problem = std::get<selector>(consumer).result(k, graph, edited);
			if(problem.empty())
			{
				found_soulution = true;
				return !write(graph, edited);
			}
			else if(k < std::get<lb>(consumer).result())
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

			if(problem.size() == 2)
			{
#ifdef STATS
				single[k]++;
#endif
				//TODO: deletion/insertion only

				// single edge editing
				if(edited.has_edge(problem.front(), problem.back()))
				{
					abort();
				}

				//edit
				graph.toggle_edge(problem.front(), problem.back());
				edited.set_edge(problem.front(), problem.back());
				if(edit_rec(k - 1)) {return true;}

				//unedit, mark
				graph.toggle_edge(problem.front(), problem.back());
				if(edit_rec(k)) {return true;}

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
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						edited.set_edge(*uit, *vit);
					}
					graph.toggle_edge(*uit, *vit);
					if(edit_rec(k - 1)) {return true;}
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
