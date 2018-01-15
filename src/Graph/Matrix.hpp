#ifndef GRAPH_MATRIX_HPP
#define GRAPH_MATRIX_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"

#include "Graph.hpp"

namespace Graph
{
	template<bool _small = true>
	class Matrix : public Graph
	{
	public:
		static constexpr size_t bits = 8 * sizeof(Packed);
		static constexpr bool small = _small;
		static constexpr char const *name = "Matrix";

	private:
		VertexID const n;
		size_t const row_length;
		std::vector<Packed> matrix;

	public:
		Matrix(VertexID n) : n(n), row_length((n + bits - 1) / bits), matrix(n * row_length, 0)
		{
			assert(!small || n <= bits);
			assert(!small || row_length == 1);
		}

		virtual VertexID size() const
		{
			return n;
		}

		virtual size_t count_edges() const
		{
			size_t count = 0;
			for(auto const &elem: matrix)
			{
				count += __builtin_popcountll(elem);
			}
			return count / 2;
		}

		virtual bool has_edge(VertexID u, VertexID v) const
		{
			assert(u != v);
			assert(u < n && v < n);

			if(small) {return matrix.data()[u] & (Packed(1) << v);}
			else {return (matrix.data() + row_length * u)[v / bits] & (Packed(1) << (v % bits));}
		}

		virtual void set_edge(VertexID u, VertexID v)
		{
			assert(u != v);
			assert(u < n && v < n);

			if(small)
			{
				matrix.data()[u] |= (Packed(1) << v);
				matrix.data()[v] |= (Packed(1) << u);
			}
			else
			{
				(matrix.data() + row_length * u)[v / bits] |= Packed(1) << (v % bits);
				(matrix.data() + row_length * v)[u / bits] |= Packed(1) << (u % bits);
			}
		}

		virtual void clear_edge(VertexID u, VertexID v)
		{
			assert(u != v);
			assert(u < n && v < n);

			if(small)
			{
				matrix.data()[u] &= ~(Packed(1) << v);
				matrix.data()[v] &= ~(Packed(1) << u);
			}
			else
			{
				(matrix.data() + row_length * u)[v / bits] &= ~(Packed(1) << (v % bits));
				(matrix.data() + row_length * v)[u / bits] &= ~(Packed(1) << (u % bits));
			}
		}

		virtual void toggle_edge(VertexID u, VertexID v)
		{
			assert(u != v);
			assert(u < n && v < n);

			if(small)
			{
				matrix.data()[u] ^= (Packed(1) << v);
				matrix.data()[v] ^= (Packed(1) << u);
			}
			else
			{
				(matrix.data() + row_length * u)[v / bits] ^= Packed(1) << (v % bits);
				(matrix.data() + row_length * v)[u / bits] ^= Packed(1) << (u % bits);
			}
		}

		virtual std::vector<VertexID> const get_neighbours(VertexID u) const
		{
			std::vector<VertexID> neighbours;
			Packed const *urow = get_row(u);

			for(size_t i = 0; i < get_row_length(); i++)
			{
				for(Packed ui = urow[i]; ui; ui &= ~(Packed(1) << __builtin_ctzll(ui)))
				{
					VertexID v = __builtin_ctzll(ui) + i * bits;
					neighbours.push_back(v);
				}
			}
			return neighbours;
		}

		virtual Packed const *get_row(VertexID u) const
		{
			return matrix.data() + row_length * u;
		}

		virtual size_t get_row_length() const
		{
			return small? 1 : row_length;
		}

		virtual std::vector<Packed> alloc_rows(size_t i) const
		{
			return std::vector<Packed>(i * row_length, 0);
		}

		virtual bool verify() const
		{
			bool valid = true;
			for(VertexID u = 0; u < n; u++)
			{
				Packed const *urow = get_row(u);

				for(size_t i = 0; i < get_row_length(); i++)
				{
					for(Packed ui = urow[i]; ui; ui &= ~(Packed(1) << __builtin_ctzll(ui)))
					{
						VertexID v = __builtin_ctzll(ui) + i * bits;
						if(!(get_row(v)[u / bits] & (Packed(1) << (u % bits))))
						{
							std::cerr << "[matrix] " << +u << " -> " << +v << " but not " << +v << " -> " << +u << "\n";
							valid = false;
						}
					}
				}
			}
			if(!valid) {abort();}
			return valid;
		}
	};
}

#endif // GRAPH_MATRIX_HPP
