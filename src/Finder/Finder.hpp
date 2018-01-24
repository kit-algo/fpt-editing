#ifndef FINDER_HPP
#define FINDER_HPP

#include "../config.hpp"

#include "../Options.hpp"

#include <array>
#include <utility>
#include <vector>

namespace Finder
{
	template<size_t N, typename... Consumer>
	struct Preparer
	{
		static void prepare(std::tuple<Consumer &...> &consumer)
		{
			Preparer<N - 1, Consumer...>::prepare(consumer);
			std::get<N - 1>(consumer).prepare();
		}

		static void prepare_near(std::tuple<Consumer &...> &consumer, VertexID u, VertexID v)
		{
			Preparer<N - 1, Consumer...>::prepare_near(consumer, u, v);
			std::get<N - 1>(consumer).prepare_near(u, v);
		}
	};

	template<typename... Consumer>
	struct Preparer<0, Consumer...>
	{
		static void prepare(std::tuple<Consumer &...> &) {;}
		static void prepare_near(std::tuple<Consumer &...> &, VertexID, VertexID) {;}
	};

	template<typename Graph, typename Graph_Edits, size_t N, typename... Consumer>
	struct Caller
	{
		static bool call(std::tuple<Consumer &...> &consumer, std::array<bool, sizeof...(Consumer)> &done, Graph const &graph, Graph_Edits const &edits, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			bool other = Caller<Graph, Graph_Edits, N - 1, Consumer...>::call(consumer, done, graph, edits, b, e);
			bool self = done[N - 1] || (done[N - 1] = !std::get<N - 1>(consumer).next(graph, edits, b, e));
			return self && other;
		}

		static bool call_near(std::tuple<Consumer &...> &consumer, std::array<bool, sizeof...(Consumer)> &done, Graph const &graph, Graph_Edits const &edits, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			bool other = Caller<Graph, Graph_Edits, N - 1, Consumer...>::call_near(consumer, done, graph, edits, b, e);
			bool self = done[N - 1] || (done[N - 1] = !std::get<N - 1>(consumer).next_near(graph, edits, b, e));
			return self && other;
		}
	};

	template<typename Graph, typename Graph_Edits, typename... Consumer>
	struct Caller<Graph, Graph_Edits, 0, Consumer...>
	{
		static bool call(std::tuple<Consumer &...> &, std::array<bool, sizeof...(Consumer)> &, Graph const &, Graph_Edits const &, std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator)
		{
			return true;
		}

		static bool call_near(std::tuple<Consumer &...> &, std::array<bool, sizeof...(Consumer)> &, Graph const &, Graph_Edits const &, std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator)
		{
			return true;
		}
	};

	template<typename Finder, typename Graph, typename Graph_Edits, typename... Consumer>
	class Feeder
	{
	private:
		Finder &finder;

		std::tuple<Consumer &...> consumer;
		std::array<bool, sizeof...(Consumer)> done;

	public:
		Feeder(Finder &finder, Consumer &...consumer) : finder(finder), consumer(consumer...)
		{
			;
		}

		void feed(Graph const &graph, Graph_Edits const &edited)
		{
			done.fill(false);
			Preparer<sizeof...(Consumer), Consumer...>::prepare(consumer);
			finder.find(graph, edited, *this);
		}

		bool callback(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			return Caller<Graph, Graph_Edits, sizeof...(Consumer), Consumer...>::call(consumer, done, graph, edited, b, e);
		}

		void feed_near(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			done.fill(false);
			Preparer<sizeof...(Consumer), Consumer...>::prepare_near(consumer, u, v);
			finder.find_near(graph, edited, u, v, *this);
		}

		bool callback_near(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			return Caller<Graph, Graph_Edits, sizeof...(Consumer), Consumer...>::call_near(consumer, done, graph, edited, b, e);
		}
	};

	template<typename Mode, typename Restriction, typename Conversion, typename Graph, typename Graph_Edits, typename Func>
	inline bool for_all_edges_ordered(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e, Func f)
	{
		if(std::is_same<Mode, Options::Modes::Edit>::value)
		{
			for(auto vit = b + 1; vit != e; vit++)
			{
				for(auto uit = b; uit != vit; uit++)
				{
					if(!std::is_same<Conversion, Options::Conversions::Normal>::value && uit == b && vit == e - 1) {continue;}
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(f(uit, vit)) {return true;}
				}
			}
			if(std::is_same<Conversion, Options::Conversions::Last>::value)
			{
				auto uit = b;
				auto vit = e - 1;
				do
				{
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(f(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if(std::is_same<Mode, Options::Modes::Delete>::value)
		{
			for(auto uit = b, vit = b + 1; vit != e; uit++, vit++)
			{
				if(!std::is_same<Restriction, Options::Restrictions::None>::value)
				{
					if(edited.has_edge(*uit, *vit)) {continue;}
				}
				if(f(uit, vit)) {return true;}
			}
			if(!std::is_same<Conversion, Options::Conversions::Skip>::value && graph.has_edge(*b, *(e - 1)))
			{
				auto uit = b;
				auto vit = e - 1;
				do
				{
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(f(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if(std::is_same<Mode, Options::Modes::Insert>::value)
		{
			for(auto vit = b + 2; vit != e; vit++)
			{
				for(auto uit = b; uit != vit - 1; uit++)
				{
					if(uit == b && vit == e - 1 && (!std::is_same<Conversion, Options::Conversions::Normal>::value || graph.has_edge(*uit, *vit))) {continue;}
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(f(uit, vit)) {return true;}
				}
			}
			if(std::is_same<Conversion, Options::Conversions::Last>::value && !graph.has_edge(*b, *(e - 1)))
			{
				auto uit = b;
				auto vit = e - 1;
				do
				{
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(f(uit, vit)) {return true;}
				} while(false);
			}
		}
		return false;
	}

	template<typename Mode, typename Restriction, typename Conversion, typename Graph, typename Graph_Edits, typename Func>
	inline bool for_all_edges_unordered(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e, Func f)
	{
		if(std::is_same<Mode, Options::Modes::Edit>::value)
		{
			for(auto vit = b + 1; vit != e; vit++)
			{
				for(auto uit = b; uit != vit; uit++)
				{
					if(std::is_same<Conversion, Options::Conversions::Skip>::value && uit == b && vit == e - 1) {continue;}
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(f(uit, vit)) {return true;}
				}
			}
		}
		else if(std::is_same<Mode, Options::Modes::Delete>::value)
		{
			for(auto uit = b, vit = b + 1; vit != e; uit++, vit++)
			{
				if(!std::is_same<Restriction, Options::Restrictions::None>::value)
				{
					if(edited.has_edge(*uit, *vit)) {continue;}
				}
				if(f(uit, vit)) {return true;}
			}
			if(!std::is_same<Conversion, Options::Conversions::Skip>::value && graph.has_edge(*b, *(e - 1)))
			{
				auto uit = b;
				auto vit = e - 1;
				do
				{
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(f(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if(std::is_same<Mode, Options::Modes::Insert>::value)
		{
			for(auto vit = b + 2; vit != e; vit++)
			{
				for(auto uit = b; uit != vit - 1; uit++)
				{
					if(uit == b && vit == e - 1 && (std::is_same<Conversion, Options::Conversions::Skip>::value || graph.has_edge(*uit, *vit))) {continue;}
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(f(uit, vit)) {return true;}
				}
			}
		}
		return false;
	}
}

#endif
