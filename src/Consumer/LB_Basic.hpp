#ifndef CONSUMER_LOWER_BOUND_BASIC_HPP
#define CONSUMER_LOWER_BOUND_BASIC_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "Base_No_Updates.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Basic : Options::Tag::Lower_Bound, public Base_No_Updates<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>
	{
	public:
		static constexpr char const *name = "Basic";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;
		using State = typename Base_No_Updates<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>::State;

		Basic(VertexID) {}

		size_t result(State&, size_t k, Graph const &graph, Graph_Edits const &edited, Options::Tag::Lower_Bound) const
		{
			Graph_Edits used(graph.size());
			Finder_impl finder(graph.size());

			size_t found = 0;

			finder.find(graph, [&](const subgraph_t& path)
			{
				bool skip = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					return used.has_edge(*uit, *vit);
				});

				if (skip)
				{
					return false;
				}

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					used.set_edge(*uit, *vit);
					return false;
				});


				++found;

				return found > k;
			});

			return found;
		}
	};
}

#endif
