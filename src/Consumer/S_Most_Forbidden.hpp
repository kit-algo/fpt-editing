#ifndef SELECTOR_MOST_HPP
#define SELECTOR_MOST_HPP

#include <algorithm>
#include <vector>

#include "../config.hpp"

#include "../Options.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Most : Options::Tag::Selector
	{
	public:
		static constexpr char const *name = "Most";

	private:
		std::vector<size_t> use_count;
		std::vector<VertexID> problem;

	public:
		Most(VertexID graph_size) : use_count((graph_size * (graph_size - 1)) / 2) {;}

		void prepare()
		{
			std::fill(use_count.begin(), use_count.end(), 0);
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				get(graph, *uit, *vit)++;
				return false;
			});
			return false;
		}

		std::vector<VertexID> const &result(size_t, Graph const &graph, Graph const &, Options::Tag::Selector)
		{
			problem.clear();
			size_t best = 0;
			for(VertexID u = 0; u < graph.size(); u++) for(VertexID v = u + 1; v < graph.size(); v++)
			{{
				if(get(graph, u, v) > best)
				{
					best = get(graph, u, v);
					problem = decltype(problem){u, v};
				}
			}}
			return problem;
		}

	private:
		size_t &get(Graph const &graph, VertexID const &u, VertexID const &v)
		{
			auto x = std::minmax(u, v);
			return use_count[graph.size() * x.first + x.second - 1 - ((x.first * x.first + 3 * x.first) / 2)];
		}
	};
}

#endif
