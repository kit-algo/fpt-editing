#include <iostream>

#include "Finder/Center.hpp"
#include "Finder/Finder.hpp"
#include "Graph/Matrix.hpp"
#include "Graph/Graph.hpp"
#include "Graph/ValueMatrix.hpp"
#include "Options.hpp"
#include "util.hpp"
#include "Editor/Gurobi.h"
#include <tlx/cmdline_parser.hpp>


namespace Options {
	Editor::GurobiOptions ParseGurobiOptions(int argc, char* argv[]) {
		tlx::CmdlineParser cp;

		cp.set_description("Solve the given graph exactly using Gurobi.");

		Editor::GurobiOptions options;

		cp.add_param_string("graph", options.graph, "The graph to solve in METIS format.");
		cp.add_string('h', "heuristic-solution", "solved_graph", options.heuristic_solution, "A heuristic solution to give to Gurobi");
		cp.add_int('j', "threads", "num_threads", options.n_threads, "Number of threads to use.");
		cp.add_string('v', "variant", "variant", options.variant, "The variant to use, one of: basic, basic-sparse, basic-single, iteratively, full.");

		cp.add_flag('e', "extended-constraints", options.use_extended_constraints, "Generate more constraints initially.");
		cp.add_flag('r', "relaxation-constraints", options.add_constraints_in_relaxation, "Add additional constraints for relaxation solutions.");
		cp.add_flag('s', "init-sparse", options.init_sparse, "Initialize with sparse constraints (only useful for -sparse and -single variants).");
		cp.add_flag('c', "single-c4", options.single_c4_constraints, "Add only one constraint per C4");
		cp.add_size_t('l', "lazy", "lazy_level", options.all_lazy, "Add all constraints as lazy constraints with level 1, 2, or 3");
		cp.add_size_t('t', "time-limit", "limit", options.time_limit, "Time limit in seconds, 0 for unlimited");
		cp.add_size_t('p', "permutation", "seed", options.permutation, "The seed for the permutation that shall be applied to the input node ids");

		if (!cp.process(argc, argv)) {
			throw std::runtime_error("Invalid options specified.");
		}

		if (options.heuristic_solution != "Do not use")
			options.use_heuristic_solution = true;

		if (options.variant == "basic-single") {
			options.add_single_constraints = true;
		} else if (options.variant == "basic-sparse") {
			options.use_sparse_constraints = true;
		} else if (options.variant != "basic" && options.variant != "full" && options.variant != "iteratively") {
			std::cerr << "Invalid variant " << options.variant << std::endl;
			cp.print_usage();
			throw std::runtime_error("Invalid variant " + options.variant);
		}

		cp.print_result();

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

	Editor::GurobiOptions options;
	try {
		options = Options::ParseGurobiOptions(argc, argv);
	} catch (std::runtime_error) {
		return 1;
	}

	const G input_graph = Graph::readMetis<G>(options.graph);

	const std::vector<VertexID> permutation = Graph::generate_permutation(input_graph, options.permutation);
	const G graph = Graph::apply_permutation(input_graph, permutation);

	G heuristic_solution(graph.size());
	if (options.use_heuristic_solution) {
		heuristic_solution = Graph::apply_permutation(Graph::readMetis<G>(options.heuristic_solution), permutation);
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

	std::string name = std::string(F::name) + "-" + options.get_name();

	std::cout << "{\"type\":\"exact\",\"graph\":\"" << options.graph << "\",\"permutation\":" << options.permutation << ",";
	std::cout << "\"n\":" << static_cast<size_t>(input_graph.size()) << ",\"m\":" << input_graph.count_edges() << ",";
	std::cout << "\"algo\":\"" << name << "\",\"threads\":" << +options.n_threads << ",\"k\":" << + editor.get_bound() << ",";
	std::cout << "\"results\":{\"solved\":\"" << (editor.is_optimal() ? "true" : "false") << "\",\"time\":" << editor.get_elapsed_seconds();
	std::cout << ",\"time_initialization\":" << 0;
	std::cout << ",\"total_time\":" << editor.get_elapsed_seconds();
	std::cout << ",\"solutions\":" << (editor.is_optimal() ? 1 : 0);
	std::cout << ",\"counters\":{";
	std::cout << '}';
	std::cout << "}}," << std::endl;

	std::cout << "{\"type\":\"heuristic\",\"graph\":\"" << options.graph << "\",\"permutation\":" << options.permutation << ",";
	std::cout << "\"n\":" << static_cast<size_t>(input_graph.size()) << ",\"m\":" << input_graph.count_edges() << ",";
	std::cout << "\"algo\":\"" << name << "-heuristic\",\"threads\":" << +options.n_threads << ",\"k\":" << + num_edits << ",";
	std::cout << "\"results\":{\"solved\":\"" << "true" << "\",\"time\":" << editor.get_elapsed_seconds();
	std::cout << ",\"time_initialization\":" << 0;
	std::cout << ",\"total_time\":" << editor.get_elapsed_seconds();
	std::cout << ",\"solutions\":" << 1;
	std::cout << ",\"counters\":{";
	std::cout << '}';
	std::cout << "}}," << std::endl;

	return 0;
}
