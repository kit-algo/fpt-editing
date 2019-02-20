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

	private:
		struct Work
		{
			Graph graph;
			Graph_Edits edited;
			size_t k;
			size_t no_edits_left;
			Lower_Bound_Storage_type lower_bound;

			Work(Graph graph, Graph_Edits edited, size_t k, size_t no_edits_left, Lower_Bound_Storage_type lower_bound) : graph(graph), edited(edited), k(k), no_edits_left(no_edits_left), lower_bound(lower_bound) {;}
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

			available_work.clear();
			available_work.push_back(std::make_unique<Work>(graph, Graph_Edits(graph.size()), k, k, Lower_Bound_Storage_type()));
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
			std::unique_ptr<Work> work;
			std::unique_ptr<Work> top;

			struct Path
			{
				ProblemSet problem;
				Lower_Bound_Storage_type lower_bound;
				std::vector<Lower_Bound_Storage_type> updated_lower_bounds;
				size_t edges_done = 0;

				Path(ProblemSet problem, Lower_Bound_Storage_type lower_bound) : problem(problem), lower_bound(lower_bound) {;}
			};
			std::deque<Path> path;

			::Finder::Feeder<Finder, Graph, Graph_Edits, Consumer...> feeder;
			::Finder::Feeder<Finder, Graph, Graph_Edits, Lower_Bound_type> lb_feeder;

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

			Worker(MT &editor, Finder const &finder, std::tuple<Consumer ...> const &consumer) : editor(editor), finder(finder), consumer(consumer), feeder(this->finder, Util::MakeTupleRef(this->consumer)), lb_feeder(this->finder, std::get<lb>(this->consumer))
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

					top = std::make_unique<Work>(work->graph, work->edited, work->k, work->no_edits_left, work->lower_bound);
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
				auto &no_edits_left = work->no_edits_left;
				auto &lower_bound = work->lower_bound;
#ifdef STATS
				calls[k]++;
#endif
				// start finder and feed into selector and lb
				feeder.feed(k, graph, edited, no_edits_left, lower_bound);

				{
					// graph solved?
					ProblemSet problem = std::get<selector>(consumer).result(k, graph, edited, Options::Tag::Selector());
					if(problem.found_solution)
					{
						std::unique_lock<std::mutex> ul(editor.write_mutex);
						editor.found_soulution = true;
						return !editor.write(graph, edited);
					}
					else if(k == 0 || k < std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound()))
					{
						// used all edits but graph still unsolved
						// or lower bound too high
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

					path.emplace_back(std::move(problem), std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound_Update()));
				}

				// Prune branches by using extra lower bounds
				if(std::is_same<Restriction, Options::Restrictions::Redundant>::value)
				{
					ProblemSet &problem = path.back().problem;

					size_t lb_counter = 0;

					for (size_t i = 0; i < problem.vertex_pairs.size(); ++i)
					{
						auto [u,v,updateLB] = problem.vertex_pairs[i];
						assert(!edited.has_edge(u, v));

						if (updateLB)
						{
#ifdef STATS
							++calls[k];
							++extra_lbs[k];
#endif
							feeder.feed(k, graph, edited, no_edits_left, lb_counter > 0 ? path.back().updated_lower_bounds.back() : path.back().lower_bound);
							if (k < std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound()))
							{
								problem.vertex_pairs.erase(problem.vertex_pairs.begin() + i, problem.vertex_pairs.end());
								break;
							}
							else
							{
								path.back().updated_lower_bounds.emplace_back(std::get<lb>(consumer).result(k, graph, edited, Options::Tag::Lower_Bound_Update()));
								++lb_counter;
							}
						}

						edited.set_edge(u, v);
					}

					for (const ProblemSet::VertexPair vp : problem.vertex_pairs)
					{
						edited.clear_edge(vp.first, vp.second);
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

						auto &graph = top->graph;
						auto &edited = top->edited;
						auto &k = top->k;
						auto &no_edits_left = top->no_edits_left;
						auto &lower_bound = path.front().lower_bound;
						ProblemSet const &problem = path.front().problem;
						auto const &edges_done = path.front().edges_done;

						size_t lb_counter = 0;

						// For non-redundant editing, we need to mark all node pairs as edited whose
						// branches were already processed.
						if(std::is_same<Restriction, Options::Restrictions::Redundant>::value)
						{
							for (size_t i = 0; i < edges_done && i < problem.vertex_pairs.size(); ++i)
							{
								auto [u,v,lb] = problem.vertex_pairs[i];
								assert(!edited.has_edge(u, v));
								edited.set_edge(u, v);
								lb_counter += lb;
							}
						}


						// For all node pairs after the current node pair, create a work package for editing
						// the node pairs.
						for (size_t i = edges_done; i < problem.vertex_pairs.size(); ++i)
						{
							auto [u,v,lb] = problem.vertex_pairs[i];
							assert(!edited.has_edge(u, v));
							lb_counter += lb;

							// Update lower bound for recursion
							Lower_Bound_Storage_type new_lower_bound;
							if (lb_counter > 0)
							{
								new_lower_bound = path.front().updated_lower_bounds[lb_counter - 1];
							}
							else
							{
								new_lower_bound = lower_bound;
							}

							new_lower_bound.remove(graph, edited, u, v);

							// Both for no-undo and for non-redundant, mark the node pair as edited
							if(!std::is_same<Restriction, Options::Restrictions::None>::value)
							{
								edited.set_edge(u, v);
							}

							// Edit the node pair
							graph.toggle_edge(u, v);
							// Create work package for recursive call with k-1
							editor.available_work.push_back(std::make_unique<Work>(graph, edited, k - 1, no_edits_left, std::move(new_lower_bound)));
							// Undo edit after the recursion
							graph.toggle_edge(u, v);

							// For no-undo, we directly unmark the node pair
							if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(u, v);}
						}

						// For single node pair editing, create an additional work package where no node pair is edited
						// but with no_edits_left - 1. Note that during the previous two loops, one or several node pairs
						// were marked as edited.
						if (problem.needs_no_edit_branch && edges_done <= problem.vertex_pairs.size())
						{
							editor.available_work.push_back(std::make_unique<Work>(graph, edited, k, no_edits_left - 1, lower_bound));
						}

						/* adjust top for recursion this thread is currently in */
						if(edges_done > 0)
						{
							if (edges_done <= problem.vertex_pairs.size())
							{
								// We are in the recursive call where (u, v) has been edited.
								auto [u,v,lb] = problem.vertex_pairs[edges_done - 1];
								graph.toggle_edge(u, v);
								--k;

								// For no-undo we also need to mark (u, v) as edited.
								// Note that for non-redundant editing, all node pairs are currently marked as edited.
								if(std::is_same<Restriction, Options::Restrictions::Undo>::value)
								{
									edited.set_edge(u, v);
								}
							}
							else
							{
								// We are in the no-edit-branch
								assert(problem.needs_no_edit_branch);
								--no_edits_left;
							}

							// For non-redundant editing, unmark all node pairs after the current node pair
							if(std::is_same<Restriction, Options::Restrictions::Redundant>::value)
							{

								for(size_t i = edges_done; i < problem.vertex_pairs.size(); ++i)
								{
									auto [u,v,lb] = problem.vertex_pairs[i];
									edited.clear_edge(u, v);
								}
							}
						}
						editor.idlers.notify_all();
						path.pop_front();
					}
				}

				if(path.empty()) {return false;}

				size_t lb_counter = 0;
				for (ProblemSet::VertexPair vertex_pair : path.back().problem.vertex_pairs)
				{
					auto [u,v,lb] = vertex_pair;
					lb_counter += lb;

					if(edited.has_edge(u, v))
					{
						abort();
					}

					// make sure the path contains the correct state so if those edits should be stolen they get the correct edits
					if (lb_counter > 0)
					{
						lower_bound = path.back().updated_lower_bounds[lb_counter - 1];
					}
					else
					{
						lower_bound = path.back().lower_bound;
					}

					lower_bound.remove(graph, edited, vertex_pair.first, vertex_pair.second);
					if(!std::is_same<Restriction, Options::Restrictions::None>::value)
					{
						edited.set_edge(vertex_pair.first, vertex_pair.second);
					}
					graph.toggle_edge(vertex_pair.first, vertex_pair.second);
					k--;
					path.back().edges_done++;
					if(edit_rec()) {return true;}
					else if(path.empty()) {return false;}
					k++;
					graph.toggle_edge(vertex_pair.first, vertex_pair.second);
					if(std::is_same<Restriction, Options::Restrictions::Undo>::value) {edited.clear_edge(vertex_pair.first, vertex_pair.second);}
				}

				if (path.back().problem.needs_no_edit_branch)
				{
					if (!std::is_same<Restriction, Options::Restrictions::Redundant>::value)
					{
						throw std::runtime_error("No edit branches are only possible with restriction Redundant");
					}

					lower_bound = path.back().lower_bound;
					--no_edits_left;
					path.back().edges_done++;
					if (edit_rec()) {return true;}
					else if(path.empty()) {return false;}
					++no_edits_left;
				}

				if(std::is_same<Restriction, Options::Restrictions::Redundant>::value)
				{
					for (ProblemSet::VertexPair vertex_pair : path.back().problem.vertex_pairs)
					{
						edited.clear_edge(vertex_pair.first, vertex_pair.second);
					}
				}
				path.pop_back();
				return false;
			}
		};
	};
}

#endif

