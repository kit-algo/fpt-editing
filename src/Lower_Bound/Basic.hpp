#ifndef LOWER_BOUND_BASIC_HPP
#define LOWER_BOUND_BASIC_HPP

#include <vector>

#include "../config.hpp"

namespace Lower_Bound
{
	template<typename Graph, typename Graph_Edits>
	class Basic
	{
	public:
		static constexpr char const *name = "Basic";

	private:
		size_t found = 0;
		Graph_Edits used;

	public:
		Basic(Graph const &graph) : used(graph.size())
		{
			;
		}

		void prepare()
		{
			used.clear();
			found = 0;
		}

		bool next(Graph const &, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			//TODO: needs Editor::Options
			//currently edit + skip

			// test if we can add it to bound
			for(auto it = b + 1; it != e; it++)
			{
				for(auto it2 = b; it2 != it; it2++)
				{
					if(edited.has_edge(*it, *it2) || (it2 == b && it + 1 == e))
					{
						continue;
					}
					if(used.has_edge(*it, *it2))
					{
						return true;
					}
				}
			}
			// add to bound
			for(auto it = b + 1; it != e; it++)
			{
				for(auto it2 = b; it2 != it; it2++)
				{
					if(edited.has_edge(*it, *it2) || (it2 == b && it + 1 == e))
					{
						continue;
					}
					used.set_edge(*it, *it2);
				}
			}
			found++;
			return true;
		}

		size_t result() const
		{
			return found;
		}
	};
}

#endif
