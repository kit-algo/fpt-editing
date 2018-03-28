#ifndef FINDER_CENTER_EDITS_SPARSE_HPP
#define FINDER_CENTER_EDITS_SPARSE_HPP

#include <assert.h>

#include <vector>

#include "../config.hpp"

#include "Finder.hpp"

namespace Finder
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t _length>
	class Center_Edits_Sparse
	{
		static_assert(_length > 3, "Can only detect path/cycles with at least 4 vertices");

	public:
		static constexpr char const *name = "Center_Edits_Sparse";
		static constexpr size_t length = _length;

	private:
		std::vector<Packed> forbidden;
		Graph_Edits offered;

	public:
		Center_Edits_Sparse(VertexID graph_size) : forbidden(Graph::alloc_rows(graph_size, length / 2 - 1)), offered(graph_size) {;}

		template<typename Feeder>
		void find(Graph const &graph, Graph_Edits const &edited, Feeder &feeder)
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);
			/* maintain matrix of unedited edges already offered, don't offer them again */
			offered.clear();

			for(VertexID u = 0; u < graph.size(); u++)
			{
				if(find_start<Feeder, false>(graph, edited, feeder, u)) {return;}
			}
		}

		template<typename Feeder>
		void find_near(Graph const &graph, Graph_Edits const &edited, VertexID uu, VertexID vv, Feeder &feeder, Graph_Edits const *used)
		{
			assert(forbidden.size() / graph.get_row_length() == length / 2 - 1);
			/* only search for forbidden subgraphs near u-v
			 * -> both vertices of starting edge must be within length / 2 of u or v
			 */
			if(used)
			{
				offered = *used;
			}
			else {offered.clear();}

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
					for(Packed cur = area[i] & ~included[i]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
					{
						VertexID add = PACKED_CTZ(cur) + i * Packed_Bits;
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
				for(Packed cur = area[ii]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
				{
					VertexID u = PACKED_CTZ(cur) + ii * Packed_Bits;
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
				VertexID vf = PACKED_CTZ(curf) + i * Packed_Bits;
				if(offered.has_edge(u, vf)) {return false;}
				bool can_skip = !std::is_same<Mode, Options::Modes::Insert>::value && !edited.has_edge(u, vf);
				bool skipping = false;

				path[length / 2 - 1] = vf;
				Packed vf_mask = ~((Packed(2) << PACKED_CTZ(curf)) - 1);

				auto inner = [&](size_t const j, Packed const curb, bool can_skip, bool &skipping)
				{
					VertexID vb = PACKED_CTZ(curb) + j * Packed_Bits;
					if(offered.has_edge(u, vb) || offered.has_edge(vf, vb)) {return false;}
					if(graph.has_edge(vf, vb)) {return false;}
					if(!can_skip)
					{
						if(!std::is_same<Mode, Options::Modes::Insert>::value && !edited.has_edge(u, vb)) {can_skip = true;}
						if(!std::is_same<Mode, Options::Modes::Delete>::value && !edited.has_edge(vf, vb)) {can_skip = true;}
					}
					path[length / 2 + 1] = vb;
					if(Find_Rec<Feeder, length / 2 - 1, length / 2 + 1, near>::find_rec(graph, edited, path, forbidden, feeder, offered, can_skip, skipping)) {return true;}
					return false;
				};

				for(size_t j = i; !skipping && j < graph.get_row_length(); j++)
				{
					for(Packed curb = (j == i? graph.get_row(u)[j] & vf_mask : graph.get_row(u)[j]) & edited.get_row(u)[j]; !skipping && curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
					{
						if(inner(j, curb, can_skip, skipping)) {return true;}
						if(!can_skip) {skipping = false;}
					}
				}
				for(size_t j = i; !skipping && j < graph.get_row_length(); j++)
				{
					for(Packed curb = (j == i? graph.get_row(u)[j] & vf_mask : graph.get_row(u)[j]) & ~edited.get_row(u)[j]; !skipping && curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
					{
						if(inner(j, curb, can_skip, skipping)) {return true;}
						if(!can_skip) {skipping = false;}
					}
				}
				return false;
			};

			for(size_t i = 0; i < graph.get_row_length(); i++)
			{
				for(Packed curf = graph.get_row(u)[i] & edited.get_row(u)[i]; curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
				{
					if(outer(i, curf)) {return true;}
				}
			}
			for(size_t i = 0; i < graph.get_row_length(); i++)
			{
				for(Packed curf = graph.get_row(u)[i] & ~edited.get_row(u)[i]; curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
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
				VertexID v = PACKED_CTZ(cur) + i * Packed_Bits;
				if(offered.has_edge(u, v)) {return false;}
				bool can_skip = !std::is_same<Mode, Options::Modes::Insert>::value && !edited.has_edge(u, v);
				bool skipping = false;

				f[v / Packed_Bits] |= Packed(1) << (v % Packed_Bits);
				path[length / 2] = v;
				if(Find_Rec<Feeder, length / 2 - 1, length / 2, near>::find_rec(graph, edited, path, forbidden, feeder, offered, can_skip, skipping)) {return true;}
				f[v / Packed_Bits] &= ~(Packed(1) << (v % Packed_Bits));
				return false;
			};

			f[u / Packed_Bits] |= Packed(1) << (u % Packed_Bits);
			path[length / 2 - 1] = u;
			for(size_t i = u / Packed_Bits; i < graph.get_row_length(); i++)// double exploration: i = 0  cur = graph.get_row(u)[i]
			{
				for(Packed cur = (i == u / Packed_Bits? graph.get_row(u)[i] & ~((Packed(2) << (u % Packed_Bits)) - 1) : graph.get_row(u)[i]) & edited.get_row(u)[i]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
				{
					if(inner(i, cur)) {return true;}
				}
			}
			for(size_t i = u / Packed_Bits; i < graph.get_row_length(); i++)// double exploration: i = 0  cur = graph.get_row(u)[i]
			{
				for(Packed cur = (i == u / Packed_Bits? graph.get_row(u)[i] & ~((Packed(2) << (u % Packed_Bits)) - 1) : graph.get_row(u)[i]) & ~edited.get_row(u)[i]; cur; cur &= ~(Packed(1) << PACKED_CTZ(cur)))
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
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder, Graph_Edits &offered, bool can_skip, bool &skipping)
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

					auto outer = [&](size_t const i, Packed const curf, bool can_skip, bool &skipping) -> bool
					{
						VertexID vf = PACKED_CTZ(curf) + i * Packed_Bits;
						for(size_t i = lf; i <= lb; i++) {if(offered.has_edge(path[i], vf)) {return false;}}
						if(!can_skip)
						{
							if(!std::is_same<Mode, Options::Modes::Insert>::value && !edited.has_edge(path[lf], vf)) {can_skip = true;}
							if(!std::is_same<Mode, Options::Modes::Delete>::value) {for(size_t i = lf + 1; i <= lb; i++) {if(!edited.has_edge(path[i], vf)) {can_skip = true;}}}
						}
						path[lf - 1] = vf;

						auto inner = [&](size_t const j, Packed const curb, bool can_skip, bool &skipping) -> bool
						{
							VertexID vb = PACKED_CTZ(curb) + j * Packed_Bits;
							if(!std::is_same<Mode, Options::Modes::Delete>::value && offered.has_edge(vf, vb)) {return false;}
							for(size_t i = lf; i <= lb; i++) {if(offered.has_edge(path[i], vf)) {return false;}}
							if(!can_skip)
							{
								if(!std::is_same<Mode, Options::Modes::Insert>::value && !edited.has_edge(path[lb], vb)) {can_skip = true;}
								if(!std::is_same<Mode, Options::Modes::Delete>::value) {for(size_t i = lf; i < lb; i++) {if(!edited.has_edge(path[i], vb)) {can_skip = true;}}}
							}
							if(vf == vb || graph.has_edge(vf, vb)) {return false;}
							path[lb + 1] = vb;
							return Find_Rec<Feeder, lf - 1, lb + 1, near>::find_rec(graph, edited, path, forbidden, feeder, offered, can_skip, skipping);
						};

						for(size_t j = 0; !skipping && j < graph.get_row_length(); j++)
						{
							for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & edited.get_row(ub)[j]; !skipping && curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
							{
								if(inner(j, curb, can_skip, skipping)) {return true;}
								if(!can_skip) {skipping = false;}
							}
						}
						for(size_t j = 0; !skipping && j < graph.get_row_length(); j++)
						{
							for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & ~edited.get_row(ub)[j]; !skipping && curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
							{
								if(inner(j, curb, can_skip, skipping)) {return true;}
								if(!can_skip) {skipping = false;}
							}
						}
						return false;
					};

					for(size_t i = 0; !skipping && i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & edited.get_row(uf)[i]; !skipping && curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
						{
							if(outer(i, curf, can_skip, skipping)) {return true;}
							if(!can_skip) {skipping = false;}
						}
					}
					for(size_t i = 0; !skipping && i < graph.get_row_length(); i++)
					{
						for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & ~edited.get_row(uf)[i]; !skipping && curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
						{
							if(outer(i, curf, can_skip, skipping)) {return true;}
							if(!can_skip) {skipping = false;}
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
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder, Graph_Edits &offered, bool can_skip, bool &skipping)
			{
				constexpr size_t lf = 1;
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				//Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				/* last vertices */
				auto outer = [&](size_t const i, Packed const curf, bool can_skip, bool &skipping) -> bool
				{
					VertexID vf = PACKED_CTZ(curf) + i * Packed_Bits;
					for(size_t i = lf; i <= lb; i++) {if(offered.has_edge(path[i], vf)) {return false;}}
					if(!can_skip)
					{
						if(!std::is_same<Mode, Options::Modes::Insert>::value && !edited.has_edge(path[lf], vf)) {can_skip = true;}
						if(!std::is_same<Mode, Options::Modes::Delete>::value) {for(size_t i = lf + 1; i <= lb; i++) {if(!edited.has_edge(path[i], vf)) {can_skip = true;}}}
					}
					path[lf - 1] = vf;

					auto inner = [&](size_t const j, Packed const curb, bool can_skip, bool &skipping) -> bool
					{
						VertexID vb = PACKED_CTZ(curb) + j * Packed_Bits;
						if(!std::is_same<Conversion, Options::Conversions::Skip>::value && offered.has_edge(vf, vb)) {return false;}
						for(size_t i = lf; i <= lb; i++) if(offered.has_edge(path[i], vb)) {return false;}
						if(vf == vb) {return false;}
						path[lb + 1] = vb;
						if(for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.cbegin(), path.cend(), [&](auto uit, auto vit) {return offered.has_edge(*uit, *vit);})) {return false;}
						for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.cbegin(), path.cend(), [&](auto uit, auto vit) {offered.set_edge(*uit, *vit); return false;});
						skipping = can_skip;
						return feeder.callback(graph, edited, path.cbegin(), path.cend());
					};

					for(size_t j = 0; !skipping && j < graph.get_row_length(); j++)
					{
						for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & edited.get_row(ub)[j]; !skipping && curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
						{
							if(inner(j, curb, can_skip, skipping)) {return true;}
						}
					}
					for(size_t j = 0; !skipping && j < graph.get_row_length(); j++)
					{
						for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & ~edited.get_row(ub)[j]; !skipping && curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
						{
							if(inner(j, curb, can_skip, skipping)) {return true;}
						}
					}
					return false;
				};

				for(size_t i = 0; !skipping && i < graph.get_row_length(); i++)
				{
					for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & edited.get_row(uf)[i]; !skipping && curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
					{
						if(outer(i, curf, can_skip, skipping)) {return true;}
						if(!can_skip) {skipping = false;}
					}
				}
				for(size_t i = 0; !skipping && i < graph.get_row_length(); i++)
				{
					for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & ~edited.get_row(uf)[i]; !skipping && curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
					{
						if(outer(i, curf, can_skip, skipping)) {return true;}
						if(!can_skip) {skipping = false;}
					}
				}
				return false;
			}
		};

		template<typename Feeder, size_t lb>
		class Find_Rec<Feeder, 1, lb, true>
		{
		public:
			static bool find_rec(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID> &path, std::vector<Packed> &forbidden, Feeder &feeder, Graph_Edits &offered, bool const can_skip, bool &skipping)
			{
				constexpr size_t lf = 1;
				Packed *f = forbidden.data() + (lf - 1) * graph.get_row_length();
				//Packed *nf = f - graph.get_row_length();
				VertexID &uf = path[lf];
				VertexID &ub = path[lb];

				/* last vertices */
				auto outer = [&](size_t const i, Packed const curf, bool can_skip, bool &skipping) -> bool
				{
					VertexID vf = PACKED_CTZ(curf) + i * Packed_Bits;
					if(!can_skip)
					{
						if(!std::is_same<Mode, Options::Modes::Insert>::value && !edited.has_edge(path[lf], vf)) {can_skip = true;}
						if(!std::is_same<Mode, Options::Modes::Delete>::value) {for(size_t i = lf + 1; i <= lb; i++) {if(!edited.has_edge(path[i], vf)) {can_skip = true;}}}
					}
					path[lf - 1] = vf;

					auto inner = [&](size_t const j, Packed const curb, bool can_skip, bool &skipping) -> bool
					{
						VertexID vb = PACKED_CTZ(curb) + j * Packed_Bits;
						if(!std::is_same<Conversion, Options::Conversions::Skip>::value && offered.has_edge(vf, vb)) {return false;}
						for(size_t i = lf; i <= lb; i++) if(offered.has_edge(path[i], vb)) {return false;}
						if(vf == vb) {return false;}
						path[lb + 1] = vb;
						for_all_edges_unordered<Mode, Restriction, Conversion>(graph, edited, path.cbegin(), path.cend(), [&](auto uit, auto vit) {offered.set_edge(*uit, *vit); return false;});
						skipping = can_skip;
						return feeder.callback_near(graph, edited, path.cbegin(), path.cend());
					};

					for(size_t j = 0; !skipping && j < graph.get_row_length(); j++)
					{
						for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & edited.get_row(ub)[j]; !skipping && curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
						{
							if(inner(j, curb, can_skip, skipping)) {return true;}
						}
					}
					for(size_t j = 0; !skipping && j < graph.get_row_length(); j++)
					{
						for(Packed curb = graph.get_row(ub)[j] & ~graph.get_row(uf)[j] & ~f[j] & ~edited.get_row(ub)[j]; !skipping && curb; curb &= ~(Packed(1) << PACKED_CTZ(curb)))
						{
							if(inner(j, curb, can_skip, skipping)) {return true;}
						}
					}
					return false;
				};

				for(size_t i = 0; !skipping && i < graph.get_row_length(); i++)
				{
					for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & edited.get_row(uf)[i]; !skipping && curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
					{
						if(outer(i, curf, can_skip, skipping)) {return true;}
						if(!can_skip) {skipping = false;}
					}
				}
				for(size_t i = 0; !skipping && i < graph.get_row_length(); i++)
				{
					for(Packed curf = graph.get_row(uf)[i] & ~graph.get_row(ub)[i] & ~f[i] & ~edited.get_row(uf)[i]; !skipping && curf; curf &= ~(Packed(1) << PACKED_CTZ(curf)))
					{
						if(outer(i, curf, can_skip, skipping)) {return true;}
						if(!can_skip) {skipping = false;}
					}
				}
				return false;
			}
		};
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_Edits_Sparse_4 : public Center_Edits_Sparse<Graph, Graph_Edits, Mode, Restriction, Conversion, 4>
	{
	private: using Parent = Center_Edits_Sparse<Graph, Graph_Edits, Mode, Restriction, Conversion, 4>;
	public:
		static constexpr char const *name = "Center_Edits_Sparse_4";
		Center_Edits_Sparse_4(VertexID graph_size) : Parent(graph_size) {;}
	};

	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Center_Edits_Sparse_5 : public Center_Edits_Sparse<Graph, Graph_Edits, Mode, Restriction, Conversion, 5>
	{
	private: using Parent = Center_Edits_Sparse<Graph, Graph_Edits, Mode, Restriction, Conversion, 5>;
	public:
		static constexpr char const *name = "Center_Edits_Sparse_5";
		Center_Edits_Sparse_5(VertexID graph_size) : Parent(graph_size) {;}
	};

}

#endif
