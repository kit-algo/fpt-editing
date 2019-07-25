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
		size_t initial_k;

		size_t solve() {
			model->optimize();
			if (model->get(GRB_IntAttr_Status) == GRB_INFEASIBLE) {
				return std::numeric_limits<size_t>::max();
			}
			assert(model->get(GRB_IntAttr_Status) == GRB_OPTIMAL);
			double found_objective = model->get(GRB_DoubleAttr_ObjVal);
			size_t result = std::ceil(found_objective);
			if (result - found_objective > 0.99) {
				std::cout << "found_objective: " << found_objective << " rounded result: " << result << std::endl;
				result = std::floor(found_objective);
			}
			return result;
		}

		void add_constraint(const subgraph_t& fs) {
			GRBLinExpr expr = 3;
			expr -= variables.at(fs[0], fs[1]);
			expr -= variables.at(fs[1], fs[2]);
			expr -= variables.at(fs[2], fs[3]);
			expr += variables.at(fs[0], fs[2]);
			expr += variables.at(fs[1], fs[3]);

			model->addConstr(expr >= 1);
		}

		void fix_pair(VertexID u, VertexID v, bool exists) {
			GRBVar var(variables.at(u, v));
			if (exists) {
			    var.set(GRB_DoubleAttr_UB, 1.0);
			    var.set(GRB_DoubleAttr_LB, 1.0);
			} else {
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
		Gurobi(VertexID graph_size) : finder(graph_size), env(std::make_unique<GRBEnv>()), variables(graph_size), initial_k(0) {}

		State initialize(size_t, Graph const &graph, Graph_Edits const &)
		{
			try {
				model = std::make_unique<GRBModel>(*env);
				model->set(GRB_IntParam_Threads,	1);
				model->getEnv().set(GRB_IntParam_OutputFlag, 1);
				model->getEnv().set(GRB_IntParam_LogToConsole, 0);
				GRBLinExpr objective = 0;
				size_t objective_offset = 0;

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
			} catch (GRBException &e) {
				std::cout << e.getMessage() << std::endl;
				throw e;
			}

			return State{};
		}

		void before_mark_and_edit(State& , Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark_and_edit(State&, Graph const &graph, Graph_Edits const &, VertexID u, VertexID v)
		{
			fix_pair(u, v, graph.has_edge(u, v));

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				add_constraint(path);

				return false;
			});
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State&, Graph const &graph, Graph_Edits const &, VertexID u, VertexID v)
		{
			fix_pair(u, v, graph.has_edge(u, v));
		}

		size_t result(State&, const Subgraph_Stats_type&, size_t, Graph const &graph, Graph_Edits const &edited, Options::Tag::Lower_Bound)
		{
			GRBLinExpr objective = 0;
			size_t objective_offset = 0;

			variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
				if (edited.has_edge(u, v)) {
					fix_pair(u, v, graph.has_edge(u, v));
				} else {
					relax_pair(u, v);
				}
				if (graph.has_edge(u, v)) {
					objective -= var;
					++objective_offset;
				} else {
					objective += var;
				}
			});

			objective += objective_offset;
			model->setObjective(objective, GRB_MINIMIZE);

			return solve();
		}

	};
}

#endif
