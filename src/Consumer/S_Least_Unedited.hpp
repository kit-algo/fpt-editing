#ifndef CONSUMER_SELECTOR_LEAST_HPP
#define CONSUMER_SELECTOR_LEAST_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../ProblemSet.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Least : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "Least";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		ProblemSet problem;
	public:
		Least(VertexID) {;}

		void prepare(size_t, const Lower_Bound_Storage_type&)
		{
			problem.vertex_pairs.clear();
			problem.found_solution = true;
			problem.needs_no_edit_branch = false;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			size_t free = 0;

			// count unedited vertex pairs
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto, auto) {
				free++;
				return false;
			});

			if(free < problem.vertex_pairs.size() || problem.found_solution)
			{
				problem.vertex_pairs.clear();
				problem.found_solution = false;
				Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
					problem.vertex_pairs.emplace_back(*uit, *vit);
					return false;
				});

				if(free == 0) {return true;}
			}
			return false;
		}

		ProblemSet const &result(size_t, Graph const &, Graph const &, Options::Tag::Selector) const
		{
			return problem;
		}
	};
}

#endif
