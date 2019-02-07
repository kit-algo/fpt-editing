#ifndef CONSUMER_SELECTOR_SINGLE_INDEPENDENT_SET_HPP
#define CONSUMER_SELECTOR_SINGLE_INDEPENDENT_SET_HPP

#include <algorithm>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <iostream>

#include "../util.hpp"

#include "../config.hpp"

#include "../Options.hpp"

#include "../Finder/Finder.hpp"
#include "../Finder/Center.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "LB_Updated.hpp"
#include "../id_queue.h"
#include "../Graph/ValueMatrix.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Single_Independent_Set : Options::Tag::Selector, Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Single_Independent_Set";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		// feeder contains references to this which turn to dangling pointers with default {copy,move} {constructor,assignment}.
		// This struct holds the members for which the default functions to the right thing,
		// so the {copy,move} {constructor,assignment} doesn't have to mention every single member.
		struct M
		{
			std::vector<typename Lower_Bound_Storage_type::subgraph_t> forbidden_subgraphs;
			Value_Matrix<std::vector<size_t>> subgraphs_per_edge;
			Value_Matrix<size_t> num_subgraphs_per_edge;
			std::vector<VertexID> fallback;
			size_t fallback_free = 0;
			bool use_single;

			Lower_Bound_Storage_type lower_bound;
			Lower_Bound_Storage_type updated_lower_bound;
			Graph_Edits in_updated_lower_bound;
			bool lower_bound_calculated;

			Finder::Center<Graph, Graph_Edits, Mode, Restriction, Conversion, length> finder;

			M(VertexID graph_size) : subgraphs_per_edge(graph_size), num_subgraphs_per_edge(graph_size), in_updated_lower_bound(graph_size), finder(graph_size) {;}
		} m;

		Finder::Feeder<decltype(m.finder), Graph, Graph_Edits, Single_Independent_Set> feeder;

	public:
		// manual {copy,move} {constructor,assignment} implementations due to feeder
		Single_Independent_Set(Single_Independent_Set const &o) : m(o.m), feeder(this->m.finder, *this) {;}
		Single_Independent_Set(Single_Independent_Set &&o) : m(std::move(o.m)), feeder(this->m.finder, *this) {;}
		Single_Independent_Set &operator =(Single_Independent_Set const &o)
		{
			m = o.m;
			feeder = decltype(feeder)(this->m.finder, *this);
		}
		Single_Independent_Set &operator =(Single_Independent_Set &&o)
		{
			m = std::move(o.m);
			feeder = decltype(feeder)(this->m.finder, *this);
		}

		Single_Independent_Set(VertexID graph_size) : m(graph_size), feeder(m.finder, *this) {;}

		void prepare(size_t no_edits_left, const Lower_Bound_Storage_type& lower_bound)
		{
			m.use_single = (no_edits_left > 0);
			m.updated_lower_bound = lower_bound;
			m.in_updated_lower_bound.clear();

			m.forbidden_subgraphs.clear();
			m.subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& v) { v.clear(); });
			m.num_subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, size_t& v) { v = 0; });
			m.fallback.clear();
			m.fallback_free = 0;
			m.lower_bound.clear();
			m.lower_bound_calculated = false;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			const size_t forbidden_index = m.forbidden_subgraphs.size();
			{ // insert into array
				m.forbidden_subgraphs.emplace_back();
				auto it = b;
				for (size_t i = 0; i < length; ++i)
					{
						if (it == e) abort();

						m.forbidden_subgraphs.back()[i] = *it;
						++it;
					}

				if (it != e) abort();
			}

			size_t free = 0;
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				m.subgraphs_per_edge.at(*uit, *vit).push_back(forbidden_index);
				++m.num_subgraphs_per_edge.at(*uit, *vit);
				free++;
				return false;
			});

			// fallback handling
			if(free < m.fallback_free || m.fallback_free == 0)
			{
				m.fallback_free = free;
				m.fallback = std::vector<VertexID>{b, e};
				if(free == 0) {return true;} // completly edited subgraph -- impossible to solve graph
			}

			return false;
		}

		void prepare_result(size_t k, Graph const &g, Graph_Edits const &e)
		{
			if (m.lower_bound_calculated) return;
			m.lower_bound_calculated = true;

			std::vector<size_t> tmp;
			auto populate_neighbor_ids = [&](const typename Lower_Bound_Storage_type::subgraph_t& fs, std::vector<size_t>& neighbors) {
				neighbors.clear();
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					const auto& new_neighbors = m.subgraphs_per_edge.at(*uit, *vit);

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

			RoutingKit::MinIDQueue pq(m.forbidden_subgraphs.size());

			std::vector<size_t> neighbors, neighbors_of_neighbors;

			size_t total_neighbors_size = 0, max_neighbor_size = 0, min_neighbor_size = std::numeric_limits<size_t>::max();

			for (size_t fsid = 0; fsid < m.forbidden_subgraphs.size(); ++fsid) {
				const typename Lower_Bound_Storage_type::subgraph_t& fs = m.forbidden_subgraphs[fsid];

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
					m.lower_bound.add(fs.begin(), fs.end());
				} else {
					pq.push({fsid, neighbors.size()});
				}
			}

			auto process_pq = [&](Lower_Bound_Storage_type &lb) {
			while (!pq.empty()) {
					auto idkey = pq.pop();

					const auto& fs = m.forbidden_subgraphs[idkey.id];

					if (idkey.key > 1) {
					    populate_neighbor_ids(fs, neighbors);
					    for (size_t nfsid : neighbors) {
						    if (pq.contains_id(nfsid)) {
							    { // simulate deletion - there should be no element with key 0
								    assert(pq.peek().key > 0);
								    pq.decrease_key({nfsid, 0});
								    auto el = pq.pop();
								    assert(el.id == nfsid);
							    }

							    populate_neighbor_ids(m.forbidden_subgraphs[nfsid], neighbors_of_neighbors);
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

			process_pq(m.lower_bound);

			for (const auto& fs : m.updated_lower_bound.get_bound()) {
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					m.in_updated_lower_bound.set_edge(*uit, *vit);
					return false;
				});
			}

			std::vector<bool> can_use(m.forbidden_subgraphs.size(), true);

			for (size_t i = 0; i < m.forbidden_subgraphs.size(); ++i) {
				const auto& fs = m.forbidden_subgraphs[i];

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					if (m.in_updated_lower_bound.has_edge(*uit, *vit)) {
						can_use[i] = false;
						return true;
					}
					return false;
				});
			}

			m.subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& fs_ids)
			{
				auto new_end = std::remove_if(fs_ids.begin(), fs_ids.end(), [&](size_t v) { return !can_use[v]; });
				fs_ids.erase(new_end, fs_ids.end());
			});

			for (size_t i = 0; i < m.forbidden_subgraphs.size(); ++i) {
				if (!can_use[i]) continue;

				const typename Lower_Bound_Storage_type::subgraph_t& fs = m.forbidden_subgraphs[i];

				populate_neighbor_ids(fs, neighbors);

				if (neighbors.size() == 1) {
					assert(neighbors.back() == fsid);
					m.updated_lower_bound.add(fs.begin(), fs.end());
				} else {
					pq.push({i, neighbors.size()});
				}
			}

			process_pq(m.updated_lower_bound);

			//std::cout << "k = " << k << " #fs = " << m.forbidden_subgraphs.size() << " max_neigh = " << max_neighbor_size << " avg_neighbors = " << total_neighbors_size / m.forbidden_subgraphs.size() << " min_neighbors = " << min_neighbor_size << " lb: " << m.lower_bound.size() << " updated lb: " << m.updated_lower_bound.size() << std::endl;

			if (m.updated_lower_bound.size() > m.lower_bound.size()) {
				m.lower_bound = m.updated_lower_bound;
			}
		}

		std::vector<VertexID> result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Selector)
		{
			prepare_result(k, g, e);

			if(m.lower_bound.empty())
			{
				// a solved graph?!
				m.fallback.clear();
				return m.fallback;
			}
			if(m.lower_bound.size() > k) {return m.lower_bound.as_vector(0);} // fast return since lb will prune anyway

			if(m.fallback_free == 0 && !m.fallback.empty())
			{
				// we found a completly edited/marked subgraph
				return m.fallback;
			}

			if (m.use_single) {
				size_t max_subgraphs = 0;
				std::pair<VertexID, VertexID> node_pair;
				m.num_subgraphs_per_edge.forAllNodePairs([&](VertexID u, VertexID v, size_t& num_fbs) {
					if (num_fbs > max_subgraphs) {
						max_subgraphs = num_fbs;
						node_pair.first = u;
						node_pair.second = v;
					}
				});

				if (max_subgraphs > 1) { // only use single editing if there is a node pair that is part of multiple forbidden subgraphs
					return std::vector<VertexID>{node_pair.first, node_pair.second};
				}
			}
			//			std::unordered_map<std::pair<VertexID, VertexID>, size_t> node_pairs_single;
			//
			//			for (const auto& fs : m.forbidden_subgraphs)
			//			{
			//				size_t in_bound = 0;
			//				std::pair<VertexID, VertexID> node_pair;
			//				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
			//																   if (m.in_lower_bound.has_edge(*uit, *vit)) {
			//						++in_bound;
			//						node_pair = std::minmax(*uit, *vit);
			//					}
			//
			//					return false;
			//				});
			//
			//				if (in_bound == 1) {
			//					++node_pairs_single[node_pair];
			//				}
			//
			//			}
			//
			//			if(!node_pairs_single.empty())
			//			{
			//				// we found edges suitable for single edge editing, pick best
			//				std::pair<std::pair<VertexID, VertexID>, size_t> best_single_edge{{0, 0}, 0};
			//				for(auto const &single_edge: node_pairs_single)
			//				{
			//					if(single_edge.second > best_single_edge.second) {best_single_edge = single_edge;}
			//				}
			//				m.fallback = std::vector<VertexID>{best_single_edge.first.first, best_single_edge.first.second};
			//				return m.fallback;
			//			}
			//			else
			//{
				// fallback
				// no edge suitable for single edge editing, according to our heuristic
				// pick least editable forbidden subgraph
				return m.fallback;
				//}
		}

		size_t result(size_t k, Graph const & graph, Graph_Edits const & edited, Options::Tag::Lower_Bound)
		{
			prepare_result(k, graph, edited);

			return m.lower_bound.size();
		}

		const Lower_Bound_Storage_type& result(size_t k, Graph const& graph, Graph_Edits const & edited, Options::Tag::Lower_Bound_Update)
		{
			prepare_result(k, graph, edited);

			return m.lower_bound;
		}
	};
}

#endif
