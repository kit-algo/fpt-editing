#ifndef CONSUMER_LOWER_BOUND_BASIC_HPP
#define CONSUMER_LOWER_BOUND_BASIC_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Basic : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Basic";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		size_t found = 0;
		Graph_Edits used;

	public:
		Basic(VertexID graph_size) : used(graph_size) {;}

		void prepare(size_t, const Lower_Bound_Storage_type&)
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

		size_t result(size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound) const
		{
			return found;
		}

		const Lower_Bound_Storage_type result(size_t, Graph const&, Graph_Edits const&, Options::Tag::Lower_Bound_Update)
		{
			return Lower_Bound_Storage_type();
		}
	};
}

#endif
