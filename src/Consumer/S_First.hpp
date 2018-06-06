#ifndef CONSUMER_SELECTOR_FIRST_HPP
#define CONSUMER_SELECTOR_FIRST_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class First : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "First";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		std::vector<VertexID> problem;

	public:
		First(VertexID) {;}

		void prepare(const Lower_Bound_Storage_type&)
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
