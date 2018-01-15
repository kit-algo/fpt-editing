#ifndef SELECTOR_HPP
#define SELECTOR_HPP

#include <vector>

#include "../config.hpp"

#include "../Finder/Finder.hpp"

namespace Selector
{
	class Selector : public Finder::Finder_Consumer
	{
	public:
		virtual std::vector<VertexID> const &result() const = 0;
	};
}

#endif
