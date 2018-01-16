#ifndef LOWER_BOUND_HPP
#define LOWER_BOUND_HPP

#include "../config.hpp"

#include "../Finder/Finder.hpp"

namespace Lower_Bound
{
	template<typename Graph, typename Graph_Edits>
	class Lower_Bound : public Finder::Finder_Consumer<Graph, Graph_Edits>
	{
	public:
		Lower_Bound(Graph const &);
		size_t result() const;
	};
}

#endif
