#ifndef SELECTOR_HPP
#define SELECTOR_HPP

#include <vector>

#include "../config.hpp"

#include "../Finder/Finder.hpp"

namespace Selector
{
	template<typename Graph, typename Graph_Edits>
	class Selector : public Finder::Finder_Consumer<Graph, Graph_Edits>
	{
	public:
		std::vector<VertexID> const &result() const;
	};
}

#endif
