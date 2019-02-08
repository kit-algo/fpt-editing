graphedit -- a framework for evaluating FPT Edge Editing algorithms

graphedit is a framework for evaluating algorithms and strategies (called components) used for solving the F-free edge editing problem. F is a set of forbidden subgraphs. If a graph doesn't contain any of them the graph is considered to be F-free. graphedit tries to find F-free graphs close to the input graph by editing (inserting and removing) as few edges as possible. Currently all implemented components are for F = {P~x~, C~x~}, i.e. paths and cycles containing x vertices are forbidden.

[TOC]

### BUILDING

* edit the top part of ``Makefile`` to adjust compilation flags as needed
* edit ``src/config.hpp`` to select the algorithms that will be available -- compiling everything will take a while (expect 2 hours on a modern machine)
* make

### USING

Specify the combination of algorithms that shall be used and the graph files to run them on. Optionally limit the number of edits that should be tried and limit the running time. Except for Consumers, selecting multiple algorithms for the same component will run multiple experiments. You can create groups with the ``-{``, ``-,`` and ``-}`` options, which work similar to ``{``, ``,`` and ``}`` of most shells.
Try ``graphedit --help``. Graphs files must be in METIS format.

For a description of the individual components and the currently available algorithms for these components see [COMPONENTS](COMPONENTS).

For each experiment the program will print a table of statistics:
```
data/karate.graph: (exact) MT-Edit-Redundant-Skip-Center_4-Least-Basic-Matrix, 4 threads, k = 21
		k:    0    1    2    3    4    5    6   7   8   9  10  11  12  13  14  15  16 17 18 19 20 21
	calls: 1846 1208 1477 7816 5880 2100 1089 630 414 336 249 243 227 224 180 159 118 75 34 13  5  1, total: 24324
fallbacks:    0  554  279  370 1693 1431  550 293 184 146 131 102 102  96  91  76  61 47 29 12  4  1, total: 6252
   prunes: 1845  654 1198 7446 4187  669  539 337 230 190 118 141 125 128  89  83  57 28  5  1  1  0, total: 18071
   single:    0    0    0    0    0    0    0   0   0   0   0   0   0   0   0   0   0  0  0  0  0  0, total: 0
  skipped:  924  187  373  649 1275  650  376 290 316 319 261 267 253 231 200 146 117 70 26  6  0  0, total: 6936
   stolen:    0    0    0    0    0    0    0   0   0   0   0   0   0   0   0   0   0  0  0  2  5  1, total: 8
data/karate.graph: (exact) MT-Edit-Redundant-Skip-Center_4-Least-Basic-Matrix, 4 threads, k = 21: yes [0.155779s]
```
The first and last line mention the graph and algorithms used and the number of number of allowed edits. The last line additionally says whether the graph could be solved and the time needed. The numbers in the table indicate how often each event occurred while there were how many edits remaining. The events shown may differ between different editors. Currently there are:
 * calls: number of recursive calls (should be equal to prunes + fallbacks + single)
 * prunes: how often was the lower bound higher than the number of edits remaining
 * single: how often did the selection algorithm produce a single edge to edits
 * fallbacks: how often did the selection algorithm produce a P~x~ or C~x~
 * skipped: how often was a recursion skipped due to editing restrictions
 * stolen: how often was work shared among threads

If graphedit was compiled without statistics only the last line will be printed.

To facilitate further processing of the results adding ``-J`` or ``--json`` will cause the results to be printed as a JSON fragment instead. (Note the trailing comma.) If compiled without statistics the ``counters`` object will be missing.
```
{
	"type":"exact",
	"graph":"data/karate.graph",
	"algo":"MT-Edit-Redundant-Skip-Center_4-Least-Basic-Matrix",
	"threads":4,
	"k":21,
	"results":{
		"solved":"true",
		"time":0.158142,
		"counters":{
			"calls":[1846,1208,1477,7816,5880,2100,1089,630,414,336,249,243,227,224,180,159,118,75,34,13,5,1],
			"fallbacks":[0,554,279,370,1693,1431,550,293,184,146,131,102,102,96,91,76,61,47,29,12,4,1],
			"prunes":[1845,654,1198,7446,4187,669,539,337,230,190,118,141,125,128,89,83,57,28,5,1,1,0],
			"single":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
			"skipped":[924,187,373,649,1275,650,376,290,316,319,261,267,253,231,200,146,117,70,26,6,0,0],
			"stolen":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,5,1]
		}
	}
},
```

If the graph can be solved, the solution will be written in the same directory as the graph. The filename is derived from the original name and indicates the algorithm choices and number of edits. The set of currently edited or marked edges is also written. Both graphs are written in METIS and DOT format. In addition a graph in DOT format is written, highlighting changes made and edges marked. Writing solutions to disk can be disabled with ``-W`` or ``--no-write``.

We recommend redirecting the output to a file when outputting JSON. JSON fragments can be assembled into a valid JSON object by using the helper scrip ``./eval2json input_file(s) > output_file``. The JSON file can then be converted into a sqlite database by ``json2db.py json_file sqlite_file``.
``plot.r`` provides R functions for loading JSON and database files and creating various plots from their data. We recommend using the database as R has some serious problems loading larger JSON files.


### HACKING

In the main source directory ``src/`` contains the files for configuration (``config.hpp``), command line parsing (``main.cpp``), setting up the individual experiments (``Run_impl.hpp``), templates used by the build system (``*.tpp``) and some general utility. Components are in subdirectories named after their type. The interface each component should adhere to can be found in ``src/Interfaces/``.

The code relies on templates instead of inheritance/polymorphism as we observed a 35% increase in running time using the latter. However, when simply adding components, the interface and the existing components should serve as sufficient example on how the templates are intended to be used. The more messy ones (implementation of ``Finder::Feeder`` and ``Run_impl``) shouldn't need to be touched.
Don't forget to add a new component to ``src/choices.hpp`` and ``src/config.hpp``; not adding them will lead to build failures or the build process ignoring the new components.

- Conventions
	- Every function that is a loop or recursion and the callbacks called by them return a boolean signaling whether it should continue executing. Receiving ``true`` from a callback means the function should stop looping resp. recursing and return ``true`` as well. Currently these functions are:
		* ``Editor::<any>.edit``
		* ``Finder::<any>.find`` with callback ``Consumer.next`` and their ``…_near`` variants
		* Helper functions ``Finder::for_all_edges_ordered`` and ``Finder::for_all_edges_unordered`` and their lambda parameter