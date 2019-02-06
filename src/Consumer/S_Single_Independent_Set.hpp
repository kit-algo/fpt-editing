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
			Updated<Graph, Graph_Edits, Mode, Restriction, Conversion, length> updated_lb;
			std::vector<std::pair<typename Lower_Bound_Storage_type::subgraph_t, size_t>> forbidden_subgraphs_degrees;
			Value_Matrix<size_t> subgraphs_per_edge;
			std::vector<VertexID> fallback;
			size_t fallback_free = 0;
			bool use_single;

			Lower_Bound_Storage_type lower_bound;
			Graph_Edits in_lower_bound;
			bool lower_bound_calculated;

			Finder::Center<Graph, Graph_Edits, Mode, Restriction, Conversion, length> finder;

			M(VertexID graph_size) : updated_lb(graph_size), subgraphs_per_edge(graph_size), in_lower_bound(graph_size), finder(graph_size) {;}
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
			m.updated_lb.prepare(no_edits_left, lower_bound);
			m.forbidden_subgraphs_degrees.clear();
			m.subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, size_t& v) { v = 0; });
			m.fallback.clear();
			m.fallback_free = 0;
			m.in_lower_bound.clear();
			m.lower_bound.clear();
			m.lower_bound_calculated = false;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			m.updated_lb.next(graph, edited, b, e);

			{ // insert into array
				m.forbidden_subgraphs_degrees.emplace_back();
				auto it = b;
				for (size_t i = 0; i < length; ++i)
					{
						if (it == e) abort();

						m.forbidden_subgraphs_degrees.back().first[i] = *it;
						++it;
					}

				if (it != e) abort();
			}

			size_t free = 0;
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				++m.subgraphs_per_edge.at(*uit, *vit);
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

			const Lower_Bound_Storage_type& lower_bound_simple = m.updated_lb.result(k, g, e, Options::Tag::Lower_Bound_Update());

			// FIXME: catch nasty initial lower bound better than checking k > 0
			if (k > 0 && lower_bound_simple.size() > k) {
				m.lower_bound = lower_bound_simple;
				return;
			}


			for (auto& fs_count : m.forbidden_subgraphs_degrees) {
				fs_count.second = 0;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs_count.first.begin(), fs_count.first.end(), [&](auto uit, auto vit) {
					fs_count.second += m.subgraphs_per_edge.at(*uit, *vit);
					return false;
				});
			}

			std::sort(m.forbidden_subgraphs_degrees.begin(), m.forbidden_subgraphs_degrees.end(), [](const auto& a, const auto& b) { return a.second < b.second; });

			for (const auto& fs_degree : m.forbidden_subgraphs_degrees) {
				bool can_use = true;
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs_degree.first.begin(), fs_degree.first.end(), [&](auto uit, auto vit) {
					if (m.in_lower_bound.has_edge(*uit, *vit)) {
						can_use = false;
						return true;
					}
					return false;
				});

				if (can_use) {
					m.lower_bound.add(fs_degree.first.begin(), fs_degree.first.end());
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs_degree.first.begin(), fs_degree.first.end(), [&](auto uit, auto vit) {
						m.in_lower_bound.set_edge(*uit, *vit);
						return false;
					});
				}
			}

			if (m.lower_bound.size() < lower_bound_simple.size()) {
				m.lower_bound = lower_bound_simple;
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
				m.subgraphs_per_edge.forAllNodePairs([&](VertexID u, VertexID v, size_t& fbs) {
					if (fbs > max_subgraphs) {
						max_subgraphs = fbs;
						node_pair.first = u;
						node_pair.second = v;
					}
				});

				return std::vector<VertexID>{node_pair.first, node_pair.second};
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
