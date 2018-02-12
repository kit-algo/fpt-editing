#ifndef LOWER_BOUND_NO_HPP
#define LOWER_BOUND_NO_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class No : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "No";

	private:
		bool found = false;

	public:
		No(VertexID) {;}

		void prepare()
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
	};
}

#endif
