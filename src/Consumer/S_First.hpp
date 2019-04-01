#ifndef CONSUMER_SELECTOR_FIRST_HPP
#define CONSUMER_SELECTOR_FIRST_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../ProblemSet.hpp"
#include "Base_No_Updates.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class First : Options::Tag::Selector, public Base_No_Updates<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>
	{
	public:
		static constexpr char const *name = "First";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;

	private:
		Finder_impl finder;

	public:
		First(VertexID graph_size) : finder(graph_size) {;}

		ProblemSet const result(size_t, Graph const &graph, Graph const &edited, Options::Tag::Selector)
		{
			ProblemSet problem;
			problem.found_solution = true;
			problem.needs_no_edit_branch = false;

			finder.find(graph, [&](const subgraph_t& sg)
			{
				Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, sg.begin(), sg.end(), [&problem = problem](auto uit, auto vit)
				{
					problem.vertex_pairs.emplace_back(*uit, *vit);
					return false;
				});

				problem.found_solution = false;

				return true;
			});

			return problem;
		}
	};
}

#endif
