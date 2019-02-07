#!/usr/bin/env python3

"""
Generates files for instanziating and selecting all combinations of components available to the user

if the preprocessor would support loops or proper recursion this would be unneeded (preprocessor libraries like P99 or BOOST_PP don't suffice)
"""

from collections import Iterable
from pathlib import Path
import itertools
import sys

need = {}
lists = {}

for line in sys.stdin:
	s = line.split(maxsplit = 2)
	if s[0] != "#define":
		print("got a line that is not a define:", define, macro, file = sys.stderr)
		raise SystemExit
	macro = s[1]
	value = s[2] if len(s) == 3 else None
	if macro.startswith("LIST_CHOICES_"):
		need[macro[13:]] = [s.strip() for s in value.split(",")] if value else []
	elif macro.startswith("CHOICES_"):
		lists[macro[8:]] = [s.strip() for s in value.split(",")] if value else []

collected = set()
for gen, arguments in need.items():
	for arg in arguments:
		if arg.endswith('...'):
			arg = arg[:-3]
		if not arg in collected:
			if not arg in lists:
				print("no definition for", "CHOICES_" + arg, "found, needed because of", "LIST_CHOICES_" + gen, file = sys.stderr)
				raise SystemExit
			print("#define GENERATED_CHOICES_", arg, " ", ', '.join('"' + e + '"' for e in lists[arg]), sep = "")
			collected.add(arg)
print()

def build_combinations(components):
	r = []
	for i in range(len(components) + 1):
		r += [x for x in itertools.combinations(components, i)]
	return r

class Formatter:
	"""
	formats an iterable into a string

	uses format specifier <delimiter><glue><delimiter><text>
	<delimiter> must be a single character
	expands into <text><glue><text><glue>...<glue><text> with one occurence of <text> for each element in the iterable (like str.join)
	occurences of <delimiter> within <text> are replaced with the current element of the iterable
	"""
	def __init__(self, data):
		self.data = data

	def __format__(self, format):
		delim = format[0]
		glue, content = format[1:].split(delim, 1)
		content = content.replace(delim, "{0}")
		return glue.join(content.format(x) for x in self.data)

def format_args(args):
	return [Formatter(x) if isinstance(x, Iterable) and not isinstance(x, (str, bytes)) else x for x in args]

# read template files
template_compare_text = open("src/generator_template_compare.tpp", "r").read()
template_instantiation_text = open("src/generator_template_instantiation.tpp", "r").read()

# prepare output
bg = Path("build/generated")
#for f in bg.glob("run-*"):
#	f.unlink()
filelist = open("build/generated/list.d", "w")
includelist = open("build/generated/generated.d", "w")

filelist.write("$(TARGET): \\\n")

# generate
for macro, arguments in need.items():
	print("#define GENERATED_RUN_", macro, " \\", sep = "")
	using = [build_combinations(lists[component[:-3]]) if component.endswith('...') else lists[component] for component in arguments]
	for combination in itertools.product(*using):
		# index/opt  0e 1 f      2 g    3 M    4 R  5 C  6 c   7 c
		# run-EDITOR-MT-Center_4-Matrix-Delete-None-Last-First-Basic.cpp
		if ("Single" in combination[6] or "Single" in combination[7]) and not combination[6] == combination[7] and (not ("Most" in combination[6] and not "Single" in combination[7])):
			continue
		if "Single" in combination[6] and (not combination[4] == "Redundant" or not combination[5] == "Skip"):
			continue
		if not combination[4] == "Redundant" and (not combination[5] == "Normal" or not combination[6] == "First" or not combination[7] == "No"):
			continue

		print(template_compare_text.format(*format_args(combination)).replace('=(=', '{').replace('=)=', '}'), end = "")
		filename = "build/generated/run-{0}-{1}-{2}-{3}-{4}-{5}-{6}-{7}-{8}".format(macro, *format_args(combination))
		if not Path(filename + ".cpp").is_file():
			with open(filename + ".cpp", "w") as of:
				of.write(template_instantiation_text.format(*format_args(combination)))
		filelist.write("\t" + filename + ".o \\\n")
		includelist.write("-include " + filename + ".d\n")
	print()

filelist.write("\n")
