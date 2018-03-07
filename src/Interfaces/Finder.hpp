#ifndef INTERFACE_FINDER_HPP
#define INTERFACE_FINDER_HPP

#include <vector>

#include "../config.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Finder
	{
	public:
		static constexpr char const *name = "Finder Interface";
		/** Number of vertices in Px/Cx found by this Finder */
		static constexpr size_t length;

		Finder(VertexID graph_size);

		/** Searches for forbidden subgraphs in graph
		 * calls feeder.callback(graph, edits, subgraph.begin(), subgraph.end()) for each forbidden subgraph found
		 * if feeder.callback returns true, no consumer wants additional forbidden subgraphs and this function should return
		 */
		template<typename Feeder>
		void find(Graph const &graph, Graph_Edits const &edited, Feeder &feeder);

		/** Searches for forbidden subgraphs in graph
		 * optional, currently only used by Consumer::Single
		 * same as find, but limits search to an area around the vertices u and v
		 * calls feeder.callback_near(graph, edits, subgraph.begin(), subgraph.end()) for each forbidden subgraph found
		 * if feeder.callback_near returns true, no consumer wants additional forbidden subgraphs and this function should return
		 */
		template<typename Feeder>
		void find_near(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v, Feeder &feeder);
	};

	/* for reference */
	template<typename Finder, typename Graph, typename Graph_Edits, typename... Consumer>
	class Feeder
	{
	public:
		Feeder(Finder &finder, std::tuple<Consumer &...> consumer);
		Feeder(Finder &finder, Consumer &... consumer);

		/** Prepares consumers for a new run of the Finder and starts it */
		void feed(Graph const &graph, Graph_Edits const &edited);
		/** Provides each consumer with the found forbidden subgraph, returns true iff no Consumer want further forbidden subgraphs */
		bool callback(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end);

		/* as above, for find_near */
		/** Prepares consumers for a new run of the Finder and starts it */
		void feed_near(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v);
		/** Provides each consumer with the found forbidden subgraph, returns true iff no Consumer want further forbidden subgraphs */
		bool callback_near(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end);

	};
}

#endif
