#ifndef EDITOR_HPP
#define EDITOR_HPP

#include <functional>
#include <vector>

#include "Finder.hpp"
#include "Graph.hpp"
#include "Graph_Matrix.hpp"
#include "Selector_First.hpp"
#include "LB_No.hpp"

namespace Editor
{
	struct is_editor {};

	class Options
	{
	public:
		bool all_solutions = false;
		std::function<void(Graph::Graph const &, Graph::Graph const &)> write;

		enum Restrictions {NONE, NO_UNDO, NO_REDUNDANT};
		Restrictions restrictions = NONE;

		enum Path_Cycle_Conversion {NORMAL, DO_LAST, SKIP};
		Path_Cycle_Conversion conversions = NORMAL;

		enum Modes {ANY, ONLY_DELETE, ONLY_INSERT};
		Modes mode = ANY;
	};

	class Editor : is_editor
	{
	public:
		static constexpr char const *name = "Editor";

	private:
		Options const &options;
		Graph::Graph &graph;
		Finder::Finder &finder;
		Selector::Selector &selector;
		Lower_Bound::Lower_Bound &lb;

		Graph::Matrix<> edited;

		bool found_soulution;

	public:
		Editor(Options const &options, Graph::Graph &graph, Finder::Finder &finder, Selector::Selector &selector, Lower_Bound::Lower_Bound &lb) : options(options), graph(graph), finder(finder), selector(selector), lb(lb), edited(graph.size())
		{
			;
		}

		bool edit(size_t k)
		{

			// lock?
			// zero stats
			//TODO: add stats
			found_soulution = false;

			edit_rec(k);
			return found_soulution;
		}

	private:

		bool edit_rec(size_t k)
		{
			// start finder and feed into selector and lb
			Finder::feed(graph, edited, finder, {&selector, &lb});

			// graph solved?
			auto problem = selector.result();
			if(problem.empty())
			{
				if(options.write) {options.write(graph, edited);}
				found_soulution = true;
				return !options.all_solutions;
			}
			else if(k < lb.result())
			{
				// lower bound too high
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

				std::vector<std::pair<size_t, size_t>> marked;
				switch(options.mode)
				{
				case Options::ANY:
					for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
					{
						for(auto uit = problem.begin(); uit != vit; uit++)
						{
							if(options.conversions != Options::NORMAL && uit == problem.begin() && vit == problem.end() - 1) {continue;}
							if(edited.has_edge(*uit, *vit)) {continue;}
							if(options.restrictions != Options::NONE) {edited.set_edge(*uit, *vit);}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(options.restrictions == Options::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
							else if(options.restrictions == Options::NO_UNDO) {edited.clear_edge(*uit, *vit);}
						}
					}
					if(options.conversions == Options::DO_LAST && !edited.has_edge(problem.front(), problem.back()))
					{
						auto uit = problem.begin();
						auto vit = problem.end() - 1;
						if(options.restrictions != Options::NONE) {edited.set_edge(*uit, *vit);}
						graph.toggle_edge(*uit, *vit);
						if(edit_rec(k - 1)) {return true;}
						graph.toggle_edge(*uit, *vit);
						if(options.restrictions == Options::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
						else if(options.restrictions == Options::NO_UNDO) {edited.clear_edge(*uit, *vit);}
					}

					break;
				case Options::ONLY_DELETE:
					for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
					{
						auto uit = vit - 1;
						if(edited.has_edge(*uit, *vit)) {continue;}
						if(options.restrictions != Options::NONE) {edited.set_edge(*uit, *vit);}
						graph.toggle_edge(*uit, *vit);
						if(edit_rec(k - 1)) {return true;}
						graph.toggle_edge(*uit, *vit);
						if(options.restrictions == Options::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
						else if(options.restrictions == Options::NO_UNDO) {edited.clear_edge(*uit, *vit);}
					}
					if(graph.has_edge(problem.front(), problem.back()) && !edited.has_edge(problem.front(), problem.back()))
					{
						auto uit = problem.begin();
						auto vit = problem.end() - 1;
						if(options.restrictions != Options::NONE) {edited.set_edge(*uit, *vit);}
						graph.toggle_edge(*uit, *vit);
						if(edit_rec(k - 1)) {return true;}
						graph.toggle_edge(*uit, *vit);
						if(options.restrictions == Options::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
						else if(options.restrictions == Options::NO_UNDO) {edited.clear_edge(*uit, *vit);}
					}
					break;
				case Options::ONLY_INSERT:
					for(auto vit = problem.begin() + 2; vit != problem.end(); vit++)
					{
						for(auto uit = problem.begin(); uit != vit - 1; uit++)
						{
							if(uit == problem.begin() && vit == problem.end() - 1 && !graph.has_edge(*uit, *vit)) {continue;}
							if(edited.has_edge(*uit, *vit)) {continue;}
							if(options.restrictions != Options::NONE) {edited.set_edge(*uit, *vit);}
							graph.toggle_edge(*uit, *vit);
							if(edit_rec(k - 1)) {return true;}
							graph.toggle_edge(*uit, *vit);
							if(options.restrictions == Options::NO_REDUNDANT) {marked.emplace_back(*uit, *vit);}
							else if(options.restrictions == Options::NO_UNDO) {edited.clear_edge(*uit, *vit);}
						}
					}
					break;
				}

				if(options.restrictions == Options::NO_REDUNDANT)
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
