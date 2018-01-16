#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <initializer_list>
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

#include "Editor/Editor.hpp"

#include "Finder/Center_Matrix.hpp"

#include "Selector/First.hpp"

#include "Lower_Bound/No.hpp"

#include "Graph/Matrix.hpp"

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
	template<typename E, typename F, typename S, typename B, typename G, typename GE>
	static void run_watch(Options const &options, std::string const &filename, Editor::Options &editor_options)
	{
		if(!options.time_max_hard) {run<E, F, S, B, G, GE>(options, filename, editor_options);}
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
					"-R", Editor::Options::restriction_names.at(editor_options.restrictions).data(),
					"-C", Editor::Options::conversion_names.at(editor_options.conversions).data(),
					"-M", Editor::Options::mode_names.at(editor_options.mode).data(),
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


	template<typename E, typename F, typename S, typename B, typename G, typename GE>
	static void run(Options const &options, std::string const &filename, Editor::Options &editor_options)
	{
		auto graph = Graph::readMetis<G>(filename);
		Graph::writeDot(filename + ".gv", graph);
		auto g_orig = graph;

		F finder(graph, 4);
		S selector;
		B lower_bound;
		E editor(editor_options, finder, selector, lower_bound, graph);

		Finder::Feeder<F, S, B, G, GE> feeder(finder, selector, lower_bound);
		feeder.feed(graph, GE(graph.size()));
		size_t bound = lower_bound.result();

		//finder.recalculate();
		for(size_t k = std::max(bound, options.k_min); !options.k_max || k <= options.k_max; k++)
		{
			std::chrono::steady_clock::time_point t1, t2;
			size_t writecount = 0;
			auto writegraph = options.no_write? std::function<void(G const &, GE const &)>() : [&](G const &graph, GE const &edited) -> void
			{
				std::ostringstream fname;
				fname << filename << ".e." << E::name << '-' << Editor::Options::restriction_names.at(editor_options.restrictions) << '-' << Editor::Options::conversion_names.at(editor_options.conversions) << '-' << Editor::Options::mode_names.at(editor_options.mode) << '-' << F::name << '-' << S::name << '-' << B::name << '-' << G::name << ".k" << k << ".w" << writecount;
				Graph::writeMetis(fname.str(), graph);
				Graph::writeDot(fname.str() + ".gv", graph);
				Graph::writeMetis(fname.str() + ".edits", edited);
				Graph::writeDot(fname.str() + ".edits.gv", edited);
				Graph::writeDotCombined(fname.str() + ".combined.gv", graph, edited, g_orig);
				writecount++;
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
					json << "{\"type\":\"exact\",\"graph\":\"" << filename << "\",\"algo\":\"" << E::name << '-' << Editor::Options::restriction_names.at(editor_options.restrictions) << '-' << Editor::Options::conversion_names.at(editor_options.conversions) << '-' << Editor::Options::mode_names.at(editor_options.mode) << '-' << F::name << '-' << G::name << "\",\"k\":" << k << ",";
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
					std::cout << std::endl << filename << ": (exact) " << E::name << '-' << Editor::Options::restriction_names.at(editor_options.restrictions) << '-' << Editor::Options::conversion_names.at(editor_options.conversions) << '-' << Editor::Options::mode_names.at(editor_options.mode) << '-' << F::name << '-' << S::name << '-' << B::name << '-' << G::name << ", k = " << k << std::endl;
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
				std::cout << filename << ": (exact) " << E::name << '-' << Editor::Options::restriction_names.at(editor_options.restrictions) << '-' << Editor::Options::conversion_names.at(editor_options.conversions) << '-' << Editor::Options::mode_names.at(editor_options.mode) << '-' << F::name << '-' << S::name << '-' << B::name << '-' << G::name << ", k = " << k << ": " << (solved? "yes" : "no") << " [" << time_passed << "s]" << std::endl;
				if(solved && options.all_solutions) {std::cout << writecount << " solutions" << std::endl;}
			}
			if(solved || (options.time_max && time_passed >= options.time_max)) {break;}
		}
	}

};

void run(Options const &options)
{
#define RUN_G(VAR, NS, CLASS, FINDER, SELECTOR, LOWER_BOUND, GRAPH, GRAPH_EDITS) \
			else if(VAR == STR(CLASS) && fi == STR(FINDER) && se == STR(SELECTOR) && lb == STR(LOWER_BOUND) && gr == STR(GRAPH)) \
			{ \
				if(gs <= 64) \
				{ \
					Run::run_watch<NS::CLASS<Finder::FINDER<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Selector::SELECTOR<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Lower_Bound::LOWER_BOUND<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Finder::FINDER<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Selector::SELECTOR<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Lower_Bound::LOWER_BOUND<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>(options, filename, editor_options); \
				} \
				else \
				{ \
					Run::run_watch<NS::CLASS<Finder::FINDER<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Selector::SELECTOR<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Lower_Bound::LOWER_BOUND<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Finder::FINDER<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Selector::SELECTOR<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Lower_Bound::LOWER_BOUND<Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>, Graph::GRAPH<true>, Graph::GRAPH_EDITS<true>>(options, filename, editor_options); \
				} \
			}

	for(auto const &filename: options.filenames)
	{
		size_t gs = Graph::get_size(filename);

#define RUN(EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH) RUN_G(ed, Editor, EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH, Matrix)

		for(auto const &x: options.combinations_edit) for(auto const &ox: x.second) for(auto const &oy: ox.second) for(auto const &oz: oy.second) for(auto const &y: oz.second) for(auto const &z: y.second) for(auto const &zz: z.second) for(auto const &gr: zz.second)
		{{{
			auto const &ed = x.first;
			auto const &re = ox.first;
			auto const &co = oy.first;
			auto const &mo = oz.first;
			auto const &fi = y.first;
			auto const &se = z.first;
			auto const &lb = zz.first;

			Editor::Options editor_options;
			editor_options.all_solutions = options.all_solutions;
			editor_options.restrictions = Editor::Options::restriction_values.at(re);
			editor_options.conversions = Editor::Options::conversion_values.at(co);
			editor_options.mode = Editor::Options::mode_values.at(mo);

			if(false) {;}
//#include "../build/combinations_edit.i"
			RUN(Editor, Center_Matrix, First, No, Matrix);
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
		{"restriction", required_argument, NULL, 'R'},
		{"conversion", required_argument, NULL, 'C'},
		{"mode", required_argument, NULL, 'M'},
		{"editor", required_argument, NULL, 'e'},
		{"heuristic", required_argument, NULL, 'h'},
		{"finder", required_argument, NULL, 'f'},
		{"selector", required_argument, NULL, 's'},
		{"lower_bound", required_argument, NULL, 'b'},
		{"graph", required_argument, NULL, 'g'},

		{NULL, 0, NULL, 0}
	};
	char const *shortopts = "k:K:at:T:j:WSJ{}R:C:M:e:h:f:s:b:g:_";

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
		{"restrictions", {'R', {"none", "undo", "redundant"}}},
		{"conversions", {'C', {"normal", "last", "skip"}}},
		{"modes", {'M', {"edit", "delete", "insert"}}},
		{"editors", {'e', {"Editor"}}},
		{"heuristics", {'h', {}}},
		{"finders", {'f', {"Center_Matrix"}}},
		{"selectors", {'s', {"First"}}},
		{"bounds", {'b', {"No"}}},
		{"graphs", {'g', {"Matrix"}}}
	};

	auto select = [&choices, &usage](std::string const &type, decltype(optarg) arg) {
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
			for(auto const &r: choices["restrictions"].selected) for(auto const &c: choices["conversions"].selected) for(auto const &m: choices["modes"].selected) for(auto const &f: choices["finders"].selected) for(auto const &s: choices["selectors"].selected) for(auto const &b: choices["bounds"].selected) for(auto const &g: choices["graphs"].selected)
			{{
				for(auto const &e: choices["editors"].selected) {options.combinations_edit[e][r][c][m][f][s][b].insert(g);}
				for(auto const &h: choices["heuristics"].selected) {options.combinations_heur[h][r][c][m][f][s][b].insert(g);}
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

	for(auto const &r: choices["restrictions"].selected) for(auto const &c: choices["conversions"].selected) for(auto const &m: choices["modes"].selected) for(auto const &f: choices["finders"].selected) for(auto const &s: choices["selectors"].selected) for(auto const &b: choices["bounds"].selected) for(auto const &g: choices["graphs"].selected)
	{{
		for(auto const &e: choices["editors"].selected) {options.combinations_edit[e][r][c][m][f][s][b].insert(g);}
		for(auto const &h: choices["heuristics"].selected) {options.combinations_heur[h][r][c][m][f][s][b].insert(g);}
	}}

	for(; optind < argc; optind++)
	{
		options.filenames.push_back(argv[optind]);
	}

	std::cerr << "k: " << options.k_min << '-' << options.k_max << ", t/T: " << options.time_max << "/" << options.time_max_hard << "s, j: " << options.threads << ". " /*<< _ << " combinations on "*/ << options.filenames.size() << " files" << std::endl;

	run(options);

	return 0;
}
