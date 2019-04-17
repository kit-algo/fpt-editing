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

		std::cout << "Testing " << decltype(finder)::name << std::endl;

		E forbidden(graph.size());

		auto print_subgraphs = [](const std::vector<subgraph_t>& subgraphs)
				       {
						for (subgraph_t sg : subgraphs)
						{
							for (VertexID x : sg)
								std::cout << static_cast<size_t>(x) << ", ";
							std::cout << std::endl;
						}
				       };
		auto verify_for_forbidden_pair = [&](VertexID uu, VertexID vv)
		{
			if (uu != vv) forbidden.set_edge(uu, vv);

			// Global listing
			Value_Matrix<std::vector<subgraph_t>> subgraphs_per_pair(graph.size());

			auto cb = [&](const subgraph_t& sg)
			{
				Finder::for_all_edges_unordered<M, R, Options::Conversions::Normal>(graph, edited, sg.begin(), sg.end(), [&](auto u, auto v)
				{
					subgraphs_per_pair.at(*u, *v).push_back(sg);
					return false;
				});
				return false;
			};

			if (uu != vv) finder.find(graph, cb, forbidden);
			else finder.find(graph, cb);

			if (uu != vv && subgraphs_per_pair.at(uu, vv).size() > 0)
			{
				for (subgraph_t& sg : subgraphs_per_pair.at(uu, vv))
				{
					if ((sg.front() != uu || sg.back() != vv) && (sg.front() != vv || sg.back() != uu)) {
						std::cout << "At forbidden pair (" << uu << ", " << vv << ")" << std::endl;
						print_subgraphs(subgraphs_per_pair.at(uu, vv));
					}
				}
			}

			subgraphs_per_pair.forAllNodePairs([&](VertexID u, VertexID v, std::vector<subgraph_t>& subgraphs)
			{
				std::vector<subgraph_t> local_subgraphs;

				auto local_cb = [&](const subgraph_t &sg)
				{
					local_subgraphs.push_back(sg);

					return false;
				};

				bool is_forbidden_pair = (uu == u && vv == v) || (uu == v && vv == u);

				if (uu != vv)
				{
					if (!is_forbidden_pair) finder.find_near(graph, u, v, local_cb, forbidden);
				}
				else finder.find_near(graph, u, v, local_cb);

				if (!is_forbidden_pair && subgraphs.size() != local_subgraphs.size())
				{
					std::cout << "(" << static_cast<size_t>(u) << ", " << static_cast<size_t>(v) << ")" << std::endl;

					std::cout << "near (" << local_subgraphs.size() << "): " << std::endl;

					print_subgraphs(local_subgraphs);

					std::cout << "global (" << subgraphs.size() << "): " << std::endl;

					print_subgraphs(subgraphs);
				}
			});


			if (uu != vv) forbidden.clear_edge(uu, vv);
		};

		verify_for_forbidden_pair(0, 0);

		for (VertexID u = 0; u < graph.size(); ++u)
		{
			for (VertexID v = u + 1; v < graph.size(); ++v)
			{
				verify_for_forbidden_pair(u, v);
			}
		}

	});

	return 0;
}
