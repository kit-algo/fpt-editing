#include <map>
#include <set>
#include <string>
#include <vector>

#include "config.hpp"

struct CMDOptions {
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

template<typename E, typename F, typename G, typename GE, typename M, typename R, typename C, typename... Con>
struct Run
{
	static void run_watch(CMDOptions const &options, std::string const &filename);
	static void run(CMDOptions const &options, std::string const &filename);

	static constexpr std::string name();
};
