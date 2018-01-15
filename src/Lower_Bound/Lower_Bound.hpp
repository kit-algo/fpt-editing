#ifndef LOWER_BOUND_HPP
#define LOWER_BOUND_HPP

#include "../config.hpp"

#include "../Finder/Finder.hpp"

namespace Lower_Bound
{
	class Lower_Bound : public Finder::Finder_Consumer
	{
	public:
		virtual size_t result() const = 0;
	};
}

#endif
