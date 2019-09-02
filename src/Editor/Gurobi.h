#ifndef EDITOR_GUROBI_HPP
#define EDITOR_GUROBI_HPP

#include <assert.h>

#include <functional>
#include <iostream>
#include <map>
#include <typeinfo>
#include <vector>
#include <memory>
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
		bool init_sparse = false;
		size_t all_lazy = 0;

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
				<< "\nUse sparse initialization: " << (init_sparse ? 1 : 0)
				<< "\nAll lazy constraints: " << all_lazy
				<< std::endl;
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

		class MyCallback : public GRBCallback {
		private:
			GurobiOptions& options;
			Finder &finder;
			Graph &graph;
			Graph input_graph;
			const Graph& heuristic_solution;
			GRBEnv env;
			GRBModel model;
			Value_Matrix<GRBVar> variables;
		public:
			MyCallback(Finder &finder, Graph &graph, const Graph& heuristic_solution, GurobiOptions& options) : options(options), finder(finder), graph(graph), input_graph(graph), heuristic_solution(heuristic_solution), model(env), variables(graph.size()) {
			}

			void initialize() {
				model.set(GRB_IntParam_Threads,	options.n_threads);
				GRBLinExpr objective = 0;

				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					var = model.addVar(0.0, 1.0, 0.0, GRB_BINARY);
					if (graph.has_edge(u, v)) {
						objective -= var;
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

				model.setObjective(objective, GRB_MINIMIZE);

			}

			void solve() {
				if (options.variant.find("basic") == 0)
					solve_basic();
				else if (options.variant == "iteratively")
					solve_iteratively();
				else if (options.variant == "full")
					solve_full();
				else
					throw std::runtime_error("Unknown constraint generation variant " + options.variant);
			}

			void solve_basic() {
				model.set(GRB_IntParam_LazyConstraints,	1);
				add_forbidden_subgraphs(false);
				model.setCallback(this);
				model.optimize();
				assert(model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);
				update_graph();
			}

			void solve_iteratively() {
				while (add_forbidden_subgraphs(false) > 0) {
				    model.optimize();
				    assert(model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);
				    update_graph();
				}
			}

			void solve_full() {
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
					}
				}

				model.optimize();
				assert(model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);
				update_graph();
			}

			void add_constraint(const subgraph_t& fs, bool lazy) {
				GRBLinExpr expr = 3;
				forNodePairs(fs, [&](VertexID u, VertexID v, bool edge) {
					if (edge) {
						expr -= variables.at(u, v);
					} else {
						expr += variables.at(u, v);
					}
					return false;
				});

				if (lazy) {
					addLazy(expr >= 1);
				} else {
					GRBConstr constr = model.addConstr(expr >= 1);

					if (options.all_lazy > 0) {
						constr.set(GRB_IntAttr_Lazy, options.all_lazy);
					}
				}
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

				return 3 - get(0, 1) - get(1, 2) - get(2, 3) + get(0, 2) + get(1, 3);
			}

			size_t add_forbidden_subgraphs(bool lazy = false) {
				size_t num_found = 0;
				if ((lazy || options.init_sparse) && options.add_single_constraints) {
					Finder_Linear linear_finder;
					subgraph_t certificate;
					if (!linear_finder.is_quasi_threshold(graph, certificate)) {
						++num_found;
						assert(graph.has_edge(certificate[0], certificate[1]));
						assert(graph.has_edge(certificate[1], certificate[2]));
						assert(graph.has_edge(certificate[2], certificate[3]));
						assert(!graph.has_edge(certificate[0], certificate[2]));
						assert(!graph.has_edge(certificate[1], certificate[3]));
						add_constraint(certificate, lazy);
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
							++num_found;
							add_constraint(fs, lazy);
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
								++num_found;
								add_constraint(fs, lazy);
								return false;
							});

							graph.toggle_edge(u, v);
						}
					});
				} else {
					finder.find(graph, [&](const subgraph_t& fs) {
						++num_found;
						add_constraint(fs, lazy);
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
				double least_constraint_value = 0.99999;
				subgraph_t best_fs;

				finder.find(graph, [&](const subgraph_t& fs) {
					double v = get_relaxed_constraint_value(fs);
					if (v < least_constraint_value) {
						least_constraint_value = v;
						++num_found;
						best_fs = fs;
					}
					return false;
				});

				if (num_found > 0) {
					add_constraint(best_fs, true);
					std::cout << "Found " << num_found << " lazy constraints in relaxation, added best with constraint value " << least_constraint_value << std::endl;
				}

				return num_found;
			}


			void update_graph() {
				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					if (var.get(GRB_DoubleAttr_X) > 0.5) {
						graph.set_edge(u, v);
					} else {
						graph.clear_edge(u, v);
					}
				});
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
						update_graph_from_relaxation();
						if (add_forbidden_subgraphs_in_relaxation() > 0) return;
						update_graph_from_relaxation_inverse_to_input();
						if (add_forbidden_subgraphs_in_relaxation() > 0) return;
					}
				}
			}
		};

	public:
		Gurobi(Finder &finder, Graph &graph) : finder(finder), graph(graph), solved_optimally(false)
		{
		}

		bool is_optimal() const { return solved_optimally; }

		void solve(const Graph& heuristic_solution, GurobiOptions& options) {		//TODO consolidate Gurobi and MyCallback
			MyCallback cb(finder, graph, heuristic_solution, options);
			cb.initialize();
			cb.solve();
			solved_optimally = true;
		}
	};
}

#endif
