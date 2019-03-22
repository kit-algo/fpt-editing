#ifndef CONSUMER_SELECTOR_FIRST_HPP
#define CONSUMER_SELECTOR_FIRST_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../ProblemSet.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class First : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "First";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		ProblemSet problem;

	public:
		First(VertexID) {;}

		void prepare(size_t, const Lower_Bound_Storage_type&)
		{
			problem.vertex_pairs.clear();
			problem.needs_no_edit_branch = false;
			problem.found_solution = true;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&problem = problem](auto uit, auto vit)
			{
				problem.vertex_pairs.emplace_back(*uit, *vit);
				return false;
			});

			problem.found_solution = false;

			return true;
		}

		ProblemSet const &result(size_t, Graph const &, Graph const &, Options::Tag::Selector) const
		{
			return problem;
		}
	};
}

#endif
