
#ifndef CONSUMER_LOWER_BOUND_UPDATED_HPP
#define CONSUMER_LOWER_BOUND_UPDATED_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../Finder/SubgraphStats.hpp"
#include "../LowerBound/Lower_Bound.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Updated : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Updated";
		using Subgraph_Stats_type = ::Finder::Subgraph_Stats<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>;
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;

		static constexpr bool needs_subgraph_stats = false;
		struct State {
			Lower_Bound_Storage_type lb;
			bool remove_last_subgraph = false;
		};

	private:
		Graph_Edits bound_uses;
		Finder_impl finder;
	public:
		Updated(VertexID graph_size) : bound_uses(graph_size), finder(graph_size) {}

		State initialize(size_t, Graph const &graph, Graph_Edits const &edited)
		{
			State state;

			bound_uses.clear();

			finder.find(graph, [&](const subgraph_t& path)
			{
				bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					return bound_uses.has_edge(*uit, *vit);
				});

				if (!touches_bound)
				{
					state.lb.add(path);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						bound_uses.set_edge(*uit, *vit);
						return false;
					});
				}

				return false;
			}, bound_uses);

			return state;
		}

		void set_initial_k(size_t, Graph const&, Graph_Edits const&) {}

		void before_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			std::vector<subgraph_t>& lb = state.lb.get_bound();
			assert(!state.remove_last_subgraph);

			for (size_t i = 0; i < lb.size(); ++i)
			{
				bool has_uv = ::Finder::for_all_edges_unordered<Mode, Restriction, Conversion, Graph, Graph_Edits>(graph, edited, lb[i].begin(), lb[i].end(), [&](auto x, auto y)
				{
					return (u == *x && v == *y) || (u == *y && v == *x);
				});

				if (has_uv)
				{
					assert(i+1 == lb.size() || !state.remove_last_subgraph);
					state.remove_last_subgraph = true;
					std::swap(lb[i], lb.back());
				}
			}
		}

		void after_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			subgraph_t removed_subgraph;

			if (state.remove_last_subgraph) {
				removed_subgraph = state.lb.get_bound().back();
				state.lb.get_bound().pop_back();
			}

			initialize_bound_uses(state, graph, edited);

			auto add_to_bound_if_possible = [&](const subgraph_t& path)
			{
				bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					return bound_uses.has_edge(*uit, *vit);
				});

				if (!touches_bound)
				{
					state.lb.add(path);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						bound_uses.set_edge(*uit, *vit);
						return false;
					});
				}

				return false;
			};

			finder.find_near(graph, u, v, add_to_bound_if_possible, bound_uses);
			if (state.remove_last_subgraph) {
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, removed_subgraph.begin(), removed_subgraph.end(), [&](auto uit, auto vit) {
					if (!bound_uses.has_edge(*uit, *vit)) {
						finder.find_near(graph, *uit, *vit, add_to_bound_if_possible, bound_uses);
					}

					return false;
				});

				state.remove_last_subgraph = false;
			}

			state.lb.assert_maximal(graph, edited, finder);
		}

		void after_undo_edit(State&, Graph const&, Graph_Edits const&, VertexID, VertexID)
		{
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			after_mark_and_edit(state, graph, edited, u, v);
		}

		void after_unmark(Graph const&, Graph_Edits const&, VertexID, VertexID)
		{
		}

		size_t result(State& state, const Subgraph_Stats_type&, size_t, Graph const &graph, Graph_Edits const &edited, Options::Tag::Lower_Bound)
		{
			state.lb.assert_maximal(graph, edited, finder);
			return state.lb.size();
		}
	private:
		void initialize_bound_uses(const State& state, const Graph& graph, const Graph_Edits &edited)
		{
			bound_uses.clear();

			for (const subgraph_t& path : state.lb.get_bound())
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					bound_uses.set_edge(*uit, *vit);
					return false;
				});
			}
		}
	};
}

#endif
