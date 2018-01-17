#ifndef INTERFACE_SELECTOR_HPP
#define INTERFACE_SELECTOR_HPP

#include <vector>

#include "../config.hpp"

namespace Selector
{
	template<typename Graph, typename Graph_Edits>
	class Selector
	{
	public:
		Selector(Graph const &);
		void prepare(Graph const &, Graph_Edits const &);
		bool next(Graph const &, Graph_Edits const &, std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator);
		std::vector<VertexID> const &result() const;
	};
}

#endif
