#ifndef CONSUMER_SELECTOR_SINGLE_HPP
#define CONSUMER_SELECTOR_SINGLE_HPP

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

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Single : Options::Tag::Selector, Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Single";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		// feeder contains references to this which turn to dangling pointers with default {copy,move} {constructor,assignment}.
		// This struct holds the members for which the default functions to the right thing,
		// so the {copy,move} {constructor,assignment} doesn't have to mention every single member.
		struct M
		{
			Graph_Edits used;

			Lower_Bound_Storage_type bounds;
			bool initialized_bound;
			std::unordered_map<std::pair<VertexID, VertexID>, size_t> bounds_rev;
			size_t replacing_bound;

			Graph_Edits new_use;
			size_t new_count;

			struct
			{
				std::pair<size_t, size_t> changes{0, 0};
				std::vector<VertexID> edge;
			} best;

			Finder::Center<Graph, Graph_Edits, Mode, Restriction, Conversion, length> finder;

			M(VertexID graph_size) : used(graph_size), new_use(graph_size), finder(graph_size) {;}
		} m;

		Finder::Feeder<decltype(m.finder), Graph, Graph_Edits, Single> feeder;

	public:
		// manual {copy,move} {constructor,assignment} implementations due to feeder
		Single(Single const &o) : m(o.m), feeder(this->m.finder, *this) {;}
		Single(Single &&o) : m(std::move(o.m)), feeder(this->m.finder, *this) {;}
		Single &operator =(Single const &o)
		{
			m = o.m;
			feeder = decltype(feeder)(this->m.finder, *this);
		}
		Single &operator =(Single &&o)
		{
			m = std::move(o.m);
			feeder = decltype(feeder)(this->m.finder, *this);
		}

		Single(VertexID graph_size) : m(graph_size), feeder(m.finder, *this) {;}

		void prepare(const Lower_Bound_Storage_type& lower_bound)
		{
			m.used.clear();
			m.bounds = lower_bound;
			m.bounds_rev.clear();
			m.best = {{0, 0}, {0, 0}};
			m.initialized_bound = false;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			auto add_subgraph = [&](auto begin, auto end)
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, begin, end, [&](auto uit, auto vit){
					m.used.set_edge(*uit, *vit);
					m.bounds_rev.insert({std::minmax(*uit, *vit), m.bounds.size()});
					return false;
				});
			};

			// initialize lower bound on first run
			if (!m.initialized_bound)
			{
				for (const auto &it : m.bounds.get_bound())
				{
					add_subgraph(it.begin(), it.end());
				}
				m.initialized_bound = true;
			}

			bool skip = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit){
				return m.used.has_edge(*uit, *vit);
			});

			if(skip) {return false;}

			add_subgraph(b, e);
			m.bounds.add(b, e);

			return false;
		}

		std::vector<VertexID> const result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Selector)
		{
			auto &graph = const_cast<Graph &>(g);
			auto &edited = const_cast<Graph_Edits &>(e);

			if(m.bounds.empty())
			{
				m.best.edge.clear();
				return m.best.edge;
			}
			if(m.bounds.size() > k) {return std::vector<VertexID>(m.bounds[0].begin(), m.bounds[0].end());} // fast return since lb will prune anyway

			auto handle_edge = [&](size_t idx, VertexID u, VertexID v) -> bool
			{
				m.replacing_bound = idx;
				::Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, m.bounds[m.replacing_bound].begin(), m.bounds[m.replacing_bound].end(), [&](auto uit, auto vit) {
					if(!m.used.has_edge(*uit, *vit)) {abort();}
					m.used.clear_edge(*uit, *vit);
					return false;
				});

				// mark
				edited.set_edge(u, v);
				// lb
				feeder.feed_near(k, graph, edited, u, v, &m.used);
				size_t mark_change = m.new_count;
				// edit
				graph.toggle_edge(u, v);
				// lb
				feeder.feed_near(k, graph, edited, u, v, &m.used);
				size_t edit_change = m.new_count;
				// unedit, unmark
				edited.clear_edge(u, v);
				graph.toggle_edge(u, v);

				::Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, m.bounds[m.replacing_bound].begin(), m.bounds[m.replacing_bound].end(), [&](auto uit, auto vit) {
					m.used.set_edge(*uit, *vit);
					return false;
				});
				/* adjustments */
				mark_change--; edit_change--;// edited/marked subgraph no longer in bound
				edit_change++;// spending an edit
				mark_change++; edit_change++;
				if(mark_change == ~0U) {abort();}

				/* compare */
				//std::cout << "current best: " << +m.best.changes.first << ", " << +m.best.changes.second << ": " << +m.best.edge.front() << " - " << +m.best.edge.back() << '\n';
				auto const changes = std::minmax(mark_change, edit_change);
				size_t space = k - m.bounds.size();
				space++;
				// both branches cut: return
				if(space < changes.first)
				{
					m.best = {changes, {u, v}};
//					std::cout << "cut: " << +changes.first << ", " << +changes.second << ": " << +u << " - " << +v << '\n';
					return true;
				}
				// one branch cut: maximize other branch
				else if(space < changes.second)
				{
					if(m.best.changes.second <= space) {;}
					else if(space < m.best.changes.second && m.best.changes.first < changes.first) {;}
					else {return false;}
				}
				// no branch cut: maximize lower value
				else if(m.best.changes.second <= space)
				{
					if(m.best.changes.first < changes.first) {;}
					//     equal lower value: maximize higher value
					else if(m.best.changes.first == changes.first && m.best.changes.second < changes.second) {;}
					else {return false;}
				}
				else {return false;}
				m.best = {changes, {u, v}};
//				std::cout << "new best: " << +changes.first << ", " << +changes.second << ": " << +u << " - " << +v << '\n';
				return false;
			};

			size_t free_count = 0;
			size_t free_idx = 0;

			for(size_t idx = 0; idx < m.bounds.size(); idx++)
			{
				auto b = m.bounds[idx].begin();
				auto e = m.bounds[idx].end();
				size_t free = 0;

				bool done = Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
					if(handle_edge(idx, *uit, *vit)) {return true;}
					free++;
					return false;
				});
				if(done)
				{
//					std::cout << "using " << +m.best.edge.front() << " - " << +m.best.edge.back() << '\n';
					return m.best.edge;
				}

				if(free < free_count || free_count == 0)
				{
					if(free == 0)
					{
						// found completly edited/marked forbidden subgraph
						return std::vector<VertexID>(m.bounds[idx].begin(), m.bounds[idx].end());
					}
					free_idx = idx;
					free_count = free;
				}
			}

			if(m.best.changes.first > 0)
			{
//				std::cout << "using " << +m.best.edge.front() << " - " << +m.best.edge.back() << '\n';
				return m.best.edge;
			}
			else
			{
//				std::cout << "using bound " << +free_idx << " (" << +free_count << " free edges)\n";
				return std::vector<VertexID>(m.bounds[free_idx].begin(), m.bounds[free_idx].end());
			}
		}

		size_t result(size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound) const
		{
			return m.bounds.size();
		}

		const Lower_Bound_Storage_type& result(size_t, Graph const&, Graph_Edits const &, Options::Tag::Lower_Bound_Update) const
		{
			return m.bounds;
		}

		void prepare_near(VertexID, VertexID)
		{
			m.new_use.clear();
			m.new_count = 0;
		}

		bool next_near(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			bool skip = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit){
				/* count if:
				 * all edges are either:
				 *   unused
				 *   or inside the current bound
				 * and we didn't find a new bound using them.
				 * */
				bool count = (!m.used.has_edge(*uit, *vit) || m.bounds_rev.at(std::minmax(*uit, *vit)) == m.replacing_bound) && !m.new_use.has_edge(*uit, *vit);
				return !count;
			});
			if(skip) {return false;}

			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit){
				m.new_use.set_edge(*uit, *vit);
				return false;
			});
			m.new_count++;
			return false;
		}
	};
}

#endif
