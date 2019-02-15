#ifndef CONSUMER_SELECTOR_SINGLE_HEUR_HPP
#define CONSUMER_SELECTOR_SINGLE_HEUR_HPP

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
#include "../ProblemSet.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "LB_Updated.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Single_Heur : Options::Tag::Selector, Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Single_Heur";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		// feeder contains references to this which turn to dangling pointers with default {copy,move} {constructor,assignment}.
		// This struct holds the members for which the default functions to the right thing,
		// so the {copy,move} {constructor,assignment} doesn't have to mention every single member.
		struct M
		{
			Updated<Graph, Graph_Edits, Mode, Restriction, Conversion, length> updated_lb;

			Value_Matrix<size_t> bounds_single;
			bool searching_single = false;
			ProblemSet problem;
			size_t no_edits_left;

			Finder::Center<Graph, Graph_Edits, Mode, Restriction, Conversion, length> finder;

			M(VertexID graph_size) : updated_lb(graph_size), bounds_single(graph_size), finder(graph_size) {;}
		} m;

		Finder::Feeder<decltype(m.finder), Graph, Graph_Edits, Single_Heur> feeder;

	public:
		// manual {copy,move} {constructor,assignment} implementations due to feeder
		Single_Heur(Single_Heur const &o) : m(o.m), feeder(this->m.finder, *this) {;}
		Single_Heur(Single_Heur &&o) : m(std::move(o.m)), feeder(this->m.finder, *this) {;}
		Single_Heur &operator =(Single_Heur const &o)
		{
			m = o.m;
			feeder = decltype(feeder)(this->m.finder, *this);
		}
		Single_Heur &operator =(Single_Heur &&o)
		{
			m = std::move(o.m);
			feeder = decltype(feeder)(this->m.finder, *this);
		}

		Single_Heur(VertexID graph_size) : m(graph_size), feeder(m.finder, *this) {;}

		void prepare(size_t no_edits_left, const Lower_Bound_Storage_type& lower_bound)
		{
			if(!m.searching_single)
			{
				m.updated_lb.prepare(no_edits_left, lower_bound);

				// No need to initialize stuff twice
				m.no_edits_left = no_edits_left;
				m.bounds_single.forAllNodePairs([](VertexID, VertexID, size_t &v) { v = 0; });
				m.problem.vertex_pairs.clear();
				m.problem.found_solution = true;
				m.problem.needs_no_edit_branch = false;
			}
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			if(!m.searching_single)
			{
				return m.updated_lb.next(graph, edited, b, e);
			}
			else
			{
				// find forbidden subgraphs with exactly one edge used in bound
				size_t count_bound = 0;
				std::pair<VertexID, VertexID> in_bound;
				size_t free = 0;
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
					if(m.updated_lb.bound_uses(*uit, *vit))
					{
						count_bound++;
						in_bound = std::make_pair(*uit, *vit);
					}
					free++;
					return false;
				});

				// fallback handling
				if(free < m.problem.vertex_pairs.size() || m.problem.found_solution)
				{
					m.problem.vertex_pairs.clear();
					m.problem.found_solution = false;
					Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
						m.problem.vertex_pairs.emplace_back(*uit, *vit);
						return false;
					});

					if(free == 0) {return true;} // completly edited subgraph -- impossible to solve graph
				}

				if(count_bound == 1) {m.bounds_single.at(in_bound.first, in_bound.second)++;}
				return false;
			}
		}

		ProblemSet result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Selector)
		{
			const Lower_Bound_Storage_type& lower_bound = m.updated_lb.result(k, g, e, Options::Tag::Lower_Bound_Update());

			// Fast return for solved graphs and branches that will be pruned
			if(lower_bound.empty() || lower_bound.size() > k)
			{
				m.problem.found_solution = lower_bound.empty();
				return m.problem;
			}

			// find forbidden subgraph with only one edge in m.bounds
			m.searching_single = true;
			feeder.feed(k, g, e, m.no_edits_left, lower_bound);
			m.searching_single = false;

			if(m.problem.vertex_pairs.size() > 1)
			{
				// we found edges suitable for single edge editing, pick best
				std::pair<VertexID, VertexID> best_single_edge = {0, 0};
				size_t max_count = 0;
				m.bounds_single.forAllNodePairs([&best_single_edge, &max_count](VertexID u, VertexID v, size_t count)
								{
					if(count > max_count)
					{
						best_single_edge = std::make_pair(u, v);
						max_count = count;
					}

				});

				if (max_count > 0)
				{
					m.problem.vertex_pairs.clear();
					m.problem.vertex_pairs.push_back(best_single_edge);
					m.problem.needs_no_edit_branch = true;
				}
			}

			// fallback
			// no edge suitable for single edge editing, according to our heuristic
			// pick least editable forbidden subgraph
			return m.problem;
		}

		size_t result(size_t k, Graph const & graph, Graph_Edits const & edited, Options::Tag::Lower_Bound)
		{
			return m.updated_lb.result(k, graph, edited, Options::Tag::Lower_Bound());
		}

		const Lower_Bound_Storage_type& result(size_t k, Graph const& graph, Graph_Edits const & edited, Options::Tag::Lower_Bound_Update)
		{
			return m.updated_lb.result(k, graph, edited, Options::Tag::Lower_Bound_Update());
		}
	};
}

#endif
