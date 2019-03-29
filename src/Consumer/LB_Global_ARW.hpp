#ifndef CONSUMER_LOWER_BOUND_GLOBAL_ARW_HPP
#define CONSUMER_LOWER_BOUND_GLOBAL_ARW_HPP

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
	class Global_ARW : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Global_ARW";
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
		Global_ARW(VertexID graph_size) : subgraphs_per_edge(graph_size), sum_subgraphs_per_edge(0), bound_calculated(false), used_updated(graph_size), used_updated_initialized(false), initial_bound_empty(true) {;}

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
					if (num_pairs > 1)
					{
						// Remove fs from lower bound
						for (auto p : pairs)
						{
							used_updated.clear_edge(p.first, p.second);
						}

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
								}
							}

							// Set node pair to avoid getting the same candidates twice.
							// WARNING: this assumes all candidates are listed for all node pairs, i.e., clean_graph_structure has not been called!
							used_updated.set_edge(p.first, p.second);
						}

						// Remove fs again from lower bound
						for (auto p : pairs)
						{
							used_updated.clear_edge(p.first, p.second);
						}

						const bool random_switch = prob(gen) < 0.3;
						size_t min_candidate_neighbors = num_neighbors;
						size_t num_candidates_considered = 0;

						auto min_candidate = fs;
						size_t min_pairs = num_pairs;

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

								++num_candidates_considered;

								if (cand_pairs == 1 || (min_pairs > 1 && (
													  (!random_switch && cand_neighbors < min_candidate_neighbors) ||
													  (random_switch && prob(gen) < 1.0 / num_candidates_considered))))
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

		void clean_graph_structure(Graph const&g, Graph_Edits const &e)
		{
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
		}

		void prepare_result(size_t k, Graph const &g, Graph_Edits const &e)
		{
			if (bound_calculated) return;
			bound_calculated = true;

			if (bound_updated.size() <= k)
			{
				// Seems to make results worse
				//clean_graph_structure(g, e);
				find_lb_2_improvements(k, g, e);
			}
		}
	};
}

#endif
