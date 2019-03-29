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

	private:
		std::vector<typename Lower_Bound_Storage_type::subgraph_t> forbidden_subgraphs;
		Value_Matrix<std::vector<size_t>> subgraphs_per_edge;
		size_t sum_subgraphs_per_edge;

		bool bound_calculated;

		Graph_Edits used_updated;
		bool used_updated_initialized;

		Lower_Bound_Storage_type bound_updated;
	public:
		Min_Deg(VertexID graph_size) : subgraphs_per_edge(graph_size), sum_subgraphs_per_edge(0), bound_calculated(false), used_updated(graph_size), used_updated_initialized(false) {;}

		void prepare(size_t, const Lower_Bound_Storage_type& lower_bound)
		{
			forbidden_subgraphs.clear();
			subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& v) { v.clear(); });

			bound_calculated = false;
			used_updated.clear();
			used_updated_initialized = false;
			bound_updated = lower_bound;
			sum_subgraphs_per_edge = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			if (!used_updated_initialized)
			{
				for (const auto fs : bound_updated.get_bound())
				{
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit)
					{
						used_updated.set_edge(*uit, *vit);
						return false;
					});
				}

				used_updated_initialized = true;
			}

			const size_t forbidden_index = forbidden_subgraphs.size();
			forbidden_subgraphs.emplace_back(Util::to_array<VertexID, length>(b));

			bool touches_bound = false;

			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				subgraphs_per_edge.at(*uit, *vit).push_back(forbidden_index);
				sum_subgraphs_per_edge++;

				touches_bound |= used_updated.has_edge(*uit, *vit);
				return false;
			});

			if (!touches_bound)
			{
				bound_updated.add(b, e);

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
					used_updated.set_edge(*uit, *vit);
					return false;
				});
			}

			return false;
		}

		size_t result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound)
		{
			prepare_result(k, g, e);
			return bound_updated.size();
		}

		const Lower_Bound_Storage_type& result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound_Update)
		{
			prepare_result(k, g, e);
			return bound_updated;
		}

	private:
		Lower_Bound_Storage_type initialize_lb_min_deg(size_t k, const Graph& g, const Graph_Edits& e)
		{
			Lower_Bound_Storage_type result;

			auto enumerate_neighbor_ids = [&subgraphs_per_edge = subgraphs_per_edge, &g, &e](const typename Lower_Bound_Storage_type::subgraph_t& fs, auto callback) {
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
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
				const typename Lower_Bound_Storage_type::subgraph_t& fs = forbidden_subgraphs[fsid];

				size_t neighbor_count = 0;
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&neighbor_count, &subgraphs_per_edge = subgraphs_per_edge](auto uit, auto vit) {
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

			result.assert_valid(g, e);

			return result;
		}


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

		void prepare_result(size_t k, Graph const &g, Graph_Edits const &e)
		{
			if (bound_calculated) return;
			bound_calculated = true;

			if (bound_updated.size() <= k)
			{
				// Seems to make results worse
				//clean_graph_structure(g, e);

				Lower_Bound_Storage_type bound_new = initialize_lb_min_deg(k, g, e);

				if (bound_updated.size() < bound_new.size())
				{
					bound_updated = bound_new;
				}
			}
		}
	};
}

#endif
