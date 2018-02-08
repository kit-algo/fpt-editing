#ifndef INTERFACE_GRAPH_HPP
#define INTERFACE_GRAPH_HPP

#include <vector>

#include "../config.hpp"

namespace Graph
{
	/** Graph implementation.
	 * A small Graph is used if the number of vertices is not more then Packed_Bits;
	 * this can be used to optimize the implementation
	 *
	 * Does not support adding or removing of vertices
	 */
	template<bool small>
	class Graph
	{
	public:
		static constexpr char const *name = "Graph Interface";

		Graph(VertexID graph_size);
		/** Numer of vertices */
		VertexID size() const;
		/** Number of edges */
		size_t count_edges() const;

		/** Removes all edges from the graph */
		void clear();

		/** Test/Modify specific edges, calling with swapped parameters must return the same result */
		bool has_edge(VertexID u, VertexID v) const;
		void set_edge(VertexID u, VertexID v);
		void clear_edge(VertexID u, VertexID v);
		void toggle_edge(VertexID u, VertexID v);

		/** The degree (the number of neighbours) of a vertex */
		size_t degree(VertexID u) const;
		/** The neighbours of a vertex */
		std::vector<VertexID> const get_neighbours(VertexID u) const;

		bool verify() const;
	};

	template<bool small>
	class GraphM : public Graph<small>
	{
	public:
		static constexpr char const *name = "additional functions for adjacency matrices";

		/** Start of the matrix row for vertex u */
		Packed const *get_row(VertexID u) const;

		/** Number of elements used for each row */
		static size_t get_row_length(VertexID vertices);
		size_t get_row_length() const;

		/** Allocates memory for rows matrix rows */
		static std::vector<Packed> alloc_rows(VertexID vertices, size_t rows);
		std::vector<Packed> alloc_rows(size_t rows) const;
	};
}

#endif
