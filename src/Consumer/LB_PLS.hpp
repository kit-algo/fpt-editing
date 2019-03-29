#ifndef CONSUMER_LOWER_BOUND_PLS_HPP
#define CONSUMER_LOWER_BOUND_PLS_HPP

#include <vector>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <memory>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../util.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class PLS : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "PLS";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		std::vector<typename Lower_Bound_Storage_type::subgraph_t> forbidden_subgraphs;
		Value_Matrix<std::vector<size_t>> subgraphs_per_edge;

		bool bound_calculated;

		Graph_Edits used_updated;
		bool used_updated_initialized;
		Lower_Bound_Storage_type bound_updated;
	public:
		PLS(VertexID graph_size) : subgraphs_per_edge(graph_size), bound_calculated(false), used_updated(graph_size), used_updated_initialized(false) {;}

		void prepare(size_t, const Lower_Bound_Storage_type& lower_bound)
		{
			forbidden_subgraphs.clear();
			subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& v) { v.clear(); });

			bound_calculated = false;
			used_updated.clear();
			used_updated_initialized = false;
			bound_updated = lower_bound;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			if (!used_updated_initialized)
			{
				for (const auto fs : bound_updated.get_bound())
				{
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit)
					{
						used_updated.set_edge(*uit, *vit);
						return false;
					});
				}

				used_updated_initialized = true;
			}

			const size_t forbidden_index = forbidden_subgraphs.size();
			forbidden_subgraphs.emplace_back(Util::to_array<VertexID, length>(b));

			bool touches_bound = false;

			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				subgraphs_per_edge.at(*uit, *vit).push_back(forbidden_index);

				touches_bound |= used_updated.has_edge(*uit, *vit);
				return false;
			});

			if (!touches_bound)
			{
				bound_updated.add(b, e);

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
					used_updated.set_edge(*uit, *vit);
					return false;
				});
			}

			return false;
		}

		size_t result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound)
		{
			prepare_result(k, g, e);
			return bound_updated.size();
		}

		const Lower_Bound_Storage_type& result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound_Update)
		{
			prepare_result(k, g, e);
			return bound_updated;
		}

	private:
		Lower_Bound_Storage_type independent_set_pls(size_t k, const Graph &g, const Graph_Edits &e)
		{
			auto enumerate_neighbor_ids = [&subgraphs_per_edge = subgraphs_per_edge, &g, &e](const typename Lower_Bound_Storage_type::subgraph_t& fs, auto callback) {
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					const auto& new_neighbors = subgraphs_per_edge.at(*uit, *vit);
					for (size_t ne : new_neighbors)
					{
						callback(ne);
					}

					return false;
				});
			};

			std::vector<size_t> first_neighbor;
			std::vector<size_t> neighbors;

			for (size_t fsid = 0; fsid < forbidden_subgraphs.size(); ++fsid)
			{
				first_neighbor.push_back(neighbors.size());

				enumerate_neighbor_ids(forbidden_subgraphs[fsid], [&neighbors, fsid](size_t ne) { if (ne != fsid) neighbors.push_back(ne); });
				const auto start_it = neighbors.begin() + first_neighbor.back();
				std::sort(start_it, neighbors.end());
				neighbors.erase(std::unique(start_it, neighbors.end()), neighbors.end());
			}

			first_neighbor.push_back(neighbors.size());

			std::stringstream fname;
			const size_t n = first_neighbor.size() - 1;
			fname << "/tmp/independent_set_" << k << "_" << n << "_" << neighbors.size()/2 << ".metis.graph";
			{
				std::ofstream of(fname.str());

				of << n << " " << neighbors.size()/2 << " 0" << std::endl;

				for (size_t u = 0; u < n; ++u)
				{
					const auto start_it = neighbors.begin() + first_neighbor[u];
					for (auto it = start_it; it != neighbors.begin() + first_neighbor[u+1]; ++it)
					{
						if (it != start_it) { of << " "; }
						of << (*it + 1);
					}

					of << std::endl;
				}
			}

			std::string cmd = "../open-pls/bin/pls --algorithm=mis --timeout=10  --input-file=" + fname.str() + " --verbose --no-reduce";
			if (k > 0)
			{
				cmd += " --target-weight=" + std::to_string(k + 1);
			}

			std::array<char, 128> buffer;
			std::stringstream output;
			std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
			if (!pipe) {
				throw std::runtime_error("popen() failed!");
			}
			while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
				output << buffer.data();
			}

			Lower_Bound_Storage_type result;

			std::string line;
			while (std::getline(output, line))
			{
				std::cout << line << std::endl;
				if (line.find("best-solution") == 0)
				{
					std::stringstream solution(line.substr(17, line.size() - 17));
					size_t fsid;
					while (solution >> fsid)
					{
						result.add(forbidden_subgraphs[fsid].begin(), forbidden_subgraphs[fsid].end());
					}
				}
			}

			result.assert_valid(g, e);

			return result;
		}

		void prepare_result(size_t k, Graph const &g, Graph_Edits const &e)
		{
			if (bound_calculated) return;
			bound_calculated = true;

			if (bound_updated.size() <= k)
			{
				Lower_Bound_Storage_type bound_pls = independent_set_pls(k, g, e);

				if (bound_updated.size() < bound_pls.size())
				{
					bound_updated = bound_pls;
				}
			}

		}
	};
}

#endif
