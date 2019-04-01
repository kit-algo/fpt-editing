#ifndef CONSUMER_LOWER_BOUND_ARW_HPP
#define CONSUMER_LOWER_BOUND_ARW_HPP

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
	class ARW : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "ARW";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;

		struct State {
			Lower_Bound_Storage_type lb;
			Graph_Edits bound_uses;
			Value_Matrix<size_t> num_subgraphs_per_edge;
			size_t num_subgraphs;
			size_t sum_subgraphs_per_edge;

			State(VertexID graph_size) : bound_uses(graph_size), num_subgraphs_per_edge(graph_size) {};
		};
	private:

		Graph_Edits candidate_pairs_used;
		Finder_impl finder;
	public:
		ARW(VertexID graph_size) : candidate_pairs_used(graph_size), finder(graph_size) {;}

		State initialize(size_t k, Graph const &graph, Graph_Edits const &edited)
		{
			State state(graph.size());

			finder.find(graph, [&](const subgraph_t& path)
			{
				bool touches_bound = false;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					state.sum_subgraphs_per_edge++;
					++state.num_subgraphs_per_edge.at(*uit, *vit);

					touches_bound |= state.bound_uses.has_edge(*uit, *vit);
					return false;
				});

				++state.num_subgraphs;

				if (!touches_bound)
				{
					state.lb.add(path);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						state.bound_uses.set_edge(*uit, *vit);
						return false;
					});
				}

				// Assumption: if the bound is too high, initialize will be called again anyway.
				return state.lb.size() > k;
			});

			return state;
		}

		void before_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit)
				{
					--state.sum_subgraphs_per_edge;
					--state.num_subgraphs_per_edge.at(*uit, *vit);
					return false;
				});

				--state.num_subgraphs;

				return false;
			});

			std::vector<subgraph_t>& lb = state.lb.get_bound();

			for (size_t i = 0; i < lb.size();)
			{
				bool has_uv = ::Finder::for_all_edges_unordered<Mode, Restriction, Conversion, Graph, Graph_Edits>(graph, edited, lb[i].begin(), lb[i].end(), [&](auto x, auto y)
				{
					return (u == *x && v == *y) || (u == *y && v == *x);
				});

				if (has_uv)
				{
					::Finder::for_all_edges_unordered<Mode, Restriction, Conversion, Graph, Graph_Edits>(graph, edited, lb[i].begin(), lb[i].end(), [&](auto x, auto y)
					{
						state.bound_uses.clear_edge(*x, *y);

						return false;
					});

					lb[i] = lb.back();
					lb.pop_back();
				}
				else
				{
					++i;
				}
			}
		}

		void after_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				bool touches_bound = false;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					state.sum_subgraphs_per_edge++;
					++state.num_subgraphs_per_edge.at(*uit, *vit);

					touches_bound |= state.bound_uses.has_edge(*uit, *vit);
					return false;
				});

				++state.num_subgraphs;

				if (!touches_bound)
				{
					state.lb.add(path);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						state.bound_uses.set_edge(*uit, *vit);
						return false;
					});
				}

				return false;
			});
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State& state, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			state.bound_uses.clear_edge(u, v);
			state.sum_subgraphs_per_edge -= state.num_subgraphs_per_edge.at(u, v);
			state.num_subgraphs_per_edge.at(u, v) = 0;

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					return state.bound_uses.has_edge(*uit, *vit);
				});

				if (!touches_bound)
				{
					state.lb.add(path);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						state.bound_uses.set_edge(*uit, *vit);
						return false;
					});
				}

				return false;
			}, state.bound_uses);
		}

		size_t result(State& state, size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound)
		{
			if (state.lb.size() <= k)
			{
				find_lb_2_improvements(state, k, g, e);
			}

			return state.lb.size();
		}

	private:
		void find_lb_2_improvements(State& state, size_t k, const Graph &g, const Graph_Edits &e)
		{
			std::vector<subgraph_t>& lb = state.lb.get_bound();

			std::vector<std::vector<subgraph_t>> candidates_per_pair(length * (length - 1) / 2);
			std::vector<std::pair<VertexID, VertexID>> pairs;

			std::mt19937_64 gen(42 * state.num_subgraphs + state.sum_subgraphs_per_edge);
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
					pairs.clear();

					// See how many candidates we have
					size_t num_pairs = 0, num_neighbors = 0;
					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, fs.begin(), fs.end(), [&](auto uit, auto vit)
					{
						size_t nn = state.num_subgraphs_per_edge.at(*uit, *vit);
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
							state.bound_uses.clear_edge(p.first, p.second);
							candidate_pairs_used.set_edge(p.first, p.second);
						}

						// Collect candidates
						for (size_t pi = 0; pi < pairs.size(); ++pi)
						{
							auto p = pairs[pi];

							candidates_per_pair[pi].clear();

							auto cb = [&](const subgraph_t &sg)
							{
								#ifndef NDEBUG
								bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, sg.begin(), sg.end(), [&bound_uses = bound_uses](auto cuit, auto cvit)
								{
									return bound_uses.has_edge(*cuit, *cvit);
								});

								assert(!touches_bound);
								#endif

								size_t pairs_covered = 0;

								Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, sg.begin(), sg.end(), [&](auto cuit, auto cvit)
								{
									pairs_covered += candidate_pairs_used.has_edge(*cuit, *cvit);

									return false;
								});

								if (pairs_covered < pairs.size())
								{
									candidates_per_pair[pi].push_back(sg);
								}

								return false;
							};

							finder.find_near(g, p.first, p.second, cb, state.bound_uses);

							#ifndef NDEBUG

							{
								std::vector<subgraph_t> debug_subgraphs;

								auto debug_cb = [&](const subgraph_t& sg)
								{
									size_t pairs_covered = 0;
									bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, sg.begin(), sg.end(), [&](auto cuit, auto cvit)
									{
										pairs_covered += candidate_pairs_used.has_edge(*cuit, *cvit);
										return bound_uses.has_edge(*cuit, *cvit);
									});

									if (!touches_bound && pairs_covered < pairs.size())
									{
										debug_subgraphs.push_back(sg);
									}

									return false;
								};

								finder.find_near(g, p.first, p.second, debug_cb);

								if (!debug_subgraphs.size() == candidates_per_pair[pi].size())
								{
									std::cout << "(" << static_cast<size_t>(p.first) << ", " << static_cast<size_t>(p.second) << ")" << std::endl;

									std::cout << "restricted (" << candidates_per_pair[pi].size() << "): " << std::endl;
									for (subgraph_t sg : candidates_per_pair[pi])
									{
										for (VertexID x : sg)
											std::cout << static_cast<size_t>(x) << ", ";
										std::cout << std::endl;
									}

									std::cout << "full (" << debug_subgraphs.size() << "): " << std::endl;
									for (subgraph_t sg : debug_subgraphs)
									{
										for (VertexID x : sg)
											std::cout << static_cast<size_t>(x) << ", ";
										std::cout << std::endl;
									}

									assert(false);

								}
							}

							#endif

							// Set node pair to avoid getting the same candidates twice.
							// WARNING: this assumes all candidates are listed for all node pairs, i.e., clean_graph_structure has not been called!
							state.bound_uses.set_edge(p.first, p.second);
						}

						// Remove fs again from lower bound
						for (auto p : pairs)
						{
							state.bound_uses.clear_edge(p.first, p.second);
							candidate_pairs_used.clear_edge(p.first, p.second);
						}

						const bool random_switch = prob(gen) < 0.3;
						size_t min_candidate_neighbors = num_neighbors;
						size_t num_candidates_considered = 0;

						auto min_candidate = fs;
						size_t min_pairs = num_pairs;

						bool found_partner = false;

						for (size_t pi = 0; pi < pairs.size(); ++pi)
						{
							for (auto cand_fs : candidates_per_pair[pi])
							{
								assert(!found_partner);

								size_t cand_pairs = 0, cand_neighbors = 0;

								Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, cand_fs.begin(), cand_fs.end(), [&](auto cuit, auto cvit)
								{
									assert(!bound_uses.has_edge(*cuit, *cvit));
									state.bound_uses.set_edge(*cuit, *cvit);
									size_t cn = state.num_subgraphs_per_edge.at(*cuit, *cvit);
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

								for (size_t ppi = 0; ppi < pairs.size(); ++ppi)
								{
									const auto partner_pair = pairs[ppi];

									if (state.bound_uses.has_edge(partner_pair.first, partner_pair.second)) continue;

									for (auto partner_fs : candidates_per_pair[ppi])
									{
										bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, partner_fs.begin(), partner_fs.end(), [&bound_uses = state.bound_uses](auto cuit, auto cvit)
										{
											return bound_uses.has_edge(*cuit, *cvit);
										});

										if (!touches_bound)
										{
											// Success!
											found_partner = true;
											improvement_found = true;

											// Directly add the partner to the lower bound, continue search to see if there is more than one partner
											lb.push_back(partner_fs);

											Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, partner_fs.begin(), partner_fs.end(), [&bound_uses = state.bound_uses](auto cuit, auto cvit)
											{
												bound_uses.set_edge(*cuit, *cvit);
												return false;
											});
										}
									}

								}

								if (found_partner)
								{
									lb[fsi] = cand_fs;

									state.lb.assert_valid(g, e);
									break;
								}
								else
								{
									Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, cand_fs.begin(), cand_fs.end(), [&bound_uses = state.bound_uses](auto cuit, auto cvit)
									{
										bound_uses.clear_edge(*cuit, *cvit);
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
								Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, min_candidate.begin(), min_candidate.end(), [&bound_uses = state.bound_uses](auto cuit, auto cvit)
								{
									assert(!bound_uses.has_edge(*cuit, *cvit));
									bound_uses.set_edge(*cuit, *cvit);
									return false;
								});

								lb[fsi] = min_candidate;
								state.lb.assert_valid(g, e);

								bound_changed = true;
							}
							else
							{
								// Add fs back to lower bound
								for (auto p : pairs)
								{
									state.bound_uses.set_edge(p.first, p.second);
								}
							}
						}
						else if (k < state.lb.size())
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

	};
}

#endif
