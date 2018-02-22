/* template used by choices.py to convert arguments to template instantiation */ \
/* =(= and =)= will be replaced by curly braces */ \
\
else if(editor == "{0}" && mode == "{3}" && restriction == "{4}" && conversion == "{5}" && finder == "{1}" && graph == "{2}" && consumers == std::set<std::string>=(="{6}", "{7}"{8:.., "."}=)=) \
=(= \
	using M = Options::Modes::{3}; \
	using R = Options::Restrictions::{4}; \
	using C = Options::Conversions::{5}; \
	if(graph_size <= Packed_Bits) \
	=(= \
		Run<Editor::{0}, Finder::{1}, Graph::{2}, Graph::Matrix, true, M, R, C, Consumer::{6}, Consumer::{7}{8:.., Consumer::.}>::run(options, filename); \
	=)= \
	else \
	=(= \
		Run<Editor::{0}, Finder::{1}, Graph::{2}, Graph::Matrix, false, M, R, C, Consumer::{6}, Consumer::{7}{8:.., Consumer::.}>::run(options, filename); \
	=)= \
=)= \
