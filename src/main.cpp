#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "config.hpp"

#include "Editor.hpp"

#include "Finder.hpp"
#include "Center_Matrix.hpp"
#include "Selector_First.hpp"
#include "LB_No.hpp"

#include "Graph_Matrix.hpp"

#define STR2(x) #x
#define STR(x) STR2(x)


void test()
{
	Editor::Options eo;
	Graph::Matrix<> g = Graph::readMetis<Graph::Matrix<>>("data/presentation.graph");
	Finder::Center_Matrix f(g, 4);
	Selector::First sel;
	Lower_Bound::No lb;

	Editor::Editor e(eo, g, f, sel, lb);
	bool result = e.edit(2);
	std::cout << (result? "solved" : "not solved") << std::endl;
	result = e.edit(3);
	std::cout << (result? "solved" : "not solved") << std::endl;
}



struct Options {
	Editor::Options editor;
	// search space
	size_t k_min = 0;
	size_t k_max = 0;
	// time constraints
	size_t time_max = 0;
	size_t time_max_hard = 0;
	size_t threads = 1;
	// output options
	bool no_write = false;
	bool no_stats = false;
	bool stats_json = false;
	std::vector<std::string> filenames;
	std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::string>>>>> combinations_edit;
	std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::string>>>>> combinations_heur;
};

struct Run
{
	template<typename E, typename F, typename S, typename B, typename G>
	static void run_watch(Options const &options, std::string const &filename)
	{
		if(!options.time_max_hard) {run<E, F, S, B, G>(options, filename);}
		else
		{
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
				if(options.editor.all_solutions) {boolopts.push_back('a');}
				if(options.no_write) {boolopts.push_back('W');}
				if(options.no_stats) {boolopts.push_back('S');}
				if(options.stats_json) {boolopts.push_back('J');}

				char const *editheur = std::is_base_of<Editor::is_editor, E>::value? "-e" : "-h";
				char const *argv[] = {buf,
					"-k", k.data(), "-K", K.data(), "-t", t.data(), "-j", j.data(), boolopts.data(),
					"-R", "redundant", "-C", "skip", "-M", "edit", /* TODO */
					editheur, E::name, "-f", F::name, "-s", S::name, "-b", B::name, "-g", G::name, filename.data(),
					NULL
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


	template<typename E, typename F, typename S, typename B, typename G>
	static void run(Options const &options, std::string const &filename)
	{
		auto graph = Graph::readMetis<G>(filename);
		Graph::writeDot(filename + ".gv", graph);
		auto g_orig = graph;

		F finder(graph, 4);
		S selector;
		B lower_bound;
		E editor(options.editor, graph, finder, selector, lower_bound);

		Finder::feed(graph, G(graph.size()), finder, {&lower_bound});
		size_t bound = lower_bound.result();

		//finder.recalculate();
		for(size_t k = std::max(bound, options.k_min); !options.k_max || k <= options.k_max; k++)
		{
			std::chrono::steady_clock::time_point t1, t2;
			size_t writecount = 0;
			auto writegraph = [&](Graph::Graph const &graph, Graph::Graph const &edited) -> void
			{
				std::ostringstream fname;
				fname << filename << ".e." << E::name << '-' << F::name << '-' << S::name << '-' << B::name << '-' << G::name << ".k" << k << ".w" << writecount;
				Graph::writeMetis(fname.str(), graph);
				Graph::writeDot(fname.str() + ".gv", graph);
				Graph::writeMetis(fname.str() + ".edits", edited);
				Graph::writeDot(fname.str() + ".edits.gv", edited);
				Graph::writeDotCombined(fname.str() + ".combined.gv", graph, edited, g_orig);
				writecount++;
			};

			((Options &) options).editor.write = options.no_write? std::function<void(Graph::Graph const &, Graph::Graph const &)>() : writegraph;
			t1 = std::chrono::steady_clock::now();
			bool solved = editor.edit(k);
			t2 = std::chrono::steady_clock::now();
			double time_passed = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1).count();
			if(!options.no_stats)
			{
#ifdef STATS
				if(options.stats_json)
				{
					std::ostringstream json;
					json << "{\"type\":\"exact\",\"graph\":\"" << filename << "\",\"algo\":\"" << E::name << '-' << F::name << '-' << G::name << "\",\"k\":" << k << ",";
					json << "\"results\":{\"solved\":\""<< (solved? "true" : "false") << "\",\"time\":" << time_passed << ",\"counters\":{";
					auto const stats = editor.stats();
					bool first_stat = true;
					for(auto stat: stats)
					{
						json << (first_stat? "" : ",") << "\"" << stat.first << "\":[";
						first_stat = false;
						bool first_value = true;
						for(auto value: stat.second)
						{
							json << (first_value? "" : ",") << +value;
							first_value = false;
						}
						json << ']';
					}
					json << "}}},";
					std::cout << json.str() << std::endl;
				}
				else
				{
					std::cout << std::endl << filename << ": (exact) " << E::name << '-' << F::name << '-' << G::name << ", k = " << k << std::endl;
# ifdef STATS_LB
					std::cout << "lb changes:" << std::endl;
					auto const &lb_diff = editor.lb_stats();
					for(size_t j = 0; j <= k; j++)
					{
						auto const &lbdk = lb_diff[j];
						std::cout << "k=" << j << ": [" << lbdk.first << ", " << lbdk.first + (ssize_t) lbdk.second.size() - 1 << "]";
						for(auto const &v: lbdk.second) {std::cout << " " << +v;}
						std::cout << std::endl;
					}
# endif
					std::cout << "recursions:" << std::endl;
					auto const stats = editor.stats();
					std::vector<std::ostringstream> output(stats.size());
					{
						size_t l = std::max_element(stats.begin(), stats.end(), [](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.first.length() < b.first.length();})->first.length();
						for(size_t i = 0; i < stats.size(); i++) {output[i] << std::setw(l) << stats[i].first << ':';}
					}
					for(size_t j = 0; j <= k; j++)
					{
						size_t m = std::max_element(stats.begin(), stats.end(), [&j](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.second[j] < b.second[j];})->second[j];
						size_t l = 0;
						do {m /= 10; l++;} while(m > 0);
						for(size_t i = 0; i < stats.size(); i++) {output[i] << " " << std::setw(l) << +stats[i].second[j];}
					}
					for(size_t i = 0; i < stats.size(); i++) {std::cout << output[i].str() << ", total: " << +std::accumulate(stats[i].second.begin(), stats[i].second.end(), 0) << std::endl;}
				}
#endif
			}
			if(!options.stats_json)
			{
				std::cout << filename << ": (exact) " << E::name << '-' << F::name << '-' << S::name << '-' << B::name << '-' << G::name << ", k = " << k << ": " << (solved? "yes" : "no") << " [" << time_passed << "s]" << std::endl;
				if(solved && options.editor.all_solutions) {std::cout << writecount << " solutions" << std::endl;}
			}
			if(solved || (options.time_max && time_passed >= options.time_max)) {break;}
		}
	}


#if 0
	template<typename E, typename F, typename G>
	static typename std::enable_if<
		std::is_base_of<Editor::is_editor, E>::value
		&& std::is_base_of<Finder::is_single, E>::value == std::is_base_of<Finder::is_single, F>::value
		&& ((!std::is_base_of<Finder::needs_adj_list, E>::value && !std::is_base_of<Finder::needs_adj_list, F>::value) || std::is_base_of<Graph::has_adj_list, G>::value)
		&& ((!std::is_base_of<Finder::needs_adj_matrix, E>::value && !std::is_base_of<Finder::needs_adj_matrix, F>::value) || std::is_base_of<Graph::has_adj_matrix, G>::value)
	, void>::type run(Options const &options, std::string const &filename)
	{
		auto graph = Graph::readMetis<G>(filename);
		Graph::writeDot(filename + ".gv", graph);
		auto g_orig = graph;
		F finder(graph, options.length);
		E editor(finder, graph, options.threads);
		size_t bound;
		std::tie(std::ignore, std::ignore, bound) = finder.find(graph, G(graph.size()));
		finder.recalculate();
		for(size_t k = std::max(bound, options.k_min); !options.k_max || k <= options.k_max; k++)
		{
			std::chrono::steady_clock::time_point t1, t2;
			size_t writecount = 0;
			auto writegraph = [&](G const &graph, typename E::Edit_Graph const &edits)
			{
				std::ostringstream fname;
				fname << filename << ".e." << E::name << '-' << F::name << '-' << G::name << ".l" << options.length << ".k" << k << ".w" << writecount;
				Graph::writeMetis(fname.str(), graph);
				Graph::writeDot(fname.str() + ".gv", graph);
				Graph::writeMetis(fname.str() + ".edits", edits);
				Graph::writeDot(fname.str() + ".edits.gv", edits);
				Graph::writeDotCombined(fname.str() + ".combined.gv", graph, edits, g_orig);
				writecount++;
			};
			t1 = std::chrono::steady_clock::now();
			bool solved = editor.edit(k, options.all_solutions, options.no_write? std::function<void(G const &, typename E::Edit_Graph const &)>() : writegraph);
			t2 = std::chrono::steady_clock::now();
			double time_passed = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1).count();
			if(!options.no_stats)
			{
#ifdef STATS
				if(options.stats_json)
				{
					std::ostringstream json;
					json << "{\"type\":\"exact\",\"graph\":\"" << filename << "\",\"algo\":\"" << E::name << '-' << F::name << '-' << G::name << "\",\"k\":" << k << ",";
					json << "\"results\":{\"solved\":\""<< (solved? "true" : "false") << "\",\"time\":" << time_passed << ",\"counters\":{";
					auto const stats = editor.stats();
					bool first_stat = true;
					for(auto stat: stats)
					{
						json << (first_stat? "" : ",") << "\"" << stat.first << "\":[";
						first_stat = false;
						bool first_value = true;
						for(auto value: stat.second)
						{
							json << (first_value? "" : ",") << +value;
							first_value = false;
						}
						json << ']';
					}
					json << "}}},";
					std::cout << json.str() << std::endl;
				}
				else
				{
					std::cout << std::endl << filename << ": (exact) " << E::name << '-' << F::name << '-' << G::name << ", k = " << k << std::endl;
# ifdef STATS_LB
					std::cout << "lb changes:" << std::endl;
					auto const &lb_diff = editor.lb_stats();
					for(size_t j = 0; j <= k; j++)
					{
						auto const &lbdk = lb_diff[j];
						std::cout << "k=" << j << ": [" << lbdk.first << ", " << lbdk.first + (ssize_t) lbdk.second.size() - 1 << "]";
						for(auto const &v: lbdk.second) {std::cout << " " << +v;}
						std::cout << std::endl;
					}
# endif
					std::cout << "recursions:" << std::endl;
					auto const stats = editor.stats();
					std::vector<std::ostringstream> output(stats.size());
					{
						size_t l = std::max_element(stats.begin(), stats.end(), [](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.first.length() < b.first.length();})->first.length();
						for(size_t i = 0; i < stats.size(); i++) {output[i] << std::setw(l) << stats[i].first << ':';}
					}
					for(size_t j = 0; j <= k; j++)
					{
						size_t m = std::max_element(stats.begin(), stats.end(), [&j](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.second[j] < b.second[j];})->second[j];
						size_t l = 0;
						do {m /= 10; l++;} while(m > 0);
						for(size_t i = 0; i < stats.size(); i++) {output[i] << " " << std::setw(l) << +stats[i].second[j];}
					}
					for(size_t i = 0; i < stats.size(); i++) {std::cout << output[i].str() << ", total: " << +std::accumulate(stats[i].second.begin(), stats[i].second.end(), 0) << std::endl;}
				}
#endif
			}
			if(!options.stats_json)
			{
				std::cout << filename << ": (exact) " << E::name << '-' << F::name << '-' << G::name << ", k = " << k << ": " << (solved? "yes" : "no") << " [" << time_passed << "s]" << std::endl;
				if(solved && options.all_solutions) {std::cout << writecount << " solutions" << std::endl;}
			}
			if(solved || (options.time_max && time_passed >= options.time_max)) {break;}
		}
	}
	template<typename E, typename F, typename G>
	static typename std::enable_if<
		std::is_base_of<Editor::is_editor, E>::value
		&& !(std::is_base_of<Finder::is_single, E>::value == std::is_base_of<Finder::is_single, F>::value
		&& ((!std::is_base_of<Finder::needs_adj_list, E>::value && !std::is_base_of<Finder::needs_adj_list, F>::value) || std::is_base_of<Graph::has_adj_list, G>::value)
		&& ((!std::is_base_of<Finder::needs_adj_matrix, E>::value && !std::is_base_of<Finder::needs_adj_matrix, F>::value) || std::is_base_of<Graph::has_adj_matrix, G>::value))
	, void>::type run(Options const &, std::string const &)
	{
		std::cerr << "combination " << E::name << '-' << F::name << '-' << G::name << " does not exist" << std::endl;
	}

	template<typename E, typename F, typename G>
	static typename std::enable_if<
		std::is_base_of<Heuristic::is_heuristic, E>::value
		&& std::is_base_of<Finder::is_single, E>::value == std::is_base_of<Finder::is_single, F>::value
		&& ((!std::is_base_of<Finder::needs_adj_list, E>::value && !std::is_base_of<Finder::needs_adj_list, F>::value) || std::is_base_of<Graph::has_adj_list, G>::value)
		&& ((!std::is_base_of<Finder::needs_adj_matrix, E>::value && !std::is_base_of<Finder::needs_adj_matrix, F>::value) || std::is_base_of<Graph::has_adj_matrix, G>::value)
	, void>::type run(Options const &options, std::string const &filename)
	{
		auto graph = Graph::readMetis<G>(filename);
		Graph::writeDot(filename + ".gv", graph);
		auto g_orig = graph;
		F finder(graph, options.length);
		E editor(finder, graph, options.threads);
		size_t k = options.k_max? options.k_max : ~0ULL;
		{
			std::cout << std::endl;
			std::chrono::steady_clock::time_point t1, t2;
			size_t writecount = 0;
			auto writeheuristic = [&](G const &graph, typename E::Edit_Graph const &edits, size_t k_used)
			{
				double time_passed = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - t1).count();
				if(options.stats_json)
				{
					std::ostringstream json;
					json << "{\"type\":\"heuristic\",\"graph\":\"" << filename << "\",\"algo\":\"" << E::name << '-' << F::name << '-' << G::name << "\",\"k\":" << k << ",";
					json << "\"results\":{\"edits\":\""<< k_used << "\",\"time\":" << time_passed << ",\"counters\":{";
					auto const stats = editor.stats();
					bool first_stat = true;
					for(auto stat: stats)
					{
						json << (first_stat? "" : ",") << "\"" << stat.first << "\":[";
						first_stat = false;
						bool first_value = true;
						for(auto value: stat.second)
						{
							json << (first_value? "" : ",") << +value;
							first_value = false;
						}
						json << ']';
					}
					json << "}}},";
					std::cout << json.str() << std::endl;
				}
				else
				{
					std::cout << filename << ": (heuristic) " << E::name << '-' << F::name << '-' << G::name << ", k_max = " << k << " with k = " << k_used << std::endl;
# ifdef STATS_LB
					std::cout << "lb changes:" << std::endl;
					auto const &lb_diff = editor.lb_stats();
					for(size_t j = 0; j <= k; j++)
					{
						auto const &lbdk = lb_diff[j];
						std::cout << "k=" << j << ": [" << lbdk.first << ", " << lbdk.first + (ssize_t) lbdk.second.size() - 1 << "]";
						for(auto const &v: lbdk.second) {std::cout << " " << +v;}
						std::cout << std::endl;
					}
# endif
					std::cout << "recursions:" << std::endl;
					auto const stats = editor.stats();
					std::vector<std::ostringstream> output(stats.size());
					{
						size_t l = std::max_element(stats.begin(), stats.end(), [](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.first.length() < b.first.length();})->first.length();
						for(size_t i = 0; i < stats.size(); i++) {output[i] << std::setw(l) << stats[i].first << ':';}
					}
					for(size_t j = 0; j <= k; j++)
					{
						size_t m = std::max_element(stats.begin(), stats.end(), [&j](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.second[j] < b.second[j];})->second[j];
						size_t l = 0;
						do {m /= 10; l++;} while(m > 0);
						for(size_t i = 0; i < stats.size(); i++) {output[i] << " " << std::setw(l) << +stats[i].second[j];}
					}
					for(size_t i = 0; i < stats.size(); i++) {std::cout << output[i].str() << ", total: " << +std::accumulate(stats[i].second.begin(), stats[i].second.end(), 0) << std::endl;}
					std::cout << filename << ": (heuristic) " << E::name << '-' << F::name << '-' << G::name << ", k_max = " << k << " with k = " << k_used << " [" << time_passed << "s]" << std::endl;
				}
				if(!options.no_write)
				{
					std::ostringstream fname;
					fname << filename << ".h." << E::name << '-' << F::name << '-' << G::name << ".l" << options.length << ".k" << k_used << ".w" << writecount;
					Graph::writeMetis(fname.str(), graph);
					Graph::writeDot(fname.str() + ".gv", graph);
					Graph::writeMetis(fname.str() + ".edits", edits);
					Graph::writeDot(fname.str() + ".edits.gv", edits);
					Graph::writeDotCombined(fname.str() + ".combined.gv", graph, edits, g_orig);
					writecount++;
				}
			};

			t1 = std::chrono::steady_clock::now();
			bool solved;
			size_t k_best;
			std::tie(solved, k_best) = editor.edit(k, options.all_solutions, writeheuristic);
			t2 = std::chrono::steady_clock::now();
			double time_passed = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1).count();
			if(!options.no_stats)
			{
#ifdef STATS
				if(options.stats_json)
				{
					std::ostringstream json;
					json << "{\"type\":\"heuristic_end\",\"graph\":\"" << filename << "\",\"algo\":\"" << E::name << '-' << F::name << '-' << G::name << "\",\"k\":" << k << ",";
					json << "\"results\":{\"solved\":\""<< (solved? "true" : "false") << "\",\"best\":" << k_best << ",\"time\":" << time_passed << ",\"counters\":{";
					auto const stats = editor.stats();
					bool first_stat = true;
					for(auto stat: stats)
					{
						json << (first_stat? "" : ",") << "\"" << stat.first << "\":[";
						first_stat = false;
						bool first_value = true;
						for(auto value: stat.second)
						{
							json << (first_value? "" : ",") << +value;
							first_value = false;
						}
						json << ']';
					}
					json << "}}},";
					std::cout << json.str() << std::endl;
				}
				else
				{
					std::cout << std::endl << filename << ": (heuristic) " << E::name << '-' << F::name << '-' << G::name << ", k_max = " << k << std::endl;
# ifdef STATS_LB
					std::cout << "lb changes:" << std::endl;
					auto const &lb_diff = editor.lb_stats();
					for(size_t j = 0; j <= k; j++)
					{
						auto const &lbdk = lb_diff[j];
						std::cout << "k=" << j << ": [" << lbdk.first << ", " << lbdk.first + (ssize_t) lbdk.second.size() - 1 << "]";
						for(auto const &v: lbdk.second) {std::cout << " " << +v;}
						std::cout << std::endl;
					}
# endif
					std::cout << "recursions:" << std::endl;
					auto const stats = editor.stats();
					std::vector<std::ostringstream> output(stats.size());
					{
						size_t l = std::max_element(stats.begin(), stats.end(), [](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.first.length() < b.first.length();})->first.length();
						for(size_t i = 0; i < stats.size(); i++) {output[i] << std::setw(l) << stats[i].first << ':';}
					}
					for(size_t j = 0; j <= k; j++)
					{
						size_t m = std::max_element(stats.begin(), stats.end(), [&j](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.second[j] < b.second[j];})->second[j];
						size_t l = 0;
						do {m /= 10; l++;} while(m > 0);
						for(size_t i = 0; i < stats.size(); i++) {output[i] << " " << std::setw(l) << +stats[i].second[j];}
					}
					for(size_t i = 0; i < stats.size(); i++) {std::cout << output[i].str() << ", total: " << +std::accumulate(stats[i].second.begin(), stats[i].second.end(), 0) << std::endl;}
				}
#endif
			}
			if(!options.stats_json)
			{
				std::cout << filename << ": (heuristic) " << E::name << '-' << F::name << '-' << G::name << ", k_max = " << k << ": " << (solved? "yes" : "no") << ", best k = " << k_best << " [" << time_passed << "s]" << std::endl;
				if(solved && options.all_solutions) {std::cout << writecount << " solutions" << std::endl;}
			}
			//if(solved || (options.time_max && time_passed >= options.time_max)) {break;}
		}
	}
	template<typename E, typename F, typename G>
	static typename std::enable_if<
		std::is_base_of<Heuristic::is_heuristic, E>::value
		&& !(std::is_base_of<Finder::is_single, E>::value == std::is_base_of<Finder::is_single, F>::value
		&& ((!std::is_base_of<Finder::needs_adj_list, E>::value && !std::is_base_of<Finder::needs_adj_list, F>::value) || std::is_base_of<Graph::has_adj_list, G>::value)
		&& ((!std::is_base_of<Finder::needs_adj_matrix, E>::value && !std::is_base_of<Finder::needs_adj_matrix, F>::value) || std::is_base_of<Graph::has_adj_matrix, G>::value))
	, void>::type run(Options const &, std::string const &)
	{
		std::cerr << "combination " << E::name << '-' << F::name << '-' << G::name << " does not exist" << std::endl;
	}
#endif
};

void run(Options const &options)
{
#define RUN_G(VAR, NS, CLASS, FINDER, SELECTOR, LOWER_BOUND, GRAPH) \
			else if(VAR == STR(CLASS) && fi == STR(FINDER) && se == STR(SELECTOR) && lb == STR(LOWER_BOUND) && gr == STR(GRAPH)) \
			{ \
				size_t gs = Graph::get_size(filename); \
				if(gs <= 64) \
				{ \
					Run::run_watch<NS::CLASS, Finder::FINDER, Selector::SELECTOR, Lower_Bound::LOWER_BOUND, Graph::GRAPH<>>(options, filename); \
				} \
				else \
				{ \
					Run::run_watch<NS::CLASS, Finder::FINDER, Selector::SELECTOR, Lower_Bound::LOWER_BOUND, Graph::GRAPH<>>(options, filename); \
				} \
			}

	for(auto const &filename: options.filenames)
	{
#define RUN(EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH) RUN_G(ed, Editor, EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH)
		for(auto const &x: options.combinations_edit) for(auto const &y: x.second) for(auto const &z: y.second) for(auto const &zz: z.second) for(auto const &gr: zz.second)
		{{{
			auto const &ed = x.first;
			auto const &fi = y.first;
			auto const &se = z.first;
			auto const &lb = zz.first;
			if(false) {;}
//#include "../build/combinations_edit.i"
			RUN(Editor, Center_Matrix, First, No, Matrix);
		}}}
#undef RUN
/*
#define RUN(HEURISTIC, FINDER, GRAPH) RUN_G(hr, Heuristic, HEURISTIC, FINDER, GRAPH)
		for(auto const &x: options.combinations_heur) for(auto const &y: x.second) for(auto const &gr: y.second)
		{{{
			auto const &hr = x.first;
			auto const &fi = y.first;
			if(false) {;}
#include "../build/combinations_heur.i"
		}}}
#undef RUN
*/
	}
	return;
}

int main(int argc, char *argv[])
{
	/* general options:
	 * -j --jobs #: number of threads
	 *
	 * selections
	 * -e editors
	 * -f problem finders
	 * -g graph classes
	 * run for every combination
	 * grouping?
	 * -c e-f-g for defining a single combination
	 *
	 *
	 */

	struct option const longopts[] =
	{
		// search
		{"kmin", required_argument, NULL, 'k'},
		{"kmax", required_argument, NULL, 'K'},
		{"all", no_argument, NULL, 'a'},
		{"restriction", required_argument, NULL, 'R'},
		{"conversion", required_argument, NULL, 'C'},
		{"mode", required_argument, NULL, 'M'},
		// time
		{"time", required_argument, NULL, 't'},
		{"time-hard", required_argument, NULL, 'T'},
		{"threads", required_argument, NULL, 'j'},
		// output
		{"no-write", no_argument, NULL, 'W'},
		{"no-stats", no_argument, NULL, 'S'},
		{"json", no_argument, NULL, 'J'},

		// combinations
		{"{", no_argument, NULL, '{'},
		{"}", no_argument, NULL, '}'},
		// algorithm choices
		{"editor", required_argument, NULL, 'e'},
		{"heuristic", required_argument, NULL, 'h'},
		{"finder", required_argument, NULL, 'f'},
		{"selector", required_argument, NULL, 's'},
		{"lower_bound", required_argument, NULL, 'b'},
		{"graph", required_argument, NULL, 'g'},

		{NULL, 0, NULL, 0}
	};
	char const *shortopts = "k:K:aR:C:M:t:T:j:WSJ{}e:h:f:s:b:g:_";

	Options options;

	bool list_restrictions = false;
	bool list_conversions = false;
	bool list_modes = false;
	bool list_editors = false;
	bool list_heuristics = false;
	bool list_finders = false;
	bool list_selectors = false;
	bool list_bounds = false;
	bool list_graphs = false;
	bool usage = false;

	std::map<std::string, Editor::Options::Restrictions> available_restrictions{{"none", Editor::Options::NONE}, {"undo", Editor::Options::NO_UNDO}, {"redundant", Editor::Options::NO_REDUNDANT}};
	std::map<std::string, Editor::Options::Path_Cycle_Conversion> available_conversions{{"normal", Editor::Options::NORMAL}, {"last", Editor::Options::DO_LAST}, {"skip", Editor::Options::SKIP}};
	std::map<std::string, Editor::Options::Modes> available_modes{{"edit", Editor::Options::ANY}, {"delete", Editor::Options::ONLY_DELETE}, {"insert", Editor::Options::ONLY_INSERT}};


	std::set<std::string> available_editors{"Editor"};
	std::set<std::string> available_heuristics{};
	std::set<std::string> available_finders{"Center_Matrix"};
	std::set<std::string> available_selectors{"First"};
	std::set<std::string> available_bounds{"No"};
	std::set<std::string> available_graphs{"Matrix"};

	std::vector<std::string> editors;
	std::vector<std::string> heuristics;
	std::vector<std::string> finders;
	std::vector<std::string> selectors;
	std::vector<std::string> bounds;
	std::vector<std::string> graphs;
	std::vector<std::tuple<size_t, size_t, size_t, size_t, size_t, size_t>> stack;

	while(true)
	{
		int c = getopt_long(argc, argv, shortopts, longopts, NULL);
		if(c == -1) {break;}
		switch(c)
		{
		case 'k':
			options.k_min = std::stoull(optarg);
			break;
		case 'K':
			options.k_max = std::stoull(optarg);
			break;
		case 'a':
			options.editor.all_solutions = true;
			break;
		case 'R':
			if(!strncmp(optarg, "list", 5))
			{
				list_restrictions = true;
			}
			else if(available_restrictions.count(optarg))
			{
				options.editor.restrictions = available_restrictions[optarg];
			}
			else
			{
				list_restrictions = true;
				usage = true;
			}
			break;
		case 'C':
			if(!strncmp(optarg, "list", 5))
			{
				list_conversions = true;
			}
			else if(available_conversions.count(optarg))
			{
				options.editor.conversions = available_conversions[optarg];
			}
			else
			{
				list_conversions = true;
				usage = true;
			}
			break;
		case 'M':
			if(!strncmp(optarg, "list", 5))
			{
				list_modes = true;
			}
			else if(available_modes.count(optarg))
			{
				options.editor.mode = available_modes[optarg];
			}
			else
			{
				list_modes = true;
				usage = true;
			}
			break;
		case 't':
			options.time_max = std::stoull(optarg);
			break;
		case 'T':
			options.time_max_hard = std::stoull(optarg);
			break;
		case 'W':
			options.no_write = true;
			break;
		case 'S':
			options.no_stats = true;
			break;
		case 'J':
			options.stats_json = true;
			break;
		case 'j':
			options.threads = std::stoull(optarg);
			break;
		case '{':
			stack.push_back(std::make_tuple(editors.size(), heuristics.size(), finders.size(), selectors.size(), bounds.size(), graphs.size()));
			break;
		case '}':
			for(auto const &f: finders) for(auto const &s: selectors) for(auto const &b: bounds) for(auto const &g: graphs)
			{{
				for(auto const &e: editors) {options.combinations_edit[e][f][s][b].insert(g);}
				for(auto const &h: heuristics) {options.combinations_heur[h][f][s][b].insert(g);}
			}}
			editors.resize(std::get<0>(stack.back()));
			heuristics.resize(std::get<1>(stack.back()));
			finders.resize(std::get<2>(stack.back()));
			selectors.resize(std::get<3>(stack.back()));
			bounds.resize(std::get<4>(stack.back()));
			graphs.resize(std::get<5>(stack.back()));
			stack.pop_back();
			break;
		case 'e':
			if(!strncmp(optarg, "list", 5))
			{
				list_editors = true;
			}
			else if(!strncmp(optarg, "all", 4))
			{
				editors.insert(editors.end(), available_editors.begin(), available_editors.end());
			}
			else if(available_editors.count(optarg))
			{
				editors.push_back(optarg);
			}
			else
			{
				list_editors = true;
				usage = true;
			}
			break;
		case 'h':
			if(!strncmp(optarg, "list", 5))
			{
				list_heuristics = true;
			}
			else if(!strncmp(optarg, "all", 4))
			{
				heuristics.insert(heuristics.end(), available_heuristics.begin(), available_heuristics.end());
			}
			else if(available_heuristics.count(optarg))
			{
				heuristics.push_back(optarg);
			}
			else
			{
				list_heuristics = true;
				usage = true;
			}
			break;
		case 'f':
			if(!strncmp(optarg, "list", 5))
			{
				list_finders = true;
			}
			else if(!strncmp(optarg, "all", 4))
			{
				finders.insert(finders.end(), available_finders.begin(), available_finders.end());
			}
			else if(available_finders.count(optarg))
			{
				finders.push_back(optarg);
			}
			else
			{
				list_finders = true;
				usage = true;
			}
			break;
		case 's':
			if(!strncmp(optarg, "list", 5))
			{
				list_selectors = true;
			}
			else if(!strncmp(optarg, "all", 4))
			{
				selectors.insert(selectors.end(), available_selectors.begin(), available_selectors.end());
			}
			else if(available_selectors.count(optarg))
			{
				selectors.push_back(optarg);
			}
			else
			{
				list_selectors = true;
				usage = true;
			}
			break;
		case 'b':
			if(!strncmp(optarg, "list", 5))
			{
				list_bounds = true;
			}
			else if(!strncmp(optarg, "all", 4))
			{
				bounds.insert(bounds.end(), available_bounds.begin(), available_bounds.end());
			}
			else if(available_bounds.count(optarg))
			{
				bounds.push_back(optarg);
			}
			else
			{
				list_bounds = true;
				usage = true;
			}
			break;
		case 'g':
			if(!strncmp(optarg, "list", 5))
			{
				list_graphs = true;
			}
			else if(!strncmp(optarg, "all", 4))
			{
				graphs.insert(graphs.end(), available_graphs.begin(), available_graphs.end());
			}
			else if(available_graphs.count(optarg))
			{
				graphs.push_back(optarg);
			}
			else
			{
				list_graphs = true;
				usage = true;
			}
			break;
		case '_':
			//dummy option
			break;
		default:
			std::cerr << "Unknown option: " << (char) c << std::endl;
			usage = true;
			break;
		}
	}

	if(usage)
	{
		std::cerr << "Usage: " << argv[0] << " options... files..." << std::endl;
	}
	if(!usage && options.threads < 1)
	{
		std::cerr << "can't run without threads" << std::endl;
		usage = true;
	}
	if(list_restrictions)
	{
		std::cerr << "Available Restrictions:";
		for(auto const &s: available_restrictions) {std::cerr << ' ' << s.first;}
		std::cerr << std::endl;
		usage = true;
	}
	if(list_conversions)
	{
		std::cerr << "Available Conversions:";
		for(auto const &s: available_conversions) {std::cerr << ' ' << s.first;}
		std::cerr << std::endl;
		usage = true;
	}
	if(list_modes)
	{
		std::cerr << "Available Modes:";
		for(auto const &s: available_modes) {std::cerr << ' ' << s.first;}
		std::cerr << std::endl;
		usage = true;
	}
	if(list_editors || (editors.empty() && heuristics.empty()))
	{
		std::cerr << "Available Editors:";
		for(auto const &s: available_editors) {std::cerr << ' ' << s;}
		std::cerr << std::endl;
		usage = true;
	}
	if(list_heuristics || (editors.empty() && heuristics.empty()))
	{
		std::cerr << "Available Heuristics:";
		for(auto const &s: available_heuristics) {std::cerr << ' ' << s;}
		std::cerr << std::endl;
		usage = true;
	}
	if(list_finders || finders.empty())
	{
		std::cerr << "Available Finders:";
		for(auto const &s: available_finders) {std::cerr << ' ' << s;}
		std::cerr << std::endl;
		usage = true;
	}
	if(list_selectors || selectors.empty())
	{
		std::cerr << "Available Selectors:";
		for(auto const &s: available_selectors) {std::cerr << ' ' << s;}
		std::cerr << std::endl;
		usage = true;
	}
	if(list_bounds || bounds.empty())
	{
		std::cerr << "Available Lower Bounds:";
		for(auto const &s: available_bounds) {std::cerr << ' ' << s;}
		std::cerr << std::endl;
		usage = true;
	}
	if(list_graphs || graphs.empty())
	{
		std::cerr << "Available Graphs:";
		for(auto const &s: available_graphs) {std::cerr << ' ' << s;}
		std::cerr << std::endl;
		usage = true;
	}

	if(usage) {return 1;}

	for(auto const &f: finders) for(auto const &s: selectors) for(auto const &b: bounds) for(auto const &g: graphs)
	{{
		for(auto const &e: editors) {options.combinations_edit[e][f][s][b].insert(g);}
		for(auto const &h: heuristics) {options.combinations_heur[h][f][s][b].insert(g);}
	}}

	for(; optind < argc; optind++)
	{
		options.filenames.push_back(argv[optind]);
	}

	std::cerr << "k: " << options.k_min << '-' << options.k_max << ", t/T: " << options.time_max << "/" << options.time_max_hard << "s, j: " << options.threads << ". " /*<< _ << " combinations on "*/ << options.filenames.size() << " files" << std::endl;

	run(options);

	return 0;
}
