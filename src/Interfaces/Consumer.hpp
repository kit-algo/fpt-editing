#ifndef INTERFACE_CONSUMER_HPP
#define INTERFACE_CONSUMER_HPP

#include <vector>

#include "../config.hpp"

#include "../Options.hpp"

namespace Consumer
{
	/** length is Finder::length, in case a Consumer uses a Finder */
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Consumer
	{
	public:
		static constexpr char const *name = "Consumer Interface";

		Consumer(VertexID graph_size);

		/** Prepares for a new run of the Finder, resets internal state */
		void prepare(Graph const &graph, Graph_Edits const &edited);
		/** Called for each forbidden subgraph found by the Finder. Returns true if no longer interessted in the remaining forbidden subgraphs */
		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end);

		/* result functions, last argument is used to differentiate between these */
		/** only if inheriting Opions::Tag::Lower_Bound, the calculated lower bound */
		size_t result(size_t k, Graph const &graph, Graph_Edits const &edited, Options::Tag::Lower_Bound) const;
		/** only if inheriting Opions::Tag::Selector, the selected forbidden subgraph */
		std::vector<VertexID> const &result(size_t k, Graph const &graph, Graph_Edits const &edited, Options::Tag::Selector) const;
		/** only if inheriting Opions::Tag::Result, allows for producing side effects */
		void result(size_t k, Graph const &graph, Graph_Edits const &edited, Options::Tag::Result) const;
	};
}

#endif
