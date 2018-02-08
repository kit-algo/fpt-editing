#include <getopt.h>
#include <string.h>

#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#include "config.hpp"
#include "Run.hpp"

#include "Graph/Graph.hpp"

#include "choices.hpp"
#include "../build/choices.i" // provides GENERATED_* macros, generated during build process

#define STR2(x) #x
#define STR(x) STR2(x)

void run(CMDOptions const &options)
{
#define RUN_G(VAR, NS, CLASS, FINDER, SELECTOR, LOWER_BOUND, GRAPH, GRAPH_EDITS, MODE, RESTRICTION, CONVERSION) \
			else if(VAR == STR(CLASS) && fi == STR(FINDER) && se == STR(SELECTOR) && lb == STR(LOWER_BOUND) && gr == STR(GRAPH) && mo == STR(MODE) && re == STR(RESTRICTION) && co == STR(CONVERSION)) \
			{ \
				using M = Options::Modes::MODE; \
				using R = Options::Restrictions::RESTRICTION; \
				using C = Options::Conversions::CONVERSION; \
				if(gs <= Packed_Bits) \
				{ \
					using G = Graph::GRAPH<true>; \
					using GE = Graph::GRAPH_EDITS<true>; \
					using F = Finder::FINDER<G, GE>; \
					using S = Consumer::SELECTOR<G, GE, M, R, C>; \
					using B = Consumer::LOWER_BOUND<G, GE, M, R, C>; \
					using E = NS::CLASS<F, G, GE, M, R, C, S, B>; \
					Run<E, F, G, GE, M, R, C, S, B>::run_watch(options, filename); \
				} \
				else \
				{ \
					using G = Graph::GRAPH<false>; \
					using GE = Graph::GRAPH_EDITS<false>; \
					using F = Finder::FINDER<G, GE>; \
					using S = Consumer::SELECTOR<G, GE, M, R, C>; \
					using B = Consumer::LOWER_BOUND<G, GE, M, R, C>; \
					using E = NS::CLASS<F, G, GE, M, R, C, S, B>; \
					Run<E, F, G, GE, M, R, C, S, B>::run_watch(options, filename); \
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
		}}}
#undef RUN
#undef RUN_G
	}
	return;
}

int main(int argc, char *argv[])
{
	/* command line options */
	struct option const longopts[] =
	{
		// general
		{"help", no_argument, NULL, '?'},
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
	char const *shortopts = "?k:K:at:T:j:WSJ{}M:R:C:e:h:f:s:b:g:_";

	CMDOptions options;
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
		case '?':
			usage = true;
			break;
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
		}
	}
	if(!choices.begin()->second.stack.empty())
	{
		std::cerr << "missing closing braces" << std::endl;
		return 1;
	}

	if(usage)
	{
		std::cerr << "Usage: " << argv[0] << " options... graphs...\n"
			<< "Options are:\n"
			<< "  -? --help: Display this help\n"
			<< "  -k --kmin <number> / -K --kmax <number>: Minimum/Maximum number of edits to try\n"
			<< "  -t --time <number> / -T --time-hard <number>: Time limit in seconds; -t allows the current experimant to end, -T aborts it\n"
			<< "  -j --threads <number>: Number of threads to use, if editor supports multithreading\n"
			<< "  -a --all: Find all solutions, not just the first one\n"
			<< "  -W --no-write: Do not write solutions to disk\n"
			<< "  -J --json: Output results as JSON fragment\n\n"
			<< "  -e --editor / -h --heuristic / -f --finder / -s --selector / -b --bound / -g --graph / -M --mode / -C --conversion / -R --restriction:\n"
			//<< "    Select an algorithm\n"
			<< "    All options take the name of an algorithm as argument; use \"list\" to show available ones, \"all\" to select all\n"
			<< "  You can group algorithm choices with -{ --{ / -} --}\n"
			<< "Graphs need to be in METIS format.\n"
		;
		std::cerr << std::endl;
	}
	if(!usage && options.threads < 1)
	{
		std::cerr << "Can't run without threads [-j < 1]" << std::endl;
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
