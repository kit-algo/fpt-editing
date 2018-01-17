#ifndef INTERFACE_LOWER_BOUND_HPP
#define INTERFACE_LOWER_BOUND_HPP

#include <vector>

#include "../config.hpp"

namespace Lower_Bound
{
	template<typename Graph, typename Graph_Edits>
	class Lower_Bound
	{
	public:
		Lower_Bound(Graph const &);
		void prepare(Graph const &, Graph_Edits const &);
		bool next(Graph const &, Graph_Edits const &, std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator);
		size_t result() const;
	};
}

#endif
