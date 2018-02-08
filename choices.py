#!/usr/bin/python3

"""
Generates files for instanziating and selecting all combinations of components available to the user

if the preprocessor would support loops or proper recursion this would be unneeded (preprocessor libraries like P99 or BOOST_PP don't suffice)
"""

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
		if not arg in collected:
			if not arg in lists:
				print("no definition for", "CHOICES_" + arg, "found, needed because of", "LIST_CHOICES_" + gen, file = sys.stderr)
				raise SystemExit
			print("#define GENERATED_CHOICES_", arg, ' "', '", "'.join(lists[arg]), '"', sep = "")
			collected.add(arg)
print()

def combinations(filename, templatefile, template, filelist, includelist, remaining, generated):
	if not remaining:
		tf = open(templatefile, "r")
		template_text = tf.read()
		tf.close()
		for g in generated:
			print(template.format(*g))
			fn = filename.format(*g)
			filelist.write("\t" + fn.replace(".cpp", ".o") + " \\\n")
			includelist.write("-include " + fn.replace(".cpp", ".d") + "\n")
			with open(fn, "w") as of:
				of.write(template_text.format(*g))
	else:
		if not lists[remaining[0]]:
			return
		if not generated:
			combinations(filename, templatefile, template, filelist, includelist, remaining[1:], [[b] for b in lists[remaining[0]]])
		else:
			combinations(filename, templatefile, template, filelist, includelist, remaining[1:], [a + [b] for a in generated for b in lists[remaining[0]]])

for macro, arguments in need.items():
	print("#define GENERATED_RUN_", macro, " \\", sep = "")
	template = "RUN(" + ", ".join(["{}"] * len(arguments)) + ") \\"
	filename = "build/generated/run-" + "-".join(["{}"] * len(arguments)) + ".cpp"
	with open("build/generated/list.d", "w") as filelist:
		with open("build/generated/generated.d", "w") as includelist:
			filelist.write("$(TARGET): \\\n")
			combinations(filename, "src/generator_template.tpp", template, filelist, includelist, arguments, [])
			filelist.write("\n")
	print("\n\n")
