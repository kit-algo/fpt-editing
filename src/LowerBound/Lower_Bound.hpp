#ifndef LOWER_BOUND_LOWER_BOUND_HPP
#define LOWER_BOUND_LOWER_BOUND_HPP

#include <vector>
#include <array>
#include <algorithm>
#include "../config.hpp"
#include "../Finder/Finder.hpp"
#include <iostream>
#include <sstream>

namespace Lower_Bound
{
	template<typename Mode, typename Restriction, typename Conversion, typename Graph, typename Graph_Edits, size_t length>
	class Lower_Bound
	{
	public:
		using subgraph_t = std::array<VertexID, length>;
	private:
		std::vector<subgraph_t> bound;
	public:
		template <typename iterator_t>
		void add(const iterator_t& begin, const iterator_t& end)
		{
			bound.emplace_back();
			iterator_t it = begin;
			for (size_t i = 0; i < length; ++i) {
				if (it == end) abort();

				bound.back()[i] = *it;
				++it;
			}

			if (it != end) abort();
		}

		/**
		 * Removes any forbidden subgraphs that contain the given vertex pair (u, v).
		 */
		void remove(const Graph& graph, const Graph_Edits& edited, VertexID u, VertexID v)
		{
			if (edited.has_edge(u, v))
			{
				abort();
			}
			auto new_end = std::remove_if(bound.begin(),
				       bound.end(),
				       [&](const subgraph_t& subgraph) -> bool
				       {
					       return ::Finder::for_all_edges_unordered<Mode, Restriction, Conversion, Graph, Graph_Edits>(graph,
											edited,
											subgraph.begin(),
											subgraph.end(),
											[&](auto x, auto y)
											{
												return (u == *x && v == *y) || (u == *y && v == *x);
											}
											);

				       }
				       );

			bound.erase(new_end, bound.end());
		}

		void clear()
		{
			bound.clear();
		}


		const std::vector<subgraph_t>& get_bound() const
		{
			return bound;
		}

		const subgraph_t& operator[](size_t i) const
		{
			return bound[i];
		}

		std::vector<VertexID> as_vector(size_t i) const
		{
			return std::vector<VertexID>(bound[i].begin(), bound[i].end());
		}

		size_t size() const
		{
			return bound.size();
		}

		bool empty() const
		{
			return bound.empty();
		}

		std::string to_string() const
		{
			std::stringstream ss;
			if (!empty())
			{
				ss << "{";
				for (const subgraph_t& subgraph : bound)
				{
					ss << "<";
					for (VertexID u : subgraph)
					{
						ss << static_cast<uint32_t>(u) << ",";
					}
					ss.get();
					ss << ">, ";
				}
				ss.get();
				ss.get();
				ss.get();
				ss << "}";
			}
			return ss.str();
		}
	};
}

#endif
