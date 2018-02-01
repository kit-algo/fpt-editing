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

#include "../Options.hpp"
#include "../Finder/Finder.hpp"

namespace Editor
{
	template<typename Finder, typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion, typename... Consumer>
	class MT
	{
	public:
		static constexpr char const *name = "MT";
		static constexpr bool valid = Options::has_tagged_consumer<Options::Tag::Selector, Consumer...>::value && Options::has_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value;

	private:
		struct Work
		{
			Graph graph;
			Graph_Edits edited;
			size_t k;

			Work(Graph graph, Graph_Edits edited, size_t k) : graph(graph), edited(edited), k(k) {;}
		};

		Finder &finder;
		std::tuple<Consumer ...> &consumer;
		static constexpr size_t selector = Options::get_tagged_consumer<Options::Tag::Selector, Consumer...>::value();
		static constexpr size_t lb = Options::get_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value();
		Graph &graph;

		std::deque<std::unique_ptr<Work>> available_work;
		std::mutex work_mutex;
		std::mutex write_mutex;
		size_t working;
		bool done = false;
		std::condition_variable idlers;
		size_t jobs = 4;

		bool found_soulution;
		std::function<bool(Graph const &, Graph_Edits const &)> write;

#ifdef STATS
		std::vector<size_t> calls;
		std::vector<size_t> prunes;
		std::vector<size_t> fallbacks;
		std::vector<size_t> single;
		std::vector<size_t> stolen;
		std::vector<size_t> inserted;
		std::vector<size_t> skipped;
#endif

	public:
		MT(Finder &finder, Graph &graph, std::tuple<Consumer ...> &consumer) : finder(finder), consumer(consumer), graph(graph)
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
			inserted = decltype(inserted)(k + 1, 0);
			skipped = decltype(skipped)(k + 1, 0);
#endif
			found_soulution = false;
			done = false;

			std::vector<Worker> workers;
			workers.reserve(jobs);
			for(size_t i = 0; i < jobs; i++)
			{
				workers.emplace_back(*this, finder, consumer);
			}

			inserted[k]++;
			available_work.push_back(std::make_unique<Work>(graph, Graph_Edits(graph.size()), k));
			working = jobs;
			std::vector<std::thread> threads;
			for(size_t t = 1; t < jobs; t++)
			{
				threads.emplace_back(&Worker::edit, &workers[t], k);
			}
			workers[0].edit(k);
			for(auto &t: threads) {t.join();}

#ifdef STATS

			std::cout << "per thread counters:\n";

			for(auto &worker: workers)
			{
				for(size_t i = 0; i <= k; i++)
				{
					calls[i] += worker.calls[i];
					prunes[i] += worker.prunes[i];
					fallbacks[i] += worker.fallbacks[i];
					single[i] += worker.single[i];
					stolen[i] += worker.stolen[i];
					inserted[i] += worker.inserted[i];
					skipped[i] += worker.skipped[i];
				}

				auto const stats = worker.stats();
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
			}
#endif
			return found_soulution;
		}

#ifdef STATS
		std::map<std::string, std::vector<size_t> const &> stats() const
		{
			return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}, {"stolen", stolen}, {"inserted", inserted}, {"skipped", skipped}};
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
				size_t edges_done = 0;

				Path(std::vector<VertexID> problem) : problem(problem) {;}
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
			std::vector<size_t> inserted;
			std::vector<size_t> skipped;
#endif

			Worker(MT &editor, Finder finder, std::tuple<Consumer ...> consumer) : editor(editor), finder(finder), consumer(consumer), feeder(this->finder, this->consumer)
			{
				;
			}

			std::map<std::string, std::vector<size_t> const &> stats() const
			{
				return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}, {"stolen", stolen}, {"inserted", inserted}, {"skipped", skipped}};
			}

			void edit(size_t kmax)
			{
#ifdef STATS
				calls = decltype(calls)(kmax + 1, 0);
				prunes = decltype(prunes)(kmax + 1, 0);
				fallbacks = decltype(fallbacks)(kmax + 1, 0);
				single = decltype(single)(kmax + 1, 0);
				stolen = decltype(stolen)(kmax + 1, 0);
				inserted = decltype(inserted)(kmax + 1, 0);
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

					top = std::make_unique<Work>(work->graph, work->edited, work->k);
					if(edit_rec(0))
					{
						editor.done = true;
						editor.idlers.notify_all();
						return;
					}
					path.clear();
				}
			}

		private:
			bool edit_rec(size_t indent)
			{
				auto &graph = work->graph;
				auto &edited = work->edited;
				auto &k = work->k;

				if(k == 17)
				{
					std::unique_lock<std::mutex> ul(editor.write_mutex);
					editor.write(graph, edited);
				}

//				std::cout << std::string(indent, ' ') << std::this_thread::get_id() << " k=" << k << ", #path=" << path.size() << std::endl;
#ifdef STATS
				calls[k]++;
#endif
				// start finder and feed into selector and lb
				feeder.feed(graph, edited);

				// graph solved?
				auto problem = std::get<selector>(consumer).result(k, graph, edited);
				if(problem.empty())
				{
					std::unique_lock<std::mutex> ul(editor.write_mutex);
					editor.found_soulution = true;
					return !editor.write(graph, edited);
				}
				else if(k < std::get<lb>(consumer).result())
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

				path.emplace_back(problem);
//				std::cout << std::string(indent, ' ') << std::this_thread::get_id() << " k=" << k << ", #path=" << path.size() << " #queue=" << editor.available_work.size() << " post feed" << std::endl;

				if(editor.available_work.size() < editor.jobs)
				{
					std::unique_lock<std::mutex> lock(editor.work_mutex);
//					std::cout << std::string(indent, ' ') << std::this_thread::get_id() << " k=" << k << ", #path=" << path.size() << " #queue=" << editor.available_work.size() << " queueing" << std::endl;
					// add new work to queue
					// take top problem and split
					// same logic as below, but no recursion
					// update top

					auto &graph = top->graph;
					auto &edited = top->edited;
					auto &k = top->k;
					auto const &problem = path.front().problem;
					auto const &edges_done = path.front().edges_done;

					if(problem.size() == 2)
					{
						//TODO: deletion/insertion only

						// single edge editing
						if(edited.has_edge(problem.front(), problem.back()))
						{
							abort();
						}

						//edit
						edited.set_edge(problem.front(), problem.back());
						if(edges_done < 1)
						{
							graph.toggle_edge(problem.front(), problem.back());
							k--;
							inserted[k]++;
							editor.available_work.push_back(std::make_unique<Work>(graph, edited, k));
							k++;
							graph.toggle_edge(problem.front(), problem.back());
						}

						//unedit, mark
						if(edges_done < 2)
						{
							inserted[k]++;
							editor.available_work.push_back(std::make_unique<Work>(graph, edited, k));
						}

						// if == 0: don't care: delete then return
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
						if(edges_done == 0) {fallbacks[k]++;}
#endif
						size_t edges_finished = 0;
						VertexID current_u, current_v;
						size_t current_edits;
						bool current_set = false;


						Graph before(graph);
						Graph_Edits before_edited(edited);

						if(edges_done == 0)
						{
							skipped[k - 1] += problem.size() * (problem.size() - 1) / 2 - (std::is_same<Conversion, Options::Conversions::Skip>::value ? 1 : 0);
							/*::Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, problem.begin(), problem.end(), [&](auto, auto)
							{
								skipped[k]--;
								return false;
							});*/
						}

						std::vector<std::pair<size_t, size_t>> marked;
						::Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, problem.begin(), problem.end(), [&](auto uit, auto vit)
						{
							if(!std::is_same<Restriction, Options::Restrictions::None>::value)
							{
								edited.set_edge(*uit, *vit);
							}
							edges_finished++;
							if(edges_done == 0 || edges_finished > edges_done)
							{
								skipped[k - 1]--;
								graph.toggle_edge(*uit, *vit);
								k--;
								inserted[k]++;
								editor.available_work.push_back(std::make_unique<Work>(graph, edited, k));
								k++;
								graph.toggle_edge(*uit, *vit);
							}
							else if(edges_finished == edges_done)
							{
								current_u = *uit;
								current_v = *vit;
								current_edits = marked.size();
								current_set = true;
							}
							if(std::is_same<Restriction, Options::Restrictions::Redundant>::value) {marked.emplace_back(*uit, *vit);}
							else if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(*uit, *vit);}
							return false;
						});

						if(edges_done > 0 && current_set)
						{
							graph.toggle_edge(current_u, current_v);
							k--;
							if(std::is_same<Restriction, Options::Restrictions::Redundant>::value)
							{
								for(auto it = marked.begin() + current_edits; it != marked.end(); it++)
								{
									edited.clear_edge(it->first, it->second);
								}
								marked.resize(current_edits);
							}
						}
						else if(edges_done > 0 && !current_set) {abort();}
					}
					path.pop_front();
				}

//				std::cout << std::string(indent, ' ') << std::this_thread::get_id() << " k=" << k << ", #path=" << path.size() << " #queue=" << editor.available_work.size() << " post queue" << std::endl;
				if(path.empty()) {return false;}
//				std::cout << std::string(indent, ' ') << std::this_thread::get_id() << " k=" << k << ", #path=" << path.size() << " post empty return" << std::endl;

				if(problem.size() == 2)
				{
#ifdef STATS
					single[k]++;
#endif
					abort();
					//TODO: deletion/insertion only

					// single edge editing
					if(edited.has_edge(problem.front(), problem.back()))
					{
						abort();
					}

					//edit
					graph.toggle_edge(problem.front(), problem.back());
					edited.set_edge(problem.front(), problem.back());
					k--;
					path.back().edges_done++;
					if(edit_rec(indent + 1)) {return true;}
					else if(path.empty()) {return false;}
					k++;

					//unedit, mark
					graph.toggle_edge(problem.front(), problem.back());
					path.back().edges_done++;
					if(edit_rec(indent + 1)) {return true;}
					else if(path.empty()) {return false;}

					//unmark
					edited.clear_edge(problem.front(), problem.back());
				}
				else
				{
					// normal editing
#ifdef STATS
					fallbacks[k]++;
#endif
					std::vector<std::pair<size_t, size_t>> marked;
					bool empty = false;
					skipped[k - 1] += problem.size() * (problem.size() - 1) / 2 - (std::is_same<Conversion, Options::Conversions::Skip>::value? 1 : 0);
					bool done = ::Finder::for_all_edges_ordered<Mode, Restriction, Conversion>(graph, edited, problem.begin(), problem.end(), [&](auto uit, auto vit)
					{
						skipped[k - 1]--;
						if(!std::is_same<Restriction, Options::Restrictions::None>::value)
						{
							edited.set_edge(*uit, *vit);
						}
						graph.toggle_edge(*uit, *vit);
						k--;
						path.back().edges_done++;
						if(edit_rec(indent + 1)) {return true;}
						else if(path.empty()) {empty = true; return true;}
						k++;
//						std::cout << std::string(indent, ' ') << std::this_thread::get_id() << " k=" << k << ", #path=" << path.size() << " post rec" << std::endl;
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
//				std::cout << std::string(indent, ' ') << std::this_thread::get_id() << " k=" << k << ", #path=" << path.size() << " post branch" << std::endl;
				path.pop_back();
				return false;
			}
		};
	};
}

#endif

