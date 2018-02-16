#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "config.hpp"

#include "Consumer/Counter.hpp"

#include "Run.hpp"
#include "Options.hpp"
#include "util.hpp"

template<typename E, typename F, typename G, typename GE, typename M, typename R, typename C, typename... Con>
void Run<E, F, G, GE, M, R, C, Con...>::run_watch(CMDOptions const &options, std::string const &filename)
{
	if(!options.time_max_hard) {run(options, filename);}
	else
	{
		// running with hard time limit
		// run experiment in child, kill it time_max_hard seconds after last output
		int pipefd[2];
		if(pipe2(pipefd, O_NONBLOCK))
		{
			std::cerr << "pipe error" << std::endl;
			abort();
		}
		int pid = fork();
		if(pid < 0)
		{
			std::cerr << "fork error" << std::endl;
			abort();
		}
		else if(pid > 0)
		{
			//parent
			close(pipefd[1]);
			struct pollfd pfd = {pipefd[0], POLLIN, 0};
			int p = poll(&pfd, 1, options.time_max_hard * 1000) > 0;
			while(p > 0)
			{
				char buf[4096];
				ssize_t r = read(pipefd[0], buf, 4095);
				if(r <= 0) {break;}
				std::cout << std::string(buf, r);

				p = poll(&pfd, 1, options.time_max_hard * 1000) > 0;
			}
			if(p == 0)
			{
				//timeout
				kill(pid, SIGKILL);
				std::cout << std::flush << filename << ": (exact) " << name() << ": Timeout" << std::endl;
			}
			wait(NULL);
		}
		else
		{
			//child
			//find binary
			char buf[4096];
			ssize_t r = readlink("/proc/self/exe", buf, 4095);
			if(r <= 0)
			{
				std::cerr << "readlink error" << std::endl;
				abort();
			}
			buf[r] = '\0';
			//construct arguments
			std::string k = std::to_string(options.k_min);
			std::string K = std::to_string(options.k_max);
			std::string t = std::to_string(std::min(options.time_max, options.time_max_hard));
			std::string j = std::to_string(options.threads);

			std::string boolopts = "-_";
			if(options.all_solutions) {boolopts.push_back('a');}
			if(options.no_write) {boolopts.push_back('W');}
			if(options.stats_json) {boolopts.push_back('J');}

			char const *editheur = "-e"; //std::is_base_of<Editor::is_editor, E>::value? "-e" : "-h";
			char const *argv[] = {buf,
				"-k", k.data(), "-K", K.data(), "-t", t.data(), "-j", j.data(), boolopts.data(),
				"-M", M::name, "-R", R::name, "-C", C::name,
				editheur, E::name, "-f", F::name, "-s", E::Selector_type::name, "-b", E::Lower_Bound_type::name, "-g", G::name,
				filename.data(), NULL
			};

			//redirect stdout
			close(pipefd[0]);
			dup2(pipefd[1], 1);
			close(pipefd[1]);

			execv(buf, (char * const *) argv);
			std::cerr << "execv error" << std::endl;
			abort();
		}

	}
}

template<typename E, typename F, typename G, typename GE, typename M, typename R, typename C, typename... Con>
void Run<E, F, G, GE, M, R, C, Con...>::run(CMDOptions const &options, std::string const &filename)
{
	G graph = Graph::readMetis<G>(filename);
	Graph::writeDot(filename + ".gv", graph);
	G g_orig = graph;

	F finder(graph.size());
	std::tuple<Con...> consumer{Con(graph.size())...};
	std::tuple<Con &...> consumer_ref = Util::MakeTupleRef(consumer);
	E editor(finder, graph, consumer_ref, options.threads);

	// calculate initial lower bound, no point in trying to edit if the lower bound abort immeditaly
	Consumer::Counter<G, GE, M, R, C> counter(graph.size());
	Finder::Feeder<F, G, GE, typename E::Lower_Bound_type, decltype(counter)> feeder(finder, std::get<E::lb>(consumer), counter);
	feeder.feed(0, graph, GE(graph.size()));
	size_t bound = std::get<E::lb>(consumer).result(0, graph, GE(graph.size()), Options::Tag::Lower_Bound());

	//finder.recalculate();
	for(size_t k = std::max(bound, options.k_min); !options.k_max || k <= options.k_max; k++)
	{
		std::chrono::steady_clock::time_point t1, t2;
		size_t writecount = 0;
		auto writegraph = [&](G const &graph, GE const &edited) -> bool
		{
			if(!options.no_write)
			{
				std::ostringstream fname;
				fname << filename << ".e." << name() << ".k" << k << ".w" << writecount;
				Graph::writeMetis(fname.str(), graph);
				Graph::writeDot(fname.str() + ".gv", graph);
				Graph::writeMetis(fname.str() + ".edits", edited);
				Graph::writeDot(fname.str() + ".edits.gv", edited);
				Graph::writeDotCombined(fname.str() + ".combined.gv", graph, edited, g_orig);
			}
			writecount++;
			return options.all_solutions;
		};

		t1 = std::chrono::steady_clock::now();
		bool solved = editor.edit(k, writegraph);
		t2 = std::chrono::steady_clock::now();
		double time_passed = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1).count();

		if(options.stats_json)
		{
			std::ostringstream json;
			json << "{\"type\":\"exact\",\"graph\":\"" << filename << "\",\"algo\":\"" << name() << "\",\"k\":" << k << ",";
			json << "\"results\":{\"solved\":\"" << (solved? "true" : "false") << "\",\"time\":" << time_passed;
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
			json << "}},";
			std::cout << json.str() << std::endl;
		}
		else
		{
#ifdef STATS
			std::cout << std::endl << filename << ": (exact) " << name() << ", k = " << k << '\n';
			auto const &stats = editor.stats();
			std::map<std::string, std::ostringstream> output;
			{
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
				size_t m = std::max(j, std::max_element(stats.begin(), stats.end(), [&j](auto const &a, auto const &b)
				{
					return a.second[j] < b.second[j];
				})->second[j]);
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
#endif
			std::cout << filename << ": (exact) " << name() << ", k = " << k << ": " << (solved ? "yes" : "no") << " [" << time_passed << "s]" << std::endl;
			if(solved && options.all_solutions)
			{
				std::cout << writecount << " solutions" << std::endl;
			}
		}
		if(solved || (options.time_max && time_passed >= options.time_max)) {break;}
	}
}

template<typename C, typename... Con>
struct Namer
{
	static constexpr std::string name() {return (std::string(C::name) + ... + (std::string("-") + Con::name));}
};

template<typename C>
struct Namer<C>
{
	static constexpr std::string name() {return C::name;}
};

template<typename E, typename F, typename G, typename GE, typename M, typename R, typename C, typename... Con>
constexpr std::string Run<E, F, G, GE, M, R, C, Con...>::name()
{
	return Namer<E, M, R, C, F, Con..., G>::name();
}
