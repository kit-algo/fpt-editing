#ifndef FINDER_S_LB_CENTER_MATRIX_HPP
#define FINDER_S_LB_CENTER_MATRIX_HPP

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
#include "../Finder/Center_Matrix.hpp"

namespace Selector
{
#define PAIR(a, b) (assert((a) != (b)), std::make_pair(std::min((a), (b)), std::max((a), (b))))

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Single : Options::Tag::Selector, Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Single";

	private:
		//size_t found = 0;
		Graph_Edits used;

		std::vector<std::vector<VertexID>> bounds;
		std::unordered_map<std::pair<VertexID, VertexID>, size_t> bounds_rev;
		size_t replacing_bound;

		Graph_Edits new_use;
		size_t new_count;

		struct
		{
			std::pair<size_t, size_t> changes{0, 0};
			std::vector<VertexID> edge;
		} best;

		Finder::Center_Matrix_4<Graph, Graph_Edits> finder;
		Finder::Feeder<decltype(finder), Graph, Graph_Edits, Single<Graph, Graph_Edits, Mode, Restriction, Conversion>> feeder;

	public:
		Single(Graph const &graph) : used(graph.size()), new_use(graph.size()), finder(graph), feeder(finder, *this)
		{
			;
		}

		void prepare()
		{
			used.clear();
			bounds.clear();
			best = {{0, 0}, {0, 0}};
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			bool skip = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit){
				return used.has_edge(*uit, *vit);
			});

			if(skip) {return true;}
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit){
				used.set_edge(*uit, *vit);
				bounds_rev.insert({std::minmax(*uit, *vit), bounds.size()});
				return false;
			});

			bounds.emplace_back(b, e);
			return true;
		}

		std::vector<VertexID> const &result(size_t k, Graph const &g, Graph_Edits const &e)
		{
			auto &graph = const_cast<Graph &>(g);
			auto &edited = const_cast<Graph_Edits &>(e);

			if(bounds.empty())
			{
				best.edge.clear();
				return best.edge;
			}
			if(bounds.size() > k) {return bounds[0];} // fast return since lb will prune anyway

			auto handle_edge = [&](size_t idx, VertexID u, VertexID v) -> bool
			{
				/* search radius for Px/Cx from u, v
				 * 4 -> +2
				 * 5 -> +3
				 * ...
				 *
				 * how do we know what we're searching for?
				 * let lb handle that
				 */
				replacing_bound = idx;

				// mark
				edited.set_edge(u, v);
				// lb
				feeder.feed_near(graph, edited, u, v);
				size_t mark_change = new_count - 1;
				// edit
				graph.toggle_edge(u, v);
				// lb
				feeder.feed_near(graph, edited, u, v);
				size_t edit_change = new_count - 1;
				// unedit, unmark
				edited.clear_edge(u, v);
				graph.toggle_edge(u, v);

				/* adjust */
				edit_change++;

				/* compare */
				auto const changes = std::minmax(mark_change, edit_change);
				// both branches cut: return
				if(changes.first > k)
				{
					best = {changes, {u, v}};
					return true;
				}
				// one branch cut: maximize other branch
				else if(changes.second > k && (best.changes.second <= k || best.changes.first < changes.first)) {;}
				// no branch cut: maximize lower value
				else if(changes.first > best.changes.first) {;}
				//     equal lower value: maximize higher value
				else if(changes.first == best.changes.first && changes.second > best.changes.second) {;}
				else {return false;}
				best = {changes, {u, v}};
				return false;
			};

			size_t free_count = 0;
			size_t free_idx = 0;

			for(size_t idx = 0; idx < bounds.size(); idx++)
			{
				auto b = bounds[idx].begin();
				auto e = bounds[idx].end();
				size_t free = 0;

				bool done = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
					if(handle_edge(idx, *uit, *vit)) {return true;}
					free++;
					return false;
				});
				if(done) {return best.edge;}

				if(free < free_count || free_count == 0)
				{
					if(free == 0)
					{
						return bounds[idx];
					}
					free_idx = idx;
					free_count = free;
				}
			}

			if(best.changes.first > 0)
			{
				return best.edge;
			}
			else
			{
				return bounds[free_idx];
			}
		}

		size_t result() const
		{
			return bounds.size();
		}

		void prepare_near(VertexID, VertexID)
		{
			new_use.clear();
			new_count = 0;
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
				bool count = (!used.has_edge(*uit, *vit) || bounds_rev.at(std::minmax(*uit, *vit)) == replacing_bound) && !new_use.has_edge(*uit, *vit);
				return !count;
			});

			if(skip) {return true;}
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit){
				new_use.set_edge(*uit, *vit);
				return false;
			});
			new_count++;
			return true;
		}

#if 0
	private:
		struct Graph_Update
		{
			std::vector<std::vector<typename Graph::VertexID>> insertions;
			std::vector<std::vector<typename Graph::VertexID>> deletions;
		};

		bool bound_valid = false;
		bool bound_update = false;
		size_t bound = 0;
		std::unordered_map<std::pair<typename Graph::VertexID, typename Graph::VertexID>, std::vector<size_t>> bounds_rev;

		std::unordered_map<std::pair<typename Graph::VertexID, typename Graph::VertexID>, Graph_Update> bounds_restore;

	public:

		template<typename Graph2>
		std::tuple<bool, std::vector<typename Graph::VertexID>, size_t> find(Graph const &graph, Graph2 const &edits, size_t k = 0)
		{
#ifndef NDEBUG
			size_t oldbound = bound;
#endif
			if(!bound_valid) {calculate(graph, edits, false);}// oldbound = 0;}
			//else if(bound_update) {bound_update = false; /*calculate(graph, edits, true); assert(oldbound == bound);*/}
#ifndef NDEBUG
			if(bound > oldbound)
			{
				std::cout << "new problems" << std::endl;
				for(size_t i = oldbound; i < bound; i++)
				{
					bool first = true;
					for(auto const &v: bounds[i]) {std::cout << (first? "{" : ", ") << v; first = false;}
					std::cout << "}" << std::endl;
				}
			}
#endif
			for(auto p: bounds)
			{
				assert(::Finder::validate(graph, p));
			}
			if(!bound) {return std::make_tuple(false, std::vector<typename Graph::VertexID>(), 0);}

			//std::cout << "--" << std::endl;

			//try all edges in bounds_rev unless edited
			//- does fixing it increase lb?
			//- does editing it increase lb?
			bool cut = false;

			std::pair<std::pair<typename Graph::VertexID, typename Graph::VertexID>, std::pair<size_t, size_t>> best = {{0, 0}, {0, 0}};
			//std::pair<std::pair<typename Graph::VertexID, typename Graph::VertexID>, std::pair<size_t, size_t>> best_old[2] = {{{0, 0}, {0, 0}}, {{0, 0}, {0, 0}}};

			for(auto e: bounds_rev)
			{
				auto u = e.first.first;
				auto v = e.first.second;
				if(edits.has_edge(u, v)) {continue;}
				if(e.second.size() != 1)
				{
					std::cout << "edge " << +u << " - " << +v << " used in problems " << std::endl;
					bool first = true;
					for(auto const &p: e.second) {std::cout << (first? "" : ", ") << p; first = false;}
					std::cout << std::endl;
				}
				assert(e.second.size() == 1);
				size_t found_fixed, found_edited;
				std::tie(found_fixed, found_edited) = lb_change(graph, edits, u, v);

				//*
				if(cut && std::max(found_fixed, found_edited) > k - bound)
				{
					if(std::min(found_fixed, found_edited) > std::min(best.second.first, best.second.second))
					{
						best = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));
					}
				}
				else if(!cut)
				{
					if(std::max(found_fixed, found_edited) > k - bound)
					{
						cut = true;
						best = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));
					}
					else if(std::min(found_fixed, found_edited) > std::min(best.second.first, best.second.second))
					{
						best = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));
					}
					else if(std::min(found_fixed, found_edited) == std::min(best.second.first, best.second.second))
					{
						if(std::max(found_fixed, found_edited) > std::max(best.second.first, best.second.second))
						{
							best = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));
						}
					}
				}
				/*/
				if(found_fixed + found_edited >= best.second.first + best.second.second)
				{
					best = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));
				}
				//
				if(std::max(found_fixed, found_edited) > std::max(best.second.first, best.second.second))
				{
					best = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));
				}
				else if(std::max(found_fixed, found_edited) == std::max(best.second.first, best.second.second))
				{
					if(std::min(found_fixed, found_edited) > std::min(best.second.first, best.second.second))
					{
						best = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));
					}
				}
				//
				if(found_fixed > best_old[0].second.first) {best_old[0] = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));}
				if(found_edited > best_old[1].second.second) {best_old[1] = std::make_pair(e.first, std::make_pair(found_fixed, found_edited));}
				/*/
			}

			//best = best_old[0];
			//if(best_old[0].second.first + best_old[0].second.second < best_old[1].second.first + best_old[1].second.second) {best = best_old[1];}


#ifndef NDEBUG
			std::cout << "best:" << std::endl;
			std::cout << +best.first.first << " - " << +best.first.second << ": " << best.second.first << ", " << best.second.second << std::endl;
			std::cout << "bound: " << bound << std::endl;
			for(auto const &p: bounds)
			{
				bool first = true;
				for(auto const &v: p) {std::cout << (first? "{" : ", ") << v; first = false;}
				std::cout << "}" << std::endl;
			}
#endif
			auto ee = best.first;
			if(!cut && (best.second.first < 1 || best.second.second < 1))
			{
				/*
				// no suitable edge, pick one
				//std::cout << "loop...";
				auto it = bounds_rev.begin();
				for(; it != bounds_rev.end() && edits.has_edge(it->first.first, it->first.second); it++) {;}
				//if(it == bounds_rev.end()) {std::cout << "argh...";}
				ee = it->first;
				//std::cout << std::endl;
				*/

				// no suitable edge, pick most edited problem
				size_t most_edited_problem = 0;
				size_t most_edits = 0;
				for(size_t i = 0; i < bounds.size(); i++)
				{
					auto const &path = bounds[i];
					size_t e = 0;
					for(auto it1 = path.begin(); it1 != path.end() - 1; it1++)
					{
						for(auto it2 = it1 + 1; it2 != path.end(); it2++)
						{
							if(it1 == path.begin() && it2 == path.end() - 1) {continue;}
							else if(edits.has_edge(*it1, *it2)) {e++;}
						}
					}
					if(e > most_edits)
					{
						most_edits = e;
						most_edited_problem = i;
					}
				}
				return std::make_tuple(true, bounds[most_edited_problem], bound);
			}
			return std::make_tuple(true, std::vector<typename Graph::VertexID>{ee.first, ee.second}, bound);
		}

		template<typename Graph2>
		size_t get_bound(Graph const &graph, Graph2 const &edits)
		{
			if(!bound_valid) {calculate(graph, edits, false);}
			return bound;
		}

	private:

		template<typename Graph2>
		void update(Graph &graph, typename Graph::VertexID u, typename Graph::VertexID v, Graph2 &edits, bool revert_only)
		{
#ifndef NDEBUG
			std::cout << "updating " << +u << " - " << +v << std::endl;
#endif
			auto e = PAIR(u, v);

			bool restore = bounds_restore.count(e);
			if(restore)
			{
				Graph_Update const &updates = bounds_restore[e];
#ifndef NDEBUG
				std::cout << "restoring/removing " << updates.insertions.size() << " problems" << std::endl;
#endif
				for(auto const &c: updates.insertions)
				{
#ifndef NDEBUG
					bool first = true;
					for(auto const p: c) {std::cout << (first? "{" : ", ") << p; first = false;}
					std::cout << "}" << std::endl;
#endif
					auto it = std::find(bounds.begin(), bounds.end(), c);
					size_t b = std::distance(bounds.begin(), it);

					bound--;
					if(b != bound)
					{
						//std::cout << "swapping " << b << " and " << bound << std::endl;
						auto const &problem = bounds.back();
						for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
						{
							for(auto uit = problem.begin(); uit != vit; uit++)
							{
								if(uit == problem.begin() && vit == problem.end() - 1) {continue;}
								auto beit = bounds_rev.find(PAIR(*uit, *vit));
								if(beit != bounds_rev.end()) {std::replace(beit->second.begin(), beit->second.end(), bound, b);}
							}
						}
						std::swap(bounds[b], bounds.back());
					}

					{
						auto const &problem = bounds.back();
						/*bool first2 = true;
						for(auto const p: problem) {std::cout << (first2? "erasing {" : ", ") << p; first2 = false;}
						std::cout << "}" << std::endl;*/

						for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
						{
							for(auto uit = problem.begin(); uit != vit; uit++)
							{
								if(uit == problem.begin() && vit == problem.end() - 1) {continue;}
								auto brit = bounds_rev.find(PAIR(*uit, *vit));
								if(brit != bounds_rev.end())
								{
									/*std::cout << +*uit << " - " << +*vit;
									bool first3 = true;
									for(auto const b: brit->second) {std::cout << (first3? " used in problem " : ", ") << b; first3 = false;}
									std::cout << std::endl;*/
									brit->second.erase(std::find(brit->second.begin(), brit->second.end(), b));
									if(brit->second.empty()) {bounds_rev.erase(brit);}
								}
							}
						}
						bounds.pop_back();
					}
					assert(bound == bounds.size());
				}
#ifndef NDEBUG
				std::cout << "restoring/adding " << updates.deletions.size() << " problems" << std::endl;
#endif
				for(auto const &c: updates.deletions)
				{
#ifndef NDEBUG
					bool first = true;
					for(auto const p: c) {std::cout << (first? "{" : ", ") << p; first = false;}
					std::cout << "}" << std::endl;
#endif
					bounds.push_back(c);
					for(auto vit = c.begin() + 1; vit != c.end(); vit++)
					{
						for(auto uit = c.begin(); uit != vit; uit++)
						{
							if(uit == c.begin() && vit == c.end() - 1) {continue;}
							bounds_rev[PAIR(*uit, *vit)].push_back(bound);
						}
					}
					bound++;
					assert(bound == bounds.size());
				}
				bounds_restore.erase(e);
			}

			if(revert_only) {return;}

			Graph_Update updates;

			auto bit = bounds_rev.find(e);
			if(bit != bounds_rev.end())
			{
				/*bool first = true;
				for(auto const b: bit->second) {std::cout << (first? "used in problem " : ", ") << b; first = false;}
				std::cout << std::endl;*/

				std::vector<size_t> itcpy(bit->second.rbegin(), bit->second.rend());
				for(auto const &b: itcpy)
				{
					bound--;
					if(b != bound)
					{
						//std::cout << "swapping " << b << " and " << bound << std::endl;
						auto const &problem = bounds.back();
						for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
						{
							for(auto uit = problem.begin(); uit != vit; uit++)
							{
								if(uit == problem.begin() && vit == problem.end() - 1) {continue;}
								auto beit = bounds_rev.find(PAIR(*uit, *vit));
								if(beit != bounds_rev.end()) {std::replace(beit->second.begin(), beit->second.end(), bound, b);}
							}
						}
						std::swap(bounds[b], bounds.back());
					}

					{
						auto const &problem = bounds.back();

						updates.deletions.push_back(problem);

						/*bool first2 = true;
						for(auto const p: problem) {std::cout << (first2? "erasing {" : ", ") << p; first2 = false;}
						std::cout << "}" << std::endl;*/

						for(auto vit = problem.begin() + 1; vit != problem.end(); vit++)
						{
							for(auto uit = problem.begin(); uit != vit; uit++)
							{
								if(uit == problem.begin() && vit == problem.end() - 1) {continue;}
								auto brit = bounds_rev.find(PAIR(*uit, *vit));
								if(brit != bounds_rev.end())
								{
									/*std::cout << +*uit << " - " << +*vit;
									bool first3 = true;
									for(auto const b: brit->second) {std::cout << (first3? " used in problem " : ", ") << b; first3 = false;}
									std::cout << std::endl;*/
									brit->second.erase(std::find(brit->second.begin(), brit->second.end(), b));
									if(brit->second.empty()) {bounds_rev.erase(brit);}
								}
							}
						}
						bounds.pop_back();
					}
					assert(bound == bounds.size());
				}
			}

			size_t old_bound = bound;

			std::set<typename Graph::VertexID> near;
			std::vector<typename Graph::VertexID> near_todo{u, v};
			for(size_t l = length - 1; l > 0; l--)
			{
				std::vector<typename Graph::VertexID> near_next;
				for(auto const x: near_todo)
				{
					bool added;
					std::tie(std::ignore, added) = near.insert(x);
					if(added)
					{
						for(size_t i = 0; i < graph.get_row_length(); i++)
						{
							for(typename Graph::Packed cur = graph.get_row(x)[i]; cur; cur &= ~(typename Graph::Packed(1) << __builtin_ctzll(cur)))
							{
								typename Graph::VertexID y = __builtin_ctzll(cur) + i * Graph::bits;
								near_next.push_back(y);
							}
						}
					}
				}
				std::swap(near_todo, near_next);
			}

			calculate(graph, edits, true, near);
			assert(bound == bounds.size());
			if(bound > old_bound)
			{
#ifndef NDEBUG
				std::cout << "new problems" << std::endl;
				for(size_t i = old_bound; i < bound; i++)
				{
					bool first = true;
					for(auto const &v: bounds[i]) {std::cout << (first? "{" : ", ") << v; first = false;}
					std::cout << "}" << std::endl;
				}
#endif
				for(auto it = bounds.begin() + old_bound; it != bounds.end(); it++) {updates.insertions.push_back(*it);}
			}

			if(1 /* sometimes? */)
			{
				decltype(bound) prev_bound(bound);
				decltype(bounds) prev_bounds;
				std::swap(bounds, prev_bounds);
				decltype(bounds_rev) prev_bounds_rev;
				std::swap(bounds_rev, prev_bounds_rev);
				decltype(bounds_restore) prev_bounds_restore;
				std::swap(bounds_restore, prev_bounds_restore);

				calculate(graph, edits, false);
				if(bound > prev_bound)
				{
					//recalculation improved lb
					for(auto &p: prev_bounds)
					{
						auto it = std::find(updates.insertions.begin(), updates.insertions.end(), p);
						if(it != updates.insertions.end()) {updates.insertions.erase(it);}
						else {updates.deletions.push_back(p);}
					}
					for(auto &p: bounds)
					{
						auto it = std::find(updates.deletions.begin(), updates.deletions.end(), p);
						if(it != updates.deletions.end()) {updates.deletions.erase(it);}
						else {updates.insertions.push_back(p);}
					}
				}
				else
				{
					//no improvement, revert
					bound = prev_bound;
					std::swap(bounds, prev_bounds);
					std::swap(bounds_rev, prev_bounds_rev);
				}
				std::swap(bounds_restore, prev_bounds_restore);
			}

			bounds_restore[e] = updates;
			bound_update = true;
		}

		template<typename Graph2>
		void calculate(Graph const &graph, Graph2 const &edits, bool update, std::set<typename Graph::VertexID> const &near = std::set<typename Graph::VertexID>{})
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);

			if(!update)
			{
				bound = 0;
				bounds.clear();
				bounds_rev.clear();
				bounds_restore.clear();
			}

			/* <finder code here> */

			bound_valid = true;
			bound_update = false;
		}

	public:
		template<typename Graph2>
		std::tuple<size_t, size_t> lb_change(Graph graph, Graph2 edits, typename Graph::VertexID u, typename Graph::VertexID v)
		{
			if(!bounds_rev.count(PAIR(u, v)) || edits.has_edge(u, v))
			{
				return std::make_tuple(0, 0);
			}

			auto const &lbi = bounds_rev[PAIR(u, v)];
			assert(lbi.size() == 1);

			std::set<std::pair<typename Graph::VertexID, typename Graph::VertexID>> used;
			size_t found_fixed = 0;
			/* P4/C4 specific, exploring any possible P/C containing u-v */
			if(graph.has_edge(u, v))
			{
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(typename Graph::Packed nu = graph.get_row(u)[i]; nu; nu &= ~(typename Graph::Packed(1) << __builtin_ctzll(nu)))
					{
						typename Graph::VertexID vnu = __builtin_ctzll(nu) + i * Graph::bits;
						if(vnu == v) {continue;}
						if(graph.has_edge(vnu, v)) {continue;}
						if(bounds_rev.count(PAIR(vnu, u))) {continue;}
						if(used.count(PAIR(vnu, u))) {continue;}
						if(bounds_rev.count(PAIR(vnu, v))) {continue;}
						if(used.count(PAIR(vnu, v))) {continue;}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nvnu = graph.get_row(vnu)[j]; nvnu; nvnu &= ~(typename Graph::Packed(1) << __builtin_ctzll(nvnu)))
							{
								typename Graph::VertexID vvnu = __builtin_ctzll(nvnu) + j * Graph::bits;
								if(vvnu == u || vvnu == v) {continue;}
								if(graph.has_edge(vvnu, u)) {continue;}
								if(bounds_rev.count(PAIR(vvnu, vnu))) {continue;}
								if(used.count(PAIR(vvnu, vnu))) {continue;}
								if(bounds_rev.count(PAIR(vvnu, u))) {continue;}
								if(used.count(PAIR(vvnu, u))) {continue;}
								//if(bounds_rev.count(PAIR(vvnu, v))) {continue;}
								if(!edits.has_edge(vnu, u)) {used.insert(PAIR(vnu, u));}
								if(!edits.has_edge(vnu, v)) {used.insert(PAIR(vnu, v));}
								if(!edits.has_edge(vvnu, vnu)) {used.insert(PAIR(vvnu, vnu));}
								if(!edits.has_edge(vvnu, u)) {used.insert(PAIR(vvnu, u));}
								//std::cout << +vvnu << " " << +vnu << " " << +u << " " << +v << std::endl;
								found_fixed++;
								if(!edits.has_edge(vnu, u) || !edits.has_edge(vnu, v)) {goto found_u;}
							}
						}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nv = graph.get_row(v)[j]; nv; nv &= ~(typename Graph::Packed(1) << __builtin_ctzll(nv)))
							{
								typename Graph::VertexID vnv = __builtin_ctzll(nv) + j * Graph::bits;
								if(u == vnv || vnu == vnv) {continue;}
								if(graph.has_edge(u, vnv)) {continue;}
								if(bounds_rev.count(PAIR(u, vnv))) {continue;}
								if(used.count(PAIR(u, vnv))) {continue;}
								if(bounds_rev.count(PAIR(v, vnv))) {continue;}
								if(used.count(PAIR(v, vnv))) {continue;}
								//if(bounds_rev.count(PAIR(vnu, vnv))) {continue;}
								if(!edits.has_edge(vnu, u)) {used.insert(PAIR(vnu, u));}
								if(!edits.has_edge(vnu, v)) {used.insert(PAIR(vnu, v));}
								if(!edits.has_edge(u, vnv)) {used.insert(PAIR(u, vnv));}
								if(!edits.has_edge(v, vnv)) {used.insert(PAIR(v, vnv));}
								//std::cout << +vnu << " " << +u << " " << +v << " " << +vnv << std::endl;
								found_fixed++;
								if(!edits.has_edge(vnu, u) || !edits.has_edge(vnu, v)) {goto found_u;}
							}
						}
						found_u:;
					}
				}
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(typename Graph::Packed nv = graph.get_row(v)[i]; nv; nv &= ~(typename Graph::Packed(1) << __builtin_ctzll(nv)))
					{
						typename Graph::VertexID vnv = __builtin_ctzll(nv) + i * Graph::bits;
						if(vnv == u) {continue;}
						if(graph.has_edge(u, vnv)) {continue;}
						if(bounds_rev.count(PAIR(vnv, u))) {continue;}
						if(used.count(PAIR(vnv, u))) {continue;}
						if(bounds_rev.count(PAIR(vnv, v))) {continue;}
						if(used.count(PAIR(vnv, v))) {continue;}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nvnv = graph.get_row(vnv)[j]; nvnv; nvnv &= ~(typename Graph::Packed(1) << __builtin_ctzll(nvnv)))
							{
								typename Graph::VertexID vvnv = __builtin_ctzll(nvnv) + j * Graph::bits;
								if(vvnv == u || vvnv == v) {continue;}
								if(graph.has_edge(v, vvnv)) {continue;}
								if(bounds_rev.count(PAIR(vvnv, vnv))) {continue;}
								if(used.count(PAIR(vvnv, vnv))) {continue;}
								if(bounds_rev.count(PAIR(vvnv, v))) {continue;}
								if(used.count(PAIR(vvnv, v))) {continue;}
								//if(bounds_rev.count(PAIR(vvnv, v))) {continue;}
								if(!edits.has_edge(vnv, u)) {used.insert(PAIR(vnv, u));}
								if(!edits.has_edge(vnv, v)) {used.insert(PAIR(vnv, v));}
								if(!edits.has_edge(vvnv, vnv)) {used.insert(PAIR(vvnv, vnv));}
								if(!edits.has_edge(vvnv, v)) {used.insert(PAIR(vvnv, v));}
								//std::cout << +u << " " << +v << " " << +vnv << " " << +vvnv << std::endl;
								found_fixed++;
								if(!edits.has_edge(vnv, u) || !edits.has_edge(vnv, v)) {goto found_v;}
							}
						}
						found_v:;
					}
				}
			}
			else
			{
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(typename Graph::Packed nc = graph.get_row(u)[i] & graph.get_row(v)[i]; nc; nc &= ~(typename Graph::Packed(1) << __builtin_ctzll(nc)))
					{
						typename Graph::VertexID vnc = __builtin_ctzll(nc) + i * Graph::bits;
						if(bounds_rev.count(PAIR(u, vnc))) {continue;}
						if(used.count(PAIR(u, vnc))) {continue;}
						if(bounds_rev.count(PAIR(vnc, v))) {continue;}
						if(used.count(PAIR(vnc, v))) {continue;}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nu = graph.get_row(u)[j]; nu; nu &= ~(typename Graph::Packed(1) << __builtin_ctzll(nu)))
							{
								typename Graph::VertexID vnu = __builtin_ctzll(nu) + j * Graph::bits;
								if(u == vnu || vnc == vnu) {continue;}
								if(graph.has_edge(vnc, vnu)) {continue;}
								if(bounds_rev.count(PAIR(vnc, vnu))) {continue;}
								if(used.count(PAIR(vnc, vnu))) {continue;}
								if(bounds_rev.count(PAIR(u, vnu))) {continue;}
								if(used.count(PAIR(u, vnu))) {continue;}
								//if(bounds_rev.count(PAIR(vnu, v))) {continue;}
								if(!edits.has_edge(u, vnc)) {used.insert(PAIR(u, vnc));}
								if(!edits.has_edge(vnc, v)) {used.insert(PAIR(vnc, v));}
								if(!edits.has_edge(vnu, vnc)) {used.insert(PAIR(vnu, vnc));}
								if(!edits.has_edge(vnu, u)) {used.insert(PAIR(vnu, u));}
								//std::cout << +vnu << " " << +u << " " << +vnc << " " << +v << std::endl;
								found_fixed++;
								if(!edits.has_edge(vnc, u) || !edits.has_edge(vnc, v)) {goto found_c;}
							}
						}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nv = graph.get_row(v)[j]; nv; nv &= ~(typename Graph::Packed(1) << __builtin_ctzll(nv)))
							{
								typename Graph::VertexID vnv = __builtin_ctzll(nv) + j * Graph::bits;
								if(u == vnv || vnc == vnv) {continue;}
								if(graph.has_edge(vnc, vnv)) {continue;}
								if(bounds_rev.count(PAIR(vnc, vnv))) {continue;}
								if(used.count(PAIR(vnc, vnv))) {continue;}
								if(bounds_rev.count(PAIR(v, vnv))) {continue;}
								if(used.count(PAIR(v, vnv))) {continue;}
								//if(bounds_rev.count(PAIR(u, vnv))) {continue;}
								if(!edits.has_edge(u, vnc)) {used.insert(PAIR(u, vnc));}
								if(!edits.has_edge(vnc, v)) {used.insert(PAIR(vnc, v));}
								if(!edits.has_edge(vnc, vnv)) {used.insert(PAIR(vnc, vnv));}
								if(!edits.has_edge(v, vnv)) {used.insert(PAIR(v, vnv));}
								//std::cout << " " << +u << " " << +vnc << " " << +v << " " << +vnv << std::endl;
								found_fixed++;
								if(!edits.has_edge(vnc, u) || !edits.has_edge(vnc, v)) {goto found_c;}
							}
						}
						found_c:;
					}
				}
			}
			used.clear();
			//std::cout << "-" << std::endl;
			size_t found_edited = 0;
			//may ignore e.second
			if(!graph.has_edge(u, v))
			{
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(typename Graph::Packed nu = graph.get_row(u)[i]; nu; nu &= ~(typename Graph::Packed(1) << __builtin_ctzll(nu)))
					{
						typename Graph::VertexID vnu = __builtin_ctzll(nu) + i * Graph::bits;
						if(vnu == v) {continue;}
						if(graph.has_edge(vnu, v)) {continue;}
						if(bounds_rev.count(PAIR(vnu, u)) && bounds_rev[PAIR(vnu, u)] != lbi) {continue;}
						if(used.count(PAIR(vnu, u))) {continue;}
						if(bounds_rev.count(PAIR(vnu, v)) && bounds_rev[PAIR(vnu, v)] != lbi) {continue;}
						if(used.count(PAIR(vnu, v))) {continue;}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nvnu = graph.get_row(vnu)[j]; nvnu; nvnu &= ~(typename Graph::Packed(1) << __builtin_ctzll(nvnu)))
							{
								typename Graph::VertexID vvnu = __builtin_ctzll(nvnu) + j * Graph::bits;
								if(vvnu == u || vvnu == v) {continue;}
								if(graph.has_edge(vvnu, u)) {continue;}
								if(bounds_rev.count(PAIR(vvnu, vnu)) && bounds_rev[PAIR(vvnu, vnu)] != lbi) {continue;}
								if(used.count(PAIR(vvnu, vnu))) {continue;}
								if(bounds_rev.count(PAIR(vvnu, u)) && bounds_rev[PAIR(vvnu, u)] != lbi) {continue;}
								if(used.count(PAIR(vvnu, u))) {continue;}
								//if(bounds_rev.count(PAIR(vvnu, v))) {continue;}
								if(!edits.has_edge(vnu, u)) {used.insert(PAIR(vnu, u));}
								if(!edits.has_edge(vnu, v)) {used.insert(PAIR(vnu, v));}
								if(!edits.has_edge(vvnu, vnu)) {used.insert(PAIR(vvnu, vnu));}
								if(!edits.has_edge(vvnu, u)) {used.insert(PAIR(vvnu, u));}
								//std::cout << +vvnu << " " << +vnu << " " << +u << " " << +v << std::endl;
								found_edited++;
								if(!edits.has_edge(vnu, u) || !edits.has_edge(vnu, v)) {goto found_ue;}
							}
						}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nv = graph.get_row(v)[j]; nv; nv &= ~(typename Graph::Packed(1) << __builtin_ctzll(nv)))
							{
								typename Graph::VertexID vnv = __builtin_ctzll(nv) + j * Graph::bits;
								if(u == vnv || vnu == vnv) {continue;}
								if(graph.has_edge(u, vnv)) {continue;}
								if(bounds_rev.count(PAIR(u, vnv)) && bounds_rev[PAIR(u, vnv)] != lbi) {continue;}
								if(used.count(PAIR(u, vnv))) {continue;}
								if(bounds_rev.count(PAIR(v, vnv)) && bounds_rev[PAIR(v, vnv)] != lbi) {continue;}
								if(used.count(PAIR(v, vnv))) {continue;}
								//if(bounds_rev.count(PAIR(vnu, vnv))) {continue;}
								if(!edits.has_edge(vnu, u)) {used.insert(PAIR(vnu, u));}
								if(!edits.has_edge(vnu, v)) {used.insert(PAIR(vnu, v));}
								if(!edits.has_edge(u, vnv)) {used.insert(PAIR(u, vnv));}
								if(!edits.has_edge(v, vnv)) {used.insert(PAIR(v, vnv));}
								//std::cout << +vnu << " " << +u << " " << +v << " " << +vnv << std::endl;
								found_edited++;
								if(!edits.has_edge(vnu, u) || !edits.has_edge(vnu, v)) {goto found_ue;}
							}
						}
						found_ue:;
					}
				}
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(typename Graph::Packed nv = graph.get_row(v)[i]; nv; nv &= ~(typename Graph::Packed(1) << __builtin_ctzll(nv)))
					{
						typename Graph::VertexID vnv = __builtin_ctzll(nv) + i * Graph::bits;
						if(vnv == u) {continue;}
						if(graph.has_edge(u, vnv)) {continue;}
						if(bounds_rev.count(PAIR(vnv, u)) && bounds_rev[PAIR(u, vnv)] != lbi) {continue;}
						if(used.count(PAIR(vnv, u))) {continue;}
						if(bounds_rev.count(PAIR(vnv, v)) && bounds_rev[PAIR(v, vnv)] != lbi) {continue;}
						if(used.count(PAIR(vnv, v))) {continue;}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nvnv = graph.get_row(vnv)[j]; nvnv; nvnv &= ~(typename Graph::Packed(1) << __builtin_ctzll(nvnv)))
							{
								typename Graph::VertexID vvnv = __builtin_ctzll(nvnv) + j * Graph::bits;
								if(vvnv == u || vvnv == v) {continue;}
								if(graph.has_edge(v, vvnv)) {continue;}
								if(bounds_rev.count(PAIR(vvnv, vnv)) && bounds_rev[PAIR(vvnv, vnv)] != lbi) {continue;}
								if(used.count(PAIR(vvnv, vnv))) {continue;}
								if(bounds_rev.count(PAIR(vvnv, v)) && bounds_rev[PAIR(vvnv, v)] != lbi) {continue;}
								if(used.count(PAIR(vvnv, v))) {continue;}
								//if(bounds_rev.count(PAIR(vvnv, v))) {continue;}
								if(!edits.has_edge(vnv, u)) {used.insert(PAIR(vnv, u));}
								if(!edits.has_edge(vnv, v)) {used.insert(PAIR(vnv, v));}
								if(!edits.has_edge(vvnv, vnv)) {used.insert(PAIR(vvnv, vnv));}
								if(!edits.has_edge(vvnv, v)) {used.insert(PAIR(vvnv, v));}
								//std::cout << +u << " " << +v << " " << +vnv << " " << +vvnv << std::endl;
								found_edited++;
								if(!edits.has_edge(vnv, u) || !edits.has_edge(vnv, v)) {goto found_ve;}
							}
						}
						found_ve:;
					}
				}
			}
			else
			{
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(typename Graph::Packed nc = graph.get_row(u)[i] & graph.get_row(v)[i]; nc; nc &= ~(typename Graph::Packed(1) << __builtin_ctzll(nc)))
					{
						typename Graph::VertexID vnc = __builtin_ctzll(nc) + i * Graph::bits;
						if(bounds_rev.count(PAIR(u, vnc)) && bounds_rev[PAIR(u, vnc)] != lbi) {continue;}
						if(used.count(PAIR(u, vnc))) {continue;}
						if(bounds_rev.count(PAIR(vnc, v)) && bounds_rev[PAIR(vnc, v)] != lbi) {continue;}
						if(used.count(PAIR(vnc, v))) {continue;}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nu = graph.get_row(u)[j]; nu; nu &= ~(typename Graph::Packed(1) << __builtin_ctzll(nu)))
							{
								typename Graph::VertexID vnu = __builtin_ctzll(nu) + j * Graph::bits;
								if(u == vnu || vnc == vnu) {continue;}
								if(graph.has_edge(vnc, vnu)) {continue;}
								if(bounds_rev.count(PAIR(vnc, vnu)) && bounds_rev[PAIR(vnc, vnu)] != lbi) {continue;}
								if(used.count(PAIR(vnc, vnu))) {continue;}
								if(bounds_rev.count(PAIR(u, vnu)) && bounds_rev[PAIR(u, vnu)] != lbi) {continue;}
								if(used.count(PAIR(u, vnu))) {continue;}
								//if(bounds_rev.count(PAIR(vnu, v))) {continue;}
								if(!edits.has_edge(u, vnc)) {used.insert(PAIR(u, vnc));}
								if(!edits.has_edge(vnc, v)) {used.insert(PAIR(vnc, v));}
								if(!edits.has_edge(vnu, vnc)) {used.insert(PAIR(vnu, vnc));}
								if(!edits.has_edge(vnu, u)) {used.insert(PAIR(vnu, u));}
								//std::cout << +vnu << " " << +u << " " << +vnc << " " << +v << std::endl;
								found_edited++;
								if(!edits.has_edge(vnc, u) || !edits.has_edge(vnc, v)) {goto found_ce;}
							}
						}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(typename Graph::Packed nv = graph.get_row(v)[j]; nv; nv &= ~(typename Graph::Packed(1) << __builtin_ctzll(nv)))
							{
								typename Graph::VertexID vnv = __builtin_ctzll(nv) + j * Graph::bits;
								if(u == vnv || vnc == vnv) {continue;}
								if(graph.has_edge(vnc, vnv)) {continue;}
								if(bounds_rev.count(PAIR(vnc, vnv)) && bounds_rev[PAIR(vnc, vnv)] != lbi) {continue;}
								if(used.count(PAIR(vnc, vnv))) {continue;}
								if(bounds_rev.count(PAIR(v, vnv)) && bounds_rev[PAIR(v, vnv)] != lbi) {continue;}
								if(used.count(PAIR(v, vnv))) {continue;}
								//if(bounds_rev.count(PAIR(u, vnv))) {continue;}
								if(!edits.has_edge(u, vnc)) {used.insert(PAIR(u, vnc));}
								if(!edits.has_edge(vnc, v)) {used.insert(PAIR(vnc, v));}
								if(!edits.has_edge(vnc, vnv)) {used.insert(PAIR(vnc, vnv));}
								if(!edits.has_edge(v, vnv)) {used.insert(PAIR(v, vnv));}
								//std::cout << +u << " " << +vnc << " " << +v << " " << +vnv << std::endl;
								found_edited++;
								if(!edits.has_edge(vnc, u) || !edits.has_edge(vnc, v)) {goto found_ce;}
							}
						}
						found_ce:;
					}
				}
			}

			found_edited++;// k will be reduced by the edit
			//std::cout << +e.first.first << " - " << +e.first.second << ": " << found_fixed << ", " << found_edited << std::endl;

			return std::make_tuple(found_fixed, found_edited);
		}
#endif
	};
#undef PAIR
}

#endif
