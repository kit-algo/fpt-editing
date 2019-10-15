#ifndef CONSUMER_LOWER_BOUND_GUROBI_HPP
#define CONSUMER_LOWER_BOUND_GUROBI_HPP

#include <vector>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <memory>
#include <gurobi_c++.h>

#include "../config.hpp"

#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../Finder/SubgraphStats.hpp"
#include "../LowerBound/Lower_Bound.hpp"
#include "../Graph/ValueMatrix.hpp"
#include "../util.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Gurobi : Options::Tag::Lower_Bound
	{
	public:
		static constexpr char const *name = "Gurobi";
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, length>;
		using Subgraph_Stats_type = ::Finder::Subgraph_Stats<Finder_impl, Graph, Graph_Edits, Mode, Restriction, Conversion, length>;
		using subgraph_t = typename Lower_Bound_Storage_type::subgraph_t;

		static constexpr bool needs_subgraph_stats = true;

		struct State {
		};
	private:

		Finder_impl finder;
		std::unique_ptr<GRBEnv> env;
		std::unique_ptr<GRBModel> model;
		Value_Matrix<GRBVar> variables;
		GRBConstr objective_constr;
		size_t initial_k;
		size_t objective_offset;
		bool shall_solve;
		std::vector<std::vector<GRBConstr>> constraint_stack;

		size_t solve() {
			model->optimize();
			if (model->get(GRB_IntAttr_Status) == GRB_INFEASIBLE) {
				return std::numeric_limits<size_t>::max();
			}
			assert(model->get(GRB_IntAttr_Status) == GRB_OPTIMAL);

			if (initial_k > 0) {
				return 0;
			} else {
				double found_objective = model->get(GRB_DoubleAttr_ObjVal);
				size_t result = std::ceil(found_objective);
				if (result - found_objective > 0.99) {
					std::cout << "found_objective: " << found_objective << " rounded result: " << result << std::endl;
					result = std::floor(found_objective);
				}
				return result;
			}
		}

		double get_constraint_value(const subgraph_t& fs, const Graph& graph, const Graph_Edits& edited) {
			auto get = [&](size_t i, size_t j) {
				if (edited.has_edge(i, j)) {
					return graph.has_edge(i, j) ? 1.0 : 0.0;
				}
				return variables.at(fs[i], fs[j]).get(GRB_DoubleAttr_X);
			};

			return 3.0 - get(0, 1) - get(1, 2) - get(2, 3) + get(0, 2) + get(1, 3);
		}

		GRBConstr add_constraint(const subgraph_t& fs) {
			GRBLinExpr expr = 3;
			expr -= variables.at(fs[0], fs[1]);
			expr -= variables.at(fs[1], fs[2]);
			expr -= variables.at(fs[2], fs[3]);
			expr += variables.at(fs[0], fs[2]);
			expr += variables.at(fs[1], fs[3]);

			return model->addConstr(expr, GRB_GREATER_EQUAL, 1);
		}

		void fix_pair(VertexID u, VertexID v, bool exists) {
			GRBVar var(variables.at(u, v));
			if (exists) {
				if (!shall_solve) {
					// round to prevent too frequent solving, not solving is not a problem.
					shall_solve = var.get(GRB_DoubleAttr_X) < 0.999;
				}
			    var.set(GRB_DoubleAttr_UB, 1.0);
			    var.set(GRB_DoubleAttr_LB, 1.0);
			} else {
				if (!shall_solve) {
					// round to prevent too frequent solving, not solving is not a problem.
					shall_solve = var.get(GRB_DoubleAttr_X) > 0.001;
				}
			    var.set(GRB_DoubleAttr_LB, 0.0);
			    var.set(GRB_DoubleAttr_UB, 0.0);
			}
		}

		void relax_pair(VertexID u, VertexID v) {
			GRBVar var(variables.at(u, v));
			var.set(GRB_DoubleAttr_LB, 0.0);
			var.set(GRB_DoubleAttr_UB, 1.0);
		}

		size_t add_forbidden_subgraphs(const Graph& graph, Finder_impl& finder) {
			size_t num_found = 0;

			finder.find(graph, [&](const subgraph_t& fs) {
				++num_found;
				add_constraint(fs);
				return false;
			});

			std::cout << "added " << num_found << " constraints" << std::endl;
			return num_found;
		}
	public:
		Gurobi(VertexID graph_size) : finder(graph_size), env(std::make_unique<GRBEnv>()), variables(graph_size), initial_k(0), shall_solve(true) {}

		State initialize(size_t, Graph &graph, Graph_Edits const &edited)
		{
			try {
				model = std::make_unique<GRBModel>(*env);
				model->set(GRB_IntParam_Threads,	1);
				model->getEnv().set(GRB_IntParam_OutputFlag, 1);
				model->getEnv().set(GRB_IntParam_LogToConsole, 0);
				GRBLinExpr objective = 0;
				objective_offset = 0;

				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					var = model->addVar(0.0, 1.0, 0.0, GRB_CONTINUOUS);
					if (graph.has_edge(u, v)) {
						objective -= var;
						++objective_offset;
					} else {
						objective += var;
					}
				});

				objective += objective_offset;
				model->setObjective(objective, GRB_MINIMIZE);

				add_forbidden_subgraphs(graph, finder);

				size_t num_constraints_added = 0;

				do {
					model->optimize();

					num_constraints_added = 0;
					variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
						const double val = var.get(GRB_DoubleAttr_X);
						bool has_edge = graph.has_edge(u, v);
						if ((has_edge && val < 0.999) || (!has_edge && val > 0.001)) {
							graph.toggle_edge(u, v);

							finder.find_near(graph, u, v, [&](const subgraph_t& path)
							{
								if (get_constraint_value(path, graph, edited) < 0.999) {
									++num_constraints_added;
									add_constraint(path);
									return true;
								}

								return false;
							});

							graph.toggle_edge(u, v);
						}
					});
					std::cout << "Added " << num_constraints_added << "  extended constraints" << std::endl;
				} while (num_constraints_added > 0);
			} catch (GRBException &e) {
				std::cout << e.getMessage() << std::endl;
				throw e;
			}

			return State{};
		}

		void set_initial_k(size_t k, Graph const &graph, Graph_Edits const&)
		{
			if (initial_k > 0) {
				objective_constr.set(GRB_DoubleAttr_RHS, static_cast<double>(k) - static_cast<double>(objective_offset));
			} else {
				GRBLinExpr expr = 0;
				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					if (graph.has_edge(u, v)) {
						expr -= var;
					} else {
						expr += var;
					}
				});

				objective_constr = model->addConstr(expr, GRB_LESS_EQUAL, static_cast<double>(k) - static_cast<double>(objective_offset));
			}
			initial_k = k;
			
			shall_solve = true;
		}

		void before_mark_and_edit(State& , Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark_and_edit(State&, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			fix_pair(u, v, graph.has_edge(u, v));

			if (false) {
				GRBColumn col = model->getCol(variables.at(u, v));
				for (size_t i = 0; i < col.size(); ++i) {
					GRBConstr constr = col.getConstr(i);
					if (!constr.sameAs(objective_constr)) {
						model->remove(constr);
					}
				}
			}

			constraint_stack.emplace_back();

			if (shall_solve) {
				if (solve() > 0) return;
				shall_solve = false;
			}

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				if (get_constraint_value(path, graph, edited) < 0.999) {
					constraint_stack.back().push_back(add_constraint(path));
					shall_solve = true;
				}

				return false;
			});
		}

		void after_undo_edit(State&, Graph const&graph, Graph_Edits const&, VertexID u, VertexID v)
		{
			fix_pair(u, v, graph.has_edge(u, v));

			for (GRBConstr cstr : constraint_stack.back()) {
				model->remove(cstr);
			}

			constraint_stack.pop_back();
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_unmark(Graph const&, Graph_Edits const&, VertexID u, VertexID v)
		{
			relax_pair(u, v);
		}

		size_t result(State&, const Subgraph_Stats_type&, size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound)
		{
			size_t result = 0;
			if (shall_solve) {
				result = solve();
				shall_solve = (result > 0);
			}

			return result;
		}

	};
}

#endif
