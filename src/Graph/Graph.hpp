#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <assert.h>
#include <malloc.h>

#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

#include "../config.hpp"

namespace Graph
{
	template<bool _small>
	class Graph
	{
	public:
		static constexpr char const *name = "Graph Interface";

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


	uint64_t get_size(std::string const &filename)
	{
		std::ifstream f(filename);
		std::string line;
		std::istringstream linestream;

		if(!f)
		{
			std::cerr << "Error opening file: " << filename << std::endl;
			throw f;
		}

		while(std::getline(f, line))
		{
			if(line[0] != '%') {break;}
		}
		if(!f)
		{
			std::cerr << "Premature end of file: " << filename << std::endl;
			throw f;
		}

		uint64_t n;
		linestream.str(line);
		linestream >> n;
		return n;
	}

	template<typename Graph>
	Graph readMetis(std::string const &filename)
	{
		std::ifstream f(filename);
		std::string line;
		std::istringstream linestream;

		if(!f)
		{
			std::cerr << "Error opening file: " << filename << std::endl;
			throw f;
		}

		while(std::getline(f, line))
		{
			if(line[0] != '%') {break;}
		}
		if(!f)
		{
			std::cerr << "Premature end of file: " << filename << std::endl;
			throw f;
		}

		uint64_t n, ignored, fmt = 0, vertexweights = 0;
		bool edgeweight = false;

		linestream.str(line);
		linestream >> n >> ignored;
		if(linestream >> fmt)
		{
			if(fmt >= 10)
			{
				linestream >> vertexweights;
			}
			edgeweight = (fmt == 1 || fmt == 11);
		}

		if(n > std::numeric_limits<VertexID>::max())
		{
			std::cerr << "graph has too many vertices: " << filename << std::endl;;
			throw f;
		}

		Graph g(n);
		uint64_t u = 0;
		uint64_t v;
		while(std::getline(f, line))
		{
			if(line[0] == '%') {continue;}
			linestream.clear();
			linestream.str(line);

			u++;

			for(uint64_t i = 0; i < vertexweights; i++) {linestream >> ignored;}
			while(linestream >> v)
			{
				g.set_edge(u - 1, v - 1);
				if(edgeweight) {linestream >> ignored;}
			}
		}

		g.verify();
		return g;
	}

	template<typename Graph>
	void writeMetis(std::string const &filename, Graph const &g)
	{
		g.verify();
		std::ofstream f(filename);
		if(!f)
		{
			std::cerr << "Error opening file: " << filename << std::endl;
			throw f;
		}

		f << +g.size() << " " << g.count_edges() << '\n';
		for(VertexID u = 0; u < g.size(); u++)
		{
			bool first = true;
			for(VertexID const &v: g.get_neighbours(u))
			{
				if(!first) {f << ' ';}
				first = false;
				f << +(v + 1);
			}
			f << '\n';
		}
	}

	template<typename Graph>
	static void writeDot(std::string const &filename, Graph const &g)
	{
		g.verify();
		std::ofstream f(filename);
		if(!f)
		{
			std::cerr << "Error opening file: " << filename << std::endl;
			throw f;
		}
		f << "graph {\n";
		for(VertexID u = 0; u < g.size(); u++)
		{
			for(VertexID const &v: g.get_neighbours(u))
			{
				if(u < v) {f << "\t" << +u << " -- " << +v << '\n';}
			}
			if(g.get_neighbours(u).empty()) {f << "\t" << +u << '\n';}
		}
		f << "}\n";
	}

	template<typename Graph, typename Graph_Edits>
	void writeDotCombined(std::string const &filename, Graph const &g, Graph_Edits const &edits, Graph const &g_orig)
	{
		assert(g.size() == edits.size() && g.size() == g_orig.size());
		g.verify();
		edits.verify();
		g_orig.verify();

		std::ofstream f(filename);
		if(!f)
		{
			std::cerr << "Error opening file " << filename << std::endl;
			throw f;
		}

		f << "graph {\n";
		for(VertexID u = 0; u < g_orig.size(); u++)
		{
			for(VertexID const &v: g_orig.get_neighbours(u))
			{
				if(u < v)
				{
					f << "\t" << +u << " -- " << +v;
					if(!g.has_edge(u, v)) {f << " [color=red]";}// removed edges
					else if(edits.has_edge(u, v)) {f << " [color=cyan]";}// fixed existing edges
					f << '\n';
				}
			}
		}
		for(VertexID u = 0; u < edits.size(); u++)
		{
			for(VertexID const &v: edits.get_neighbours(u))
			{
				if(u < v && !g_orig.has_edge(u, v))
				{
					if(g.has_edge(u, v)) {f << "\t" << +u << " -- " << +v << " [color=green]\n";}// inserted edges
					else {f << "\t" << +u << " -- " << +v << " [color=purple]\n";}// fixed missing edges
				}
			}
		}
		f << "}\n";
	}
}

#endif
