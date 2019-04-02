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
#include "../ProblemSet.hpp"

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
		using State_Tuple_type = std::tuple<typename Consumer::State...>;

	private:
		struct Work
		{
			Graph graph;
			Graph_Edits edited;
			size_t k;
			State_Tuple_type state;

			Work(Graph graph, Graph_Edits edited, size_t k, State_Tuple_type state) : graph(std::move(graph)), edited(std::move(edited)), k(k), state(std::move(state)) {;}
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
		std::vector<size_t> extra_lbs;
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
			extra_lbs = decltype(extra_lbs)(k + 1, 0);
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

			workers[0].initialize(graph, Graph_Edits(graph.size()), k);

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
					extra_lbs[i] += worker.extra_lbs[i];
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
			return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}, {"extra_lbs", extra_lbs}, {"stolen", stolen}, {"skipped", skipped}};
		}
#endif

	private:
		class Worker
		{
		private:
			MT &editor;
			Finder finder;
			std::tuple<Consumer ...> consumer;
			std::unique_ptr<Graph> top_graph, bottom_graph;
			std::unique_ptr<Graph_Edits> top_edited, bottom_edited;
			size_t top_k;

			struct Path
			{
				ProblemSet problem;
				std::vector<State_Tuple_type> states;
				size_t edges_done = 0;

				Path(ProblemSet problem) : problem(std::move(problem)) {;}
			};
			std::deque<Path> path;
		public:
#ifdef STATS
			std::vector<size_t> calls;
			std::vector<size_t> prunes;
			std::vector<size_t> fallbacks;
			std::vector<size_t> single;
			std::vector<size_t> extra_lbs;
			std::vector<size_t> stolen;
			std::vector<size_t> skipped;
#endif

			Worker(MT &editor, Finder const &finder, std::tuple<Consumer ...> const &consumer) : editor(editor), finder(finder), consumer(consumer)
			{
				;
			}

			std::map<std::string, std::vector<size_t> const &> stats() const
			{
#ifdef STATS
				return {{"calls", calls}, {"prunes", prunes}, {"fallbacks", fallbacks}, {"single", single}, {"stolen", stolen}, {"skipped", skipped}, {"extra_lbs", extra_lbs}};
#else
				return {};
#endif
			}

			void initialize(const Graph& graph, const Graph_Edits &edited, size_t kmax)
			{
				editor.available_work.clear();
				editor.available_work.push_back(std::make_unique<Work>(graph, edited, kmax, Util::for_make_tuple<sizeof...(Consumer)>([&](auto i) {
					return std::get<i.value>(consumer).initialize(kmax, graph, edited);
				})));
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
				extra_lbs = decltype(extra_lbs)(kmax + 1, 0);
				stolen = decltype(stolen)(kmax + 1, 0);
				skipped = decltype(skipped)(kmax + 1, 0);
#endif
				while(true)
				{
					std::unique_lock<std::mutex> lock(editor.work_mutex);
					std::unique_ptr<Work> work;
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

					top_graph = std::make_unique<Graph>(work->graph);
					bottom_graph = std::make_unique<Graph>(std::move(work->graph));
					top_edited = std::make_unique<Graph>(work->edited);
					bottom_edited = std::make_unique<Graph>(std::move(work->edited));
					top_k = work->k;
					if(edit_rec(work->k, std::move(work->state)))
					{
						editor.done = true;
						editor.idlers.notify_all();
						return;
					}

					path.clear();
				}
			}

		private:
			bool edit_rec(size_t k, State_Tuple_type initial_state)
			{
#ifdef STATS
				calls[k]++;
#endif
				{
					if (k < std::get<lb>(consumer).result(std::get<lb>(initial_state), k, *bottom_graph, *bottom_edited, Options::Tag::Lower_Bound()))
					{
						// lower bound too high
#ifdef STATS
						prunes[k]++;
#endif
						return false;
					}

					// graph solved?
					ProblemSet problem = std::get<selector>(consumer).result(std::get<selector>(initial_state), k, *bottom_graph, *bottom_edited, Options::Tag::Selector());
					if(problem.found_solution)
					{
						std::unique_lock<std::mutex> ul(editor.write_mutex);
						editor.found_soulution = true;
						return !editor.write(*bottom_graph, *bottom_edited);
					}
					else if(k == 0)
					{
						// used all edits but graph still unsolved
#ifdef STATS
						prunes[k]++;
#endif
						return false;
					}

#ifdef STATS
					if (!problem.needs_no_edit_branch)
					{
						fallbacks[k]++;
						skipped[k - 1] += Finder::length * (Finder::length - 1) / 2 - (std::is_same<Conversion, Options::Conversions::Skip>::value ? 1 : 0) - problem.vertex_pairs.size();
					}
					else
					{
						single[k]++;
					}
#endif

					path.emplace_back(std::move(problem));
					path.back().states.emplace_back(std::move(initial_state));
				}

				// Prune branches by using extra lower bounds
				if constexpr (std::is_same<Restriction, Options::Restrictions::Redundant>::value)
				{
					ProblemSet &problem = path.back().problem;
					std::vector<State_Tuple_type> &states = path.back().states;

					for (size_t i = 0; i < problem.vertex_pairs.size(); ++i)
					{
						auto [u,v,updateLB] = problem.vertex_pairs[i];
						assert(!bottom_edited->has_edge(u, v));

						if (updateLB)
						{
#ifdef STATS
							++calls[k];
							++extra_lbs[k];
#endif
							if (k < std::get<lb>(consumer).result(std::get<lb>(states.back()), k, *bottom_graph, *bottom_edited, Options::Tag::Lower_Bound()))
							{
								problem.vertex_pairs.erase(problem.vertex_pairs.begin() + i, problem.vertex_pairs.end());
								break;
							}
						}

						states.emplace_back(states.back());

						Util::for_<sizeof...(Consumer)>([&, u = u, v = v](auto i)
						{
							std::get<i.value>(consumer).before_mark(std::get<i.value>(states.back()), *bottom_graph, *bottom_edited, u, v);
						});

						bottom_edited->set_edge(u, v);

						Util::for_<sizeof...(Consumer)>([&, u = u, v = v](auto i)
						{
							std::get<i.value>(consumer).after_mark(std::get<i.value>(states.back()), *bottom_graph, *bottom_edited, u, v);
						});
					}

					for (const ProblemSet::VertexPair vp : problem.vertex_pairs)
					{
						bottom_edited->clear_edge(vp.first, vp.second);
					}
				}

				if(editor.available_work.size() < editor.threads)
				{
					// skip sharing if some other thread already does
					std::unique_lock<std::mutex> lock(editor.work_mutex, std::try_to_lock);
					if(lock)
					{
						// add new work to queue
						// take top problem and split (same logic as below, but no recursion)
						// update top

						const ProblemSet &problem = path.front().problem;
						const size_t &edges_done = path.front().edges_done;

						// For non-redundant editing, we need to mark all node pairs as edited whose
						// branches were already processed.
						if constexpr (std::is_same<Restriction, Options::Restrictions::Redundant>::value)
						{
							for (size_t i = 0; i < edges_done && i < problem.vertex_pairs.size(); ++i)
							{
								auto [u,v,lb] = problem.vertex_pairs[i];
								assert(!top_edited->has_edge(u, v));
								top_edited->set_edge(u, v);
							}
						}


						// For all node pairs after the current node pair, create a work package for editing
						// the node pairs.
						for (size_t i = edges_done; i < problem.vertex_pairs.size(); ++i)
						{
							auto [u,v,lb] = problem.vertex_pairs[i];
							assert(!top_edited->has_edge(u, v));

							// Update lower bound for recursion
							State_Tuple_type next_state(std::is_same<Restriction, Options::Restrictions::Redundant>::value ? std::move(path.front().states[i]) : path.front().states[0]);

							Util::for_<sizeof...(Consumer)>([&, u = u, v = v](auto i)
							{
								std::get<i.value>(consumer).before_mark_and_edit(std::get<i.value>(next_state), *top_graph, *top_edited, u, v);
							});

							// Both for no-undo and for non-redundant, mark the node pair as edited
							if constexpr (!std::is_same<Restriction, Options::Restrictions::None>::value)
							{
								top_edited->set_edge(u, v);
							}

							// Edit the node pair
							top_graph->toggle_edge(u, v);

							Util::for_<sizeof...(Consumer)>([&, u = u, v = v](auto i)
							{
								std::get<i.value>(consumer).after_mark_and_edit(std::get<i.value>(next_state), *top_graph, *top_edited, u, v);
							});

							// Create work package for recursive call with k-1
							editor.available_work.push_back(std::make_unique<Work>(*top_graph, *top_edited, top_k - 1, std::move(next_state)));

							// Undo edit after the recursion
							top_graph->toggle_edge(u, v);

							// For no-undo, we directly unmark the node pair
							if constexpr (std::is_same<Restriction, Options::Restrictions::Undo>::value) {top_edited->clear_edge(u, v);}
						}

						// For single node pair editing, create an additional work package where no node pair is edited
						// but with no_edits_left - 1. Note that during the previous two loops, one or several node pairs
						// were marked as edited.
						if (problem.needs_no_edit_branch && edges_done <= problem.vertex_pairs.size())
						{
							editor.available_work.push_back(std::make_unique<Work>(*top_graph, *top_edited, top_k, std::move(path.front().states.back())));
						}

						/* adjust top for recursion this thread is currently in */
						if(edges_done > 0)
						{
							if (edges_done <= problem.vertex_pairs.size())
							{
								// We are in the recursive call where (u, v) has been edited.
								auto [u,v,lb] = problem.vertex_pairs[edges_done - 1];
								top_graph->toggle_edge(u, v);
								--top_k;

								// For no-undo we also need to mark (u, v) as edited.
								// Note that for non-redundant editing, all node pairs are currently marked as edited.
								if(std::is_same<Restriction, Options::Restrictions::Undo>::value)
								{
									top_edited->set_edge(u, v);
								}
							}
							else
							{
								// We are in the no-edit-branch
								assert(problem.needs_no_edit_branch);
							}

							// For non-redundant editing, unmark all node pairs after the current node pair
							if constexpr (std::is_same<Restriction, Options::Restrictions::Redundant>::value)
							{

								for(size_t i = edges_done; i < problem.vertex_pairs.size(); ++i)
								{
									auto [u,v,lb] = problem.vertex_pairs[i];
									top_edited->clear_edge(u, v);
								}
							}
						}
						editor.idlers.notify_all();
						path.pop_front();
					}
				}

				if(path.empty()) {return false;}

				for (size_t i = 0; i < path.back().problem.vertex_pairs.size(); ++i)
				{
					auto [u,v,lb] = path.back().problem.vertex_pairs[i];

					if(bottom_edited->has_edge(u, v))
					{
						abort();
					}

					// Update state for recursion
					State_Tuple_type next_state(std::is_same<Restriction, Options::Restrictions::Redundant>::value ? std::move(path.back().states[i]) : path.back().states[0]);

					Util::for_<sizeof...(Consumer)>([&, u = u, v = v](auto i)
					{
						std::get<i.value>(consumer).before_mark_and_edit(std::get<i.value>(next_state), *bottom_graph, *bottom_edited, u, v);
					});

					if constexpr (!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						bottom_edited->set_edge(u, v);
					}

					bottom_graph->toggle_edge(u, v);

					Util::for_<sizeof...(Consumer)>([&, u = u, v = v](auto i)
					{
						std::get<i.value>(consumer).after_mark_and_edit(std::get<i.value>(next_state), *bottom_graph, *bottom_edited, u, v);
					});

					path.back().edges_done++;

					if(edit_rec(k - 1, std::move(next_state))) {return true;}
					else if(path.empty()) {return false;}

					bottom_graph->toggle_edge(u, v);

					if constexpr (std::is_same<Restriction, Options::Restrictions::Undo>::value) {bottom_edited->clear_edge(u, v);}
				}

				if (path.back().problem.needs_no_edit_branch)
				{
					if constexpr (!std::is_same<Restriction, Options::Restrictions::Redundant>::value)
					{
						throw std::runtime_error("No edit branches are only possible with restriction Redundant");
					}

					path.back().edges_done++;
					if (edit_rec(k, std::move(path.back().states.back()))) {return true;}
					else if(path.empty()) {return false;}
				}

				if constexpr (std::is_same<Restriction, Options::Restrictions::Redundant>::value)
				{
					for (ProblemSet::VertexPair vertex_pair : path.back().problem.vertex_pairs)
					{
						bottom_edited->clear_edge(vertex_pair.first, vertex_pair.second);
					}
				}
				path.pop_back();
				return false;
			}
		};
	};
}

#endif

