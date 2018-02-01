#ifndef FINDER_HPP
#define FINDER_HPP

#include "../config.hpp"

#include "../Options.hpp"

#include <array>
#include <utility>
#include <vector>

namespace Finder
{
	template<typename Finder, typename Graph, typename Graph_Edits, typename... Consumer>
	class Feeder
	{
	private:
		Finder &finder;

		std::tuple<Consumer &...> consumer;
		std::array<bool, sizeof...(Consumer)> done;

	public:
		Feeder(Finder &finder, std::tuple<Consumer &...> consumer) : finder(finder), consumer(consumer) {;}
		Feeder(Finder &finder, Consumer &... consumer) : finder(finder), consumer(consumer...) {;}

		void feed(Graph const &graph, Graph_Edits const &edited)
		{
			done.fill(false);
			prepare_impl(std::index_sequence_for<Consumer ...>{});
			finder.find(graph, edited, *this);
		}

		bool callback(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			return call_impl(graph, edited, b, e, std::index_sequence_for<Consumer ...>{});
		}

		void feed_near(Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v)
		{
			done.fill(false);
			prepare_near_impl(u, v, std::index_sequence_for<Consumer ...>{});
			finder.find_near(graph, edited, u, v, *this);
		}

		bool callback_near(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			return call_near_impl(graph, edited, b, e, std::index_sequence_for<Consumer ...>{});
		}

	private:
		template<size_t... Is>
		void prepare_impl(std::index_sequence<Is ...>)
		{
			((std::get<Is>(consumer).prepare()), ...);
		}

		template<size_t... Is>
		bool call_impl(Graph const &graph, Graph_Edits const &edits, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e, std::index_sequence<Is ...>)
		{
			return ((!done[Is] && (done[Is] = std::get<Is>(consumer).next(graph, edits, b, e))) & ...);
		}

		template<size_t... Is>
		void prepare_near_impl(VertexID u, VertexID v, std::index_sequence<Is ...>)
		{
			((std::get<Is>(consumer).prepare_near(u, v)), ...);
		}

		template<size_t... Is>
		bool call_near_impl(Graph const &graph, Graph_Edits const &edits, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e, std::index_sequence<Is ...>)
		{
			return ((!done[Is] && (done[Is] = std::get<Is>(consumer).next_near(graph, edits, b, e))) & ...);
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
