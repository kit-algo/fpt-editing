#ifndef CONSUMER_LOWER_BOUND_NO_HPP
#define CONSUMER_LOWER_BOUND_NO_HPP

#include <vector>

#include "../config.hpp"

#include "../Finder/SubgraphStats.hpp"
#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "Base_No_Updates.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class No : Options::Tag::Lower_Bound, public Base_No_Updates<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>
	{
	public:
		static constexpr char const *name = "No";
		using State = typename Base_No_Updates<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>::State;
		using Subgraph_Stats_type = ::Finder::Subgraph_Stats<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>;
	public:
		No(VertexID) {;}

		size_t result(State&, const Subgraph_Stats_type&, size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound) const
		{
			return 0;
		}
	};
}

#endif
