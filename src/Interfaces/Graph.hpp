#ifndef INTERFACE_GRAPH_HPP
#define INTERFACE_GRAPH_HPP

#include <vector>

#include "../config.hpp"

namespace Graph
{
	template<bool _small>
	class Graph
	{
	public:
		static constexpr char const *name = "Graph Interface";

		Graph(VertexID);
		VertexID size() const;
		size_t count_edges() const;
		bool has_edge(VertexID u, VertexID v) const;
		void set_edge(VertexID u, VertexID v);
		void clear_edge(VertexID u, VertexID v);
		void toggle_edge(VertexID u, VertexID v);
		std::vector<VertexID> const get_neighbours(VertexID u) const;
		bool verify() const;
	};

	template<bool _small>
	class GraphM : public Graph<_small>
	{
	public:
		static constexpr char const *name = "Graph Interface for adjecency matrix";

		Packed const *get_row(VertexID u) const;
		size_t get_row_length() const;
		std::vector<Packed> alloc_rows(size_t i) const;
	};
}

#endif
