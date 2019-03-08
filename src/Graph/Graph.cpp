#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "../config.hpp"

#include "Graph.hpp"

uint64_t Graph::get_size(std::string const &filename, bool edgelist)
{
	std::ifstream f(filename);
	std::string line;
	std::istringstream linestream;

	if(!f)
	{
		throw std::runtime_error(std::string("Error opening file: ") + filename);
	}

	if (edgelist)
	{
		uint64_t n = 0;
		uint64_t u, v;
		while(f >> u >> v)
		{
			n = std::max(n, std::max(u, v));
		}

		return n + 1;
	}

	while(std::getline(f, line))
	{
		if(line[0] != '%') {break;}
	}
	if(!f)
	{
		throw std::runtime_error(std::string("Premature end of file: ") + filename);
	}

	uint64_t n;
	linestream.str(line);
	linestream >> n;
	return n;
}
