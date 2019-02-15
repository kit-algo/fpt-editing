#ifndef CONSUMER_LOWER_BOUND_MIN_DEG_HPP
#define CONSUMER_LOWER_BOUND_MIN_DEG_HPP

#include <vector>
#include <stdexcept>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Bucket_PQ.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../util.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
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

		Lower_Bound_Storage_type bound_updated;
		Lower_Bound_Storage_type bound_new;
	public:
		Min_Deg(VertexID graph_size) : subgraphs_per_edge(graph_size), sum_subgraphs_per_edge(0), bound_calculated(false), used_updated(graph_size) {;}

		void prepare(size_t, const Lower_Bound_Storage_type& lower_bound)
		{
			forbidden_subgraphs.clear();
			subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& v) { v.clear(); });

			bound_calculated = false;
			used_updated.clear();
			bound_new.clear();
			bound_updated = lower_bound;
			sum_subgraphs_per_edge = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			const size_t forbidden_index = forbidden_subgraphs.size();
			forbidden_subgraphs.emplace_back(Util::to_array<VertexID, length>(b));

			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				subgraphs_per_edge.at(*uit, *vit).push_back(forbidden_index);
				sum_subgraphs_per_edge++;

				return false;
			});

			return false;
		}

		size_t result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound)
		{
			prepare_result(k, g, e);
			return bound_new.size();
		}

		const Lower_Bound_Storage_type& result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound_Update)
		{
			prepare_result(k, g, e);
			return bound_new;
		}

	private:
		void prepare_result(size_t k, Graph const &g, Graph_Edits const &e)
		{
			if (bound_calculated) return;
			bound_calculated = true;

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

			std::vector<bool> can_use(forbidden_subgraphs.size(), true);
			BucketPQ pq(forbidden_subgraphs.size(), 42 * forbidden_subgraphs.size() + sum_subgraphs_per_edge);

			auto calculate_lb = [&pq, &enumerate_neighbor_ids, &can_use, &forbidden_subgraphs = forbidden_subgraphs, k, &g, &e, &subgraphs_per_edge = subgraphs_per_edge](Lower_Bound_Storage_type &lb) {
				size_t total_neighbors_size = 0, max_neighbor_size = 0, min_neighbor_size = std::numeric_limits<size_t>::max();

				for (size_t fsid = 0; fsid < forbidden_subgraphs.size(); ++fsid) {
					if (!can_use[fsid]) continue;

					const typename Lower_Bound_Storage_type::subgraph_t& fs = forbidden_subgraphs[fsid];

					size_t neighbor_count = 0;
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&neighbor_count, &subgraphs_per_edge = subgraphs_per_edge](auto uit, auto vit) {
						neighbor_count += subgraphs_per_edge.at(*uit, *vit).size();
						return false;
					});

					total_neighbors_size += neighbor_count;
					if (neighbor_count > max_neighbor_size) {
						max_neighbor_size = neighbor_count;
					}

					if (neighbor_count < min_neighbor_size) {
						min_neighbor_size = neighbor_count;
					}

					pq.insert(fsid, neighbor_count);
				}

				if (!pq.empty()) {
					pq.build();

					while (!pq.empty()) {
						auto idkey = pq.pop();

						const auto& fs = forbidden_subgraphs[idkey.first];

						lb.add(fs.begin(), fs.end());
						if (k > 0 && lb.size() > k) return;

						if (idkey.second > 1) {
							enumerate_neighbor_ids(fs, [&pq, &enumerate_neighbor_ids, &forbidden_subgraphs](size_t nfsid)
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

				lb.assert_valid(g, e);

				//std::cout << "k = " << k << " #fs = " << forbidden_subgraphs.size() << " max_neigh = " << max_neighbor_size << " avg_neighbors = " << total_neighbors_size / forbidden_subgraphs.size() << " min_neighbors = " << min_neighbor_size << " lb: " << lb.size() << std::endl;
			};

			calculate_lb(bound_new);

			if (k > 0 && bound_new.size() > k) return;

			// update existing lower bound
			// Mark all node pairs that are used as used
			for (const auto& fs : bound_updated.get_bound()) {
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					used_updated.set_edge(*uit, *vit);
					return false;
				});
			}

			bool found_usable = false;
			// Check for all forbidden subgraphs if they can be used
			for (size_t i = 0; i < forbidden_subgraphs.size(); ++i) {
				const auto& fs = forbidden_subgraphs[i];

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					if (used_updated.has_edge(*uit, *vit)) {
						can_use[i] = false;
						return true;
					}
					return false;
				});

				found_usable |= can_use[i];
			}

			if (found_usable)
			{
				// Remove all subgraphs that cannot be used from neighbors
				subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& fs_ids)
				{
					auto new_end = std::remove_if(fs_ids.begin(), fs_ids.end(), [&](size_t v) { return !can_use[v]; });
					fs_ids.erase(new_end, fs_ids.end());
				});

				pq.clear();

				calculate_lb(bound_updated);
			}


			if (bound_updated.size() > bound_new.size()) {
				bound_new = bound_updated;
			}
		}
	};
}

#endif
