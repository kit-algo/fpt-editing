#include <iostream>

#include "Finder/Center.hpp"
#include "Finder/Finder.hpp"
#include "Graph/Matrix.hpp"
#include "Graph/Graph.hpp"
#include "Graph/ValueMatrix.hpp"
#include "Options.hpp"
#include "util.hpp"
#include "Editor/Gurobi.h"

int main(int argc, char * argv[])
{
	if (argc < 4)
	{
		std::cout << "Please specify a graph file, a heuristic solution graph file and \"iteratively\" if the graph shall be solved iteratively or \"single\", if single constraints shall be added in the callback." << std::endl;
		return 1;
	}

	using G = Graph::Matrix<false>;
	using E = Graph::Matrix<false>;
	using M = Options::Modes::Edit;
	using R = Options::Restrictions::Redundant;
	using C = Options::Conversions::Normal;

	G graph = Graph::readMetis<G>(argv[1]);
	G heuristic_solution = Graph::readMetis<G>(argv[2]);

	E edited(graph.size());

	using F = Finder::Center_4<G, E, M, R, C>;
	F finder(graph.size());

	G edited_graph = graph;
	Editor::Gurobi<F, G> editor(finder, edited_graph, heuristic_solution);

	std::string variant(argv[3]);

	if (variant == "iteratively") {
		std::cout << "Solving iteratively" << std::endl;
		editor.solve_iteratively();
	} else if (variant == "full") {
		editor.solve_full();
	} else {
		editor.solve(variant == "single");
	}

	size_t num_edits = 0;
	for (VertexID u = 0; u < graph.size(); ++u) {
		for (VertexID v = u + 1; v < graph.size(); ++v) {
			if (graph.has_edge(u, v) != edited_graph.has_edge(u, v)) {
				++num_edits;
			}
		}
	}

	std::cout << "Needed " << num_edits << " edits" << std::endl;

	return 0;
}
