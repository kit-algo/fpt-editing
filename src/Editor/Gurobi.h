#ifndef EDITOR_GUROBI_HPP
#define EDITOR_GUROBI_HPP

#include <assert.h>

#include <functional>
#include <iostream>
#include <map>
#include <typeinfo>
#include <vector>
#include <memory>
#include <chrono>
#include <gurobi_c++.h>

#include "../config.hpp"

#include "Editor.hpp"
#include "../Options.hpp"
#include "../Finder/Linear.hpp"
#include "../Graph/ValueMatrix.hpp"

namespace Editor
{

	struct GurobiOptions {
		std::string graph = "Not Read";								//specify as first argument
		std::string heuristic_solution = "Do not use";				//specifiy path with -h
		int n_threads = 1;											//specify with -t
		std::string variant = "basic-single";						//specify with -v Options are "basic", "basic-single" (for adding single constraints), "full", "iteratively"
		bool add_single_constraints = false;
		bool use_heuristic_solution = false;
		bool use_sparse_constraints = false;
		bool use_extended_constraints = false;
		bool add_constraints_in_relaxation = false;
		bool single_c4_constraints = false;
		bool init_sparse = false;
		size_t time_limit = 0;
		size_t all_lazy = 0;
		size_t permutation = 0;

		void print() {
			std::cout
				<< "Gurobi Editor options:"
				<< "\nGraph: " << graph
				<< "\nHeuristic Solution: " << heuristic_solution
				<< "\nThreads: " << n_threads
				<< "\nContraint Generation Variant: " << variant
				<< "\nSparse constraints: " << (use_sparse_constraints ? 1 : 0)
				<< "\nExtended constraints: " << (use_extended_constraints ? 1 : 0)
				<< "\nAdd constraints in relaxation: " << (add_constraints_in_relaxation ? 1 : 0)
				<< "\nAdd a single constraint per C4: " << (single_c4_constraints ? 1 : 0)
				<< "\nUse sparse initialization: " << (init_sparse ? 1 : 0)
				<< "\nAll lazy constraints: " << all_lazy
				<< "\nTime limit: " << time_limit
				<< "\nNode id permutation: " << permutation
				<< std::endl;
		}

		std::string get_name() {
			std::stringstream name;

			if (n_threads > 1) {
				name << "MT-";
			} else {
				name << "ST-";
			}

			name << variant;
			if (init_sparse) {
				name << "-init-sparse";
			}

			if (use_extended_constraints) {
				name << "-extended-constraints";
			}

			if (add_constraints_in_relaxation) {
				name << "-constraints-in-relaxation";
			}

			if (all_lazy > 0) {
				name << "-lazy-" << all_lazy;
			}

			if (use_heuristic_solution) {
				name << "-heuristic-init";
			}

			if (single_c4_constraints) {
				name << "-single-c4";
			}

			return name.str();
		}
	};

	template<typename Finder, typename Graph>
	class Gurobi
	{
	public:
		static constexpr char const *name = "Gurobi";
		using Finder_Linear = typename ::Finder::Linear<Graph>;
		using subgraph_t = std::array<VertexID, Finder::length>;
	private:
		Finder &finder;
		Graph &graph;
		bool solved_optimally;
		size_t bound;
		double elapsed_seconds;

		class MyCallback : public GRBCallback {
		private:
			using TimeT = std::chrono::steady_clock::time_point;
			TimeT start, end;
			GurobiOptions& options;
			Finder &finder;
			Graph &graph;
			Graph input_graph;
			const Graph& heuristic_solution;
			GRBEnv env;
			GRBModel model;
			Value_Matrix<GRBVar> variables;

			struct subgraph_hash {
				size_t graph_size;
				subgraph_hash(size_t graph_size) : graph_size(graph_size) {}
				size_t operator()(const subgraph_t& sg) const {
					size_t result =	0;
					for (VertexID v : sg) {
						result = result * graph_size + v;
					}
					return result;
				};
			};
			std::unordered_set<subgraph_t, subgraph_hash> added_constraints;
			std::unordered_set<subgraph_t, subgraph_hash> added_c4_constraints;

			bool update_timelimit_exceeded() {
				end = std::chrono::steady_clock::now();

				if (options.time_limit > 0) {
					double elapsed = get_elapsed_seconds();
					if (options.time_limit > elapsed) {
						model.set(GRB_DoubleParam_TimeLimit, options.time_limit - get_elapsed_seconds());
						return false;
					} else {
						return true;
					}
				}

				return false;
			}

		public:
			MyCallback(Finder &finder, Graph &graph, const Graph& heuristic_solution, GurobiOptions& options) : start(std::chrono::steady_clock::now()), options(options), finder(finder), graph(graph), input_graph(graph), heuristic_solution(heuristic_solution), model(env), variables(graph.size()), added_constraints(graph.size(), subgraph_hash(graph.size())), added_c4_constraints(graph.size(), subgraph_hash(graph.size())) {
				env.start();
			}

			void initialize() {
				start = std::chrono::steady_clock::now();

				model.set(GRB_IntParam_Threads,	options.n_threads);
				GRBLinExpr objective = 0;

				size_t num_edges = 0;

				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					var = model.addVar(0.0, 1.0, 0.0, GRB_BINARY);
					if (graph.has_edge(u, v)) {
						objective -= var;
						++num_edges;
					} else {
						objective += var;
					}

					if (options.use_heuristic_solution) {
						if (heuristic_solution.has_edge(u, v)) {
							var.set(GRB_DoubleAttr_Start, 1);
						} else {
							var.set(GRB_DoubleAttr_Start, 0);
						}
					}
				});

				objective += num_edges;

				model.setObjective(objective, GRB_MINIMIZE);

				update_timelimit_exceeded();
			}

			bool solve() {
				bool result = false;

				if (options.variant.find("basic") == 0)
					result = solve_basic();
				else if (options.variant == "iteratively")
					result = solve_iteratively();
				else if (options.variant == "full")
					result = solve_full();
				else
					throw std::runtime_error("Unknown constraint generation variant " + options.variant);

				end = std::chrono::steady_clock::now();

				return result;
			}

			double get_elapsed_seconds() const {
				std::chrono::duration<double> duration(end - start);
				return duration.count();
			}

			bool solve_basic() {
				model.set(GRB_IntParam_LazyConstraints,	1);
				add_forbidden_subgraphs(false);
				model.setCallback(this);

				if (update_timelimit_exceeded()) {
					update_graph();
					return false;
				}

				model.optimize();
				assert(options.time_limit > 0 || model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);
				update_graph();
				return model.get(GRB_IntAttr_Status) == GRB_OPTIMAL;
			}

			bool solve_iteratively() {
				bool is_optimal = false;

				while (add_forbidden_subgraphs(false) > 0) {
				    model.optimize();
				    assert(options.time_limit > 0 || model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);
				    update_graph();

				    is_optimal = model.get(GRB_IntAttr_Status) == GRB_OPTIMAL;

				    if (!is_optimal) break;

				    if (update_timelimit_exceeded()) {
					    break;
				    }
				}

				if (!is_optimal) {
					Finder_Linear linear_finder;
					subgraph_t certificate;
					if (!linear_finder.is_quasi_threshold(graph, certificate)) {
						if (options.use_heuristic_solution) {
							graph = heuristic_solution;
						} else {
							graph.clear();
						}
					}
				}

				return is_optimal;
			}

			size_t get_bound() {
				if (model.get(GRB_IntAttr_Status) == GRB_LOADED) {
					return 0;
				} else {
					return std::max<double>(0, model.get(GRB_DoubleAttr_ObjBound));
				}
			}

			bool solve_full() {
				subgraph_t fs;
				for (fs[0] = 0; fs[0] < graph.size(); ++fs[0]) {
					for (fs[1] = 0; fs[1] < graph.size(); ++fs[1]) {
						if (fs[0] == fs[1]) continue;
						for (fs[2] = 0; fs[2] < graph.size(); ++fs[2]) {
							if (fs[0] == fs[2] || fs[1] == fs[2]) continue;
							for (fs[3] = fs[0] + 1; fs[3] < graph.size(); ++fs[3]) {
								if (fs[1] == fs[3] || fs[2] == fs[3]) continue;

								// add constraint for subgraph fs[0] - fs[1] - fs[2] - fs[3]
								add_constraint(fs, false);
							}
						}
						if (update_timelimit_exceeded()) {
							update_graph();
							return false;
						}
					}
				}

				model.optimize();
				assert(options.time_limit > 0 || model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);
				update_graph();
				return model.get(GRB_IntAttr_Status) == GRB_OPTIMAL;
			}

			bool can_add(const subgraph_t& fs) {
				subgraph_t canonical_fs = fs;
				if (canonical_fs.front() > canonical_fs.back()) {
					std::reverse(canonical_fs.begin(), canonical_fs.end());
				}
				if (options.single_c4_constraints && graph.has_edge(fs.front(), fs.back())) {
					VertexID min_id = *std::min_element(fs.begin(), fs.end());
					if (canonical_fs.front() != min_id) return false;
					if (canonical_fs.back() > canonical_fs[1]) return false;

					return added_c4_constraints.find(canonical_fs) == added_c4_constraints.end();
				} else {
					return added_constraints.find(canonical_fs) == added_constraints.end();
				}
			}

			bool add_constraint(const subgraph_t& fs, bool lazy) {
				size_t constraint_value = 1;
				GRBLinExpr expr;
				subgraph_t canonical_fs = fs;
				if (canonical_fs.front() > canonical_fs.back()) {
					std::reverse(canonical_fs.begin(), canonical_fs.end());
				}
				if (options.single_c4_constraints && graph.has_edge(fs.front(), fs.back())) {
					VertexID min_id = *std::min_element(fs.begin(), fs.end());
					if (canonical_fs.front() != min_id) return false;
					if (canonical_fs.back() > canonical_fs[1]) return false;
					bool constraint_new = false;
					std::tie(std::ignore, constraint_new) = added_c4_constraints.insert(canonical_fs);
					if (!constraint_new) return false;

					expr = 4;
					forNodePairs(fs, [&](VertexID u, VertexID v, bool edge) {
						if (edge) {
							expr -= variables.at(u, v);
						} else {
							expr += variables.at(u, v) * 2;
						}
						return false;
					});
					expr -= variables.at(fs.front(), fs.back());
					constraint_value = 2;
				} else {
					bool constraint_new = false;
					std::tie(std::ignore, constraint_new) = added_constraints.insert(canonical_fs);
					if (!constraint_new) return false;

					expr = 3;
					forNodePairs(fs, [&](VertexID u, VertexID v, bool edge) {
						if (edge) {
							expr -= variables.at(u, v);
						} else {
							expr += variables.at(u, v);
						}
						return false;
					});
				}

				if (lazy) {
					addLazy(expr >= constraint_value);
				} else {
					GRBConstr constr = model.addConstr(expr >= constraint_value);

					if (options.all_lazy > 0) {
						constr.set(GRB_IntAttr_Lazy, options.all_lazy);
					}
				}

				return true;
			}

			template <typename F>
			bool forNodePairs(const subgraph_t& fs, F callback) {
				if (callback(fs[0], fs[1], true)) return true;
				if (callback(fs[1], fs[2], true)) return true;
				if (callback(fs[2], fs[3], true)) return true;
				if (callback(fs[0], fs[2], false)) return true;
				if (callback(fs[1], fs[3], false)) return true;
				return false;
			}

			double get_relaxed_constraint_value(const subgraph_t& fs) {
				auto get = [&](size_t i, size_t j) {
					return getNodeRel(variables.at(fs[i], fs[j]));
				};

				if (options.single_c4_constraints && graph.has_edge(fs.front(), fs.back())) {
					return (4.0 - get(0, 1) - get(1, 2) - get(2, 3) - get(0, 3) + 2.0 * get(0, 2) + 2.0 * get(1, 3))/2.0;
				}
				return 3 - get(0, 1) - get(1, 2) - get(2, 3) + get(0, 2) + get(1, 3);
			}

			size_t add_forbidden_subgraphs(bool lazy = false) {
				size_t num_found = 0;
				if ((lazy || options.init_sparse) && options.add_single_constraints) {
					Finder_Linear linear_finder;
					subgraph_t certificate;
					if (!linear_finder.is_quasi_threshold(graph, certificate)) {
						assert(graph.has_edge(certificate[0], certificate[1]));
						assert(graph.has_edge(certificate[1], certificate[2]));
						assert(graph.has_edge(certificate[2], certificate[3]));
						assert(!graph.has_edge(certificate[0], certificate[2]));
						assert(!graph.has_edge(certificate[1], certificate[3]));
						num_found += add_constraint(certificate, lazy);
					}
				} else if ((lazy || options.init_sparse) && options.use_sparse_constraints) {
					Graph used_pairs(graph.size());
					finder.find(graph, [&](const subgraph_t& fs) {

						bool not_covered = forNodePairs(fs, [&](VertexID u, VertexID v, bool) {
							return !used_pairs.has_edge(u, v);
						});

						if (not_covered) {
							forNodePairs(fs, [&](VertexID u, VertexID v, bool) {
								used_pairs.set_edge(u, v);
								return false;
							});
							num_found += add_constraint(fs, lazy);
						}
						return false;
					});
				} else if (!lazy && options.use_extended_constraints) {
					Graph used_pairs(graph.size());
					finder.find(graph, [&](const subgraph_t& fs) {
						++num_found;
						add_constraint(fs, lazy);
						forNodePairs(fs, [&](VertexID u, VertexID v, bool) {
							used_pairs.set_edge(u, v);
							return false;
						});
						return false;
					});

					variables.forAllNodePairs([&](VertexID u, VertexID v, const auto&) {
						if (used_pairs.has_edge(u, v)) {
							graph.toggle_edge(u, v);

							finder.find_near(graph, u, v, [&](const subgraph_t& fs) {
								num_found += add_constraint(fs, lazy);
								return false;
							});

							graph.toggle_edge(u, v);
						}
					});
				} else {
					finder.find(graph, [&](const subgraph_t& fs) {
						num_found += add_constraint(fs, lazy);
						return false;
					});
				}

				std::cout << "added " << num_found;
				if (lazy) std::cout << " lazy";
				std::cout << " constraints" << std::endl;
				return num_found;
			}

			size_t add_forbidden_subgraphs_in_relaxation() {
				size_t num_found = 0;
				constexpr double least_constraint_value = 0.999;

				finder.find(graph, [&](const subgraph_t& fs) {
					double v = get_relaxed_constraint_value(fs);
					if (v < least_constraint_value) {
						num_found += add_constraint(fs, true);
					}
					return false;
				});

				if (num_found > 0) {
					std::cout << "Found and added " << num_found << " lazy constraints in relaxation" << std::endl;
				}

				return num_found;
			}

			size_t add_forbidden_subgraphs_in_relaxation_near(VertexID u, VertexID v) {
				size_t num_found = 0;
				constexpr double least_constraint_value = 0.999;

				finder.find_near(graph, u, v, [&](const subgraph_t& fs) {
					double v = get_relaxed_constraint_value(fs);
					if (v < least_constraint_value) {
						num_found += add_constraint(fs, true);
					}
					return false;
				});

				if (num_found > 0) {
					std::cout << "Found and added " << num_found << " lazy constraints in relaxation near " << (size_t)u << ", " << (size_t)v << std::endl;
				}

				return num_found;
			}

			size_t add_forbidden_subgraphs_close_to_input() {
				graph = input_graph;
				constexpr double constraint_value_bound = 0.999;
				double least_constraint_value = constraint_value_bound;
				subgraph_t best_constraint;

				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					double val = getNodeRel(var);
					bool edge_changed = (input_graph.has_edge(u, v) && val < 0.99) || (!input_graph.has_edge(u, v) && val > 0.01);
					if (edge_changed) {
						graph.toggle_edge(u, v);

						finder.find_near(graph, u, v, [&](const subgraph_t& fs) {
							double v = get_relaxed_constraint_value(fs);
							if (v < least_constraint_value && can_add(fs)) {
								best_constraint = fs;
								least_constraint_value = v;
							}

							return false;
						});
						graph.toggle_edge(u, v);
					}
				});

				if (least_constraint_value < constraint_value_bound) {
					bool added = add_constraint(best_constraint, true);
					if (added) {
					std::cout << "Added close constraint of value " << least_constraint_value << std::endl;
					} else {
						std::cout << "Did not add duplicate constrint" << std::endl;
					}
					return 1;
				}
				return 0;
			}

			/**
			 * Updates the graph after the solver finished.
			 *
			 * If there is a solution from the solver, it
			 * is used.  If not, but a heuristic solution
			 * was specified, that solution is used.
			 * Otherwise, the graph is emptied.
			 */
			void update_graph() {
				if (model.get(GRB_IntAttr_SolCount) > 0) {
					variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
						if (var.get(GRB_DoubleAttr_X) > 0.5) {
							graph.set_edge(u, v);
						} else {
							graph.clear_edge(u, v);
						}
					});
				} else if (options.use_heuristic_solution) {
					graph = heuristic_solution;
				} else {
					graph.clear();
				}
			}

			void update_graph_in_callback() {
				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					if (getSolution(var) > 0.5) {
						graph.set_edge(u, v);
					} else {
						graph.clear_edge(u, v);
					}
				});
			}

			void update_graph_from_relaxation() {
				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					if (getNodeRel(var) > 0.5) {
						graph.set_edge(u, v);
					} else {
						graph.clear_edge(u, v);
					}
				});
			}

			void update_graph_from_relaxation_inverse_to_input() {
				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					double val = getNodeRel(var);

					if (input_graph.has_edge(u, v)) {
						if (val > 0.999) {
							graph.set_edge(u, v);
						} else {
							graph.clear_edge(u, v);
						}
					} else {
						if (val > 0.001) {
							graph.set_edge(u, v);
						} else {
							graph.clear_edge(u, v);
						}
					}
				});
			}
		protected:
			void callback() {
				if (where == GRB_CB_MIPSOL) {
					update_graph_in_callback();
					add_forbidden_subgraphs(true);
				} else if (where == GRB_CB_MIPNODE && options.add_constraints_in_relaxation) {
					if (getIntInfo(GRB_CB_MIPNODE_STATUS) == GRB_OPTIMAL) {
						if (options.init_sparse) {
							graph = input_graph;
							if (add_forbidden_subgraphs_in_relaxation() > 0) return;
						}
						add_forbidden_subgraphs_close_to_input();
						/*
						if (found_constraint) return;
						update_graph_from_relaxation();
						if (add_forbidden_subgraphs_in_relaxation() > 0) return;
						update_graph_from_relaxation_inverse_to_input();
						if (add_forbidden_subgraphs_in_relaxation() > 0) return;
						*/
					}
				}
			}
		};

	public:
		Gurobi(Finder &finder, Graph &graph) : finder(finder), graph(graph), solved_optimally(false), bound(std::numeric_limits<size_t>::max()), elapsed_seconds(0)
		{
		}

		bool is_optimal() const { return solved_optimally; }

		void solve(const Graph& heuristic_solution, GurobiOptions& options) {		//TODO consolidate Gurobi and MyCallback
			MyCallback cb(finder, graph, heuristic_solution, options);
			cb.initialize();
			solved_optimally = cb.solve();
			bound = cb.get_bound();
			elapsed_seconds = cb.get_elapsed_seconds();
		}

		size_t get_bound() const { return bound; }

		double get_elapsed_seconds() const { return elapsed_seconds; }
	};
}

#endif
