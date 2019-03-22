
#ifndef CONSUMER_LOWER_BOUND_UPDATED_HPP
#define CONSUMER_LOWER_BOUND_UPDATED_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Updated : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Updated";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		Graph_Edits used_updated;
		Graph_Edits used_new;

		Lower_Bound_Storage_type bound_updated;
		Lower_Bound_Storage_type bound_new;

		bool initialized_bound_updated;

	public:
		Updated(VertexID graph_size) : used_updated(graph_size), used_new(graph_size), initialized_bound_updated(false) {;}

		void prepare(size_t, const Lower_Bound_Storage_type& lower_bound)
		{
			used_updated.clear();
			used_new.clear();
			bound_new.clear();
			bound_updated = lower_bound;
			initialized_bound_updated = false;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			auto any_used = [&](const Graph_Edits& used, auto begin, auto end) -> bool
			{
				return Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, begin, end, [&](auto uit, auto vit) {
					return used.has_edge(*uit, *vit);
				});
			};

			auto add_subgraph = [&](Graph_Edits& used, auto begin, auto end)
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, begin, end, [&](auto uit, auto vit) {
					used.set_edge(*uit, *vit);
					return false;
				});
			};

			if (!initialized_bound_updated)
			{
				for (const auto &it : bound_updated.get_bound())
				{
					assert(!any_used(used_updated, it.begin(), it.end()));
					add_subgraph(used_updated, it.begin(), it.end());
				}

				initialized_bound_updated = true;
			}

			if (!any_used(used_updated, b, e))
			{
				add_subgraph(used_updated, b, e);
				bound_updated.add(b, e);
			}

			if (!any_used(used_new, b, e))
			{
				add_subgraph(used_new, b, e);
				bound_new.add(b, e);
			}

			return false;
		}

		size_t result(size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound)
		{
			ensure_updated_is_larger_bound();
			return bound_updated.size();
		}

		const Lower_Bound_Storage_type& result(size_t, Graph const&, Graph_Edits const&, Options::Tag::Lower_Bound_Update)
		{
			ensure_updated_is_larger_bound();
			return bound_updated;
		}

		bool bound_uses(VertexID u, VertexID v) const
		{
			return used_updated.has_edge(u, v);
		}


	private:
		void ensure_updated_is_larger_bound()
		{
			if (bound_new.size() > bound_updated.size())
			{
				std::swap(bound_new, bound_updated);
				std::swap(used_new, used_updated);
			}
		}
	};
}

#endif
