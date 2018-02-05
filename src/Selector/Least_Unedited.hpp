#ifndef SELECTOR_LEAST_HPP
#define SELECTOR_LEAST_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"

namespace Selector
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Least : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "Least";

	private:
		std::vector<VertexID> problem;
		size_t free_pairs = 0;

	public:
		Least(Graph const &) {;}

		void prepare()
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

		std::vector<VertexID> const &result(size_t, Graph const &, Graph const &) const
		{
			return problem;
		}
	};
}

#endif
