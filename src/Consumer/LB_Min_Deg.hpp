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
		std::vector<size_t> initialize_independent_set_min_deg(size_t max_size, const std::vector<size_t>& first_neighbor, const std::vector<size_t>& neighbors)
		{
			std::vector<size_t> result;

			const size_t n = first_neighbor.size() - 1;

			BucketPQ pq(n, 42 * forbidden_subgraphs.size() + sum_subgraphs_per_edge);

			for (size_t u = 0; u < n; ++u) {
				pq.insert(u, first_neighbor[u+1] - first_neighbor[u]);
			}

			if (!pq.empty()) {
				pq.build();

				while (!pq.empty()) {
					auto idkey = pq.pop();

					size_t u = idkey.first;

					result.push_back(u);

					if (max_size > 0 && result.size() > max_size) break;

					if (idkey.second > 0) {
						for (auto itv = neighbors.begin() + first_neighbor[u]; itv != neighbors.begin() + first_neighbor[u+1]; ++itv)
						{
							if (pq.contains(*itv)) {
								pq.erase(*itv);
								for (auto itw = neighbors.begin() + first_neighbor[*itv]; itw != neighbors.begin() + first_neighbor[(*itv) + 1]; ++itw)
								{
									if (pq.contains(*itw)) {
										pq.decrease_key_by_one(*itw);
									}
								}
							}
						}
					}
				}
			}

			return result;
		}

		std::vector<size_t> independent_set_pls(size_t max_size, const std::vector<size_t>& first_neighbor, const std::vector<size_t>& neighbors)
		{
			std::stringstream fname;
			const size_t n = first_neighbor.size() - 1;
			fname << "/tmp/independent_set_" << max_size << "_" << n << "_" << neighbors.size()/2 << ".metis.graph";
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
			if (max_size > 0)
			{
				cmd += " --target-weight=" + std::to_string(max_size + 1);
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

			std::vector<size_t> result;

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
						result.push_back(fsid);
					}
				}
			}

			return result;
		}

		void prepare_result(size_t k, Graph const &g, Graph_Edits const &e)
		{
			if (bound_calculated) return;
			bound_calculated = true;

			std::vector<size_t> first_neighbor;
			std::vector<size_t> neighbors;

			std::vector<std::pair<std::vector<size_t>::const_iterator, std::vector<size_t>::const_iterator>> neighbor_lists;
			std::vector<size_t> tmp1, tmp2;

			for (size_t u = 0; u < forbidden_subgraphs.size(); ++u)
			{
				neighbor_lists.clear();
				const auto fs = forbidden_subgraphs[u];

				first_neighbor.push_back(neighbors.size());

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
					const auto& new_neighbors = subgraphs_per_edge.at(*uit, *vit);
					if (new_neighbors.size() > 1)
					{
						neighbor_lists.emplace_back(new_neighbors.begin(), new_neighbors.end());
					}

					return false;
				});

				if (neighbor_lists.size() == 1)
				{
					std::copy(neighbor_lists.front().first, neighbor_lists.front().second, std::back_inserter(neighbors));
				}
				else if (neighbor_lists.size() == 2)
				{
					std::set_union(neighbor_lists.front().first, neighbor_lists.front().second, neighbor_lists.back().first, neighbor_lists.back().second, std::back_inserter(neighbors));
				}
				else if (neighbor_lists.size() > 2)
				{
				    std::sort(neighbor_lists.begin(), neighbor_lists.end(), [](const auto& a, const auto& b) { return std::distance(a.first, a.second) < std::distance(b.first, b.second); });

				    tmp1.clear();

				    std::set_union(neighbor_lists[0].first, neighbor_lists[0].second, neighbor_lists[1].first, neighbor_lists[1].second, std::back_inserter(tmp1));

				    for (auto it = neighbor_lists.begin() + 2; it + 1 != neighbor_lists.end(); ++it)
				    {
					    std::swap(tmp1, tmp2);
					    tmp1.clear();
					    std::set_union(it->first, it->second, tmp2.begin(), tmp2.end(), std::back_inserter(tmp1));
				    }

				    std::set_union(neighbor_lists.back().first, neighbor_lists.back().second, tmp1.begin(), tmp1.end(), std::back_inserter(neighbors));
				}

				neighbors.erase(std::remove(neighbors.begin() + first_neighbor[u], neighbors.end(), u), neighbors.end());

				assert(std::is_sorted(neighbors.begin() + first_neighbor[u], neighbors.end()));
			}

			first_neighbor.push_back(neighbors.size());

			std::vector<size_t> best_is(initialize_independent_set_min_deg(k, first_neighbor, neighbors));

			{
				std::vector<size_t> existing_independent_set;
				existing_independent_set.reserve(bound_updated.size());

				for (size_t i = 0; i < forbidden_subgraphs.size(); ++i)
				{
					if (std::find(bound_updated.get_bound().begin(), bound_updated.get_bound().end(), forbidden_subgraphs[i]) != bound_updated.get_bound().end())
					{
						existing_independent_set.push_back(i);
					}
				}

				if (existing_independent_set.size() > best_is.size())
				{
					best_is = std::move(existing_independent_set);
				}
			}

			if (false && best_is.size() <= k)
			{
				std::vector<size_t> is_pls(independent_set_pls(k, first_neighbor, neighbors));
				if (is_pls.size() >= best_is.size())
				{
					best_is = std::move(is_pls);
				}
			}

			for (size_t fsid : best_is)
			{
				bound_new.add(forbidden_subgraphs[fsid].begin(), forbidden_subgraphs[fsid].end());
			}

			bound_new.assert_valid(g, e);
		}
	};
}

#endif
