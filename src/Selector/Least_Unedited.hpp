#ifndef SELECTOR_LEAST_UNEDITED_HPP
#define SELECTOR_LEAST_UNEDITED_HPP

#include <vector>

#include "../config.hpp"

namespace Selector
{
	template<typename Graph, typename Graph_Edits>
	class Least_Unedited
	{
	public:
		static constexpr char const *name = "Least";

	private:
		std::vector<VertexID> problem;
		size_t free_pairs;

	public:
		void prepare()
		{
			problem.clear();
			free_pairs = 0;
		}

		bool next(Graph const &, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			size_t free = 0;
			// count unedited vertex pairs
			for(auto it = b + 1; it != e; it++)
			{
				for(auto it2 = b; it2 != it; it2++)
				{
					if(edited.has_edge(*it, *it2) || (it2 == b && it + 1 == e))
					{
						continue;
					}
					free++;
				}
			}
			if(free < free_pairs || free_pairs == 0)
			{
				problem.clear();
				problem.insert(problem.end(), b, e);
				//problem = std::vector<VertexID>(b, e);
				if(free == 0) {return false;}
			}
			return true;
		}

		std::vector<VertexID> const &result() const
		{
			return problem;
		}
	};
}

#endif
