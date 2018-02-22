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

void run(CMDOptions const &options)
{
	for(auto const &filename : options.filenames)
	{
		size_t graph_size = Graph::get_size(filename);
		for(auto const &e: options.combinations_edit) for(auto const &m: e.second) for(auto const &r: m.second) for(auto const &c: r.second) for(auto const &f: c.second) for(auto const &g: f.second) for(auto const &consumers: g.second)
		{{
			auto const &editor = e.first;
			auto const &mode = m.first;
			auto const &restriction = r.first;
			auto const &conversion = c.first;
			auto const &finder = f.first;
			auto const &graph = g.first;
			if(false) {;} // macro starts with "else if"
			GENERATED_RUN_EDITOR
		}}
	}
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
		{"repeat", required_argument, NULL, 'n'},
		{"repeat-time", required_argument, NULL, 'N'},
		// output
		{"no-write", no_argument, NULL, 'W'},
		{"json", no_argument, NULL, 'J'},
		// misc
		{"warmup", no_argument, NULL, 'D'},

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
		{"consumer", required_argument, NULL, 'c'},
		{"graph", required_argument, NULL, 'g'},

		{NULL, 0, NULL, 0}
	};
	char const *shortopts = "?k:K:at:T:j:n:N:WSJD{}M:R:C:e:h:f:c:g:_";

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
		{"consumers", {'c', {GENERATED_CHOICES_CONSUMER_SELECTOR, GENERATED_CHOICES_CONSUMER_BOUND, GENERATED_CHOICES_CONSUMER_RESULT}}},
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
		case 'n':
			options.repeats = std::stoull(optarg);
			break;
		case 'N':
			options.repeat_time = std::stoull(optarg);
			break;
		case 'W':
			options.no_write = true;
			break;
		case 'J':
			options.stats_json = true;
			break;
		case 'D':
			options.do_warmup = true;
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
			{
				std::set<std::string> consumers(choices["consumers"].selected.begin(), choices["consumers"].selected.end());
				for(auto const &m: choices["modes"].selected) for(auto const &r: choices["restrictions"].selected) for(auto const &c: choices["conversions"].selected) for(auto const &f: choices["finders"].selected) for(auto const &g: choices["graphs"].selected)
				{{
					for(auto const &e: choices["editors"].selected) {options.combinations_edit[e][m][r][c][f][g].insert(consumers);}
					for(auto const &h: choices["heuristics"].selected) {options.combinations_heur[h][m][r][c][f][g].insert(consumers);}
				}}
			}
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
		case 'c':
			select("consumers", optarg);
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
			<< "  -n --repeats <number>: Repeat each experiment n times\n"
			<< "  -N --repeat-time <number>: Repeat each experiment for N seconds\n"
			<< "  -D --warmup: Spend some time before each set of experiments in dry-runs\n"
			<< "  -a --all: Find all solutions, not just the first one\n"
			<< "  -W --no-write: Do not write solutions to disk\n"
			<< "  -J --json: Output results as JSON fragment\n\n"
			<< "  -e --editor / -h --heuristic / -f --finder / -c --consumer / -g --graph / -M --mode / -C --conversion / -R --restriction:\n"
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
				// having either editors or heuristics is OK
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

	std::set<std::string> consumers(choices["consumers"].selected.begin(), choices["consumers"].selected.end());
	for(auto const &m: choices["modes"].selected) for(auto const &r: choices["restrictions"].selected) for(auto const &c: choices["conversions"].selected) for(auto const &f: choices["finders"].selected) for(auto const &g: choices["graphs"].selected)
	{{
		for(auto const &e: choices["editors"].selected) {options.combinations_edit[e][m][r][c][f][g].insert(consumers);}
		for(auto const &h: choices["heuristics"].selected) {options.combinations_heur[h][m][r][c][f][g].insert(consumers);}
	}}

	for(; optind < argc; optind++)
	{
		options.filenames.push_back(argv[optind]);
	}

	std::cerr << "k: " << options.k_min << '-' << options.k_max << ", t/T: " << options.time_max << "/" << options.time_max_hard << "s, n/N: " << options.repeats << "/" << options.repeat_time << "s, j: " << options.threads << ". " /*<< _ << " combinations on "*/ << options.filenames.size() << " files" << std::endl;

	run(options);

	return 0;
}
