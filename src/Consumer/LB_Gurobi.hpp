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
			GRBModel model;
			Value_Matrix<GRBVar> variables;
			State(const Graph& graph, Finder_impl &finder, GRBEnv& env) : model(env), variables(graph.size()) {
				model.set(GRB_IntParam_Threads,	1);
				GRBLinExpr objective = 0;
				size_t objective_offset = 0;

				variables.forAllNodePairs([&](VertexID u, VertexID v, GRBVar& var) {
					var = model.addVar(0.0, 1.0, 0.0, GRB_CONTINUOUS);
					if (graph.has_edge(u, v)) {
						objective -= var;
						++objective_offset;
					} else {
						objective += var;
					}
				});

				objective += objective_offset;
				model.setObjective(objective, GRB_MINIMIZE);

				add_forbidden_subgraphs(graph, finder);

				model.update();
			}

			size_t solve() {
				model.optimize();
				assert(model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);
				return std::ceil(model.get(GRB_DoubleAttr_ObjVal));
			}

			void add_constraint(const Graph& graph, const subgraph_t& fs) {
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
				model.addConstr(expr >= 1);
			}

			void fix_pair(VertexID u, VertexID v, bool exists) {
				GRBVar var(variables.at(u, v));
				if (exists) {
				    var.set(GRB_DoubleAttr_LB, 1.0);
				} else {
				    var.set(GRB_DoubleAttr_UB, 0.0);
				}
			}

			size_t add_forbidden_subgraphs(const Graph& graph, Finder_impl& finder) {
				size_t num_found = 0;

				finder.find(graph, [&](const subgraph_t& fs) {
					++num_found;
					add_constraint(graph, fs);
					return false;
				});

				std::cout << "added " << num_found << " constraints" << std::endl;
				return num_found;
			}
		};
	private:

		Finder_impl finder;
		GRBEnv env;
	public:
		Gurobi(VertexID graph_size) : finder(graph_size) {}

		State initialize(size_t, Graph const &graph, Graph_Edits const &)
		{
			try {
			    return State(graph, finder, env);
			} catch (GRBException &e) {
				std::cout << e.getMessage() << std::endl;
				throw e;
			}
		}

		void before_mark_and_edit(State& , Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark_and_edit(State& state, Graph const &graph, Graph_Edits const &, VertexID u, VertexID v)
		{
			state.fix_pair(u, v, graph.has_edge(u, v));

			finder.find_near(graph, u, v, [&](const subgraph_t& path)
			{
				state.add_constraint(graph, path);

				return false;
			});
			state.model.update();
		}

		void before_mark(State&, Graph const &, Graph_Edits const &, VertexID, VertexID)
		{
		}

		void after_mark(State& state, Graph const &graph, Graph_Edits const &, VertexID u, VertexID v)
		{
			state.fix_pair(u, v, graph.has_edge(u, v));
			state.model.update();
		}

		size_t result(State& state, const Subgraph_Stats_type&, size_t, Graph const &, Graph_Edits const &, Options::Tag::Lower_Bound)
		{
			return state.solve();
		}

	};
}

#endif
