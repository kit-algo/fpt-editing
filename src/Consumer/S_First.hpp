#ifndef SELECTOR_FIRST_HPP
#define SELECTOR_FIRST_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class First : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "First";

	private:
		std::vector<VertexID> problem;

	public:
		First(VertexID) {;}

		void prepare()
		{
			problem.clear();
		}

		bool next(Graph const &, Graph_Edits const &, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			problem.insert(problem.end(), b, e);
			return true;
		}

		std::vector<VertexID> const &result(size_t, Graph const &, Graph const &, Options::Tag::Selector) const
		{
			return problem;
		}
	};
}

#endif
