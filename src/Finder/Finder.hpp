#ifndef FINDER_HPP
#define FINDER_HPP

#include "../config.hpp"

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
	};

	template<typename... Consumer>
	struct Preparer<0, Consumer...>
	{
		static void prepare(std::tuple<Consumer &...> &) {;}
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
	};

	template<typename Graph, typename Graph_Edits, typename... Consumer>
	struct Caller<Graph, Graph_Edits, 0, Consumer...>
	{
		static bool call(std::tuple<Consumer &...> &, std::array<bool, sizeof...(Consumer)> &, Graph const &, Graph_Edits const &, std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator)
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
	};
}

#endif
