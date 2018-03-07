#ifndef CONSUMER_COUNTER_HPP
#define CONSUMER_COUNTER_HPP

#include <iostream>
#include <vector>

#include "../config.hpp"

#include "../Options.hpp"

namespace Consumer
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, size_t length>
	class Counter : Options::Tag::Result
	{
	public:
		static constexpr char const *name = "Counter";

	private:
		size_t paths = 0;
		size_t cycles = 0;

	public:
		Counter(VertexID) {;}

		void prepare()
		{
			paths = 0;
			cycles = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			/*std::cout << "got " << (graph.has_edge(*b, *(e - 1))? 'C' : 'P');
			for(auto it = b; it != e; it++) {std::cout << ' ' << +*it;}
			std::cout << '\n';*/

			if(graph.has_edge(*b, *(e - 1))) {cycles++;}
			else {paths++;}
			return false;
		}

		void result(size_t, Graph const &, Graph const &, Options::Tag::Result)
		{
			cycles /= length;
			//std::cout << 'P' << +length << ": " << +paths << ", C" << +length << ": " << +cycles << '\n';
		}
	};
}

#endif
