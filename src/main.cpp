#include <fcntl.h>
#include <getopt.h>
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
#include <functional>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "config.hpp"

#include "Editor/Editor.hpp"

#include "Finder/Finder.hpp"
#include "Finder/Center_Matrix.hpp"

#include "Selector/First.hpp"
#include "Selector/Least_Unedited.hpp"
#include "Selector/Single.hpp"

#include "Lower_Bound/No.hpp"
#include "Lower_Bound/Basic.hpp"

#include "Graph/Matrix.hpp"

#define MINIMAL

#ifndef MINIMAL
#define CHOICES_MODE Edit, Delete, Insert
#define CHOICES_RESTRICTION None, Undo, Redundant
#define CHOICES_CONVERSION Normal, Last, Skip
#define CHOICES_EDITOR Editor
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_Matrix_4, Center_Matrix_5
#define CHOICES_SELECTOR First, Least_Unedited
#define CHOICES_LOWER_BOUND No, Basic
#define CHOICES_GRAPH Matrix
#else
#define CHOICES_MODE Edit
#define CHOICES_RESTRICTION Redundant
#define CHOICES_CONVERSION Skip
#define CHOICES_EDITOR Editor
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_Matrix_4
#define CHOICES_SELECTOR Single
#define CHOICES_LOWER_BOUND Basic
#define CHOICES_GRAPH Matrix
#endif

#define LIST_CHOICES_EDITOR EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH, MODE, RESTRICTION, CONVERSION
#define LIST_CHOICES_HEURISTIC HEURISTIC, FINDER, SELECTOR, LOWER_BOUND, GRAPH, MODE, RESTRICTION, CONVERSION

#include "../build/choices.i"

#define STR2(x) #x
#define STR(x) STR2(x)

struct Options {
	// search space
	size_t k_min = 0;
	size_t k_max = 0;
	bool all_solutions = false;
	// time constraints
	size_t time_max = 0;
	size_t time_max_hard = 0;
	size_t threads = 1;
	// output options
	bool no_write = false;
	bool no_stats = false;
	bool stats_json = false;
	//combinations
	std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::string>>>>>>>> combinations_edit;
	std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::string>>>>>>>> combinations_heur;
	//graphs
	std::vector<std::string> filenames;
};

struct Run
{
	template<typename E, typename F, typename G, typename GE, typename M, typename R, typename C, typename S, typename B>
	static void run_watch(Options const &options, std::string const &filename)
	{
		if(!options.time_max_hard) {run<E, F, G, GE, M, R, C, S, B>(options, filename);}
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
				if(options.no_stats) {boolopts.push_back('S');}
				if(options.stats_json) {boolopts.push_back('J');}

				char const *editheur = "-e"; //std::is_base_of<Editor::is_editor, E>::value? "-e" : "-h";
				char const *argv[] = {buf,
					"-k", k.data(), "-K", K.data(), "-t", t.data(), "-j", j.data(), boolopts.data(),
					"-M", M::name, "-R", R::name, "-C", C::name,
					editheur, E::name, "-f", F::name, "-s", S::name, "-b", B::name, "-g", G::name,
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


	template<typename E, typename F, typename G, typename GE, typename M, typename R, typename C, typename S, typename B>
	static void run(Options const &options, std::string const &filename)
	{
		G graph = Graph::readMetis<G>(filename);
		Graph::writeDot(filename + ".gv", graph);
		G g_orig = graph;

		F finder(graph);
		S selector(graph);
		B lower_bound(graph);
		E editor(finder, graph, selector, lower_bound);

		Finder::Feeder<F, G, GE, B> feeder(finder, lower_bound);
		feeder.feed(graph, GE(graph.size()));
		size_t bound = lower_bound.result();

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
					fname << filename << ".e." << E::name << '-' << M::name << '-' << R::name << '-' << C::name << '-' << F::name << '-' << S::name << '-' << B::name << '-' << G::name << ".k" << k << ".w" << writecount;
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

			if(!options.no_stats)
			{
#ifdef STATS
				if(options.stats_json)
				{
					std::ostringstream json;
					json << "{\"type\":\"exact\",\"graph\":\"" << filename << "\",\"algo\":\"" << E::name << '-' << M::name << '-' << R::name << '-' << C::name << '-' << F::name << '-' << G::name << "\",\"k\":" << k << ",";
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
					std::cout << std::endl << filename << ": (exact) " << E::name << '-' << M::name << '-' << R::name << '-' << C::name << '-' << F::name << '-' << S::name << '-' << B::name << '-' << G::name << ", k = " << k << std::endl;
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
					std::map<std::string, std::ostringstream> output;
					{
						size_t l = std::max_element(stats.begin(), stats.end(), [](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.first.length() < b.first.length();})->first.length();
						for(auto &stat: stats) {output[stat.first] << std::setw(l) << stat.first << ':';}
					}
					for(size_t j = 0; j <= k; j++)
					{
						size_t m = std::max_element(stats.begin(), stats.end(), [&j](typename decltype(stats)::value_type const &a, typename decltype(stats)::value_type const &b) {return a.second[j] < b.second[j];})->second[j];
						size_t l = 0;
						do {m /= 10; l++;} while(m > 0);
						for(auto &stat: stats) {output[stat.first] << " " << std::setw(l) << +stat.second[j];}
					}
					for(auto &stat: stats) {std::cout << output[stat.first].str() << ", total: " << +std::accumulate(stat.second.begin(), stat.second.end(), 0) << std::endl;}
				}
#endif
			}
			if(!options.stats_json)
			{
				std::cout << filename << ": (exact) " << E::name << '-' << M::name << '-' << R::name << '-' << C::name << '-' << F::name << '-' << S::name << '-' << B::name << '-' << G::name << ", k = " << k << ": " << (solved? "yes" : "no") << " [" << time_passed << "s]" << std::endl;
				if(solved && options.all_solutions) {std::cout << writecount << " solutions" << std::endl;}
			}
			if(solved || (options.time_max && time_passed >= options.time_max)) {break;}
		}
	}

};

void run(Options const &options)
{
#define RUN_G(VAR, NS, CLASS, FINDER, SELECTOR, LOWER_BOUND, GRAPH, GRAPH_EDITS, MODE, RESTRICTION, CONVERSION) \
			else if(VAR == STR(CLASS) && fi == STR(FINDER) && se == STR(SELECTOR) && lb == STR(LOWER_BOUND) && gr == STR(GRAPH) && mo == STR(MODE) && re == STR(RESTRICTION) && co == STR(CONVERSION)) \
			{ \
				using M = Editor::Options::Modes::MODE; \
				using R = Editor::Options::Restrictions::RESTRICTION; \
				using C = Editor::Options::Conversions::CONVERSION; \
				if(gs <= 64) \
				{ \
					using G = Graph::GRAPH<true>; \
					using GE = Graph::GRAPH_EDITS<true>; \
					using F = Finder::FINDER<G, GE>; \
					using S = Selector::SELECTOR<G, GE, M, R, C>; \
					using B = Lower_Bound::LOWER_BOUND<G, GE, M, R, C>; \
					using E = NS::CLASS<F, G, GE, M, R, C, S, B>; \
					Run::run_watch<E, F, G, GE, M, R, C, S, B>(options, filename); \
				} \
				else \
				{ \
					using G = Graph::GRAPH<false>; \
					using GE = Graph::GRAPH_EDITS<false>; \
					using F = Finder::FINDER<G, GE>; \
					using S = Selector::SELECTOR<G, GE, M, R, C>; \
					using B = Lower_Bound::LOWER_BOUND<G, GE, M, R, C>; \
					using E = NS::CLASS<F, G, GE, M, R, C, S, B>; \
					Run::run_watch<E, F, G, GE, M, R, C, S, B>(options, filename); \
				} \
			}

	for(auto const &filename: options.filenames)
	{
		size_t gs = Graph::get_size(filename);

#define RUN(EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH, MODE, RESTRICTION, CONVERSION) RUN_G(ed, Editor, EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH, Matrix, MODE, RESTRICTION, CONVERSION)

		for(auto const &x: options.combinations_edit) for(auto const &ox: x.second) for(auto const &oy: ox.second) for(auto const &oz: oy.second) for(auto const &y: oz.second) for(auto const &z: y.second) for(auto const &zz: z.second) for(auto const &gr: zz.second)
		{{{
			auto const &ed = x.first;
			auto const &mo = ox.first;
			auto const &re = oy.first;
			auto const &co = oz.first;
			auto const &fi = y.first;
			auto const &se = z.first;
			auto const &lb = zz.first;

			if(false) {;}
			GENERATED_RUN_EDITOR
			RUN(Editor, Center_Matrix_4, First, No, Matrix, Edit, Redundant, Skip)
			/*RUN(Editor, Center_Matrix_4, First, Basic, Matrix, Edit, Redundant, Skip)
			RUN(Editor, Center_Matrix_4, Least_Unedited, No, Matrix, Edit, Redundant, Skip)
			RUN(Editor, Center_Matrix_4, Least_Unedited, Basic, Matrix, Edit, Redundant, Skip)
			RUN(Editor, Center_Matrix_5, First, No, Matrix, Edit, Redundant, Skip)
			RUN(Editor, Center_Matrix_5, First, Basic, Matrix, Edit, Redundant, Skip)
			RUN(Editor, Center_Matrix_5, Least_Unedited, No, Matrix, Edit, Redundant, Skip)
			RUN(Editor, Center_Matrix_5, Least_Unedited, Basic, Matrix, Edit, Redundant, Skip)*/
		}}}
#undef RUN
#undef RUN_G
	}
	return;
}

int main(int argc, char *argv[])
{
	struct option const longopts[] =
	{
		// search
		{"kmin", required_argument, NULL, 'k'},
		{"kmax", required_argument, NULL, 'K'},
		{"all", no_argument, NULL, 'a'},
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
		{"mode", required_argument, NULL, 'M'},
		{"restriction", required_argument, NULL, 'R'},
		{"conversion", required_argument, NULL, 'C'},
		{"editor", required_argument, NULL, 'e'},
		{"heuristic", required_argument, NULL, 'h'},
		{"finder", required_argument, NULL, 'f'},
		{"selector", required_argument, NULL, 's'},
		{"lower_bound", required_argument, NULL, 'b'},
		{"graph", required_argument, NULL, 'g'},

		{NULL, 0, NULL, 0}
	};
	char const *shortopts = "k:K:at:T:j:WSJ{}M:R:C:e:h:f:s:b:g:_";

	Options options;
	bool usage = false;

	struct Category
	{
		std::set<std::string> const available;
		std::vector<std::string> selected;
		std::vector<size_t> stack;
		bool list = false;
		char const option;

		Category(char const option, std::set<std::string> const &available): available(available), option(option) {;}
		Category() : option('?') {;}
	};
	std::map<std::string, Category> choices{
		{"modes", {'M', {GENERATED_CHOICES_MODE}}},
		{"restrictions", {'R', {GENERATED_CHOICES_RESTRICTION}}},
		{"conversions", {'C', {GENERATED_CHOICES_CONVERSION}}},
		{"editors", {'e', {GENERATED_CHOICES_EDITOR}}},
		{"heuristics", {'h', {GENERATED_CHOICES_HEURISTIC}}},
		{"finders", {'f', {GENERATED_CHOICES_FINDER}}},
		{"selectors", {'s', {GENERATED_CHOICES_SELECTOR}}},
		{"bounds", {'b', {GENERATED_CHOICES_LOWER_BOUND}}},
		{"graphs", {'g', {GENERATED_CHOICES_GRAPH}}}
	};

	auto select = [&choices, &usage](std::string const &type, decltype(optarg) const arg) {
		auto &category = choices[type];
		if(!strncmp(arg, "list", 5))
		{
			category.list = true;
		}
		else if(!strncmp(arg, "all", 4))
		{
			category.selected.insert(category.selected.end(), category.available.begin(), category.available.end());
		}
		else if(category.available.count(optarg))
		{
			category.selected.push_back(optarg);
		}
		else
		{
			category.list = true;
			usage = true;
		}
	};

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
			options.all_solutions = true;
			break;
		case 't':
			options.time_max = std::stoull(optarg);
			break;
		case 'T':
			options.time_max_hard = std::stoull(optarg);
			break;
		case 'j':
			options.threads = std::stoull(optarg);
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
		case '{':
			for(auto &category: choices)
			{
				category.second.stack.push_back(category.second.selected.size());
			}
			break;
		case '}':
			if(choices.begin()->second.stack.empty())
			{
				std::cerr << "missmatched braces" << std::endl;
				return 1;
			}
			for(auto const &m: choices["modes"].selected) for(auto const &r: choices["restrictions"].selected) for(auto const &c: choices["conversions"].selected) for(auto const &f: choices["finders"].selected) for(auto const &s: choices["selectors"].selected) for(auto const &b: choices["bounds"].selected) for(auto const &g: choices["graphs"].selected)
			{{
				for(auto const &e: choices["editors"].selected) {options.combinations_edit[e][m][r][c][f][s][b].insert(g);}
				for(auto const &h: choices["heuristics"].selected) {options.combinations_heur[h][m][r][c][f][s][b].insert(g);}
			}}
			for(auto &category: choices)
			{
				category.second.selected.resize(category.second.stack.back());
				category.second.stack.pop_back();
			}
			break;
		case 'R':
			select("restrictions", optarg);
			break;
		case 'C':
			select("conversions", optarg);
			break;
		case 'M':
			select("modes", optarg);
			break;
		case 'e':
			select("editors", optarg);
			break;
		case 'h':
			select("heuristics", optarg);
			break;
		case 'f':
			select("finders", optarg);
			break;
		case 's':
			select("selectors", optarg);
			break;
		case 'b':
			select("bounds", optarg);
			break;
		case 'g':
			select("graphs", optarg);
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
	if(!choices.begin()->second.stack.empty())
	{
		std::cerr << "missing closing braces" << std::endl;
		return 1;
	}

	if(usage)
	{
		std::cerr << "Usage: " << argv[0] << " options... files..." << std::endl;
	}
	if(!usage && options.threads < 1)
	{
		std::cerr << "can't run without threads [-j < 1]" << std::endl;
		usage = true;
	}
	// if no combinations were specified yet (use of -{, -} ), force showing options for categories with no selection
	if(options.combinations_edit.empty() && options.combinations_heur.empty())
	{
		for(auto &category: choices)
		{
			if(category.second.selected.empty())
			{
				// having either editors or heuritcs is OK
				if(category.first == "editors" && !choices["heuristics"].selected.empty()) {continue;}
				if(category.first == "heuristics" && !choices["editors"].selected.empty()) {continue;}
				category.second.list = true;
			}
		}
	}
	// list options for categories where requested or an invalid selection was made
	for(auto const &category: choices)
	{
		if(category.second.list)
		{
			std::cerr << "Available " << category.first << " [-" << category.second.option << "]:";
			for(auto const &choice: category.second.available) {std::cerr << ' ' << choice;}
			std::cerr << std::endl;
			usage = true;
		}
	}
	// errors on comand line, terminate
	if(usage) {return 1;}

	for(auto const &m: choices["modes"].selected) for(auto const &r: choices["restrictions"].selected) for(auto const &c: choices["conversions"].selected) for(auto const &f: choices["finders"].selected) for(auto const &s: choices["selectors"].selected) for(auto const &b: choices["bounds"].selected) for(auto const &g: choices["graphs"].selected)
	{{
		for(auto const &e: choices["editors"].selected) {options.combinations_edit[e][m][r][c][f][s][b].insert(g);}
		for(auto const &h: choices["heuristics"].selected) {options.combinations_heur[h][m][r][c][f][s][b].insert(g);}
	}}

	for(; optind < argc; optind++)
	{
		options.filenames.push_back(argv[optind]);
	}

	std::cerr << "k: " << options.k_min << '-' << options.k_max << ", t/T: " << options.time_max << "/" << options.time_max_hard << "s, j: " << options.threads << ". " /*<< _ << " combinations on "*/ << options.filenames.size() << " files" << std::endl;

	run(options);

	return 0;
}
