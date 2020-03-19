#include <iostream>

#include "Finder/Center.hpp"
#include "Graph/Matrix.hpp"
#include "Graph/Graph.hpp"
#include "Options.hpp"
#include "util.hpp"
#include "Consumer/LB_ARW.hpp"
#include "Finder/SubgraphStats.hpp"
#include <tlx/cmdline_parser.hpp>


int main(int argc, char * argv[])
{
	using G = Graph::Matrix;
	using E = Graph::Matrix;
	using M = Options::Modes::Edit;
	using R = Options::Restrictions::Redundant;
	using C = Options::Conversions::Skip;
	using F = Finder::Center_4<G, E, M, R, C>;

	std::string input_filename;
	size_t permutation_id = 0;

	tlx::CmdlineParser cp;
	cp.set_description("Calculate the local search lower bound for the input file.");
	cp.add_string('i', "input", input_filename, "The input filename");
	cp.add_size_t("permutation", permutation_id, "permutation of the input instance");

	if (!cp.process(argc, argv)) {
		return 1;
	}

	if (input_filename.empty()) {
		cp.print_usage();
		return 1;
	}

	const G input_graph = Graph::readMetis<G>(input_filename);

	const std::vector<VertexID> permutation = Graph::generate_permutation(input_graph, permutation_id);
	const G graph = Graph::apply_permutation(input_graph, permutation);
	const E edited(graph.size());

	Finder::Subgraph_Stats<F, G, E, M, R, C, 4> subgraph_stats(graph.size());
	subgraph_stats.initialize(graph, edited);

	Consumer::ARW<F, G, E, M, R, C, 4> lb(graph.size());
	auto state = lb.initialize(std::numeric_limits<size_t>::max(), graph, edited);

	size_t bound = lb.result(state, subgraph_stats, std::numeric_limits<size_t>::max(), graph, edited, Options::Tag::Lower_Bound());

	std::cout << "forbidden_subgraphs: C4P4" << std::endl;
	std::cout << "lower_bound_algorithm: LocalSearch" << std::endl;
	std::cout << "instance: " << input_filename << std::endl;
	std::cout << "value: " << bound << std::endl;

	return 0;
}
