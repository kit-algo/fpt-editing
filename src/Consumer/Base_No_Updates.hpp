#ifndef CONSUMER_BASE_NO_UPDATES_HPP
#define CONSUMER_BASE_NO_UPDATES_HPP

#include <cstddef>

#include "../config.hpp"

namespace Consumer
{
	/**
	 * Base class that implements empty methods for all update methods, i.e.,
	 * initialize, before/after_mark_and_edit, before/after_undo_edit
	 * and before/after_undo_mark.
	 */
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Base_No_Updates
	{
	public:
		static constexpr bool needs_subgraph_stats = false;

		struct State {};

		State initialize(size_t, Graph const &, Graph_Edits const &)
		{
			return State{};
		};

		void set_initial_k(size_t, Graph const&, Graph_Edits const&) {}

		void before_mark_and_edit(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		};

		void after_mark_and_edit(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		};

		void after_undo_edit(State&, Graph const&, Graph_Edits const&, VertexID, VertexID)
		{
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		};

		void after_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		};

		void after_unmark(Graph const&, Graph_Edits const&, VertexID, VertexID)
		{
		}
	};
}

#endif
