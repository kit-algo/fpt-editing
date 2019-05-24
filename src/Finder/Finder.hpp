#ifndef FINDER_HPP
#define FINDER_HPP

#include "../config.hpp"

#include "../Options.hpp"

#include <array>
#include <utility>
#include <vector>

namespace Finder
{
	/* Helper functions */

	/** Iterate over a forbidden subgraph, limited to edges currently editable
	 * calls f(iterator, iterator) for each editable edge
	 */
	template<typename Mode, typename Restriction, typename Conversion, typename Graph, typename Graph_Edits, typename VertexIterator, typename Func>
	inline bool for_all_edges_ordered(Graph const &graph, Graph_Edits const &edited, VertexIterator begin, VertexIterator end, Func func)
	{
		if constexpr (std::is_same<Mode, Options::Modes::Edit>::value)
		{
			for(auto vit = begin + 1; vit != end; vit++)
			{
				for(auto uit = begin; uit != vit; uit++)
				{
					if(!std::is_same<Conversion, Options::Conversions::Normal>::value && uit == begin && vit == end - 1) {continue;}
					if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				}
			}
			if(std::is_same<Conversion, Options::Conversions::Last>::value)
			{
				auto uit = begin;
				auto vit = end - 1;
				do
				{
					if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if constexpr (std::is_same<Mode, Options::Modes::Delete>::value)
		{
			for(auto uit = begin, vit = begin + 1; vit != end; uit++, vit++)
			{
				if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
				{
					if(edited.has_edge(*uit, *vit)) {continue;}
				}
				if(func(uit, vit)) {return true;}
			}
			if(!std::is_same<Conversion, Options::Conversions::Skip>::value && graph.has_edge(*begin, *(end - 1)))
			{
				auto uit = begin;
				auto vit = end - 1;
				do
				{
					if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if constexpr (std::is_same<Mode, Options::Modes::Insert>::value)
		{
			for(auto vit = begin + 2; vit != end; vit++)
			{
				for(auto uit = begin; uit != vit - 1; uit++)
				{
					if(uit == begin && vit == end - 1 && (!std::is_same<Conversion, Options::Conversions::Normal>::value || graph.has_edge(*uit, *vit))) {continue;}
					if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				}
			}
			if(std::is_same<Conversion, Options::Conversions::Last>::value && !graph.has_edge(*begin, *(end - 1)))
			{
				auto uit = begin;
				auto vit = end - 1;
				do
				{
					if constexpr (!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				} while(false);
			}
		}
		return false;
	}

	/** Iterate over a forbidden subgraph, limited to edges currently editable, without adhering to editing order (i.e. Options::Conversion::Skip is treated as Normal)
	 * useful if order doesn't matter, e.g. when just counting edges
	 * calls f(iterator, iterator) for each editable edge
	 */
	template<typename Mode, typename Restriction, typename Conversion, typename Graph, typename Graph_Edits, typename VertexIterator, typename Func>
	inline bool for_all_edges_unordered(Graph const &graph, Graph_Edits const &edited, VertexIterator begin, VertexIterator end, Func func)
	{
		if constexpr (std::is_same<Mode, Options::Modes::Edit>::value)
		{
			for(auto vit = begin + 1; vit != end; vit++)
			{
				for(auto uit = begin; uit != vit; uit++)
				{
					if(std::is_same<Conversion, Options::Conversions::Skip>::value && uit == begin && vit == end - 1) {continue;}
					if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				}
			}
		}
		else if constexpr (std::is_same<Mode, Options::Modes::Delete>::value)
		{
			for(auto uit = begin, vit = begin + 1; vit != end; uit++, vit++)
			{
				if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
				{
					if(edited.has_edge(*uit, *vit)) {continue;}
				}
				if(func(uit, vit)) {return true;}
			}
			if(!std::is_same<Conversion, Options::Conversions::Skip>::value && graph.has_edge(*begin, *(end - 1)))
			{
				auto uit = begin;
				auto vit = end - 1;
				do
				{
					if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if constexpr(std::is_same<Mode, Options::Modes::Insert>::value)
		{
			for(auto vit = begin + 2; vit != end; vit++)
			{
				for(auto uit = begin; uit != vit - 1; uit++)
				{
					if(uit == begin && vit == end - 1 && (std::is_same<Conversion, Options::Conversions::Skip>::value || graph.has_edge(*uit, *vit))) {continue;}
					if constexpr(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				}
			}
		}
		return false;
	}
}

#endif
