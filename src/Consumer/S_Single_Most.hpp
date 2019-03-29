#ifndef CONSUMER_SELECTOR_SINGLE_INDEPENDENT_SET_HPP
#define CONSUMER_SELECTOR_SINGLE_INDEPENDENT_SET_HPP

#include <algorithm>
#include <utility>
#include <vector>
#include <limits>

#include <iostream>

#include "../util.hpp"

#include "../config.hpp"

#include "../Options.hpp"

#include "../Finder/Finder.hpp"
#include "../Finder/Center.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../ProblemSet.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Single_Most : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "Single_Most";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;

	private:
		Value_Matrix<size_t> num_subgraphs_per_edge;
		size_t num_subgraphs;
		ProblemSet problem;
		int64_t num_single_left;

		std::vector<size_t> counter_before_mark;
		Finder_impl finder;
	public:
		Single_Most(VertexID graph_size) : num_subgraphs_per_edge(graph_size), num_subgraphs(0), num_single_left(0), finder(graph_size) {;}

		void initialize(size_t k, Graph const &graph, Graph_Edits const &edited)
		{
			num_single_left = k;
			num_subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, size_t& v) { v = 0; });
			num_subgraphs = 0;
			problem.vertex_pairs.clear();
			problem.found_solution = true;
			problem.needs_no_edit_branch = false;
			counter_before_mark.clear();

			finder.find(graph, [&](const subgraph_t& path)
			{
				size_t free = 0;
				++num_subgraphs;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					++num_subgraphs_per_edge.at(*uit, *vit);
					++free;
					return false;
				});

				// fallback handling
				if(free < problem.vertex_pairs.size() || problem.empty())
				{
					problem.vertex_pairs.clear();

					Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						problem.vertex_pairs.emplace_back(*uit, *vit);
						return false;
					});

					problem.found_solution = false;

					if(free == 0) {return true;} // completly edited subgraph -- impossible to solve graph
				}

				return false;
			});

			verify_num_subgraphs_per_edge(graph, edited);
		}

		void verify_num_subgraphs_per_edge(Graph const &graph, Graph_Edits const &edited)
		{
			(void) graph; (void) edited;
#ifndef NDEBUG
			Value_Matrix<size_t> debug_subgraphs(graph.size());
			size_t debug_num_subgraphs = 0;

			finder.find(graph, [&](const subgraph_t& path)
			{
				++debug_num_subgraphs;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					++debug_subgraphs.at(*uit, *vit);
					return false;
				});

				return false;
			});

			debug_subgraphs.forAllNodePairs([&](VertexID u, VertexID v, size_t debug_num)
			{
				assert(num_subgraphs_per_edge.at(u, v) == debug_num);
				assert(!edited.has_edge(u, v) || debug_num == 0);
			});
#endif
		}

		void before_mark_and_edit(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			counter_before_mark.push_back(num_subgraphs_per_edge.at(u, v));

			verify_num_subgraphs_per_edge(graph, edited);

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				--num_subgraphs;
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit)
				{
					--num_subgraphs_per_edge.at(*uit, *vit);
					return false;
				});

				return false;
			});

			assert(num_subgraphs_per_edge.at(u, v) == 0);

			problem.vertex_pairs.clear();
			problem.found_solution = true;
			problem.needs_no_edit_branch = false;
		}

		void after_mark_and_edit(size_t /*k*/, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				++num_subgraphs;
				size_t free = 0;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					++num_subgraphs_per_edge.at(*uit, *vit);
					++free;
					return false;
				});

				// fallback handling
				if(free < problem.vertex_pairs.size() || problem.found_solution)
				{
					problem.vertex_pairs.clear();

					Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						problem.vertex_pairs.emplace_back(*uit, *vit);
						return false;
					});

					problem.found_solution = false;
				}

				return false;
			});

			verify_num_subgraphs_per_edge(graph, edited);
			assert(num_subgraphs_per_edge.at(u, v) == 0);
		}

		void before_undo_edit(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				--num_subgraphs;
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit)
				{
					--num_subgraphs_per_edge.at(*uit, *vit);
					return false;
				});

				return false;
			});

			problem.vertex_pairs.clear();
			problem.found_solution = true;
			problem.needs_no_edit_branch = false;
		}

		void after_undo_edit(size_t /*k*/, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			assert(edited.has_edge(u, v));

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				++num_subgraphs;

				size_t free = 0;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					++num_subgraphs_per_edge.at(*uit, *vit);
					++free;
					return false;
				});

				// fallback handling
				if(free < problem.vertex_pairs.size() || problem.found_solution)
				{
					problem.vertex_pairs.clear();

					Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						problem.vertex_pairs.emplace_back(*uit, *vit);
						return false;
					});

					problem.found_solution = false;
				}

				return false;
			});

			verify_num_subgraphs_per_edge(graph, edited);
			assert(num_subgraphs_per_edge.at(u, v) == 0);

			--num_single_left;
		}

		void before_undo_mark(Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
			problem.vertex_pairs.clear();
			problem.found_solution = true;
			problem.needs_no_edit_branch = false;
		}

		void after_undo_mark(size_t /*k*/, Graph const & graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			num_subgraphs_per_edge.at(u, v) = counter_before_mark.back();
			counter_before_mark.pop_back();
			++num_single_left;
			verify_num_subgraphs_per_edge(graph, edited);
		}

		ProblemSet result(size_t, Graph const &graph, Graph_Edits const &edited, Options::Tag::Selector)
		{
			// Do not use single vertex pair editing if we have a solved graph or we found a completely edited/marked subgraph or one with just a single branch.
			if (num_subgraphs > 0 && (problem.found_solution || (problem.vertex_pairs.size() > 1 && num_single_left >= 0))) {
				size_t max_subgraphs = 0;
				std::pair<VertexID, VertexID> node_pair;
				num_subgraphs_per_edge.forAllNodePairs([&](VertexID u, VertexID v, size_t& num_fbs) {
					if (num_fbs > max_subgraphs) {
						max_subgraphs = num_fbs;
						node_pair.first = u;
						node_pair.second = v;
					}
				});

				if (num_single_left < 0 || max_subgraphs == 1) // only use single editing if there is a node pair that is part of multiple forbidden subgraphs
				{
					finder.find_near(graph, node_pair.first, node_pair.second, [&](const subgraph_t &sg)
					{
						problem.found_solution = false;
						problem.vertex_pairs.clear();

						Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, sg.begin(), sg.end(), [&](auto uit, auto vit) {
							problem.vertex_pairs.emplace_back(*uit, *vit);
							return false;
						});

						return true;
					});
				}
				else
				{
					problem.vertex_pairs.clear();
					problem.vertex_pairs.push_back(node_pair);
					problem.found_solution = false;
					problem.needs_no_edit_branch = true;
				}
			}

			return problem;
		}
	};
}

#endif
