#ifndef GRAPH_VALUE_MATRIX_HPP
#define GRAPH_VALUE_MATRIX_HPP

#include <assert.h>

#include <stdexcept>
#include <vector>

#include "../config.hpp"

#include "Graph.hpp"

template<typename T>
class Value_Matrix
{
public:
	using value_type = T;
	static constexpr char const *name = "Value_Matrix";

private:
	VertexID n;
	std::vector<value_type> matrix;

public:
	Value_Matrix(VertexID n) : n(n), matrix(n*(n-1)/2)
	{}

	VertexID size() const
	{
		return n;
	}

	value_type& at(VertexID u, VertexID v)
	{
		if (u > v) std::swap(u, v);
		return matrix[v * (v - 1) / 2 + u];
	}

	const value_type& at(VertexID u, VertexID v) const
	{
		if (u > v) std::swap(u, v);
		return matrix[v * (v - 1) / 2 + u];
	}

	template<typename F>
	void forAllNodePairs(F f) {
		VertexID u = 0, v = 1;
		for (size_t i = 0; i < matrix.size(); ++i) {
			assert(v * static_cast<size_t>(v - 1) / 2 + u == i);
			f(u, v, matrix[i]);
			++u;
			if (u == v)
			{
				u = 0;
				++v;
			}
		}
	}


};

#endif
