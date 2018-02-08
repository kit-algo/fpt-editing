#ifndef GRAPH_MATRIX_HPP
#define GRAPH_MATRIX_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"

#include "Graph.hpp"

namespace Graph
{
	template<bool small>
	class Matrix
	{
	public:
		static constexpr char const *name = "Matrix";

	private:
		VertexID const n;
		size_t const row_length;
		std::vector<Packed> matrix;

	public:
		Matrix(VertexID n) : n(n), row_length(small? 1 : (n + Packed_Bits - 1) / Packed_Bits), matrix(n * row_length, 0)
		{
			if(small && n > Packed_Bits) {throw "Too many Vertices";}
		}

		VertexID size() const
		{
			return n;
		}

		size_t count_edges() const
		{
			size_t count = 0;
			for(auto const &elem: matrix)
			{
				count += __builtin_popcountll(elem);
			}
			return count / 2;
		}

		bool has_edge(VertexID u, VertexID v) const
		{
			assert(u != v);
			assert(u < n && v < n);

			if(small) {return matrix.data()[u] & (Packed(1) << v);}
			else {return (matrix.data() + row_length * u)[v / Packed_Bits] & (Packed(1) << (v % Packed_Bits));}
		}

		void set_edge(VertexID u, VertexID v)
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
				(matrix.data() + row_length * u)[v / Packed_Bits] |= Packed(1) << (v % Packed_Bits);
				(matrix.data() + row_length * v)[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
			}
		}

		void clear_edge(VertexID u, VertexID v)
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
				(matrix.data() + row_length * u)[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
				(matrix.data() + row_length * v)[u / Packed_Bits] &= ~(Packed(1) << (u % Packed_Bits));
			}
		}

		void toggle_edge(VertexID u, VertexID v)
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
				(matrix.data() + row_length * u)[v / Packed_Bits] ^= Packed(1) << (v % Packed_Bits);
				(matrix.data() + row_length * v)[u / Packed_Bits] ^= Packed(1) << (u % Packed_Bits);
			}
		}

		size_t degree(VertexID u) const
		{
			Packed const *urow = get_row(u);
			size_t deg = 0;
			for(size_t i = 0; i < get_row_length(); i++)
			{
				deg += __builtin_popcount(urow[i]);
			}
			return deg;
		}

		std::vector<VertexID> const get_neighbours(VertexID u) const
		{
			std::vector<VertexID> neighbours;
			Packed const *urow = get_row(u);

			for(size_t i = 0; i < get_row_length(); i++)
			{
				for(Packed ui = urow[i]; ui; ui &= ~(Packed(1) << __builtin_ctzll(ui)))
				{
					VertexID v = __builtin_ctzll(ui) + i * Packed_Bits;
					neighbours.push_back(v);
				}
			}
			return neighbours;
		}

		Packed const *get_row(VertexID u) const
		{
			return matrix.data() + row_length * u;
		}

		static size_t get_row_length(VertexID graph_size)
		{
			if(small && graph_size > Packed_Bits) {throw "Too many Vertices";}
			return small? 1 : (graph_size + Packed_Bits - 1) / Packed_Bits;
		}

		size_t get_row_length() const
		{
			return small? 1 : row_length;
		}

		static std::vector<Packed> alloc_rows(VertexID graph_size, size_t rows)
		{
			return std::vector<Packed>(rows * get_row_length(graph_size), 0);
		}

		std::vector<Packed> alloc_rows(size_t rows) const
		{
			return std::vector<Packed>(rows * row_length, 0);
		}

		bool verify() const
		{
			bool valid = true;
			for(VertexID u = 0; u < n; u++)
			{
				Packed const *urow = get_row(u);

				for(size_t i = 0; i < get_row_length(); i++)
				{
					for(Packed ui = urow[i]; ui; ui &= ~(Packed(1) << __builtin_ctzll(ui)))
					{
						VertexID v = __builtin_ctzll(ui) + i * Packed_Bits;
						if(!(get_row(v)[u / Packed_Bits] & (Packed(1) << (u % Packed_Bits))))
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

		void clear()
		{
			matrix = decltype(matrix)(matrix.size(), 0);
		}
	};
}

#endif // GRAPH_MATRIX_HPP
