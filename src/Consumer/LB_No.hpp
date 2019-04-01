#ifndef CONSUMER_LOWER_BOUND_NO_HPP
#define CONSUMER_LOWER_BOUND_NO_HPP

#include <vector>

#include "../config.hpp"

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
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	public:
		No(VertexID) {;}

		size_t result(size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound) const
		{
			return 0;
		}

		const Lower_Bound_Storage_type result(size_t, Graph const&, Graph_Edits const&, Options::Tag::Lower_Bound_Update) const
		{
			return Lower_Bound_Storage_type();
		}
	};
}

#endif
