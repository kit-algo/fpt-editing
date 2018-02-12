#ifndef FINDER_HPP
#define FINDER_HPP

#include "../config.hpp"

#include "../Options.hpp"

#include <array>
#include <utility>
#include <vector>

namespace Finder
{
	template<size_t NN, typename... Consumer>
	struct Result_impl
	{
		template<typename Graph, typename Graph_Edits, size_t N = NN>
		static typename std::enable_if<std::is_base_of<Options::Tag::Result, typename std::tuple_element<N - 1, std::tuple<Consumer ...>>::type>::value,
		void>::type result(std::tuple<Consumer &...> &consumer, size_t k, Graph const &graph, Graph_Edits const &edited)
		{
			Result_impl<N - 1, Consumer...>::result(consumer, k, graph, edited);
			std::get<N - 1>(consumer).result(k, graph, edited, Options::Tag::Result());
		}

		template<typename Graph, typename Graph_Edits, size_t N = NN>
		static typename std::enable_if<!std::is_base_of<Options::Tag::Result, typename std::tuple_element<N - 1, std::tuple<Consumer ...>>::type>::value,
		void>::type result(std::tuple<Consumer &...> &consumer, size_t k, Graph const &graph, Graph_Edits const &edited)
		{
			Result_impl<N - 1, Consumer...>::result(consumer, k, graph, edited);
		}

		template<typename Graph, typename Graph_Edits, size_t N = NN>
		static typename std::enable_if<std::is_base_of<Options::Tag::Result, typename std::tuple_element<N - 1, std::tuple<Consumer ...>>::type>::value,
		void>::type result_near(std::tuple<Consumer &...> &consumer, size_t k, Graph const &graph, Graph_Edits const &edited)
		{
			Result_impl<N - 1, Consumer...>::result_near(consumer, k, graph, edited);
			std::get<N - 1>(consumer).result_near(k, graph, edited, Options::Tag::Result());
		}

		template<typename Graph, typename Graph_Edits, size_t N = NN>
		static typename std::enable_if<!std::is_base_of<Options::Tag::Result, typename std::tuple_element<N - 1, std::tuple<Consumer ...>>::type>::value,
		void>::type result_near(std::tuple<Consumer &...> &consumer, size_t k, Graph const &graph, Graph_Edits const &edited)
		{
			Result_impl<N - 1, Consumer...>::result_near(consumer, k, graph, edited);
		}
	};
	template<typename... Consumer>
	struct Result_impl<0, Consumer...>
	{
		template<typename Graph, typename Graph_Edits>
		static void result(std::tuple<Consumer &...> &, size_t, Graph const &, Graph_Edits const &) {;}
		template<typename Graph, typename Graph_Edits>
		static void result_near(std::tuple<Consumer &...> &, size_t, Graph const &, Graph_Edits const &) {;}
	};

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

		/** Prepares consumers for a new run of the Finder and starts it */
		void feed(size_t k, Graph const &graph, Graph_Edits const &edited)
		{
			done.fill(false);
			prepare_impl(std::index_sequence_for<Consumer ...>{});
			finder.find(graph, edited, *this);
			Result_impl<sizeof...(Consumer), Consumer...>::result(consumer, k, graph, edited);
		}

		/** Provides each consumer with the found forbidden subgraph, returns true iff no Consumer want further forbidden subgraphs */
		bool callback(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end)
		{
			return call_impl(graph, edited, begin, end, std::index_sequence_for<Consumer ...>{});
		}

		/** Prepares consumers for a new run of the Finder and starts it, but only search area near u and v */
		void feed_near(size_t k, Graph const &graph, Graph_Edits const &edited, VertexID u, VertexID v, Graph_Edits const *used)
		{
			done.fill(false);
			prepare_near_impl(u, v, std::index_sequence_for<Consumer ...>{});
			finder.find_near(graph, edited, u, v, *this, used);
			Result_impl<sizeof...(Consumer), Consumer...>::result_near(consumer, k, graph, edited);
		}

		/** Provides each consumer with the found forbidden subgraph, returns true iff no Consumer want further forbidden subgraphs */
		bool callback_near(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end)
		{
			return call_near_impl(graph, edited, begin, end, std::index_sequence_for<Consumer ...>{});
		}

	private:
		template<size_t... Is>
		void prepare_impl(std::index_sequence<Is ...>)
		{
			((std::get<Is>(consumer).prepare()), ...);
		}

		template<size_t... Is>
		bool call_impl(Graph const &graph, Graph_Edits const &edits, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end, std::index_sequence<Is ...>)
		{
			return ((!done[Is] && (done[Is] = std::get<Is>(consumer).next(graph, edits, begin, end))) & ...);
		}

		template<size_t... Is>
		void prepare_near_impl(VertexID u, VertexID v, std::index_sequence<Is ...>)
		{
			((std::get<Is>(consumer).prepare_near(u, v)), ...);
		}

		template<size_t... Is>
		bool call_near_impl(Graph const &graph, Graph_Edits const &edits, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end, std::index_sequence<Is ...>)
		{
			return ((!done[Is] && (done[Is] = std::get<Is>(consumer).next_near(graph, edits, begin, end))) & ...);
		}
	};

	/* Helper functions */

	/** Iterate over a forbidden subgraph, limited to edges currently editable
	 * calls f(iterator, iterator) for each editable edge
	 */
	template<typename Mode, typename Restriction, typename Conversion, typename Graph, typename Graph_Edits, typename Func>
	inline bool for_all_edges_ordered(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end, Func func)
	{
		if(std::is_same<Mode, Options::Modes::Edit>::value)
		{
			for(auto vit = begin + 1; vit != end; vit++)
			{
				for(auto uit = begin; uit != vit; uit++)
				{
					if(!std::is_same<Conversion, Options::Conversions::Normal>::value && uit == begin && vit == end - 1) {continue;}
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				}
			}
			if(std::is_same<Conversion, Options::Conversions::Last>::value)
			{
				auto uit = begin;
				auto vit = end - 1;
				do
				{
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if(std::is_same<Mode, Options::Modes::Delete>::value)
		{
			for(auto uit = begin, vit = begin + 1; vit != end; uit++, vit++)
			{
				if(!std::is_same<Restriction, Options::Restrictions::None>::value)
				{
					if(edited.has_edge(*uit, *vit)) {continue;}
				}
				if(func(uit, vit)) {return true;}
			}
			if(!std::is_same<Conversion, Options::Conversions::Skip>::value && graph.has_edge(*begin, *(end - 1)))
			{
				auto uit = begin;
				auto vit = end - 1;
				do
				{
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if(std::is_same<Mode, Options::Modes::Insert>::value)
		{
			for(auto vit = begin + 2; vit != end; vit++)
			{
				for(auto uit = begin; uit != vit - 1; uit++)
				{
					if(uit == begin && vit == end - 1 && (!std::is_same<Conversion, Options::Conversions::Normal>::value || graph.has_edge(*uit, *vit))) {continue;}
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				}
			}
			if(std::is_same<Conversion, Options::Conversions::Last>::value && !graph.has_edge(*begin, *(end - 1)))
			{
				auto uit = begin;
				auto vit = end - 1;
				do
				{
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				} while(false);
			}
		}
		return false;
	}

	/** Iterate over a forbidden subgraph, limited to edges currently editable, without adhering to editing order (i.e. Options::Conversion::Skip is treated as Normal)
	 * useful if order doesn't matter, e.g. when just counting edges
	 * calls f(iterator, iterator) for each editable edge
	 */
	template<typename Mode, typename Restriction, typename Conversion, typename Graph, typename Graph_Edits, typename Func>
	inline bool for_all_edges_unordered(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator begin, std::vector<VertexID>::const_iterator end, Func func)
	{
		if(std::is_same<Mode, Options::Modes::Edit>::value)
		{
			for(auto vit = begin + 1; vit != end; vit++)
			{
				for(auto uit = begin; uit != vit; uit++)
				{
					if(std::is_same<Conversion, Options::Conversions::Skip>::value && uit == begin && vit == end - 1) {continue;}
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				}
			}
		}
		else if(std::is_same<Mode, Options::Modes::Delete>::value)
		{
			for(auto uit = begin, vit = begin + 1; vit != end; uit++, vit++)
			{
				if(!std::is_same<Restriction, Options::Restrictions::None>::value)
				{
					if(edited.has_edge(*uit, *vit)) {continue;}
				}
				if(func(uit, vit)) {return true;}
			}
			if(!std::is_same<Conversion, Options::Conversions::Skip>::value && graph.has_edge(*begin, *(end - 1)))
			{
				auto uit = begin;
				auto vit = end - 1;
				do
				{
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				} while(false);
			}
		}
		else if(std::is_same<Mode, Options::Modes::Insert>::value)
		{
			for(auto vit = begin + 2; vit != end; vit++)
			{
				for(auto uit = begin; uit != vit - 1; uit++)
				{
					if(uit == begin && vit == end - 1 && (std::is_same<Conversion, Options::Conversions::Skip>::value || graph.has_edge(*uit, *vit))) {continue;}
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(func(uit, vit)) {return true;}
				}
			}
		}
		return false;
	}
}

#endif
