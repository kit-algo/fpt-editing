#include <getopt.h>
#include <string.h>

#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <stack>
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
		size_t graph_size = Graph::get_size(filename, options.edgelist);
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
			else
			{
				// accepted combination with no matching template instanziation
				std::stringstream name;
				name << editor << '-' << mode << '-' << restriction << '-' << conversion << '-' << finder;
				for(auto const &c: consumers) {name << '-' << c;}
				name << '-' << graph;
				std::cerr << name.str() << " is an invalid combination" << std::endl;
			}
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
		// input
		{"edgelist", no_argument, NULL, 'l'},
		{"permutation", required_argument, NULL, 'P'},
		// output
		{"no-write", no_argument, NULL, 'W'},
		{"json", no_argument, NULL, 'J'},
		// misc
		{"warmup", no_argument, NULL, 'D'},
		{"execute-only", required_argument, NULL, 'X'},

		// combinations
		{"{", no_argument, NULL, '{'},
		{",", no_argument, NULL, ','},
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
	char const *shortopts = "?k:K:at:T:j:n:N:lP:WSJDX:{,}M:R:C:e:h:f:c:g:_";

	CMDOptions options;
	bool usage = false;
	size_t execute_only = 0;

	struct Category
	{
		std::set<std::string> const available;
		char const option;

		Category(char const option, std::set<std::string> const &available): available(available), option(option) {;}
		Category() : option('?') {;}
	};
	std::map<std::string, Category> const choices{
		{"modes", {'M', {GENERATED_CHOICES_MODE}}},
		{"restrictions", {'R', {GENERATED_CHOICES_RESTRICTION}}},
		{"conversions", {'C', {GENERATED_CHOICES_CONVERSION}}},
		{"editors", {'e', {GENERATED_CHOICES_EDITOR}}},
		{"heuristics", {'h', {GENERATED_CHOICES_HEURISTIC}}},
		{"finders", {'f', {GENERATED_CHOICES_FINDER}}},
		{"consumers", {'c', {GENERATED_CHOICES_CONSUMER_SELECTOR, GENERATED_CHOICES_CONSUMER_BOUND, GENERATED_CHOICES_CONSUMER_RESULT}}},
		{"graphs", {'g', {GENERATED_CHOICES_GRAPH}}}
	};

	std::map<std::string, bool> list;

	struct Selection
	{
		std::vector<std::vector<Selection>> groups;
		std::map<std::string, std::set<std::string>> selection;

		// recursively flatten groups into a single vector of selections, similar to how a shell handles {,}
		std::vector<Selection> flatten() const
		{
			std::vector<Selection> r{*this};
			if(!groups.empty())
			{
				for(auto const &g: groups)
				{
					std::vector<Selection> next_r;
					for(auto const &subselection: g)
					{
						auto children = subselection.flatten();
						next_r.reserve(next_r.size() + children.size() * r.size());
						for(auto const &child: children)
						{
							for(auto const &r_elem: r)
							{
								next_r.push_back(child);
								for(auto const &sel: r_elem.selection)
								{
									next_r.back().selection[sel.first].insert(sel.second.begin(), sel.second.end());
								}
							}
						}
					}
					std::swap(r, next_r);
				}
			}
			return r;
		}
	};

	Selection selection;
	std::stack<std::reference_wrapper<Selection>> selection_stack;
	selection_stack.push(selection);

	auto select = [&](std::string const &type, decltype(optarg) const arg) {
		auto &category = choices.at(type);
		if(!strncmp(arg, "list", 5))
		{
			list[type] = true;
		}
		else if(!strncmp(arg, "all", 4))
		{
			selection_stack.top().get().selection[type].insert(category.available.begin(), category.available.end());
		}
		else if(category.available.count(optarg))
		{
			selection_stack.top().get().selection[type].insert(optarg);
		}
		else
		{
			list[type] = true;
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
		case 'l':
			options.edgelist = true;
			break;
		case 'P':
			options.permutation = std::stoull(optarg);
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
		case 'X':
			execute_only = std::stoull(optarg);
			break;
		case '{':
			selection_stack.top().get().groups.push_back({Selection()});
			selection_stack.push(selection_stack.top().get().groups.back().back());
			break;
		case ',':
			selection_stack.pop();
			if(selection_stack.empty()) {std::cerr << "'-,' option without preceeding '-{'" << std::endl; return 1;}
			selection_stack.top().get().groups.back().emplace_back();
			selection_stack.push(selection_stack.top().get().groups.back().back());
			break;
		case '}':
			selection_stack.pop();
			if(selection_stack.empty()) {std::cerr << "'-}' option without preceeding '-{'" << std::endl; return 1;}
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
	if(selection_stack.size() != 1)
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
			<< "  -l --edge-list: Graph is in edge list format \n"
			<< "  -P --permutation <number>: Seed for the random permutation of node ids, 0 (default) disables permutation. \n"
			<< "  -a --all: Find all solutions, not just the first one\n"
			<< "  -W --no-write: Do not write solutions to disk\n"
			<< "  -J --json: Output results as JSON fragment\n\n"
			<< "  -e --editor / -h --heuristic / -f --finder / -c --consumer / -g --graph / -M --mode / -C --conversion / -R --restriction:\n"
			//<< "    Select an algorithm\n"
			<< "    All options take the name of an algorithm as argument; use \"list\" to show available ones, \"all\" to select all\n"
			<< "  You can group algorithm choices with -{ --{ / -, --, / -} --} in a shell-like manner\n"
			<< "Graphs need to be in METIS format unless -l is given.\n"
		;
		std::cerr << std::endl;
	}
	if(options.threads < 1)
	{
		std::cerr << "Can't run without threads [-j < 1]" << std::endl;
		usage = true;
	}

	size_t combination_count = 0;
	bool no_groups = true;
	for(auto const &combination: selection.flatten())
	{
		no_groups = false;
		// test if all components were specified, exempting editors and heuristics as having only either of them is fine
		if(std::all_of(choices.begin(), choices.end(), [&](auto const &choice) {return choice.first == "editors" || choice.first == "heuristics" || combination.selection.count(choice.first) > 0;}))
		{
			for(auto const &m: combination.selection.at("modes")) for(auto const &r: combination.selection.at("restrictions")) for(auto const &c: combination.selection.at("conversions")) for(auto const &f: combination.selection.at("finders")) for(auto const &g: combination.selection.at("graphs"))
			{{
				size_t old_count = combination_count;
				if(combination.selection.count("editors")) for(auto const &e: combination.selection.at("editors"))
				{{
					combination_count++;
					if(execute_only && execute_only != combination_count) {continue;}
					options.combinations_edit[e][m][r][c][f][g].insert(combination.selection.at("consumers"));
				}}
				if(combination.selection.count("heuristics")) for(auto const &h: combination.selection.at("heuristics"))
				{{
					combination_count++;
					if(execute_only && execute_only != combination_count) {continue;}
					options.combinations_heur[h][m][r][c][f][g].insert(combination.selection.at("consumers"));
				}}

				if(combination_count == old_count)
				{
					// missing both editors and heuristics
					list["editors"] = true;
					list["heuristics"] = true;
				}
			}}
		}
		else
		{
			// missing selections in this group
			for(auto const &category : choices)
			{
				if(combination.selection.count(category.first) == 0)
				{
					// having either editors or heuristics is fine
					if(category.first == "editors" && combination.selection.count("heuristics") != 0) {continue;}
					else if(category.first == "heuristics" && combination.selection.count("editors") != 0) {continue;}
					list[category.first] = true;
				}
			}
		}
	}

	// no components were given, show list for all
	if(no_groups)
	{
		for(auto &category: choices) {list[category.first] = true;}
	}
	// list options for categories where requested or an invalid selection was made
	for(auto const &category: choices)
	{
		if(list.count(category.first))
		{
			std::cerr << "Available " << category.first << " [-" << category.second.option << "]:";
			for(auto const &choice: category.second.available) {std::cerr << ' ' << choice;}
			std::cerr << std::endl;
			usage = true;
		}
	}
	// errors on comand line, terminate
	if(usage) {return 1;}

	for(; optind < argc; optind++)
	{
		options.filenames.push_back(argv[optind]);
	}

	std::cerr << "k: " << options.k_min << '-' << options.k_max << ", t/T: " << options.time_max << "s/" << options.time_max_hard << "s, n/N: " << options.repeats << "x/" << options.repeat_time << "s, j: " << options.threads << ". " << combination_count << " combinations on " << options.filenames.size() << " files" << std::endl;
	if(execute_only > 0) {std::cerr << "only using combination " << execute_only << std::endl;}

	// if the hard limit is stricter, overwrite soft limit with hard limit,
	// since the child will be killed before reaching the soft limit.
	if(options.time_max == 0) {options.time_max = options.time_max_hard;}
	else if(options.time_max_hard > 0) {options.time_max = std::min(options.time_max, options.time_max_hard);}

	options.argc = argc;
	options.argv = argv;
	run(options);

	std::cerr << "done" << std::endl;
	return 0;
}
