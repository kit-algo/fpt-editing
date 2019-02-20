#ifndef CONSUMER_LOWER_BOUND_MIN_DEG_HPP
#define CONSUMER_LOWER_BOUND_MIN_DEG_HPP

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
#include "../Bucket_PQ.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../util.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Min_Deg : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Min_Deg";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;

	private:
		std::vector<typename Lower_Bound_Storage_type::subgraph_t> forbidden_subgraphs;
		Value_Matrix<std::vector<size_t>> subgraphs_per_edge;
		size_t sum_subgraphs_per_edge;

		bool bound_calculated;

		Graph_Edits used_updated;

		Lower_Bound_Storage_type bound_updated;
		Lower_Bound_Storage_type bound_new;
	public:
		Min_Deg(VertexID graph_size) : subgraphs_per_edge(graph_size), sum_subgraphs_per_edge(0), bound_calculated(false), used_updated(graph_size) {;}

		void prepare(size_t, const Lower_Bound_Storage_type& lower_bound)
		{
			forbidden_subgraphs.clear();
			subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& v) { v.clear(); });

			bound_calculated = false;
			used_updated.clear();
			bound_new.clear();
			bound_updated = lower_bound;
			sum_subgraphs_per_edge = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			const size_t forbidden_index = forbidden_subgraphs.size();
			forbidden_subgraphs.emplace_back(Util::to_array<VertexID, length>(b));

			Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, b, e, [&](auto uit, auto vit) {
				subgraphs_per_edge.at(*uit, *vit).push_back(forbidden_index);
				sum_subgraphs_per_edge++;

				return false;
			});

			return false;
		}

		size_t result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound)
		{
			prepare_result(k, g, e);
			return bound_new.size();
		}

		const Lower_Bound_Storage_type& result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound_Update)
		{
			prepare_result(k, g, e);
			return bound_new;
		}

	private:
		Lower_Bound_Storage_type initialize_lb_min_deg(size_t k, const Graph& g, const Graph_Edits& e)
		{
			Lower_Bound_Storage_type result;

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

			BucketPQ pq(forbidden_subgraphs.size(), 42 * forbidden_subgraphs.size() + sum_subgraphs_per_edge);

			for (size_t fsid = 0; fsid < forbidden_subgraphs.size(); ++fsid) {
				const typename Lower_Bound_Storage_type::subgraph_t& fs = forbidden_subgraphs[fsid];

				size_t neighbor_count = 0;
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&neighbor_count, &subgraphs_per_edge = subgraphs_per_edge](auto uit, auto vit) {
					neighbor_count += subgraphs_per_edge.at(*uit, *vit).size();
					return false;
				});

				pq.insert(fsid, neighbor_count);
			}

			if (!pq.empty()) {
				pq.build();

				while (!pq.empty()) {
					auto idkey = pq.pop();

					const auto& fs = forbidden_subgraphs[idkey.first];

					result.add(fs.begin(), fs.end());
					if (k > 0 && result.size() > k) break;

					if (idkey.second > 1) {
						enumerate_neighbor_ids(fs, [&pq, &enumerate_neighbor_ids, &forbidden_subgraphs = forbidden_subgraphs](size_t nfsid)
						{
							if (pq.contains(nfsid)) {
								pq.erase(nfsid);
								enumerate_neighbor_ids(forbidden_subgraphs[nfsid], [&pq](size_t nnfsid)
								{
									if (pq.contains(nnfsid)) {
										pq.decrease_key_by_one(nnfsid);
									}
								});
							}
						});
					}
				}
			}

			result.assert_valid(g, e);

			return result;
		}

		std::vector<size_t> independent_set_pls(size_t k, const Graph &g, const Graph_Edits &e)
		{
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

				of << n << " " << neighbors.size()/2 << " 1" << std::endl;

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

			bound_new = initialize_lb_min_deg(k, g, e);
		}
	};
}

#endif
