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
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../ProblemSet.hpp"
#include "../Finder/SubgraphStats.hpp"

namespace Editor
{
	template<typename Finder, typename Graph>
	class Gurobi
	{
	public:
		static constexpr char const *name = "Gurobi";
	private:
		Finder &finder;
		Graph &graph;

		using subgraph_t = std::array<VertexID, Finder::length>;
		bool solved_optimally;

		class MyCallback : public GRBCallback {
		private:
			Finder &finder;
			Graph &graph;
			GRBEnv env;
			GRBModel model;
			Value_Matrix<GRBVar> variables;
		public:
			MyCallback(Finder &finder, Graph &graph) : finder(finder), graph(graph), model(env), variables(graph.size()) {
			}

			void initialize() {
				model.set(GRB_IntParam_LazyConstraints,	1);
				model.setCallback(this);

				GRBLinExpr objective = 0;

				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					var = model.addVar(0.0, 1.0, 0.0, GRB_BINARY);
					if (graph.has_edge(u, v)) {
						objective -= var;
					} else {
						objective += var;
					}
				});

				model.setObjective(objective, GRB_MINIMIZE);

				add_forbidden_subgraphs(false);
			}

			void solve() {
				model.optimize();
				assert(model.get(GRB_InAttr_Status) == GRB_OPTIMAL);
				update_graph();
			}

			size_t add_forbidden_subgraphs(bool lazy = false) {
				size_t num_found = 0;
				finder.find(graph, [&](const subgraph_t& fs) {
					++num_found;
					GRBLinExpr expr;
					for (size_t i = 0; i < fs.size(); ++i) {
						for (size_t j = i + 1; j < fs.size(); ++j) {
							VertexID u = fs[i];
							VertexID v = fs[j];
							if (graph.has_edge(u, v)) {
								expr += 1 - variables.at(u, v);
							} else {
								expr += variables.at(u, v);
							}
						}
					}

					if (lazy) {
						addLazy(expr >= 1);
					} else {
						model.addConstr(expr >= 1);
					}
				});
				return num_found;
			}

			void update_graph() {
				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					if (var.get(GRB_DoubleAttr_X) > 0) {
						graph.set_edge(u, v);
					} else {
						graph.clear_edge(u, v);
					}
				});
			}

			void update_graph_in_callback() {
				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					if (getSolution(var) > 0) {
						graph.set_edge(u, v);
					} else {
						graph.clear_edge(u, v);
					}
				});
			}
		protected:
			void callback() {
				if (where == GRB_CB_MIPSOL) {
					update_graph_in_callback();
					add_forbidden_subgraphs(true);
				}
			}
		};

	public:
		Gurobi(Finder &finder, Graph &graph) : finder(finder), graph(graph), solved_optimally(false)
		{
		}

		bool is_optimal() const { return solved_optimally; }

		void solve() {
			MyCallback cb(finder, graph);
			cb.initialize();
			cb.solve();
			solved_optimally = true;
		}
	};
}

#endif
