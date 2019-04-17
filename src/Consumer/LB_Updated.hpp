
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
		};

	private:
		Graph_Edits bound_uses;
		Finder_impl finder;
	public:
		Updated(VertexID graph_size) : bound_uses(graph_size), finder(graph_size) {}

		State initialize(size_t k, Graph const &graph, Graph_Edits const &edited)
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

				// Assumption: if the bound is too high, initialize will be called again anyway.
				return state.lb.size() > k;
			});

			return state;
		}

		void before_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			std::vector<subgraph_t>& lb = state.lb.get_bound();

			for (size_t i = 0; i < lb.size();)
			{
				bool has_uv = ::Finder::for_all_edges_unordered<Mode, Restriction, Conversion, Graph, Graph_Edits>(graph, edited, lb[i].begin(), lb[i].end(), [&](auto x, auto y)
				{
					return (u == *x && v == *y) || (u == *y && v == *x);
				});

				if (has_uv)
				{
					lb[i] = lb.back();
					lb.pop_back();
				}
				else
				{
					++i;
				}
			}
		}

		void after_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			initialize_bound_uses(state, graph, edited);

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
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
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			after_mark_and_edit(state, graph, edited, u, v);
		}

		size_t result(State& state, const Subgraph_Stats_type&, size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound)
		{
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
