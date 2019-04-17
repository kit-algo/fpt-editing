#ifndef FINDER_SUBGRAPH_COUNTS_HPP
#define FINDER_SUBGRAPH_COUNTS_HPP

#include "../Graph/ValueMatrix.hpp"
#include "../Finder/Finder.hpp"
#include <array>
#include <cassert>

namespace Finder
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Subgraph_Stats
	{
	public:
		using subgraph_t = std::array<VertexID, length>;
		Value_Matrix<size_t> num_subgraphs_per_edge;
		size_t num_subgraphs;
		size_t sum_subgraphs_per_edge;
		std::vector<size_t> before_mark_count;

		Finder_impl finder;

		Subgraph_Stats(VertexID n) : num_subgraphs_per_edge(n), num_subgraphs(0), sum_subgraphs_per_edge(0), finder(n) {}

		void initialize(const Graph& graph, const Graph_Edits& edited)
		{
			if (num_subgraphs_per_edge.size() == 0) return;
			if (num_subgraphs > 0)
			{
				num_subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, size_t &c) {c = 0;});
				sum_subgraphs_per_edge = 0;
				num_subgraphs = 0;
			}

			finder.find(graph, [&](const subgraph_t& path)
			{
				register_subgraph(graph, edited, path);
				return false;
			});
		}

		void before_edit(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			if (num_subgraphs_per_edge.size() == 0) return;

			assert((edited.has_edge(u, v) || std::is_same<Restriction, Options::Restrictions::None>::value));

			verify_num_subgraphs_per_edge(graph, edited);

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				remove_subgraph(graph, edited, path);
				return false;
			});

			assert(num_subgraphs_per_edge.at(u, v) == 0);
		}

		void after_edit(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			if (num_subgraphs_per_edge.size() == 0) return;
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				register_subgraph(graph, edited, path);
				return false;
			});

			verify_num_subgraphs_per_edge(graph, edited);
			assert(num_subgraphs_per_edge.at(u, v) == 0);
		}

		void after_mark(Graph const &graph, const Graph_Edits &edited, VertexID u, VertexID v)
		{
			if (num_subgraphs_per_edge.size() == 0) return;
			sum_subgraphs_per_edge -= num_subgraphs_per_edge.at(u, v);
			before_mark_count.push_back(num_subgraphs_per_edge.at(u, v));
			num_subgraphs_per_edge.at(u, v) = 0;

			verify_num_subgraphs_per_edge(graph, edited);
		}

		void after_unmark(const Graph& graph, const Graph_Edits &edited, VertexID u, VertexID v)
		{
			if (num_subgraphs_per_edge.size() == 0) return;
			num_subgraphs_per_edge.at(u, v) = before_mark_count.back();
			sum_subgraphs_per_edge += before_mark_count.back();
			before_mark_count.pop_back();
			verify_num_subgraphs_per_edge(graph, edited);
		}
	private:
		void register_subgraph(Graph const &graph, Graph_Edits const &edited, const subgraph_t& path)
		{
			++num_subgraphs;

			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
				++num_subgraphs_per_edge.at(*uit, *vit);
				++sum_subgraphs_per_edge;
				return false;
			});
		}

		void remove_subgraph(Graph const &graph, const Graph_Edits &edited, const subgraph_t& path)
		{
			--num_subgraphs;
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit)
			{
				--num_subgraphs_per_edge.at(*uit, *vit);
				--sum_subgraphs_per_edge;
				return false;
			});
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
	};
}

#endif
