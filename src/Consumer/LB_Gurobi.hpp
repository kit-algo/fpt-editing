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
#include "../ProblemSet.hpp"

namespace Consumer
{
	template<typename Finder_impl, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Gurobi : Options::Tag::Lower_Bound, Options::Tag::Selector
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
		size_t initial_k;
		size_t objective_offset;
		bool shall_solve;
		std::vector<std::vector<GRBConstr>> constraint_stack;

		size_t solve() {
			model->optimize();
			if (model->get(GRB_IntAttr_Status) == GRB_INFEASIBLE) {
				return std::numeric_limits<size_t>::max();
			}

			if (model->get(GRB_IntAttr_Status) != GRB_OPTIMAL) {
				std::cerr << model->get(GRB_IntAttr_Status) << std::endl;
				throw std::runtime_error("model not optimal after optimization");
			}

			double found_objective = model->get(GRB_DoubleAttr_ObjVal);
			size_t result = std::ceil(found_objective);
			if (result - found_objective > 0.99) {
				//std::cerr << "found_objective: " << found_objective << " rounded result: " << result << std::endl;
				result = std::floor(found_objective);
			}

			return result;
		}

		double get_constraint_value(const subgraph_t& fs, const Graph& graph, const Graph_Edits& edited) {
			auto get = [&](size_t i, size_t j) {
				if (edited.has_edge(fs[i], fs[j])) {
					return graph.has_edge(fs[i], fs[j]) ? 1.0 : 0.0;
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
			// No need to set shall_solve here as either we pruned, then shall_solve is set anyway, or we cannot prune
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

			std::cerr << "added " << num_found << " constraints" << std::endl;
			return num_found;
		}
	public:
		Gurobi(VertexID graph_size) : finder(graph_size), env(std::make_unique<GRBEnv>()), variables(graph_size), initial_k(0), shall_solve(true) {}

		Gurobi(const Gurobi& o) : finder(o.finder), variables(o.variables) { throw std::runtime_error("Not implemented"); }

		Gurobi(Gurobi&& o) = default;

		State initialize(size_t, const Graph &graph, Graph_Edits const &/*edited*/)
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

				shall_solve = true;

/*
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
					std::cerr << "Added " << num_constraints_added << "  extended constraints" << std::endl;
				} while (num_constraints_added > 0);
*/
			} catch (GRBException &e) {
				std::cerr << e.getMessage() << std::endl;
				throw e;
			}

			return State{};
		}

		void set_initial_k(size_t k, Graph const &, Graph_Edits const&)
		{
			initial_k = k;
			shall_solve = true;
		}

		void before_mark_and_edit(State& , Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark_and_edit(State&, Graph &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			try {
				fix_pair(u, v, graph.has_edge(u, v));

				if (false) {
					GRBColumn col = model->getCol(variables.at(u, v));
					for (size_t i = 0; i < col.size(); ++i) {
						GRBConstr constr = col.getConstr(i);
						model->remove(constr);
					}
				}

				constraint_stack.emplace_back();

				if (shall_solve) {
					assert(initial_k > 0);
					if (solve() > initial_k) return;
					shall_solve = false;
				}

				finder.find_near(graph, u, v, [&](const subgraph_t& path)
				{
					constraint_stack.back().push_back(add_constraint(path));
					if (!shall_solve && get_constraint_value(path, graph, edited) < 0.999) {
						shall_solve = true;
					}

					return false;
				});
			} catch (GRBException &e) {
				std::cerr << e.getMessage() << std::endl;
				throw e;
			}

/*
			if (shall_solve) {
				if (solve() > initial_k) return;
				shall_solve = false;
			}

			variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
				const double val = var.get(GRB_DoubleAttr_X);
				bool has_edge = graph.has_edge(u, v);
				if ((has_edge && val < 0.999) || (!has_edge && val > 0.001)) {
					graph.toggle_edge(u, v);

					finder.find_near(graph, u, v, [&](const subgraph_t& path)
					{
						if (get_constraint_value(path, graph, edited) < 0.999) {
							constraint_stack.back().push_back(add_constraint(path));
							shall_solve = true;
						}

						return false;
					});

					graph.toggle_edge(u, v);
				}
			});
*/
		}

		void after_undo_edit(State&, Graph const&graph, Graph_Edits const&, VertexID u, VertexID v)
		{
			try {
				fix_pair(u, v, graph.has_edge(u, v));

				for (GRBConstr cstr : constraint_stack.back()) {
					model->remove(cstr);
				}

				constraint_stack.pop_back();
			} catch (GRBException &e) {
				std::cerr << e.getMessage() << std::endl;
				throw e;
			}
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_unmark(Graph const&, Graph_Edits const&, VertexID u, VertexID v)
		{
			try {
				relax_pair(u, v);
			} catch (GRBException &e) {
				std::cerr << e.getMessage() << std::endl;
				throw e;
			}
		}

		size_t result(State&, const Subgraph_Stats_type&, size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound)
		{
			try {
				if (shall_solve) {
					size_t result = solve();
					if (result > initial_k) {
						return result;
					}
					shall_solve = false;
				}
			} catch (GRBException &e) {
				std::cerr << e.getMessage() << std::endl;
				throw e;
			}

			return 0;
		}
	private:
		struct forbidden_count
		{
			std::pair<VertexID, VertexID> node_pair;
			double integrality_gap;
			double graph_diff;
			size_t num_forbidden;

			forbidden_count(std::pair<VertexID, VertexID> pair, double integrality_gap, double graph_diff, size_t num_forbidden) : node_pair(pair), integrality_gap(integrality_gap), graph_diff(graph_diff),  num_forbidden(num_forbidden) {}

			bool operator<(const forbidden_count& other) const
			{
				return std::tie(this->integrality_gap, this->graph_diff, this->num_forbidden) > std::tie(other.integrality_gap, other.graph_diff, other.num_forbidden);
			}

			operator std::pair<VertexID, VertexID> () const
			{
				return node_pair;
			}
		};

	public:
		ProblemSet result(State&, const Subgraph_Stats_type& subgraph_stats, size_t k, Graph const &graph, Graph_Edits const &edited, Options::Tag::Selector)
		{
			ProblemSet problem;
			problem.found_solution = (subgraph_stats.num_subgraphs == 0);
			problem.needs_no_edit_branch = false;
			if (!problem.found_solution && k > 0 && !shall_solve) {
				assert(model->get(GRB_IntAttr_Status) == GRB_OPTIMAL);

				double max_integrality_gap = -1;
				VertexID uu = 0, vv = 0;
				size_t max_subgraphs = 0;
				double max_diff = -1;
				variables.forAllNodePairs([&](VertexID u, VertexID v, const GRBVar& var) {
					double integrality_gap = 0.5 - std::abs(0.5 - var.get(GRB_DoubleAttr_X));
					const double diff = std::abs((graph.has_edge(u, v) ? 1.0 : 0.0) - var.get(GRB_DoubleAttr_X));
					const size_t subgraphs = subgraph_stats.num_subgraphs_per_edge.at(u, v);
					if (subgraphs == 0) return false;
					if (integrality_gap < 0.0001) integrality_gap = 0.0;
					if (std::tie(integrality_gap, diff, subgraphs) > std::tie(max_integrality_gap, max_diff, max_subgraphs)) {
						uu = u;
						vv = v;
						max_integrality_gap = integrality_gap;
						max_subgraphs = subgraphs;
						max_diff = diff;
					}
					return false;
				});

				std::vector<forbidden_count> best_pairs, current_pairs;
				finder.find_near(graph, uu, vv, [&](const subgraph_t& fs) {
					current_pairs.clear();

					Finder::for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, fs.begin(), fs.end(), [&](auto uit, auto vit) {
						VertexID u = *uit;
						VertexID v = *vit;

						double val = variables.at(u, v).get(GRB_DoubleAttr_X);
						if (val < 0.0001) val = 0;
						else if (val > 0.9999) val = 1.0;
						else if (std::abs(val - 0.5) < 0.0001) val = 0.5;
						current_pairs.emplace_back(std::make_pair(u, v), 0.5 - std::abs(0.5 - val), std::abs((graph.has_edge(u, v) ? 1.0 : 0.0) - val), subgraph_stats.num_subgraphs_per_edge.at(u, v));
						return false;
					});

					assert(current_pairs.size() > 0);

					std::sort(current_pairs.begin(), current_pairs.end());

					if (best_pairs.empty()) {
						best_pairs = current_pairs;
					} else {
						size_t bi = 0, ci = 0;

						while (bi < best_pairs.size() && ci < current_pairs.size() && (!(best_pairs[bi] < current_pairs[ci]) && !(current_pairs[ci] < best_pairs[bi])))
						{
							++bi;
							++ci;
						}

						if (ci == current_pairs.size() || (bi != best_pairs.size() && current_pairs[ci] < best_pairs[ci]))
						{
							best_pairs = current_pairs;
						}
					}

					return false;
				});

				assert(best_pairs.size() > 0);

				for (size_t i = 0; i < best_pairs.size(); ++i)
				{
					const forbidden_count& pair_count = best_pairs[i];
					problem.vertex_pairs.emplace_back(pair_count.node_pair, (i > 0 && i + 1 < best_pairs.size() && best_pairs[i-1].num_forbidden > 1));
				}
			}

			return problem;
		}
	};
}

#endif
