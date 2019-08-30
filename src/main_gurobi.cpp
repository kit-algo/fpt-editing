#include <iostream>

#include "Finder/Center.hpp"
#include "Finder/Finder.hpp"
#include "Graph/Matrix.hpp"
#include "Graph/Graph.hpp"
#include "Graph/ValueMatrix.hpp"
#include "Options.hpp"
#include "util.hpp"
#include "Editor/Gurobi.h"


namespace Options {
	Editor::GurobiOptions ParseGurobiOptions(int argc, char* argv[]) {
		//does not handle unknown options
		if (argc < 2) {
			std::cout << "Usage: <graph path> [ optional -t #threads -v { basic, basic-single, basic-sparse, full, iteratively } -h <path to heuristic solution>  ]" << std::endl;
			std::exit(-1);
		}
		Editor::GurobiOptions options;
		options.graph = argv[1];
		std::vector<std::string> remaining;
		for (int i = 2; i < argc; ++i) {
			remaining.push_back(argv[i]);
		}

		const size_t not_found = std::numeric_limits<size_t>::max();
		auto search = [&](const std::string x) {
			if (remaining.empty())
				return not_found;
			for (size_t i = 0; i < remaining.size() - 1; ++i) {
				if (remaining[i].find(x) != std::string::npos)
					return i+1;
			}
			return not_found;
		};

		size_t pos_t = search("-t");
		if (pos_t != not_found)
			options.n_threads = std::stoi(remaining[pos_t]);

		size_t pos_h = search("-h");
		if (pos_h != not_found)
			options.heuristic_solution = remaining[pos_h];
		if (options.heuristic_solution != "Do not use")
			options.use_heuristic_solution = true;

		size_t pos_v = search("-v");
		if (pos_v != not_found)
			options.variant = remaining[pos_v];
		if (options.variant == "basic-single")
			options.add_single_constraints = true;
		else if (options.variant == "basic-sparse")
			options.use_sparse_constraints = true;
		else
			options.add_single_constraints = false;

		options.print();
		return options;
	}
}



int main(int argc, char * argv[])
{
	using G = Graph::Matrix<false>;
	using E = Graph::Matrix<false>;
	using M = Options::Modes::Edit;
	using R = Options::Restrictions::Redundant;
	using C = Options::Conversions::Normal;

	auto options = Options::ParseGurobiOptions(argc, argv);

	G graph = Graph::readMetis<G>(options.graph);
	G heuristic_solution(graph.size());
	if (options.use_heuristic_solution) {
		heuristic_solution = Graph::readMetis<Graph::Matrix<false>>(options.heuristic_solution);
	}
	//E edited(graph.size());

	using F = Finder::Center_4<G, E, M, R, C>;
	F finder(graph.size());

	G edited_graph = graph;
	Editor::Gurobi<F, G> editor(finder, edited_graph);

	editor.solve(heuristic_solution, options);


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
