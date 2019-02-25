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
		bool used_updated_initialized;
		bool initial_bound_empty;

		Lower_Bound_Storage_type bound_updated;
	public:
		Min_Deg(VertexID graph_size) : subgraphs_per_edge(graph_size), sum_subgraphs_per_edge(0), bound_calculated(false), used_updated(graph_size), used_updated_initialized(false), initial_bound_empty(true) {;}

		void prepare(size_t, const Lower_Bound_Storage_type& lower_bound)
		{
			forbidden_subgraphs.clear();
			subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& v) { v.clear(); });

			bound_calculated = false;
			used_updated.clear();
			used_updated_initialized = false;
			bound_updated = lower_bound;
			initial_bound_empty = bound_updated.empty();
			sum_subgraphs_per_edge = 0;
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
				sum_subgraphs_per_edge++;

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
		Lower_Bound_Storage_type initialize_lb_min_deg(size_t k, const Graph& g, const Graph_Edits& e, const std::vector<bool>& can_use)
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
				if (!can_use[fsid]) continue;
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

		void find_lb_2_improvements(size_t k, const Graph &g, const Graph_Edits &e)
		{
			std::vector<typename Lower_Bound_Storage_type::subgraph_t>& lb = bound_updated.get_bound();

			std::vector<std::vector<size_t>> candidates_per_pair;
			std::vector<std::pair<VertexID, VertexID>> pairs;

			std::mt19937_64 gen(42 * forbidden_subgraphs.size() + sum_subgraphs_per_edge);
			std::uniform_real_distribution prob(.0, 1.0);

			bool improvement_found = false;
			bool bound_changed = false;
			size_t rounds_no_improvement = 0;
			do
			{
				improvement_found = false;
				bound_changed = false;

				std::shuffle(lb.begin(), lb.end(), gen);

				for (size_t fsi = 0; fsi < lb.size(); ++fsi)
				{
					auto fs = lb[fsi];
					candidates_per_pair.clear();
					pairs.clear();

					// See how many candidates we have
					size_t num_pairs = 0, num_neighbors = 0;
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit)
					{
						size_t nn = subgraphs_per_edge.at(*uit, *vit).size();
						num_neighbors += nn;

						pairs.emplace_back(*uit, *vit);

						if (nn > 1) ++num_pairs;

						return false;
					});

					// Skip if this forbidden subgraph if it is the only one we can add.
					if (pairs.size() > 1)
					{
						// Remove fs from lower bound
						for (auto p : pairs)
						{
							used_updated.clear_edge(p.first, p.second);
						}

						size_t min_candidate_neighbors = num_neighbors;
						auto min_candidate = fs;
						size_t min_pairs = num_pairs;

						size_t num_candidates = 0;

						// Collect candidates
						for (auto p : pairs)
						{
							candidates_per_pair.emplace_back();
							for (size_t cand : subgraphs_per_edge.at(p.first, p.second))
							{
								const auto cand_fs = forbidden_subgraphs[cand];
								if (cand_fs == fs) continue;

								bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, cand_fs.begin(), cand_fs.end(), [&used_updated = used_updated](auto cuit, auto cvit)
								{
									return used_updated.has_edge(*cuit, *cvit);
								});

								if (!touches_bound)
								{
									candidates_per_pair.back().push_back(cand);
									++num_candidates;
								}
							}
						}

						const double prob_switch = 1.0 / num_candidates;
						const double prob_noswitch = 0.5;

						bool found_partner = false;

						for (size_t pi = 0; pi < pairs.size(); ++pi)
						{
							for (size_t cand : candidates_per_pair[pi])
							{
								const auto cand_fs = forbidden_subgraphs[cand];

								assert(!found_partner);

								size_t cand_pairs = 0, cand_neighbors = 0;

								Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, cand_fs.begin(), cand_fs.end(), [&](auto cuit, auto cvit)
								{
									assert(!used_updated.has_edge(*cuit, *cvit));
									used_updated.set_edge(*cuit, *cvit);
									size_t cn = subgraphs_per_edge.at(*cuit, *cvit).size();
									cand_neighbors += cn;
									if (cn > 1) ++cand_pairs;
									return false;
								});

								if (cand_pairs == 1 || (min_pairs > 1 && ((cand_neighbors < min_candidate_neighbors && prob(gen) > prob_noswitch) || prob(gen) < prob_switch )))
								{
									min_pairs = cand_pairs;
									min_candidate = cand_fs;
									min_candidate_neighbors = cand_neighbors;
								}

								for (size_t pi = 0; pi < pairs.size(); ++pi)
								{
									const auto partner_pair = pairs[pi];

									if (used_updated.has_edge(partner_pair.first, partner_pair.second)) continue;

									for (size_t partner : candidates_per_pair[pi])
									{
										const auto partner_fs = forbidden_subgraphs[partner];
										bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, partner_fs.begin(), partner_fs.end(), [&used_updated = used_updated](auto cuit, auto cvit)
										{
											return used_updated.has_edge(*cuit, *cvit);
										});

										if (!touches_bound)
										{
											// Success!
											found_partner = true;
											improvement_found = true;

											// Directly add the partner to the lower bound, continue search to see if there is more than one partner
											lb.push_back(partner_fs);

											Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, partner_fs.begin(), partner_fs.end(), [&used_updated = used_updated](auto cuit, auto cvit)
											{
												used_updated.set_edge(*cuit, *cvit);
												return false;
											});
										}
									}

								}

								if (found_partner)
								{
									lb[fsi] = cand_fs;

									bound_updated.assert_valid(g, e);
									break;
								}
								else
								{
									Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, cand_fs.begin(), cand_fs.end(), [&used_updated = used_updated](auto cuit, auto cvit)
									{
										used_updated.clear_edge(*cuit, *cvit);
										return false;
									});
								}
							}

							if (found_partner) break;
						}

						if (!found_partner)
						{
							if (min_candidate != fs)
							{
								Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, min_candidate.begin(), min_candidate.end(), [&used_updated = used_updated](auto cuit, auto cvit)
								{
									assert(!used_updated.has_edge(*cuit, *cvit));
									used_updated.set_edge(*cuit, *cvit);
									return false;
								});

								lb[fsi] = min_candidate;
								bound_updated.assert_valid(g, e);

								bound_changed = true;
							}
							else
							{
								// Add fs back to lower bound
								for (auto p : pairs)
								{
									used_updated.set_edge(p.first, p.second);
								}
							}
						}
						else if (k > 0 && k < bound_updated.size())
						{
							return;
						}
					}
				}

				if (improvement_found)
				{
					rounds_no_improvement = 0;
				}
				else
				{
					++rounds_no_improvement;
				}
			} while (improvement_found || (rounds_no_improvement < 5 && bound_changed));

		}

		Lower_Bound_Storage_type independent_set_kamis(size_t k, const Graph &g, const Graph_Edits &e)
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

			std::string out_name = "/tmp/kamis.out.txt";

			std::string cmd = "../KaMIS/deploy/redumis --time_limit=25  " + fname.str() + " --console_log --output=" + out_name;

			std::system(cmd.c_str());

			Lower_Bound_Storage_type result;

			std::ifstream output(out_name);

			std::string line;
			for (size_t i = 0; i < n; ++i)
			{
				if (!std::getline(output, line)) { throw std::runtime_error("Error, premature output file end"); }
				size_t in_set = std::stoull(line);
				if (in_set == 1)
				{
					result.add(forbidden_subgraphs[i].begin(), forbidden_subgraphs[i].end());
				}
			}

			result.assert_valid(g, e);

			return result;
		}

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

			bool found_something = false;
			size_t num_removed = 0;
			Lower_Bound_Storage_type lb_offset;
			std::vector<bool> can_use(forbidden_subgraphs.size(), true);

			do
			{

				found_something = false;
				size_t num_cleared = 0;
				subgraphs_per_edge.forAllNodePairs([&](VertexID u, VertexID v, std::vector<size_t>& subgraphs) {
					if (subgraphs.empty()) return;

					auto fs = forbidden_subgraphs[subgraphs.front()];

					if (u > v) std::swap(u, v);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto xit, auto yit) {
						size_t x = *xit, y = *yit;
						if (x > y) std::swap(x, y);
						if (u == x && v == y) return false;

						std::vector<size_t> &inner_subgraphs = subgraphs_per_edge.at(x, y);
						if (subgraphs.size() <= inner_subgraphs.size() && std::includes(inner_subgraphs.begin(), inner_subgraphs.end(), subgraphs.begin(), subgraphs.end()))
						{
							subgraphs.clear();
							++num_cleared;
							return true;
						}

						return false;
					});
				});


				for (size_t i = 0; i < forbidden_subgraphs.size(); ++i)
				{
					if (!can_use[i]) continue;
					auto fs = forbidden_subgraphs[i];

					size_t num_pairs = 0;
					std::pair<VertexID, VertexID> single_pair;
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) {
						if (!subgraphs_per_edge.at(*uit, *vit).empty())
						{
							++num_pairs;
							single_pair = {*uit, *vit};
						}

						return false;
					});

					if (num_pairs == 1)
					{
						lb_offset.get_bound().push_back(fs);

						for (size_t fnid : subgraphs_per_edge.at(single_pair.first, single_pair.second))
						{
							num_removed += can_use[fnid];
							can_use[fnid] = false;
						}

						found_something = true;
					}
				}

				if (found_something)
				{
					subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, std::vector<size_t>& subgraphs) {
						subgraphs.erase(std::remove_if(subgraphs.begin(), subgraphs.end(), [&](size_t fsid) { return !can_use[fsid]; }), subgraphs.end());
					});
				}
			} while (found_something);

			//std::cout << "k = " << k << ", lb offset: " << lb_offset.size() << ", removed " << num_removed << " of " << forbidden_subgraphs.size() << std::endl;

			used_updated.clear();

			for (const auto fs : lb_offset.get_bound())
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit)
				{
					used_updated.set_edge(*uit, *vit);
					return false;
				});
			}

			for (const auto fs : bound_updated.get_bound())
			{
				if (!Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) { return used_updated.has_edge(*uit, *vit); }))
				{
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit)
					{
						used_updated.set_edge(*uit, *vit);
						return false;
					});

					lb_offset.get_bound().push_back(fs);
				}
			}

			bound_updated = lb_offset;

			for (size_t i = 0; i < forbidden_subgraphs.size(); ++i)
			{
				if (!can_use[i]) continue;
				auto fs = forbidden_subgraphs[i];

				if (!Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit) { return used_updated.has_edge(*uit, *vit); }))
				{
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit)
					{
						used_updated.set_edge(*uit, *vit);
						return false;
					});

					bound_updated.get_bound().push_back(fs);
				}
			}

			if (k == 0 || bound_updated.size() <= k)
			{
				find_lb_2_improvements(k, g, e);
			}

			if (false)
			{
				Lower_Bound_Storage_type bound_new = initialize_lb_min_deg(k, g, e, can_use);

				if (bound_updated.size() < bound_new.size())
				{
					bound_updated = bound_new;
				}
			}

			if (false) {
				Lower_Bound_Storage_type bound_kamis = independent_set_kamis(k, g, e);

				if (bound_updated.size() < bound_kamis.size())
				{
					bound_updated = bound_kamis;
				}
			}

		}
	};
}

#endif
