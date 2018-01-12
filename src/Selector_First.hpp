#ifndef SELECTOR_FIRST_HPP
#define SELECTOR_FIRST_HPP

#include <vector>

#include "Finder.hpp"
#include "Graph.hpp"

namespace Selector
{
	class Selector : public Finder::Finder_Consumer
	{
	public:
		virtual std::vector<VertexID> const &result() const = 0;
	};

	class First : public Selector
	{
	public:
		static constexpr char const *name = "First";

	private:
		std::vector<VertexID> problem;

	public:
		virtual void prepare()
		{
			problem.clear();
		}

		virtual bool next(Graph::Graph const &, Graph::Graph const &, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			problem.insert(problem.end(), b, e);
			return false;
		}

		virtual std::vector<VertexID> const &result() const
		{
			return problem;
		}
	};
}

#endif
