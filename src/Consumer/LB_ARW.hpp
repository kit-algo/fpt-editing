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

	private:
		Value_Matrix<size_t> num_subgraphs_per_edge;
		size_t num_subgraphs;
		size_t sum_subgraphs_per_edge;


		Lower_Bound_Storage_type current_bound;
		bool bound_calculated;
		Graph_Edits bound_uses;

		Finder_impl finder;

		std::vector<size_t> counter_before_mark;
		std::vector<Lower_Bound_Storage_type> lb_before_mark_and_edit;
	public:
		ARW(VertexID graph_size) : num_subgraphs_per_edge(graph_size), num_subgraphs(0), sum_subgraphs_per_edge(0), bound_calculated(false), bound_uses(graph_size), finder(graph_size) {;}

		void initialize(size_t k, Graph const &graph, Graph_Edits const &edited)
		{
			current_bound.clear();
			bound_uses.clear();
			counter_before_mark.clear();
			lb_before_mark_and_edit.clear();
			sum_subgraphs_per_edge = 0;
			num_subgraphs = 0;
			num_subgraphs_per_edge.forAllNodePairs([&](VertexID, VertexID, size_t& v) { v = 0; });

			finder.find(graph, [&](const subgraph_t& path)
			{
				bool touches_bound = false;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					sum_subgraphs_per_edge++;
					++num_subgraphs_per_edge.at(*uit, *vit);

					touches_bound |= bound_uses.has_edge(*uit, *vit);
					return false;
				});

				++num_subgraphs;

				if (!touches_bound)
				{
					current_bound.add(path);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						bound_uses.set_edge(*uit, *vit);
						return false;
					});
				}

				// Assumption: if the bound is too high, initialize will be called again anyway.
				return current_bound.size() > k;
			});

			bound_calculated = current_bound.size() > k;
		}

		void before_mark_and_edit(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			lb_before_mark_and_edit.push_back(current_bound);
			lb_before_mark_and_edit.push_back(current_bound);
			counter_before_mark.push_back(num_subgraphs_per_edge.at(u, v));

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit)
				{
					--sum_subgraphs_per_edge;
					--num_subgraphs_per_edge.at(*uit, *vit);
					return false;
				});

				--num_subgraphs;

				return false;
			});

			std::vector<subgraph_t>& lb = current_bound.get_bound();

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
						bound_uses.clear_edge(*x, *y);

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

		void after_mark_and_edit(size_t /*k*/, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				bool touches_bound = false;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					sum_subgraphs_per_edge++;
					++num_subgraphs_per_edge.at(*uit, *vit);

					touches_bound |= bound_uses.has_edge(*uit, *vit);
					return false;
				});

				++num_subgraphs;

				if (!touches_bound)
				{
					current_bound.add(path);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						bound_uses.set_edge(*uit, *vit);
						return false;
					});
				}

				return false;
			});

			bound_calculated = false;
		}

		void before_undo_edit(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit)
				{
					--sum_subgraphs_per_edge;
					--num_subgraphs_per_edge.at(*uit, *vit);
					return false;
				});

				--num_subgraphs;

				return false;
			});
		}

		void after_undo_edit(size_t /*k*/, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			current_bound = std::move(lb_before_mark_and_edit.back());
			lb_before_mark_and_edit.pop_back();
			bound_uses.clear();

			for (const auto fs : current_bound.get_bound())
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit)
				{
					bound_uses.set_edge(*uit, *vit);
					return false;
				});
			}


			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				bool touches_bound = false;

				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
					sum_subgraphs_per_edge++;
					++num_subgraphs_per_edge.at(*uit, *vit);

					touches_bound |= bound_uses.has_edge(*uit, *vit);
					return false;
				});

				++num_subgraphs;

				if (!touches_bound)
				{
					current_bound.add(path);

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.begin(), path.end(), [&](auto uit, auto vit) {
						bound_uses.set_edge(*uit, *vit);
						return false;
					});
				}

				return false;
			});

			bound_calculated = false;
		}

		void before_undo_mark(Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
			// nothing to do
		}

		void after_undo_mark(size_t /*k*/, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			current_bound = std::move(lb_before_mark_and_edit.back());
			lb_before_mark_and_edit.pop_back();
			bound_uses.clear();

			for (const auto fs : current_bound.get_bound())
			{
				Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit)
				{
					bound_uses.set_edge(*uit, *vit);
					return false;
				});
			}

			sum_subgraphs_per_edge += counter_before_mark.back();
			num_subgraphs_per_edge.at(u, v) = counter_before_mark.back();
			counter_before_mark.pop_back();

			bound_calculated = false;
		}

		size_t result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound)
		{
			prepare_result(k, g, e);
			return current_bound.size();
		}

		const Lower_Bound_Storage_type& result(size_t k, Graph const &g, Graph_Edits const &e, Options::Tag::Lower_Bound_Update)
		{
			prepare_result(k, g, e);
			return current_bound;
		}

	private:
		void find_lb_2_improvements(size_t k, const Graph &g, const Graph_Edits &e)
		{
			std::vector<subgraph_t>& lb = current_bound.get_bound();

			std::vector<std::vector<subgraph_t>> candidates_per_pair(length * (length - 1) / 2);
			std::vector<std::pair<VertexID, VertexID>> pairs;

			std::mt19937_64 gen(42 * num_subgraphs + sum_subgraphs_per_edge);
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
						size_t nn = num_subgraphs_per_edge.at(*uit, *vit);
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
							bound_uses.clear_edge(p.first, p.second);
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

								candidates_per_pair[pi].push_back(sg);

								return false;
							};

							finder.find_near(g, p.first, p.second, cb, bound_uses);

							#ifndef NDEBUG

							{
								std::vector<subgraph_t> debug_subgraphs;

								auto debug_cb = [&](const subgraph_t& sg)
								{
									bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, sg.begin(), sg.end(), [&bound_uses = bound_uses](auto cuit, auto cvit)
									{
										return bound_uses.has_edge(*cuit, *cvit);
									});

									if (!touches_bound)
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
							bound_uses.set_edge(p.first, p.second);
						}

						// Remove fs again from lower bound
						for (auto p : pairs)
						{
							bound_uses.clear_edge(p.first, p.second);
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
									bound_uses.set_edge(*cuit, *cvit);
									size_t cn = num_subgraphs_per_edge.at(*cuit, *cvit);
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

									if (bound_uses.has_edge(partner_pair.first, partner_pair.second)) continue;

									for (auto partner_fs : candidates_per_pair[ppi])
									{
										bool touches_bound = Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, partner_fs.begin(), partner_fs.end(), [&bound_uses = bound_uses](auto cuit, auto cvit)
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

											Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, partner_fs.begin(), partner_fs.end(), [&bound_uses = bound_uses](auto cuit, auto cvit)
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

									current_bound.assert_valid(g, e);
									break;
								}
								else
								{
									Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, cand_fs.begin(), cand_fs.end(), [&bound_uses = bound_uses](auto cuit, auto cvit)
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
								Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(g, e, min_candidate.begin(), min_candidate.end(), [&bound_uses = bound_uses](auto cuit, auto cvit)
								{
									assert(!bound_uses.has_edge(*cuit, *cvit));
									bound_uses.set_edge(*cuit, *cvit);
									return false;
								});

								lb[fsi] = min_candidate;
								current_bound.assert_valid(g, e);

								bound_changed = true;
							}
							else
							{
								// Add fs back to lower bound
								for (auto p : pairs)
								{
									bound_uses.set_edge(p.first, p.second);
								}
							}
						}
						else if (k > 0 && k < current_bound.size())
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

		void prepare_result(size_t k, Graph const &g, Graph_Edits const &e)
		{
			if (bound_calculated) return;
			bound_calculated = true;

			if (current_bound.size() <= k)
			{
				find_lb_2_improvements(k, g, e);
			}
		}
	};
}

#endif
