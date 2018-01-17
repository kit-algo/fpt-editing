#ifndef FINDER_HPP
#define FINDER_HPP

#include <vector>

namespace Finder
{
	template<typename Finder, typename Selector, typename Lower_Bound, typename Graph, typename Graph_Edits>
	class Feeder
	{
	private:
		Finder &finder;
		Selector &selector;
		Lower_Bound &lower_bound;

		bool selector_done;
		bool lower_bound_done;

	public:
		Feeder(Finder &finder, Selector &selector, Lower_Bound &lower_bound) : finder(finder), selector(selector), lower_bound(lower_bound)
		{
			;
		}

		void feed(Graph const &graph, Graph_Edits const &edited)
		{
			selector_done = false;
			lower_bound_done = false;

			selector.prepare();
			lower_bound.prepare();

			finder.find(graph, edited, *this);
		}

		bool callback(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			if(!selector_done && !selector.next(graph, edited, b, e)) {selector_done = true;}
			if(!lower_bound_done && !lower_bound.next(graph, edited, b, e)) {lower_bound_done = true;}
			return selector_done && lower_bound_done;
		}
	};
}

#endif
