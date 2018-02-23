#ifndef INTERFACE_EDITOR_HPP
#define INTERFACE_EDITOR_HPP

#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "../config.hpp"

#include "../Options.hpp"

namespace Editor
{
	template<typename Finder, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, typename... Consumer>
	class Editor
	{
	public:
		static constexpr char const *name = "Editor Interface";

		/** Do the consumers provide all Tags this Editor requires to run */
		static constexpr bool valid = Options::has_tagged_consumer<Options::Tag::Selector, Consumer...>::value && Options::has_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value;

		/** Index of the consumer used as selector resp. lower bound in Consumer... */
		static constexpr size_t selector = Options::get_tagged_consumer<Options::Tag::Selector, Consumer...>::value;
		static constexpr size_t lb = Options::get_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value;

		/** The type of the consumer used as selector resp. lower bound */
		using Selector_type = typename std::tuple_element<selector, std::tuple<Consumer ...>>::type;
		using Lower_Bound_type = typename std::tuple_element<lb, std::tuple<Consumer ...>>::type;

	public:
		Editor(Finder &finder, Graph &graph, std::tuple<Consumer &...> consumer, size_t threads);

		/** Tries to solve the graph editing problem for graph with at most k edits
		 * calls writegraph(graph, edited) for each solutions found
		 * returns whether a solution was found
		 */
		bool edit(size_t k, std::function<bool(Graph const &, Graph_Edits const &)> const &writegraph);

#ifdef STATS
		/** provides statistics for the previous call of edit, stats()[name][k] denotes how often event "name" occured while having k edits remaining */
		std::map<std::string, std::vector<size_t> const &> stats() const;
#endif
	};
}

#endif
