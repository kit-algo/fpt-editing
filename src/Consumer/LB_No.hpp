#ifndef CONSUMER_LOWER_BOUND_NO_HPP
#define CONSUMER_LOWER_BOUND_NO_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class No : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "No";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		bool found = false;

	public:
		No(VertexID) {;}

		void prepare(size_t, const Lower_Bound_Storage_type)
		{
			found = false;
		}

		bool next(Graph const &, Graph_Edits const &, std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator)
		{
			found = true;
			return true;
		}

		size_t result(size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound) const
		{
			return found? 1 : 0;
		}

		const Lower_Bound_Storage_type result(size_t, Graph const&, Graph_Edits const&, Options::Tag::Lower_Bound_Update) const
		{
			return Lower_Bound_Storage_type();
		}
	};
}

#endif
