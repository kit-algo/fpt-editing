#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "../config.hpp"

#include "Graph.hpp"

uint64_t Graph::get_size(std::string const &filename)
{
	std::ifstream f(filename);
	std::string line;
	std::istringstream linestream;

	if(!f)
	{
		std::cerr << "Error opening file: " << filename << std::endl;
		throw f;
	}

	while(std::getline(f, line))
	{
		if(line[0] != '%') {break;}
	}
	if(!f)
	{
		std::cerr << "Premature end of file: " << filename << std::endl;
		throw f;
	}

	uint64_t n;
	linestream.str(line);
	linestream >> n;
	return n;
}
