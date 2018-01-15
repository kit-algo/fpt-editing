#ifndef LOWER_BOUND_NO_HPP
#define LOWER_BOUND_NO_HPP

#include <vector>

#include "../config.hpp"

#include "Lower_Bound.hpp"
#include "../Graph/Graph.hpp"

namespace Lower_Bound
{
	class No : public Lower_Bound
	{
	public:
		static constexpr char const *name = "No";

	private:
		bool found = false;

	public:
		virtual void prepare()
		{
			found = false;
		}

		virtual bool next(Graph::Graph const &, Graph::Graph const &, std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator)
		{
			found = true;
			return false;
		}

		virtual size_t result() const
		{
			return found? 1 : 0;
		}
	};
}

#endif
