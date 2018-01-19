#ifndef SELECTOR_FIRST_HPP
#define SELECTOR_FIRST_HPP

#include <vector>

#include "../config.hpp"

namespace Selector
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class First
	{
	public:
		static constexpr char const *name = "First";

	private:
		std::vector<VertexID> problem;

	public:
		First(Graph const &) {;}

		void prepare()
		{
			problem.clear();
		}

		bool next(Graph const &, Graph_Edits const &, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			problem.insert(problem.end(), b, e);
			return false;
		}

		std::vector<VertexID> const &result() const
		{
			return problem;
		}
	};
}

#endif
