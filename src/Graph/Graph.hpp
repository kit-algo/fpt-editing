#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <assert.h>

#include <vector>
#include <random>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "../config.hpp"

namespace Graph
{
	/** Number of vertices in the graph stored in filename */
	uint64_t get_size(std::string const &filename, bool edgelist);

	/** reads a graph in METIS format */
	template<typename Graph>
	Graph readEdgeList(std::string const &filename)
	{
		size_t n = get_size(filename, true);

		if(n > std::numeric_limits<VertexID>::max())
		{
			throw std::range_error(std::string("Graph has too many vertices: ") + filename);
		}

		Graph g(n);

		std::ifstream f(filename);

		uint64_t u, v;
		while(f >> u >> v)
		{
			g.set_edge(u, v);
		}

		g.verify();
		return g;
	}

	/** reads a graph in METIS format */
	template<typename Graph>
	Graph readMetis(std::string const &filename)
	{
		std::ifstream f(filename);
		std::string line;
		std::istringstream linestream;

		if(!f)
		{
			throw std::runtime_error(std::string("Error opening file: ") + filename);
		}

		while(std::getline(f, line))
		{
			if(line[0] != '%') {break;}
		}
		if(!f)
		{
			throw std::runtime_error(std::string("Premature end of file: ") + filename);
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
			throw std::range_error(std::string("Graph has too many vertices: ") + filename);
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

	/** Writes a graph in METIS format */
	template<typename Graph>
	void writeMetis(std::string const &filename, Graph const &g)
	{
		g.verify();
		std::ofstream f(filename);
		if(!f)
		{
			throw std::runtime_error(std::string("Error opening file: ") + filename);
		}

		f << +g.size() << " " << +g.count_edges() << '\n';
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

	/** Writes a graph in Dot format */
	template<typename Graph>
	static void writeDot(std::string const &filename, Graph const &g)
	{
		g.verify();
		std::ofstream f(filename);
		if(!f)
		{
			throw std::runtime_error(std::string("Error opening file: ") + filename);
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

	/** writes a graph in Dot format, annotating edges that were modified or marked */
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
			throw std::runtime_error(std::string("Error opening file: ") + filename);
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

	/**
	 * Generate a random permutation of node ids for the given Graph @a g and seed @as seed.
	 *
	 * @param g    The graph to generate the permutation for.
	 * @param seed The random seed to use. 0 generates the identity permutation.
	 */
	template <typename Graph>
	std::vector<VertexID> generate_permutation(const Graph &g, size_t seed)
	{
		std::vector<VertexID> result(g.size());
		for (VertexID u = 0; u < g.size(); ++u)
		{
			result[u] = u;
		}

		if (seed != 0)
		{
			std::mt19937_64 gen(seed);
			std::shuffle(result.begin(), result.end(), gen);
		}

		return result;
	}

	template <typename Graph>
	Graph apply_permutation(const Graph &g, const std::vector<VertexID> &permutation)
	{
		Graph result(g.size());

		for (VertexID u = 0; u < g.size(); ++u)
		{
			for (VertexID v : g.get_neighbours(u))
			{
				result.set_edge(permutation[u], permutation[v]);
			}
		}

		return result;
	}

	std::vector<VertexID> invert_permutation(const std::vector<VertexID> &permutation);
}

#endif
