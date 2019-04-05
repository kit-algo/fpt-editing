#ifndef RUN_IMPL_HPP
#define RUN_IMPL_HPP

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "config.hpp"

#include "Run.hpp"
#include "Options.hpp"
#include "util.hpp"

#include "Editor/Editor.hpp"
#include "Graph/Graph.hpp"

/* Helper template to remove Consumers without effect */
template<bool S, bool LB, typename... TCon>
struct Minimize_impl;

template<bool S, bool LB, typename... T>
struct Minimize_impl<S, LB, std::tuple<T...>>
{
	using type = std::tuple<T...>;
};

template<bool S, bool LB, typename... T, typename C, typename... Con>
struct Minimize_impl<S, LB, std::tuple<T...>, C, Con...>
{
private:
	static constexpr bool is_selector = std::is_base_of<Options::Tag::Selector, C>::value;
	static constexpr bool is_lower_bound = std::is_base_of<Options::Tag::Lower_Bound, C>::value;
	static constexpr bool is_result = std::is_base_of<Options::Tag::Result, C>::value;

public:
	using type = typename std::conditional<!S && is_selector,
				typename std::conditional<!LB && is_lower_bound,
					typename Minimize_impl<true, true, std::tuple<T..., C>, Con...>::type,
					typename Minimize_impl<true, LB, std::tuple<T..., C>, Con...>::type
				>::type,
				typename std::conditional<!LB && is_lower_bound,
					typename Minimize_impl<S, true, std::tuple<T..., C>, Con...>::type,
					typename std::conditional<is_result,
						typename Minimize_impl<S, LB, std::tuple<T..., C>, Con...>::type,
						typename Minimize_impl<S, LB, std::tuple<T...>, Con...>::type
					>::type
				>::type
	>::type;
};

/* Helper template to remove Consumers without effect
 * i.e. Consumers tagged as Selector or Lower_Bound beyond the first unless they are also tagged Result
 */
template<typename... Con>
struct Minimize
{
	using type = typename Minimize_impl<false, false, std::tuple<>, Con...>::type;
};

/* Helper template to create a name from the individual components */
template<typename C, typename... Con>
struct Namer
{
	static std::string name() {return (std::string(C::name) + ... + (std::string("-") + Con::name));}
};

/* Helper template to create a name from the individual components */
template<typename C>
struct Namer<C>
{
	static std::string name() {return C::name;}
};

template<template<typename, typename, typename, typename, typename, typename, typename...> typename _E, template<typename, typename, typename, typename, typename> typename _F, template<bool> typename _G, template<bool> typename _GE, bool small, typename M, typename R, typename C, typename TCon>
struct Run_impl;

template<template<typename, typename, typename, typename, typename, typename, typename...> typename _E, template<typename, typename, typename, typename, typename> typename _F, template<bool> typename _G, template<bool> typename _GE, bool small, typename M, typename R, typename C, template<typename, typename, typename, typename, typename, typename, size_t> typename... Con>
struct Run_impl<_E, _F, _G, _GE, small, M, R, C, std::tuple<Con<_F<_G<small>, _GE<small>, M, R, C>, _G<small>, _GE<small>, M, R, C, _F<_G<small>, _GE<small>, M, R, C>::length>...>>
{
private:
	using G = _G<small>;
	using GE = _GE<small>;
	using F = _F<G, GE, M, R, C>;
	static constexpr bool valid = Editor::Consumer_valid<Con<F, G, GE, M, R, C, F::length>...>::value;

public:
	template<bool v = valid>
	static typename std::enable_if<!v, void>::type run_watch(CMDOptions const &options, std::string const &filename)
	{
		if(options.stats_json)
		{
			std::cout << "{\"type\":\"exact\",\"graph\":\"" << filename << "\",\"algo\":\"" << name() << "\",\"results\":{\"error\":\"Missing components\"}},\n";
		}
		else
		{
			std::cout << filename << ": (exact) " << name() << ": Missing Components\n";
		}
		std::cout << std::flush;
	}

	template<bool v = valid>
	static typename std::enable_if<v, void>::type run_watch(CMDOptions const &options, std::string const &filename)
	{
		int pipefd[2];
		if(pipe(pipefd))
		{
			throw std::runtime_error(std::string("pipe error: ") + strerror(errno));
		}
		int pid = fork();
		if(pid < 0)
		{
			throw std::runtime_error(std::string("fork error: ") + strerror(errno));
		}
		else if(pid > 0)
		{
			// parent
			close(pipefd[1]);
			struct pollfd pfd = {pipefd[0], POLLIN, 0};
			int p = poll(&pfd, 1, options.time_max_hard? options.time_max_hard * 1000 : -1) > 0;
			while(p > 0)
			{
				char buf[4096];
				ssize_t r = read(pipefd[0], buf, 4095);
				if(r <= 0) {break;}
				buf[r] = '\0';
				std::cout << buf << std::flush;

				p = poll(&pfd, 1, options.time_max_hard? options.time_max_hard * 1000 : -1) > 0;
			}
			if(p == 0)
			{
				// timeout (time_max_hard expired)
				kill(pid, SIGKILL);
				if(options.stats_json)
				{
					std::cout << "{\"type\":\"exact\",\"graph\":\"" << filename << "\",\"algo\":\"" << name() << "\",\"threads\":" << +options.threads << ",\"results\":{\"error\":\"Timeout\"}},\n";
				}
				else
				{
					std::cout << filename << ": (exact) " << name() << ", " << options.threads << " threads: Timeout\n";
				}
				std::cout << std::flush;
			}
			wait(NULL);
			close(pipefd[0]);
		}
		else
		{
			// child
			// redirect stdout
			close(pipefd[0]);
			dup2(pipefd[1], 1);
			close(pipefd[1]);

			// show current experiment in cmdline args
			std::string n = name() + ' ' + filename;
			strncpy(options.argv[1], n.c_str(), options.argv[options.argc - 1] + strlen(options.argv[options.argc - 1]) - options.argv[1]);

			run_nowatch(options, filename);
			exit(0);
		}
	}

	static void run_nowatch(CMDOptions const &options, std::string const &filename)
	{
		using E = _E<F, G, GE, M, R, C, Con<F, G, GE, M, R, C, F::length>...>;

		G input_graph = options.edgelist ? Graph::readEdgeList<G>(filename) : Graph::readMetis<G>(filename);
		if (!options.no_write)
		{
			Graph::writeDot(filename + ".gv", input_graph);
		}

		// permute node ids
		const std::vector<VertexID> permutation = Graph::generate_permutation(input_graph, options.permutation);
		const std::vector<VertexID> reverse_permutation = Graph::invert_permutation(permutation);
		const G g_orig = Graph::apply_permutation(input_graph, permutation);

		G graph = g_orig;
		GE edited(graph.size());

		F finder(graph.size());
		std::tuple<Con<F, G, GE, M, R, C, F::length>...> consumer{Con<F, G, GE, M, R, C, F::length>(graph.size())...};
		std::tuple<Con<F, G, GE, M, R, C, F::length> &...> consumer_ref = Util::MakeTupleRef(consumer);
		E editor(finder, graph, consumer_ref, options.threads);

		// warmup
		if(options.do_warmup)
		{
			/* CPUs are weird...
			 * executing dry runs or even NOPs(!) for a few seconds reduces the running time significantly
			 */
			std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
			std::chrono::steady_clock::duration repeat_total_time(0);
			do
			{
				std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
				repeat_total_time = t2 - t1;
			} while(std::chrono::duration_cast<std::chrono::duration<double>>(repeat_total_time).count() < 10);
		}

		size_t k_min = options.k_min;

		if (options.calculate_initial_bound)
		{
			// calculate initial lower bound, no point in trying to edit if the lower bound abort immeditaly
			typename E::Lower_Bound_type::State state =  std::get<E::lb>(consumer).initialize(std::numeric_limits<size_t>::max(), graph, edited);
			size_t bound = std::get<E::lb>(consumer).result(state, std::numeric_limits<size_t>::max(), graph, edited, Options::Tag::Lower_Bound());
			if (bound > k_min) k_min = bound;
		}

                // actual experiment
		for(size_t k = k_min; !options.k_max || k <= options.k_max; k++)
		{
			bool repeat_solved = false;
			size_t repeat_n = 0;
			double repeat_max_time = 0;
			std::chrono::steady_clock::duration repeat_total_time(0);
			do
			{
				std::chrono::steady_clock::time_point t1, t2;
				size_t writecount = 0;
				auto writegraph = [&](G const &graph, GE const &edited) -> bool
				{
					if(!options.no_write)
					{
						std::ostringstream fname;
						fname << filename << ".e." << name() << ".k" << k << ".w" << writecount;

						G graph_orig_ids = Graph::apply_permutation(graph, reverse_permutation);
						GE edited_orig_ids = Graph::apply_permutation(edited, reverse_permutation);
						Graph::writeMetis(fname.str(), graph_orig_ids);
						Graph::writeDot(fname.str() + ".gv", graph_orig_ids);
						Graph::writeMetis(fname.str() + ".edits", edited_orig_ids);
						Graph::writeDot(fname.str() + ".edits.gv", edited_orig_ids);
						Graph::writeDotCombined(fname.str() + ".combined.gv", graph_orig_ids, edited_orig_ids, input_graph);
					}
					writecount++;
					return options.all_solutions;
				};

				graph = g_orig;
				edited.clear();
				t1 = std::chrono::steady_clock::now();
				bool solved = editor.edit(k, writegraph);
				t2 = std::chrono::steady_clock::now();
				auto time_passed = t2 - t1;
				double time_passed_print = std::chrono::duration_cast<std::chrono::duration<double>>(time_passed).count();
				repeat_max_time = std::max(repeat_max_time, time_passed_print);
				repeat_solved |= solved;

				if(options.stats_json)
				{
					std::ostringstream json;
					json << "{\"type\":\"exact\",\"graph\":\"" << filename << "\",\"permutation\":" << options.permutation << ",";
					json << "\"n\":" << static_cast<size_t>(input_graph.size()) << ",\"m\":" << input_graph.count_edges() << ",";
					json << "\"algo\":\"" << name() << "\",\"threads\":" << +options.threads << ",\"k\":" << +k << ",";
					json << "\"results\":{\"solved\":\"" << (solved? "true" : "false") << "\",\"time\":" << time_passed_print;
#ifdef STATS
					json << ",\"counters\":{";
					auto const stats = editor.stats();
					bool first_stat = true;
					for(auto stat : stats)
					{
						json << (first_stat? "" : ",") << "\"" << stat.first << "\":[";
						first_stat = false;
						bool first_value = true;
						for(auto value : stat.second)
						{
							json << (first_value? "" : ",") << +value;
							first_value = false;
						}
						json << ']';
					}
					json << '}';
#endif
					json << "}},\n";
					std::cout << json.str() << std::flush;
				}
				else
				{
#ifdef STATS
					auto const &stats = editor.stats();
					if(!stats.empty())
					{
						// print stats table with aligned columns
						std::cout << '\n' << filename << " (permutation: " << options.permutation << "): (exact) " << name() << ", " << +options.threads << " threads, k = " << +k << '\n';
						std::map<std::string, std::ostringstream> output;
						{
							// header cloumn: find longest name
							size_t l = std::max_element(stats.begin(), stats.end(), [](auto const &a, auto const &b)
							{
								return a.first.length() < b.first.length();
							})->first.length();
							for(auto const &stat: stats)
							{
								output[stat.first] << std::setw(l) << stat.first << ':';
							}
							output["k"] << std::setw(l) << "k" << ':';
						}
						for(size_t j = 0; j <= k; j++)
						{
							// data columns: find largest number
							size_t m = std::max(j, std::max_element(stats.begin(), stats.end(), [&j](auto const &a, auto const &b)
							{
								return a.second[j] < b.second[j];
							})->second[j]);
							// figure out charakters needed to print it [ ceil(log_10(m)) ]
							size_t l = 0;
							do
							{
								m /= 10;
								l++;
							}
							while(m > 0);
							for(auto const &stat: stats)
							{
								output[stat.first] << " " << std::setw(l) << +stat.second[j];
							}
							output["k"] << " " << std::setw(l) << +j;
						}
						std::cout << output["k"].str() << '\n';
						for(auto const &stat: stats)
						{
							std::cout << output[stat.first].str() << ", total: " << +std::accumulate(stat.second.begin(), stat.second.end(), 0) << '\n';
						}
					}
#endif
					std::cout << filename << " (permutation: " << options.permutation << "): (exact) " << name() << ", " << +options.threads << " threads, k = " << +k << ": " << (solved ? "yes" : "no") << " [" << time_passed_print << "s]\n";
					if(solved && options.all_solutions)
					{
						std::cout << writecount << " solutions\n";
					}
					std::cout << std::flush;
				}
				repeat_n++;
				repeat_total_time += time_passed;
			} while(repeat_n < options.repeats || std::chrono::duration_cast<std::chrono::duration<double>>(repeat_total_time).count() < options.repeat_time);
			if(repeat_solved || (options.time_max && repeat_max_time >= options.time_max)) {break;}
		}
	}

	static std::string name()
	{
		/* Editor must be valid to be able to access Editor::name */
		using E = _E<F, G, GE, M, R, C, Con<F, G, GE, M, R, C, F::length>...>;
		return Namer<E, M, R, C, F, Con<F, G, GE, M, R, C, F::length>..., G>::name();
	}
};


template<template<typename, typename, typename, typename, typename, typename, typename...> typename _E, template<typename, typename, typename, typename, typename> typename _F, template<bool> typename _G, template<bool> typename _GE, bool small, typename M, typename R, typename C, template<typename, typename, typename, typename, typename, typename, size_t> typename... Con>
void Run<_E, _F, _G, _GE, small, M, R, C, Con...>::run(CMDOptions const &options, std::string const &filename)
{
	using G = _G<small>;
	using GE = _GE<small>;
	using F = _F<G, GE, M, R, C>;

	Run_impl<_E, _F, _G, _GE, small, M, R, C, typename Minimize<Con<F, G, GE, M, R, C, F::length>...>::type>::run_watch(options, filename);
}

template<template<typename, typename, typename, typename, typename, typename, typename...> typename _E, template<typename, typename, typename, typename, typename> typename _F, template<bool> typename _G, template<bool> typename _GE, bool small, typename M, typename R, typename C, template<typename, typename, typename, typename, typename, typename, size_t> typename... Con>
std::string Run<_E, _F, _G, _GE, small, M, R, C, Con...>::name()
{
	using G = _G<small>;
	using GE = _GE<small>;
	using F = _F<G, GE, M, R, C>;
	return Run_impl<_E, _F, _G, _GE, small, M, R, C, typename Minimize<Con<F, G, GE, M, R, C, F::length>...>::type>::name();
}

#endif
