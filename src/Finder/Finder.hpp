#ifndef FINDER_HPP
#define FINDER_HPP

#include <functional>
#include <vector>

#include "../Graph/Graph.hpp"

namespace Finder
{
	class Finder_Consumer
	{
	public:
		virtual void prepare() {;}
		virtual bool next(Graph::Graph const &, Graph::Graph const &, std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator) = 0;
	};

	using Feeder_Callback = std::function<bool(std::vector<VertexID>::const_iterator, std::vector<VertexID>::const_iterator)>;

	class Finder
	{
	public:
		virtual void find(Graph::Graph const &graph, Graph::Graph const &edited, Feeder_Callback &) = 0;
	};

	void feed(Graph::Graph const &graph, Graph::Graph const &edited, Finder &finder, std::vector<Finder_Consumer *> consumer)
	{
		for(auto c: consumer) {c->prepare();}
		Feeder_Callback callback = [&](std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e) -> bool
		{
			auto it = consumer.end();
			do
			{
				it--;
				if(!(*it)->next(graph, edited, b, e)) {consumer.erase(it);}
			} while (it != consumer.begin());
			return consumer.empty();
		};

		finder.find(graph, edited, callback);
	}
}

#endif
