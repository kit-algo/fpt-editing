#ifndef FINDER_CENTER_EDITS_HPP
#define FINDER_CENTER_EDITS_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits, size_t length>
	class Center_Edits
	{
		static_assert(length > 3, "Can only detect path/cycles with at least 4 vertices");

	public:
		static constexpr char const *name = "Center_Edits";

	private:
		std::vector<Packed> forbidden;

	public:
		Center_Edits(VertexID graph_size) : forbidden(Graph::alloc_rows(graph_size, length / 2 - 1)) {;}

		template<typename Feeder>
		void find(Graph const &graph, Graph_Edits const &edited, Feeder &feeder)
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);

			for(VertexID u = 0; u < graph.size(); u++)
			{
				if(find_start<Feeder, false>(graph, edited, feeder, u)) {return;}
			}
		}

		template<typename Feeder>
		void find_near(Graph const &graph, Graph_Edits const &edited, VertexID uu, VertexID vv, Feeder &feeder)
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);
			/* only search for forbidden subgraphs near u-v
			 * -> both vertices of starting edge must be within length / 2 of u or v
			 */

			std::vector<Packed> start = graph.alloc_rows(3);//start vertices and neighbourhoods included
			Packed *area = start.data();
			Packed *included = start.data() + graph.get_row_length();
			Packed *adding = start.data() + 2 * graph.get_row_length();
			area[uu / Packed_Bits] |= Packed(1) << (uu % Packed_Bits);
			area[vv / Packed_Bits] |= Packed(1) << (vv % Packed_Bits);
			for(size_t rounds = 0; rounds < length / 2; rounds++)
			{
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(Packed cur = area[i] & ~included[i]; cur; cur &= ~(Packed(1) << __builtin_ctzll(cur)))
					{
						VertexID add = __builtin_ctzll(cur) + i * Packed_Bits;
						included[add / Packed_Bits] |= Packed(1) << (add % Packed_Bits);
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							adding[j] |= graph.get_row(add)[j];
						}
					}
				}
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					area[i] |= adding[i];
				}
			}

			for(size_t ii = 0; ii < graph.get_row_length(); ii++)
			{
				for(Packed cur = area[ii]; cur; cur &= ~(Packed(1) << __builtin_ctzll(cur)))
				{
					VertexID u = __builtin_ctzll(cur) + ii * Packed_Bits;
					if(find_start<Feeder, true>(graph, edited, feeder, u)) {return;}
				}
			}
		}

	private:

		template<typename Feeder, bool near, size_t l = length>
		typename std::enable_if<l & 1, bool>::type find_start(Graph const &graph, Graph_Edits const &edited, Feeder &feeder, VertexID u)
		{
			std::vector<VertexID> path(length);
			Packed *f = forbidden.data() + (length / 2 - 2) * graph.get_row_length();
			for(size_t i = 0; i < graph.get_row_length(); i++)
			{
				f[i] = graph.get_row(u)[i];
			}
			f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
			path[length / 2] = u;

			auto outer = [&](size_t const i, Packed const curf)
			{
				VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
				path[length / 2 - 1] = vf;
				Packed vf_mask = ~((Packed(2) << __builtin_ctzll(curf)) - 1);

				auto inner = [&](size_t const j, Packed const curb)
				{
					VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
					if(graph.has_edge(vf, vb)) {return false;}
					path[length / 2 + 1] = vb;
					if(Find_Rec<Feeder, length / 2 - 1, length / 2 + 1, near>::find_rec(graph, edited, path, forbidden, feeder)) {return true;}
					return false;
				};

				for(size_t j = i; j < graph.get_row_length(); j++)
				{
					for(Packed curb = (j == i? graph.get_row(u)[j] & vf_mask : graph.get_row(u)[j]) & edited.get_row(u)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
					{
						if(inner(j, curb)) {return true;}
					}
				}
				for(size_t j = i; j < graph.get_row_length(); j++)
				{
					for(Packed curb = (j == i? graph.get_row(u)[j] & vf_mask : graph.get_row(u)[j]) & ~edited.get_row(u)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
					{
						if(inner(j, curb)) {return true;}
					}
				}
				return false;
			};

			for(size_t i = 0; i < graph.get_row_length(); i++)
			{
				for(Packed curf = graph.get_row(u)[i] & edited.get_row(u)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
				{
					if(outer(i, curf)) {return true;}
				}
			}
			for(size_t i = 0; i < graph.get_row_length(); i++)
			{
				for(Packed curf = graph.get_row(u)[i] & ~edited.get_row(u)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
				{
					if(outer(i, curf)) {return true;}
				}
			}
			return false;
		}

		template<typename Feeder, bool near, size_t l = length>
		typename std::enable_if<!(l & 1), bool>::type find_start(Graph const &graph, Graph_Edits const &edited, Feeder &feeder, VertexID u)
		{
			std::vector<VertexID> path(length);
			Packed *f = forbidden.data() + (length / 2 - 2) * graph.get_row_length();
			auto inner = [&](size_t const i, Packed const cur) -> bool
			{
				VertexID v = __builtin_ctzll(cur) + i * Packed_Bits;
				f[v / Packed_Bits] |= Packed(1) << (v % Packed_Bits);
				path[length / 2] = v;
				if(Find_Rec<Feeder, length / 2 - 1, length / 2, near>::find_rec(graph, edited, path, forbidden, feeder)) {return true;}
				f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
				return false;
			};

			f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
			path[length / 2 - 1] = u;
			for(size_t i = u / Packed_Bits; i < graph.get_row_length(); i++)// double exploration: i = 0  cur = graph.get_row(u)[i]
			{
				for(Packed cur = (i == u / Packed_Bits? graph.get_row(u)[i] & ~((Packed(2) << (u % Packed_Bits)) - 1) : graph.get_row(u)[i]) & edited.get_row(u)[i]; cur; cur &= ~(Packed(1) << __builtin_ctzll(cur)))
				{
					if(inner(i, cur)) {return true;}
				}
			}
			for(size_t i = u / Packed_Bits; i < graph.get_row_length(); i++)// double exploration: i = 0  cur = graph.get_row(u)[i]
			{
				for(Packed cur = (i == u / Packed_Bits? graph.get_row(u)[i] & ~((Packed(2) << (u % Packed_Bits)) - 1) : graph.get_row(u)[i]) & ~edited.get_row(u)[i]; cur; cur &= ~(Packed(1) << __builtin_ctzll(cur)))
				{
					if(inner(i, cur)) {return true;}
				}
			}
			f[u / Packed_Bits] = 0;
			return false;
		}

		template<typename Feeder, size_t lf, size_t lb, bool near>
		class Find_Rec
		{
		public:
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder)
			{
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				{
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						nf[i] = graph.get_row(uf)[i] | graph.get_row(ub)[i] | f[i];
					}

					auto outer = [&](size_t const i, Packed const curf) -> bool
					{
						VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
						path[lf - 1] = vf;

						auto inner = [&](size_t const j, Packed const curb) -> bool
						{
							VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
							if(vf == vb || graph.has_edge(vf, vb)) {return false;}
							path[lb + 1] = vb;
							return Find_Rec<Feeder, lf - 1, lb + 1, near>::find_rec(graph, edited, path, forbidden, feeder);
						};

						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & edited.get_row(ub)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
							{
								if(inner(j, curb)) {return true;}
							}
						}
						for(size_t j = 0; j < graph.get_row_length(); j++)
						{
							for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & ~edited.get_row(ub)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
							{
								if(inner(j, curb)) {return true;}
							}
						}
						return false;
					};

					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & edited.get_row(uf)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
						{
							if(outer(i, curf)) {return true;}
						}
					}
					for(size_t i = 0; i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & ~edited.get_row(uf)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
						{
							if(outer(i, curf)) {return true;}
						}
					}
				}
				return false;
			}
		};

		template<typename Feeder, size_t lb>
		class Find_Rec<Feeder, 1, lb, false>
		{
		public:
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder)
			{
				constexpr size_t lf = 1;
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				//Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				/* last vertices */
				auto outer = [&](size_t const i, Packed const curf) -> bool
				{
					VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
					path[lf - 1] = vf;

					auto inner = [&](size_t const j, Packed const curb) -> bool
					{
						VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
						if(vf == vb) {return false;}
						path[lb + 1] = vb;
						return feeder.callback(graph, edited, path.cbegin(), path.cend());
					};

					for(size_t j = 0; j < graph.get_row_length(); j++)
					{
						for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & edited.get_row(ub)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
						{
							if(inner(j, curb)) {return true;}
						}
					}
					for(size_t j = 0; j < graph.get_row_length(); j++)
					{
						for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & ~edited.get_row(ub)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
						{
							if(inner(j, curb)) {return true;}
						}
					}
					return false;
				};

				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & edited.get_row(uf)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
					{
						if(outer(i, curf)) {return true;}
					}
				}
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & ~edited.get_row(uf)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
					{
						if(outer(i, curf)) {return true;}
					}
				}
				return false;
			}
		};

		template<typename Feeder, size_t lb>
		class Find_Rec<Feeder, 1, lb, true>
		{
		public:
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder)
			{
				constexpr size_t lf = 1;
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				//Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				/* last vertices */
				auto outer = [&](size_t const i, Packed const curf) -> bool
				{
					VertexID vf = __builtin_ctzll(curf) + i * Packed_Bits;
					path[lf - 1] = vf;

					auto inner = [&](size_t const j, Packed const curb) -> bool
					{
						VertexID vb = __builtin_ctzll(curb) + j * Packed_Bits;
						if(vf == vb) {return false;}
						path[lb + 1] = vb;
						return feeder.callback_near(graph, edited, path.cbegin(), path.cend());
					};

					for(size_t j = 0; j < graph.get_row_length(); j++)
					{
						for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & edited.get_row(ub)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
						{
							if(inner(j, curb)) {return true;}
						}
					}
					for(size_t j = 0; j < graph.get_row_length(); j++)
					{
						for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & ~edited.get_row(ub)[j]; curb; curb &= ~(Packed(1) << __builtin_ctzll(curb)))
						{
							if(inner(j, curb)) {return true;}
						}
					}
					return false;
				};

				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & edited.get_row(uf)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
					{
						if(outer(i, curf)) {return true;}
					}
				}
				for(size_t i = 0; i < graph.get_row_length(); i++)
				{
					for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & ~edited.get_row(uf)[i]; curf; curf &= ~(Packed(1) << __builtin_ctzll(curf)))
					{
						if(outer(i, curf)) {return true;}
					}
				}
				return false;
			}
		};
	};

	template<typename Graph, typename Graph_Edits>
	class Center_Edits_4 : public Center_Edits<Graph, Graph_Edits, 4>
	{
	private: using Parent = Center_Edits<Graph, Graph_Edits, 4>;
	public:
		static constexpr char const *name = "Center_Edits_4";
		Center_Edits_4(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits>
	class Center_Edits_5 : public Center_Edits<Graph, Graph_Edits, 5>
	{
	private: using Parent = Center_Edits<Graph, Graph_Edits, 5>;
	public:
		static constexpr char const *name = "Center_Edits_5";
		Center_Edits_5(VertexID graph_size) : Parent(graph_size) {;}
	};

}

#endif
