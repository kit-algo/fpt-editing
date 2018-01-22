#ifndef EDITOR_HPP
#define EDITOR_HPP

#include <functional>
#include <typeinfo>
#include <vector>

#include "../config.hpp"

#include "../Finder/Finder.hpp"

namespace Editor
{
	namespace Options
	{
		namespace Restrictions
		{
			struct None {static constexpr char const *name = "None";};
			struct Undo {static constexpr char const *name = "Undo";};
			struct Redundant {static constexpr char const *name = "Redundant";};
		}

		namespace Conversions
		{
			struct Normal {static constexpr char const *name = "Normal";};
			struct Last {static constexpr char const *name = "Last";};
			struct Skip {static constexpr char const *name = "Skip";};
		}

		namespace Modes
		{
			struct Edit {static constexpr char const *name = "Edit";};
			struct Delete {static constexpr char const *name = "Delete";};
			struct Insert {static constexpr char const *name = "Insert";};
		}
	}

	namespace Tag
	{
		struct Selector {};
		struct Lower_Bound {};
	}

	template<typename Tag, typename... Consumer>
	struct get_tagged_consumer;

	template<typename Tag, typename Consumer, typename... Consumer_Tail>
	struct get_tagged_consumer<Tag, Consumer, Consumer_Tail...>
	{
		static constexpr size_t value() {return v<Tag, Consumer>();}

		template<typename T, typename C>
		static constexpr
		typename std::enable_if<std::is_base_of<T, C>::value, size_t>::type
		v()
		{
			return 0;
		}

		template<typename T, typename C>
		static constexpr
		typename std::enable_if<!std::is_base_of<T, C>::value, size_t>::type
		v()
		{
			return 1 + get_tagged_consumer<Tag, Consumer_Tail...>::value();
		}
	};

	template<typename Tag>
	struct get_tagged_consumer<Tag>
	{
		static_assert(!std::is_base_of<Tag, Tag>::value, "No matching consumer for tag");
		//static constexpr size_t value() {return std::numeric_limits<size_t>::max();}
	};

	template<typename Finder, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, typename... Consumer>
	class Editor
	{
	public:
		static constexpr char const *name = "Editor";

	private:
		Finder &finder;
		std::tuple<Consumer &...> consumer;
		static constexpr size_t selector = get_tagged_consumer<Tag::Selector, Consumer...>::value();
		static constexpr size_t lb = get_tagged_consumer<Tag::Lower_Bound, Consumer...>::value();
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
				// used all edits btu graph still unsolved
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
				if(std::is_same<Mode, Options::Modes::Edit>::value)
				{
					for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
					{
						for(auto uit = problem.begin(); uit != vit; uit++)
						{
							if(!std::is_same<Conversion, Options::Conversions::Normal>::value && uit == problem.begin() && vit == problem.end() - 1) {continue;}
							if(!std::is_same<Restriction, Options::Restrictions::None>::value)
							{
								if(edited.has_edge(*uit, *vit)) {continue;}
								edited.set_edge(*uit, *vit);
							}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
							else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
						}
					}
					if(std::is_same<Conversion, Options::Conversions::Last>::value)
					{
						auto uit = problem.begin();
						auto vit = problem.end() - 1;
						do
						{
							if(!std::is_same<Restriction, Options::Restrictions::None>::value)
							{
								if(edited.has_edge(*uit, *vit)) {continue;}
								edited.set_edge(*uit, *vit);
							}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
							else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
						} while(false);
					}
				}
				else if(std::is_same<Mode, Options::Modes::Delete>::value)
				{
					for(auto uit = problem.begin(), vit = problem.begin() + 1; vit != problem.end(); uit++, vit++)
					{
						if(!std::is_same<Restriction, Options::Restrictions::None>::value)
						{
							if(edited.has_edge(*uit, *vit)) {continue;}
							edited.set_edge(*uit, *vit);
						}
						graph.toggle_edge(*uit, *vit);
						if(edit_rec(k - 1)) {return true;}
						graph.toggle_edge(*uit, *vit);
						if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
						else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
					}
					if(!std::is_same<Conversion, Options::Conversions::Skip>::value && graph.has_edge(problem.front(), problem.back()))
					{
						auto uit = problem.begin();
						auto vit = problem.end() - 1;
						do
						{
							if(!std::is_same<Restriction, Options::Restrictions::None>::value)
							{
								if(edited.has_edge(*uit, *vit)) {continue;}
								edited.set_edge(*uit, *vit);
							}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
							else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
						} while(false);
					}
				}
				else if(std::is_same<Mode, Options::Modes::Insert>::value)
				{
					for(auto vit = problem.begin() + 2; vit != problem.end(); vit++)
					{
						for(auto uit = problem.begin(); uit != vit - 1; uit++)
						{
							if(uit == problem.begin() && vit == problem.end() - 1 && (!std::is_same<Conversion, Options::Conversions::Normal>::value || graph.has_edge(problem.front(), problem.back()))) {continue;}
							if(!std::is_same<Restriction, Options::Restrictions::None>::value)
							{
								if(edited.has_edge(*uit, *vit)) {continue;}
								edited.set_edge(*uit, *vit);
							}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
							else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
						}
					}
					if(std::is_same<Conversion, Options::Conversions::Last>::value && !graph.has_edge(problem.front(), problem.back()))
					{
						auto uit = problem.begin();
						auto vit = problem.end() - 1;
						do
						{
							if(!std::is_same<Restriction, Options::Restrictions::None>::value)
							{
								if(edited.has_edge(*uit, *vit)) {continue;}
								edited.set_edge(*uit, *vit);
							}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
							else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
						} while(false);
					}
				}

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
