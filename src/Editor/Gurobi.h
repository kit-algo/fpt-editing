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

namespace Editor
{
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
		const Graph &heuristic_solution;

		bool solved_optimally;

		class MyCallback : public GRBCallback {
		private:
			Finder &finder;
			Graph &graph;
			Graph input_graph;
			const Graph &heuristic_solution;
			GRBEnv env;
			GRBModel model;
			Value_Matrix<GRBVar> variables;
			bool add_single_constraints;
		public:
			MyCallback(Finder &finder, Graph &graph, const Graph& heuristic_solution, bool add_single_constraints) : finder(finder), graph(graph), input_graph(graph), heuristic_solution(heuristic_solution),  model(env), variables(graph.size()), add_single_constraints(add_single_constraints) {
			}

			void initialize() {
				model.set(GRB_IntParam_Threads,	1);
				GRBLinExpr objective = 0;

				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					var = model.addVar(0.0, 1.0, 0.0, GRB_BINARY);
					if (graph.has_edge(u, v)) {
						objective -= var;
					} else {
						objective += var;
					}

					if (heuristic_solution.has_edge(u, v)) {
						var.set(GRB_DoubleAttr_Start, 1);
					} else {
						var.set(GRB_DoubleAttr_Start, 0);
					}
				});

				model.setObjective(objective, GRB_MINIMIZE);

			}

			void solve() {
				add_forbidden_subgraphs(false);
				model.set(GRB_IntParam_LazyConstraints,	1);
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
				expr -= variables.at(fs[0], fs[1]);
				expr -= variables.at(fs[1], fs[2]);
				expr -= variables.at(fs[2], fs[3]);
				expr += variables.at(fs[0], fs[2]);
				expr += variables.at(fs[1], fs[3]);

				if (lazy) {
					addLazy(expr >= 1);
				} else {
					model.addConstr(expr >= 1);
				}
			}

			double get_relaxed_constraint_value(const subgraph_t& fs) {
				auto get = [&](size_t i, size_t j) {
					return getNodeRel(variables.at(fs[i], fs[j]));
				};

				return 3 - get(0, 1) - get(1, 2) - get(2, 3) + get(0, 2) + get(1, 3);
			}

			size_t add_forbidden_subgraphs(bool lazy = false) {
				size_t num_found = 0;
				if (add_single_constraints && lazy) {
					Finder_Linear linear_finder;
					subgraph_t certificate;
					if (!linear_finder.is_quasi_threshold(graph, certificate)) {
						++num_found;
						assert(graph.has_edge(certificate[0], certificate[1]));
						assert(graph.has_edge(certificate[1], certificate[2]));
						assert(graph.has_edge(certificate[2], certificate[3]));
						assert(!graph.has_edge(certificate[0], certificate[2]));
						assert(!graph.has_edge(certificate[1], certificate[3]));
						add_constraint(certificate, true);
					}
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
				} else if (where == GRB_CB_MIPNODE) {
					if (getIntInfo(GRB_CB_MIPNODE_STATUS) == GRB_OPTIMAL) {
						update_graph_from_relaxation();
						add_forbidden_subgraphs_in_relaxation();
						update_graph_from_relaxation_inverse_to_input();
						add_forbidden_subgraphs_in_relaxation();
					}
				}
			}
		};

	public:
		Gurobi(Finder &finder, Graph &graph, const Graph &heuristic_solution) : finder(finder), graph(graph), heuristic_solution(heuristic_solution), solved_optimally(false)
		{
		}

		bool is_optimal() const { return solved_optimally; }

		void solve(bool add_single_constraints) {
			MyCallback cb(finder, graph, heuristic_solution, add_single_constraints);
			cb.initialize();
			cb.solve();
			solved_optimally = true;
		}

		void solve_iteratively() {
			MyCallback cb(finder, graph, heuristic_solution, false);
			cb.initialize();
			cb.solve_iteratively();
			solved_optimally = true;
		}

		void solve_full() {
			MyCallback cb(finder, graph, heuristic_solution, false);
			cb.initialize();
			cb.solve_full();
			solved_optimally = true;
		}
	};
}

#endif
