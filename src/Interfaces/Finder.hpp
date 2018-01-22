#ifndef INTERFACE_FINDER_HPP
#define INTERFACE_FINDER_HPP

#include <vector>

#include "../config.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits>
	class Finder
	{
	public:
		static constexpr char const *name = "Finder Interface";

		Finder(Graph const &);

		template<typename Feeder>
		void find(Graph const &, Graph_Edits const &, Feeder &);
	};

	template<typename Finder, typename Graph, typename Graph_Edits, typename Selector, typename Lower_Bound>
	class Feeder
	{
	public:
		Feeder(Finder &finder, Selector &selector, Lower_Bound &lower_bound);
		void feed(Graph const &graph, Graph_Edits const &edited);
		bool callback(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e);
	};
}

#endif
