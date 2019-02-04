#ifndef CONSUMER_SELECTOR_LEAST_HPP
#define CONSUMER_SELECTOR_LEAST_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../LowerBound/Lower_Bound.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Least : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "Least";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		std::vector<VertexID> problem;
		size_t free_pairs = 0;

	public:
		Least(VertexID) {;}

		void prepare(size_t, const Lower_Bound_Storage_type&)
		{
			problem.clear();
			free_pairs = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			size_t free = 0;

			// count unedited vertex pairs
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto, auto) {
				free++;
				return false;
			});

			if(free < free_pairs || free_pairs == 0)
			{
				free_pairs = free;
				problem = std::vector<VertexID>(b, e);
				if(free == 0) {return true;}
			}
			return false;
		}

		std::vector<VertexID> const &result(size_t, Graph const &, Graph const &, Options::Tag::Selector) const
		{
			return problem;
		}
	};
}

#endif
