#include <iostream>

#include "Finder/Center.hpp"
#include "Finder/Finder.hpp"
#include "Graph/Matrix.hpp"
#include "Graph/Graph.hpp"
#include "Graph/ValueMatrix.hpp"
#include "Options.hpp"
#include "util.hpp"

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

	auto finders = std::make_tuple(
				       Finder::Center_P2<G, E, M, R, C>(graph.size()),
				       Finder::Center_P3<G, E, M, R, C>(graph.size()),
				       Finder::Center_P4<G, E, M, R, C>(graph.size()),
				       Finder::Center_P5<G, E, M, R, C>(graph.size()),
				       Finder::Center_4<G, E, M, R, C>(graph.size()),
				       Finder::Center_5<G, E, M, R, C>(graph.size()),
				       Finder::Center_4<G, E, M, R, Options::Conversions::Skip>(graph.size()),
				       Finder::Center_5<G, E, M, R, Options::Conversions::Skip>(graph.size())
				       );

	Util::for_<std::tuple_size<decltype(finders)>::value>([&](auto i)
	{
		auto finder = std::get<i.value>(finders);
		constexpr size_t length = decltype(finder)::length;
		using subgraph_t = std::array<VertexID, length>;
		using Conversion = typename decltype(finder)::Conversion;

		std::cout << "Testing " << decltype(finder)::name << std::endl;

		// Global listing
		Value_Matrix<std::vector<subgraph_t>> subgraphs_per_pair(graph.size());

		auto cb = [&](const subgraph_t& sg)
		{
			Finder::for_all_edges_unordered<M, R, Conversion>(graph, edited, sg.begin(), sg.end(), [&](auto u, auto v)
			{
				subgraphs_per_pair.at(*u, *v).push_back(sg);
				return false;
			});
			return false;
		};

		finder.find(graph, cb);

		subgraphs_per_pair.forAllNodePairs([&](VertexID u, VertexID v, std::vector<subgraph_t>& subgraphs)
		{
			std::vector<subgraph_t> local_subgraphs;

			auto local_cb = [&](const subgraph_t &sg)
			{
				local_subgraphs.push_back(sg);

				return false;
			};

			finder.find_near(graph, u, v, local_cb);

			if (subgraphs.size() != local_subgraphs.size())
			{
				std::cout << "(" << static_cast<size_t>(u) << ", " << static_cast<size_t>(v) << ")" << std::endl;

				std::cout << "near (" << local_subgraphs.size() << "): " << std::endl;
				for (subgraph_t sg : local_subgraphs)
				{
					for (VertexID x : sg)
						std::cout << static_cast<size_t>(x) << ", ";
					std::cout << std::endl;
				}

				std::cout << "global (" << subgraphs.size() << "): " << std::endl;
				for (subgraph_t sg : subgraphs)
				{
					for (VertexID x : sg)
						std::cout << static_cast<size_t>(x) << ", ";
					std::cout << std::endl;
				}
			}
		});
	});

	return 0;
}
