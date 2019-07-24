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
	if (argc < 2)
	{
		std::cout << "Please specify a graph file" << std::endl;
		return 1;
	}

	using G = Graph::Matrix<false>;
	using E = Graph::Matrix<false>;
	using M = Options::Modes::Edit;
	using R = Options::Restrictions::Redundant;
	using C = Options::Conversions::Normal;

	G graph = Graph::readMetis<G>(argv[1]);
	E edited(graph.size());

	using F = Finder::Center_4<G, E, M, R, C>;
	F finder(graph.size());

        G edited_graph = graph;
        Editor::Gurobi<G, F> editor(finder, edited_graph);
        editor.solve();

        // TODO: compare edited to orig graph

	return 0;
}
