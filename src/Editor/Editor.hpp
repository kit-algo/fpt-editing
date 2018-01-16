#ifndef EDITOR_HPP
#define EDITOR_HPP

#include <functional>
#include <map>
#include <vector>

#include "../config.hpp"

#include "../Finder/Finder.hpp"
#include "../Selector/Selector.hpp"
#include "../Lower_Bound/Lower_Bound.hpp"
#include "../Graph/Graph.hpp"
#include "../Graph/Matrix.hpp"

namespace Editor
{
	class Options
	{
	public:
		bool all_solutions = false;

		enum class Restrictions {NONE, NO_UNDO, NO_REDUNDANT};
		Restrictions restrictions = Restrictions::NONE;
		static std::map<std::string, Restrictions> const restriction_values;
		static std::map<Restrictions, std::string> const restriction_names;

		enum class Path_Cycle_Conversion {NORMAL, DO_LAST, SKIP};
		Path_Cycle_Conversion conversions = Path_Cycle_Conversion::NORMAL;
		static std::map<std::string, Path_Cycle_Conversion> const conversion_values;
		static std::map<Path_Cycle_Conversion, std::string> const conversion_names;

		enum class Modes {EDIT, ONLY_DELETE, ONLY_INSERT};
		Modes mode = Modes::EDIT;
		static std::map<std::string, Modes> const mode_values;
		static std::map<Modes, std::string> const mode_names;
	};

	std::map<std::string, Options::Restrictions> const Options::restriction_values{
		{"none", Options::Restrictions::NONE},
		{"undo", Options::Restrictions::NO_UNDO},
		{"redundant", Options::Restrictions::NO_REDUNDANT}
	};
	std::map<Options::Restrictions, std::string> const Options::restriction_names{
		{Options::Restrictions::NONE, "none"},
		{Options::Restrictions::NO_UNDO, "undo"},
		{Options::Restrictions::NO_REDUNDANT, "redundant"}
	};

	std::map<std::string, Options::Path_Cycle_Conversion> const Options::conversion_values{
		{"normal", Options::Path_Cycle_Conversion::NORMAL},
		{"last", Options::Path_Cycle_Conversion::DO_LAST},
		{"skip", Options::Path_Cycle_Conversion::SKIP}
	};
	std::map<Options::Path_Cycle_Conversion, std::string> const Options::conversion_names{
		{Options::Path_Cycle_Conversion::NORMAL, "normal"},
		{Options::Path_Cycle_Conversion::DO_LAST, "last"},
		{Options::Path_Cycle_Conversion::SKIP, "skip"}
	};

	std::map<std::string, Options::Modes> const Options::mode_values{
		{"edit", Options::Modes::EDIT},
		{"delete", Options::Modes::ONLY_DELETE},
		{"insert", Options::Modes::ONLY_INSERT}
	};
	std::map<Options::Modes, std::string> const Options::mode_names{
		{Options::Modes::EDIT, "edit"},
		{Options::Modes::ONLY_DELETE, "delete"},
		{Options::Modes::ONLY_INSERT, "insert"}
	};

	template<typename Finder, typename Selector, typename Lower_Bound, typename Graph, typename Graph_Edits>
	class Editor
	{
	public:
		static constexpr char const *name = "Editor";

	private:
		Options const &options;

		Finder &finder;
		Selector &selector;
		Lower_Bound &lb;
		Graph &graph;
		Graph_Edits edited;

		::Finder::Feeder<Finder, Selector, Lower_Bound, Graph, Graph_Edits> feeder;
		bool found_soulution;

		std::function<void(Graph const &, Graph_Edits const &)> write;
#ifdef STATS
		std::vector<size_t> calls;
		std::vector<size_t> prunes;
		std::vector<size_t> fallbacks;
#endif

	public:
		Editor(Options const &options, Finder &finder, Selector &selector, Lower_Bound &lb, Graph &graph) : options(options), finder(finder), selector(selector), lb(lb), graph(graph), edited(graph.size()), feeder(finder, selector, lb)
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
#endif
			found_soulution = false;

			edit_rec(k);
			return found_soulution;
		}

#ifdef STATS
		std::map<std::string, std::vector<size_t> const &> stats() const
		{
			return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}};
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
			auto problem = selector.result();
			if(problem.empty())
			{
				if(write) {write(graph, edited);}
				found_soulution = true;
				return !options.all_solutions;
			}
			else if(k < lb.result())
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
				//TODO: deletion/insertion only

				// single edge editing
				if(edited.has_edge(problem.front(), problem.back()))
				{
					abort();
				}

				//edit
				graph.toggle_edge(problem.front(), problem.back());
				edited.set_edge(problem.front(), problem.back());
				edit_rec(k - 1);

				//unedit, mark
				graph.toggle_edge(problem.front(), problem.back());
				edit_rec(k);

				//unmark
				edited.set_edge(problem.front(), problem.back());
			}
			else
			{
				// normal editing
				//TODO: add checks for undo and conversion options
#ifdef STATS
				fallbacks[k]++;
#endif
				std::vector<std::pair<size_t, size_t>> marked;
				switch(options.mode)
				{
				case Options::Modes::EDIT:
					for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
					{
						for(auto uit = problem.begin(); uit != vit; uit++)
						{
							if(options.conversions != Options::Path_Cycle_Conversion::NORMAL && uit == problem.begin() && vit == problem.end() - 1) {continue;}
							if(edited.has_edge(*uit, *vit)) {continue;}
							if(options.restrictions != Options::Restrictions::NONE) {edited.set_edge(*uit, *vit);}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(options.restrictions == Options::Restrictions::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
							else if(options.restrictions == Options::Restrictions::NO_UNDO) {edited.clear_edge(*uit, *vit);}
						}
					}
					if(options.conversions == Options::Path_Cycle_Conversion::DO_LAST && !edited.has_edge(problem.front(), problem.back()))
					{
						auto uit = problem.begin();
						auto vit = problem.end() - 1;
						if(options.restrictions != Options::Restrictions::NONE) {edited.set_edge(*uit, *vit);}
						graph.toggle_edge(*uit, *vit);
						if(edit_rec(k - 1)) {return true;}
						graph.toggle_edge(*uit, *vit);
						if(options.restrictions == Options::Restrictions::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
						else if(options.restrictions == Options::Restrictions::NO_UNDO) {edited.clear_edge(*uit, *vit);}
					}

					break;
				case Options::Modes::ONLY_DELETE:
					for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
					{
						auto uit = vit - 1;
						if(edited.has_edge(*uit, *vit)) {continue;}
						if(options.restrictions != Options::Restrictions::NONE) {edited.set_edge(*uit, *vit);}
						graph.toggle_edge(*uit, *vit);
						if(edit_rec(k - 1)) {return true;}
						graph.toggle_edge(*uit, *vit);
						if(options.restrictions == Options::Restrictions::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
						else if(options.restrictions == Options::Restrictions::NO_UNDO) {edited.clear_edge(*uit, *vit);}
					}
					if(graph.has_edge(problem.front(), problem.back()) && !edited.has_edge(problem.front(), problem.back()))
					{
						auto uit = problem.begin();
						auto vit = problem.end() - 1;
						if(options.restrictions != Options::Restrictions::NONE) {edited.set_edge(*uit, *vit);}
						graph.toggle_edge(*uit, *vit);
						if(edit_rec(k - 1)) {return true;}
						graph.toggle_edge(*uit, *vit);
						if(options.restrictions == Options::Restrictions::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
						else if(options.restrictions == Options::Restrictions::NO_UNDO) {edited.clear_edge(*uit, *vit);}
					}
					break;
				case Options::Modes::ONLY_INSERT:
					for(auto vit = problem.begin() + 2; vit != problem.end(); vit++)
					{
						for(auto uit = problem.begin(); uit != vit - 1; uit++)
						{
							if(uit == problem.begin() && vit == problem.end() - 1 && !graph.has_edge(*uit, *vit)) {continue;}
							if(edited.has_edge(*uit, *vit)) {continue;}
							if(options.restrictions != Options::Restrictions::NONE) {edited.set_edge(*uit, *vit);}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(options.restrictions == Options::Restrictions::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
							else if(options.restrictions == Options::Restrictions::NO_UNDO) {edited.clear_edge(*uit, *vit);}
						}
					}
					break;
				}

				if(options.restrictions == Options::Restrictions::NO_REDUNDANT)
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
