#ifndef LB_NO_HPP
#define LB_NO_HPP

#include <vector>

#include "Finder.hpp"
#include "Graph.hpp"

namespace Lower_Bound
{
	class Lower_Bound : public Finder::Finder_Consumer
	{
	public:
		virtual size_t result() const = 0;
	};

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
