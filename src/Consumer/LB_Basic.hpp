#ifndef LOWER_BOUND_BASIC_HPP
#define LOWER_BOUND_BASIC_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Basic : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Basic";

	private:
		size_t found = 0;
		Graph_Edits used;

	public:
		Basic(VertexID graph_size) : used(graph_size) {;}

		void prepare()
		{
			used.clear();
			found = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			bool skip = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit){
				return used.has_edge(*uit, *vit);
			});

			if(skip) {return false;}
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit){
				used.set_edge(*uit, *vit);
				return false;
			});

			found++;
			return false;
		}

		size_t result() const
		{
			return found;
		}
	};
}

#endif
