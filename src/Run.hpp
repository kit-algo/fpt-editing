#ifndef RUN_HPP
#define RUN_HPP

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
	size_t repeats = 0;
	size_t repeat_time = 0;
	// output options
	bool no_write = false;
	bool stats_json = false;
	// misc
	bool do_warmup = false;
	// combinations: e/h - M - R - C - f - g - c...
	std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::set<std::string>>>>>>>> combinations_edit;
	std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::set<std::string>>>>>>>> combinations_heur;
	// graphs
	std::vector<std::string> filenames;
	bool edgelist = false;
	size_t permutation = 0;
	// used to show current experiment in child's cmdline
	int argc = 0;
	char **argv = NULL;
};

template<template<typename, typename, typename, typename, typename, typename, typename...> typename E, template<typename, typename, typename, typename, typename> typename F, template<bool> typename G, template<bool> typename GE, bool small, typename M, typename R, typename C, template<typename, typename, typename, typename, typename, typename, size_t> typename... Con>
struct Run
{
	static void run(CMDOptions const &options, std::string const &filename);
	static std::string name();
};

#endif
