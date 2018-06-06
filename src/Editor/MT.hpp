#ifndef EDITOR_MT_HPP
#define EDITOR_MT_HPP

#include <assert.h>

#include <iomanip>
#include <algorithm>
#include <iostream>
#include <numeric>

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <typeinfo>
#include <utility>
#include <vector>

#include "../config.hpp"

#include "Editor.hpp"
#include "../util.hpp"
#include "../Options.hpp"
#include "../Finder/Finder.hpp"
#include "../LowerBound/Lower_Bound.hpp"

namespace Editor
{
	template<typename Finder, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, typename... Consumer>
	class MT
	{
	public:
		static constexpr char const *name = "MT";
		static_assert(Consumer_valid<Consumer...>::value, "Missing Selector and/or Lower_Bound");

		static constexpr size_t selector = Options::get_tagged_consumer<Options::Tag::Selector, Consumer...>::value;
		static constexpr size_t lb = Options::get_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value;
		using Selector_type = typename std::tuple_element<selector, std::tuple<Consumer ...>>::type;
		using Lower_Bound_type = typename std::tuple_element<lb, std::tuple<Consumer ...>>::type;
		using Lower_Bound_Storage_type = ::Lower_Bound::Lower_Bound<Mode, Restriction, Conversion, Graph, Graph_Edits, Finder::length>;

	private:
		struct Work
		{
			Graph graph;
			Graph_Edits edited;
			size_t k;
			Lower_Bound_Storage_type lower_bound;

			Work(Graph graph, Graph_Edits edited, size_t k, Lower_Bound_Storage_type lower_bound) : graph(graph), edited(edited), k(k), lower_bound(lower_bound) {;}
		};

		Finder &finder;
		std::tuple<Consumer &...> consumer;
		Graph &graph;

		std::deque<std::unique_ptr<Work>> available_work;
		std::mutex work_mutex;
		std::mutex write_mutex;
		size_t working;
		bool done = false;
		std::condition_variable idlers;
		size_t threads;

		bool found_soulution;
		std::function<bool(Graph const &, Graph_Edits const &)> write;

#ifdef STATS
		std::vector<size_t> calls;
		std::vector<size_t> prunes;
		std::vector<size_t> fallbacks;
		std::vector<size_t> single;
		std::vector<size_t> stolen;
		std::vector<size_t> skipped;
#endif

	public:
		MT(Finder &finder, Graph &graph, std::tuple<Consumer &...> consumer, size_t threads) : finder(finder), consumer(consumer), graph(graph), threads(threads)
		{
			;
		}

		bool edit(size_t k, decltype(write) const &writegraph)
		{
			write = writegraph;
#ifdef STATS
			calls = decltype(calls)(k + 1, 0);
			prunes = decltype(prunes)(k + 1, 0);
			fallbacks = decltype(fallbacks)(k + 1, 0);
			single = decltype(single)(k + 1, 0);
			stolen = decltype(stolen)(k + 1, 0);
			skipped = decltype(skipped)(k + 1, 0);
#endif
			found_soulution = false;
			done = false;

			std::vector<Worker> workers;
			workers.reserve(threads);
			for(size_t i = 0; i < threads; i++)
			{
				workers.emplace_back(*this, finder, consumer);
			}

			available_work.clear();
			available_work.push_back(std::make_unique<Work>(graph, Graph_Edits(graph.size()), k, Lower_Bound_Storage_type()));
			working = threads;
			std::vector<std::thread> work_threads;
			for(size_t t = 1; t < threads; t++)
			{
				work_threads.emplace_back(&Worker::edit, &workers[t], k);
			}
			workers[0].edit(k);
			for(auto &t: work_threads) {t.join();}

#ifdef STATS
			for(auto &worker: workers)
			{
				for(size_t i = 0; i <= k; i++)
				{
					calls[i] += worker.calls[i];
					prunes[i] += worker.prunes[i];
					fallbacks[i] += worker.fallbacks[i];
					single[i] += worker.single[i];
					stolen[i] += worker.stolen[i];
					skipped[i] += worker.skipped[i];
				}

				/*auto const stats = worker.stats();
				std::ostringstream json;
				json << "{";
				bool first_stat = true;
				for(auto stat : stats)
				{
					json << (first_stat ? "" : ",") << "\"" << stat.first << "\":[";
					first_stat = false;
					bool first_value = true;
					for(auto value : stat.second)
					{
						json << (first_value ? "" : ",") << +value;
						first_value = false;
					}
					json << ']';
				}
				json << "},";
				std::cout << json.str() << std::endl;

				std::map<std::string, std::ostringstream> output;
				{
					size_t l = std::max_element(stats.begin(), stats.end(), [](typename decltype(stats)::value_type const & a, typename decltype(stats)::value_type const & b)
					{
						return a.first.length() < b.first.length();
					})->first.length();
					for(auto &stat : stats)
					{
						output[stat.first] << std::setw(l) << stat.first << ':';
					}
				}
				for(size_t j = 0; j <= k; j++)
				{
					size_t m = std::max_element(stats.begin(), stats.end(), [&j](typename decltype(stats)::value_type const & a, typename decltype(stats)::value_type const & b)
					{
						return a.second[j] < b.second[j];
					})->second[j];
					size_t l = 0;
					do
					{
						m /= 10;
						l++;
					}
					while(m > 0);
					for(auto &stat : stats)
					{
						output[stat.first] << " " << std::setw(l) << +stat.second[j];
					}
				}
				for(auto &stat: stats) {std::cout << output[stat.first].str() << ", total: " << +std::accumulate(stat.second.begin(), stat.second.end(), 0) << std::endl;}
				*/
			}
#endif
			return found_soulution;
		}

#ifdef STATS
		std::map<std::string, std::vector<size_t> const &> stats() const
		{
			return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}, {"stolen", stolen}, {"skipped", skipped}};
		}
#endif

	private:
		class Worker
		{
		private:
			MT &editor;
			Finder finder;
			std::tuple<Consumer ...> consumer;
			std::unique_ptr<Work> work;
			std::unique_ptr<Work> top;

			struct Path
			{
				std::vector<VertexID> problem;
				Lower_Bound_Storage_type lower_bound;
				size_t edges_done = 0;

				Path(std::vector<VertexID> problem, Lower_Bound_Storage_type lower_bound) : problem(problem), lower_bound(lower_bound) {;}
			};
			std::deque<Path> path;

			::Finder::Feeder<Finder, Graph, Graph_Edits, Consumer...> feeder;

		public:
#ifdef STATS
			std::vector<size_t> calls;
			std::vector<size_t> prunes;
			std::vector<size_t> fallbacks;
			std::vector<size_t> single;
			std::vector<size_t> stolen;
			std::vector<size_t> skipped;
#endif

			Worker(MT &editor, Finder const &finder, std::tuple<Consumer ...> const &consumer) : editor(editor), finder(finder), consumer(consumer), feeder(this->finder, Util::MakeTupleRef(this->consumer))
			{
				;
			}

			std::map<std::string, std::vector<size_t> const &> stats() const
			{
#ifdef STATS
				return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}, {"stolen", stolen}, {"skipped", skipped}};
#else
				return {};
#endif
			}

#ifdef STATS
			void edit(size_t kmax)
#else
			void edit(size_t)
#endif
			{
#ifdef STATS
				calls = decltype(calls)(kmax + 1, 0);
				prunes = decltype(prunes)(kmax + 1, 0);
				fallbacks = decltype(fallbacks)(kmax + 1, 0);
				single = decltype(single)(kmax + 1, 0);
				stolen = decltype(stolen)(kmax + 1, 0);
				skipped = decltype(skipped)(kmax + 1, 0);
#endif
				while(true)
				{
					std::unique_lock<std::mutex> lock(editor.work_mutex);
//					std::cout << std::this_thread::get_id() << " needs work" << std::endl;
					editor.working--;
					while(true)
					{
						if(editor.done)
						{
//							std::cout << std::this_thread::get_id() << " other done" << std::endl;
							return;
						}
						else if(!editor.available_work.empty())
						{
							work = std::move(editor.available_work.front());
							editor.available_work.pop_front();
							//finder.recalculate();
#ifdef STATS
							stolen[work->k]++;
#endif
							break;
						}
						else if(editor.working == 0)
						{
							/* all threads are waiting for work -> we are done */
//							std::cout << std::this_thread::get_id() << " detect done" << std::endl;
							editor.done = true;
							editor.idlers.notify_all();
							return;
						}
						else
						{
//							std::cout << std::this_thread::get_id() << " idle" << std::endl;
							editor.idlers.wait(lock);
//							std::cout << std::this_thread::get_id() << " notified" << std::endl;
						}
					}
					editor.working++;
//					std::cout << std::this_thread::get_id() << " got work" << std::endl;
					lock.unlock();

					top = std::make_unique<Work>(work->graph, work->edited, work->k, work->lower_bound);
					if(edit_rec())
					{
						editor.done = true;
						editor.idlers.notify_all();
						return;
					}
					path.clear();
				}
			}

		private:
			bool edit_rec()
			{
				auto &graph = work->graph;
				auto &edited = work->edited;
				auto &k = work->k;
				auto &lower_bound = work->lower_bound;
#ifdef STATS
				calls[k]++;
#endif
				// start finder and feed into selector and lb
				feeder.feed(k, graph, edited, lower_bound);

				// graph solved?
				auto problem = std::get<selector>(consumer).result(k, graph, edited, Options::Tag::Selector());
				if(problem.empty())
				{
					std::unique_lock<std::mutex> ul(editor.write_mutex);
					editor.found_soulution = true;
					return !editor.write(graph, edited);
				}
				else if(k < std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound()))
				{
					// lower bound too high
#ifdef STATS
					prunes[k]++;
#endif
					return false;
				}
				else if(k == 0 && !problem.empty())
				{
					// used all edits but graph still unsolved
					return false;
				}

				path.emplace_back(problem, std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound_Update()));

				if(editor.available_work.size() < editor.threads)
				{
					// skip sharing if some other thread already does
					std::unique_lock<std::mutex> lock(editor.work_mutex, std::try_to_lock);
					if(lock)
					{
						// add new work to queue
						// take top problem and split (same logic as below, but no recursion)
						// update top

						auto &graph = top->graph;
						auto &edited = top->edited;
						auto &k = top->k;
						auto &lower_bound = path.front().lower_bound;
						auto const &problem = path.front().problem;
						auto const &edges_done = path.front().edges_done;

						if(problem.size() == 2)
						{
							// single edge editing
#ifdef STATS
							if(edges_done == 0)
							{
								single[k]++;
							}
#endif
							if(edited.has_edge(problem.front(), problem.back()))
							{
								abort();
							}

							//edit
							if(edges_done < 1)
							{
								Lower_Bound_Storage_type new_lower_bound(lower_bound);
								new_lower_bound.remove(graph, edited, problem.front(), problem.back());

								edited.set_edge(problem.front(), problem.back());
								graph.toggle_edge(problem.front(), problem.back());
								k--;
								editor.available_work.push_back(std::make_unique<Work>(graph, edited, k, new_lower_bound));
								k++;
								graph.toggle_edge(problem.front(), problem.back());
							} else {
								edited.set_edge(problem.front(), problem.back());
							}

							//unedit, mark
							if(edges_done < 2)
							{
								editor.available_work.push_back(std::make_unique<Work>(graph, edited, k, lower_bound));
							}

							// adjust top:
							// if edges_done == 0: don't care: delete then return
							// if == 1: dec k, g.toggle
							if(edges_done == 1)
							{
								k--;
								graph.toggle_edge(problem.front(), problem.back());
							}

							// if == 2: top in correct state
						}
						else
						{
							// normal editing
#ifdef STATS
							if(edges_done == 0)
							{
								fallbacks[k]++;
								skipped[k - 1] += problem.size() * (problem.size() - 1) / 2 - (std::is_same<Conversion, Options::Conversions::Skip>::value ? 1 : 0);
							}
#endif
							size_t edges_finished = 0;
							VertexID current_u = 0, current_v = 0;
							size_t current_marks = 0;

							std::vector<std::pair<size_t, size_t>> marked;
							::Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, problem.begin(), problem.end(), [&](auto uit, auto vit)
							{
								Lower_Bound_Storage_type updated_lower_bound(lower_bound);
								updated_lower_bound.remove(graph, edited, *uit, *vit);

								if(!std::is_same<Restriction, Options::Restrictions::None>::value)
								{
									edited.set_edge(*uit, *vit);
								}
								edges_finished++;
								if(edges_done == 0 || edges_finished > edges_done)
								{
#ifdef STATS
									skipped[k - 1]--;
#endif
									graph.toggle_edge(*uit, *vit);
									k--;
									editor.available_work.push_back(std::make_unique<Work>(graph, edited, k, updated_lower_bound));
									k++;
									graph.toggle_edge(*uit, *vit);
								}
								else if(edges_finished == edges_done)
								{
									current_u = *uit;
									current_v = *vit;
									current_marks = marked.size() + 1;
								}
								if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
								else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
								return false;
							});

							/* adjust top for recursion this thread is currently in */
							if(edges_done > 0)
							{
								assert(current_u || current_v);
								graph.toggle_edge(current_u, current_v);
								k--;
								if(std::is_same<Restriction, Options::Restrictions::Redundant>::value)
								{
									for(auto it = marked.begin() + current_marks; it != marked.end(); it++)
									{
										edited.clear_edge(it->first, it->second);
									}
									marked.resize(current_marks);
								}
								else if(std::is_same<Restriction, Options::Restrictions::Undo>::value)
								{
									edited.set_edge(current_u, current_v);
								}
#define COMMA ,
								assert(std::is_same<Restriction COMMA Options::Restrictions::None>::value || edited.has_edge(current_u, current_v));
#undef COMMA
							}
						}
						editor.idlers.notify_all();
						path.pop_front();
					}
				}

				if(path.empty()) {return false;}

				if(problem.size() == 2)
				{
#ifdef STATS
					single[k]++;
#endif
					// single edge editing
					if(edited.has_edge(problem.front(), problem.back()))
					{
						abort();
					}

					//edit
					// update lower bound
					lower_bound = path.back().lower_bound;
					lower_bound.remove(graph, edited, problem.front(), problem.back());
					graph.toggle_edge(problem.front(), problem.back());
					edited.set_edge(problem.front(), problem.back());
					k--;
					path.back().edges_done++;
					if(edit_rec()) {return true;}
					else if(path.empty()) {return false;}
					k++;

					//unedit, mark
					// restore lower bound from path
					lower_bound = path.back().lower_bound;
					graph.toggle_edge(problem.front(), problem.back());
					path.back().edges_done++;
					if(edit_rec()) {return true;}
					else if(path.empty()) {return false;}

					//unmark
					edited.clear_edge(problem.front(), problem.back());
				}
				else
				{
					// normal editing
#ifdef STATS
					fallbacks[k]++;
					skipped[k - 1] += problem.size() * (problem.size() - 1) / 2 - (std::is_same<Conversion, Options::Conversions::Skip>::value? 1 : 0);
#endif
					std::vector<std::pair<size_t, size_t>> marked;
					bool empty = false;
					bool done = ::Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, problem.begin(), problem.end(), [&](auto uit, auto vit)
					{
#ifdef STATS
						skipped[k - 1]--;
						lower_bound = path.back().lower_bound;
						lower_bound.remove(graph, edited, *uit, *vit);
#endif
						if(!std::is_same<Restriction, Options::Restrictions::None>::value)
						{
							edited.set_edge(*uit, *vit);
						}
						graph.toggle_edge(*uit, *vit);
						k--;
						path.back().edges_done++;
						if(edit_rec()) {return true;}
						else if(path.empty()) {empty = true; return true;}
						k++;
						graph.toggle_edge(*uit, *vit);
						if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
						else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
						return false;
					});
					if(done) {return !empty;}

					if(std::is_same<Restriction, Options::Restrictions::Redundant>::value)
					{
						for(auto it = marked.rbegin(); it != marked.rend(); it++)
						{
							edited.clear_edge(it->first, it->second);
						}
					}
				}
				path.pop_back();
				return false;
			}
		};
	};
}

#endif

