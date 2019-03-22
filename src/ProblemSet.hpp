#ifndef PROBLEM_SET_HPP
#define PROBLEM_SET_HPP
#include <vector>
#include "config.hpp"

class ProblemSet
{
public:
	struct VertexPair
	{
		VertexID first;
		VertexID second;
		/** if the lower bound shall be updated before considering the recursion. */
		bool updateLB;

		VertexPair(VertexID first, VertexID second, bool updateLB = false) : first(first), second(second), updateLB(updateLB) {}
		VertexPair(std::pair<VertexID, VertexID> node_pair, bool updateLB = false) : first(node_pair.first), second(node_pair.second), updateLB(updateLB) {}
	};
	std::vector<VertexPair> vertex_pairs;
	bool needs_no_edit_branch;
	bool found_solution;

	ProblemSet() : needs_no_edit_branch(false), found_solution(false) {}

	bool empty() const
	{
		return vertex_pairs.empty();
	}
};

#endif
