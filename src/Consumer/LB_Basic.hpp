#ifndef CONSUMER_LOWER_BOUND_BASIC_HPP
#define CONSUMER_LOWER_BOUND_BASIC_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../Finder/SubgraphStats.hpp"
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
		using Subgraph_Stats_type = ::Finder::Subgraph_Stats<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>;
	private:
			Graph_Edits used;
			Finder_impl finder;
	public:

		Basic(VertexID graph_size) : used(graph_size), finder(graph_size) {}

		size_t result(State&, const Subgraph_Stats_type&, size_t k, Graph const &graph, Graph_Edits const &edited, Options::Tag::Lower_Bound)
		{
			used.clear();

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
			}, used);

			return found;
		}
	};
}

#endif
