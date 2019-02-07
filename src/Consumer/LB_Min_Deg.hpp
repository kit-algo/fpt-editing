#ifndef CONSUMER_LOWER_BOUND_MIN_DEG_HPP
#define CONSUMER_LOWER_BOUND_MIN_DEG_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../id_queue.h"
#include "../Graph/ValueMatrix.hpp"

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

		bool bound_calculated;

		Graph_Edits used_updated;

		Lower_Bound_Storage_type bound_updated;
		Lower_Bound_Storage_type bound_new;
	public:
		Min_Deg(VertexID graph_size) : subgraphs_per_edge(graph_size), used_updated(graph_size) {;}

		void prepare(size_t, const Lower_Bound_Storage_type& lower_bound)
		{
			forbidden_subgraphs.clear();
			subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& v) { v.clear(); });

			bound_calculated = false;
			used_updated.clear();
			bound_new.clear();
			bound_updated = lower_bound;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			const size_t forbidden_index = forbidden_subgraphs.size();
			{ // insert into array
				forbidden_subgraphs.emplace_back();
				auto it = b;
				for (size_t i = 0; i < length; ++i)
					{
						if (it == e) abort();

						forbidden_subgraphs.back()[i] = *it;
						++it;
					}

				if (it != e) abort();
			}

			size_t free = 0;
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				subgraphs_per_edge.at(*uit, *vit).push_back(forbidden_index);
				free++;
				return false;
			});

			return false;
		}

		size_t result(size_t, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound)
		{
			prepare_result(g, e);
			return bound_new.size();
		}

		const Lower_Bound_Storage_type& result(size_t, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound_Update)
		{
			prepare_result(g, e);
			return bound_new;
		}

	private:
		void prepare_result(Graph const &g, Graph_Edits const &e)
		{
			if (bound_calculated) return;
			bound_calculated = true;

			std::vector<size_t> tmp;
			auto populate_neighbor_ids = [&](const typename Lower_Bound_Storage_type::subgraph_t& fs, std::vector<size_t>& neighbors) {
				neighbors.clear();
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					const auto& new_neighbors = subgraphs_per_edge.at(*uit, *vit);

					if (neighbors.size() < 2) {
						// One neighbors is always fs itself
						// And it is in every list of neighbors - no need to merge.
						neighbors = new_neighbors;
					} else if (new_neighbors.size() > 1) {
						// As one neighbor is always fs itself, no need to merge
						// new neighbors that contain only one node.
						std::set_union(neighbors.begin(), neighbors.end(), new_neighbors.begin(), new_neighbors.end(), std::back_inserter(tmp));
						std::swap(tmp, neighbors);
						tmp.clear();
					}
					return false;
				});
			};

			RoutingKit::MinIDQueue pq(forbidden_subgraphs.size());

			std::vector<size_t> neighbors, neighbors_of_neighbors;

			size_t total_neighbors_size = 0, max_neighbor_size = 0, min_neighbor_size = std::numeric_limits<size_t>::max();

			for (size_t fsid = 0; fsid < forbidden_subgraphs.size(); ++fsid) {
				const typename Lower_Bound_Storage_type::subgraph_t& fs = forbidden_subgraphs[fsid];

				populate_neighbor_ids(fs, neighbors);

				total_neighbors_size += neighbors.size();
				if (neighbors.size() > max_neighbor_size) {
					max_neighbor_size = neighbors.size();
				}

				if (neighbors.size() < min_neighbor_size) {
					min_neighbor_size = neighbors.size();
				}

				if (neighbors.size() == 1) {
					assert(neighbors.back() == fsid);
					bound_new.add(fs.begin(), fs.end());
				} else {
					pq.push({fsid, neighbors.size()});
				}
			}

			auto process_pq = [&](Lower_Bound_Storage_type &lb) {
			while (!pq.empty()) {
					auto idkey = pq.pop();

					const auto& fs = forbidden_subgraphs[idkey.id];

					if (idkey.key > 1) {
					    populate_neighbor_ids(fs, neighbors);
					    for (size_t nfsid : neighbors) {
						    if (pq.contains_id(nfsid)) {
							    { // simulate deletion - there should be no element with key 0
								    assert(pq.peek().key > 0);
								    pq.decrease_key({nfsid, 0});
								    auto el = pq.pop();
								    if (el.id != nfsid) { throw std::logic_error("Invalid pq element found"); }
							    }

							    populate_neighbor_ids(forbidden_subgraphs[nfsid], neighbors_of_neighbors);
							    for (size_t nnfsid : neighbors_of_neighbors) {
								    if (pq.contains_id(nnfsid)) {
									    pq.decrease_key({nnfsid, pq.get_key(nnfsid) - 1});
								    }
							    }
						    }
					    }
					}

					lb.add(fs.begin(), fs.end());
				}
			};

			process_pq(bound_new);

			for (const auto& fs : bound_updated.get_bound()) {
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					used_updated.set_edge(*uit, *vit);
					return false;
				});
			}

			std::vector<bool> can_use(forbidden_subgraphs.size(), true);

			for (size_t i = 0; i < forbidden_subgraphs.size(); ++i) {
				const auto& fs = forbidden_subgraphs[i];

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					if (used_updated.has_edge(*uit, *vit)) {
						can_use[i] = false;
						return true;
					}
					return false;
				});
			}

			subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& fs_ids)
			{
				auto new_end = std::remove_if(fs_ids.begin(), fs_ids.end(), [&](size_t v) { return !can_use[v]; });
				fs_ids.erase(new_end, fs_ids.end());
			});

			for (size_t i = 0; i < forbidden_subgraphs.size(); ++i) {
				if (!can_use[i]) continue;

				const typename Lower_Bound_Storage_type::subgraph_t& fs = forbidden_subgraphs[i];

				populate_neighbor_ids(fs, neighbors);

				if (neighbors.size() == 1) {
					assert(neighbors.back() == fsid);
					bound_updated.add(fs.begin(), fs.end());
				} else {
					pq.push({i, neighbors.size()});
				}
			}

			process_pq(bound_updated);

			//std::cout << "k = " << k << " #fs = " << m.forbidden_subgraphs.size() << " max_neigh = " << max_neighbor_size << " avg_neighbors = " << total_neighbors_size / m.forbidden_subgraphs.size() << " min_neighbors = " << min_neighbor_size << " lb: " << m.lower_bound.size() << " updated lb: " << m.updated_lower_bound.size() << std::endl;

			if (bound_updated.size() > bound_new.size()) {
				bound_new = bound_updated;
			}
		}
	};
}

#endif
