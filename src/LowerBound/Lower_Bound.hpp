#ifndef LOWER_BOUND_LOWER_BOUND_HPP
#define LOWER_BOUND_LOWER_BOUND_HPP

#include <vector>
#include <array>
#include <algorithm>
#include "../config.hpp"
#include "../Finder/Finder.hpp"
#include <iostream>
#include <sstream>
#include <cassert>

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

		void add(const subgraph_t& sg)
		{
			bound.emplace_back(sg);
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

		std::vector<subgraph_t>& get_bound()
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

		void assert_valid(const Graph&
#ifndef NDEBUG
				  graph
#endif
				  , const Graph_Edits&
#ifndef NDEBUG
				  edited
#endif
				  ) const
		{
			#ifndef NDEBUG
			Graph_Edits in_bound(graph.size());

			for (const auto& fs : bound)
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion, Graph, Graph_Edits>(graph, edited, fs.begin(), fs.end(), [&in_bound](auto uit, auto vit)
				{
					assert(!in_bound.has_edge(*uit, *vit));
					in_bound.set_edge(*uit, *vit);
					return false;
				});
			}
			#endif
		}

		template <typename finder_t>
		void assert_maximal(const Graph&
#ifndef NDEBUG
				  graph
#endif
				  , const Graph_Edits&
#ifndef NDEBUG
				  edited
#endif
				  ,
				    finder_t &
				    #ifndef NDEBUG
				    finder
				    #endif
				  ) const
		{
			#ifndef NDEBUG
			Graph_Edits in_bound(graph.size());

			for (const auto& fs : bound)
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion, Graph, Graph_Edits>(graph, edited, fs.begin(), fs.end(), [&in_bound](auto uit, auto vit)
				{
					assert(!in_bound.has_edge(*uit, *vit));
					in_bound.set_edge(*uit, *vit);
					return false;
				});
			}

			finder.find(graph, [&](const subgraph_t& fs) {
				// Fully edited subgraphs are not marked in any way!
				bool has_any_non_edited_edge = false;
				bool is_in_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion, Graph, Graph_Edits>(graph, edited, fs.begin(), fs.end(), [&in_bound, &has_any_non_edited_edge](auto uit, auto vit)
				{
					has_any_non_edited_edge =  true;
					return in_bound.has_edge(*uit, *vit);
				});

				assert(!has_any_non_edited_edge || is_in_bound);

				return false;
			});

			#endif
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
