#ifndef CONSUMER_LOWER_BOUND_MIN_DEG_HPP
#define CONSUMER_LOWER_BOUND_MIN_DEG_HPP

#include <vector>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <memory>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Bucket_PQ.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../util.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Min_Deg : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Min_Deg";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;
		using Subgraph_Stats_type = ::Finder::Subgraph_Stats<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>;

		static constexpr bool needs_subgraph_stats = false;
		struct State {
			Lower_Bound_Storage_type lb;
			bool remove_last_subgraph = false;
		};
	private:
		std::vector<subgraph_t> forbidden_subgraphs;
		Value_Matrix<std::vector<size_t>> subgraphs_per_edge;

		Graph_Edits bound_uses;
		Finder_impl finder;
	public:
		Min_Deg(VertexID graph_size) : subgraphs_per_edge(graph_size), bound_uses(graph_size), finder(graph_size) {;}

		State initialize(size_t, Graph const &, Graph_Edits const &)
		{
			return State{};
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

		size_t result(State& state, const Subgraph_Stats_type&, size_t k, Graph const &graph, Graph_Edits const &edited, Options::Tag::Lower_Bound)
		{
			if (state.lb.size() <= k) {
				Lower_Bound_Storage_type lb = calculate_min_deg_bound(graph, edited, k);
				if (lb.size() >= state.lb.size()) {
					state.lb = lb;
				}
			}

			if (state.lb.size() <= k) {
				state.lb.assert_maximal(graph, edited, finder);
			}

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

		Lower_Bound_Storage_type calculate_min_deg_bound(Graph const &graph, const Graph_Edits &edited, size_t k)
		{
			forbidden_subgraphs.clear();
			subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& v) { v.clear(); });
			bound_uses.clear();
			size_t sum_subgraphs_per_edge = 0;

			finder.find(graph, [&](const subgraph_t& path)
			{
				const size_t forbidden_index = forbidden_subgraphs.size();
				forbidden_subgraphs.push_back(path);

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					subgraphs_per_edge.at(*uit, *vit).push_back(forbidden_index);
					sum_subgraphs_per_edge++;
					return false;
				});
				return false;
			});

			Lower_Bound_Storage_type result;

			auto enumerate_neighbor_ids = [&subgraphs_per_edge = subgraphs_per_edge, &graph, &edited](const typename Lower_Bound_Storage_type::subgraph_t& fs, auto callback) {
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					const auto& new_neighbors = subgraphs_per_edge.at(*uit, *vit);
					for (size_t ne : new_neighbors)
					{
						callback(ne);
					}

					return false;
				});
			};

			BucketPQ pq(forbidden_subgraphs.size(), 42 * forbidden_subgraphs.size() + sum_subgraphs_per_edge);

			for (size_t fsid = 0; fsid < forbidden_subgraphs.size(); ++fsid) {
				const subgraph_t& fs = forbidden_subgraphs[fsid];

				size_t neighbor_count = 0;
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&neighbor_count, &subgraphs_per_edge = subgraphs_per_edge](auto uit, auto vit) {
					neighbor_count += subgraphs_per_edge.at(*uit, *vit).size();
					return false;
				});

				pq.insert(fsid, neighbor_count);
			}

			if (!pq.empty()) {
				pq.build();

				while (!pq.empty()) {
					auto idkey = pq.pop();

					const auto& fs = forbidden_subgraphs[idkey.first];

					result.add(fs.begin(), fs.end());
					if (k > 0 && result.size() > k) break;

					if (idkey.second > 1) {
						enumerate_neighbor_ids(fs, [&pq, &enumerate_neighbor_ids, &forbidden_subgraphs = forbidden_subgraphs](size_t nfsid)
						{
							if (pq.contains(nfsid)) {
								pq.erase(nfsid);
								enumerate_neighbor_ids(forbidden_subgraphs[nfsid], [&pq](size_t nnfsid)
								{
									if (pq.contains(nnfsid)) {
										pq.decrease_key_by_one(nnfsid);
									}
								});
							}
						});
					}
				}
			}

			result.assert_valid(graph, edited);

			return result;
		}


		// Some code to simplify the graph, seems to make results worse in fact.
		void clean_graph_structure(Graph const&g, Graph_Edits const &e)
		{
			size_t num_cleared = 0;
			subgraphs_per_edge.forAllNodePairs([&](VertexID u, VertexID v, std::vector<size_t>& subgraphs) {
				if (subgraphs.empty()) return;

				auto fs = forbidden_subgraphs[subgraphs.front()];

				if (u > v) std::swap(u, v);

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto xit, auto yit) {
					size_t x = *xit, y = *yit;
					if (x > y) std::swap(x, y);
					if (u == x && v == y) return false;

					std::vector<size_t> &inner_subgraphs = subgraphs_per_edge.at(x, y);
					if (subgraphs.size() <= inner_subgraphs.size() && std::includes(inner_subgraphs.begin(), inner_subgraphs.end(), subgraphs.begin(), subgraphs.end()))
					{
						subgraphs.clear();
						++num_cleared;
						return true;
					}

					return false;
				});
			});
		}
	};
}

#endif
