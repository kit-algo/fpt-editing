A framework for evaluating exact Edge Editing algorithms

This code base contains both the ILP and the FPT-based approach for exact F-free edge editing. F is a set of forbidden subgraphs. If a graph doesn't contain any of them the graph is considered to be F-free. graphedit tries to find F-free graphs close to the input graph by editing (inserting and removing) as few edges as possible. Currently all implemented components are for F = {P<sub>l</sub>, C<sub>l</sub>}, i.e. paths and cycles containing l vertices are forbidden. The ILP is currently only implemented for F = {P<sub>4</sub>, C<sub>4<sub>}. Further, various evaluation scripts for generating statistics and plots are provided.

The ILP algorithm can be compiled using CMake, while the FPT algorithm uses the ``Makefile`` at the top of the repository. Both approaches currently need Gurobi for building, as the FPT algorithm contains a lower bound based on an LP relaxation.

Most of the instructions below mainly concern the FPT implementation. The ILP works similarly and produces similar JSON output, except that it offers much less variants.

### BUILDING

* edit the top part of ``Makefile`` to adjust compilation flags as needed
* edit ``src/config.hpp`` to select the algorithms that will be available -- compiling everything will take a while (expect 2 hours on a modern machine). The selected default configuration that includes all variants used in the paper should compile in less than 2 minutes on a current laptop using 4 parallel jobs (``make -j4``).
* ``make [-jX]``

For the parts using CMake:

* Run ``git submodule update --init`` if you haven't initialized the submodule yet, we use  [tlx](https://tlx.github.io/) for various parts which is embedded as a submodule.
* Run ``mkdir release && cd release`` to create a build directory and change to it.
* Run ``cmake .. -DCMAKE_BUILD_TYPE=Release`` to initialize the build directory. If Gurobi is not found, you might need to set the environment variable ``GUROBI_HOME`` to the path that contains the Gurobi include and lib directories, e.g. using ``export GUROBI_HOME=/opt/gurobi/linux64/`` and add the version of Gurobi you use to ``cmake/modules/FindGUROBI.cmake``.
* Build using ``make [-jX]``.

### USING

Specify the combination of algorithms that shall be used and the graph files to run them on. Optionally limit the number of edits that should be tried and limit the running time. Except for Consumers, selecting multiple algorithms for the same component will run multiple experiments. You can create groups with the ``-{``, ``-,`` and ``-}`` options, which work similar to ``{``, ``,`` and ``}`` of most shells.
Try ``graphedit --help``. Graphs files must be in METIS format.

For a description of the individual components and the currently available algorithms for these components see [COMPONENTS](COMPONENTS.md).

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
 * fallbacks: how often did the selection algorithm produce a P<sub>l</sub> or C<sub>l<sub>
 * skipped: how often was a recursion skipped due to editing restrictions
 * stolen: how often was work shared among threads

If graphedit was compiled without statistics only the last line will be printed.
Further, there is a simple stats mode that prints only the sum of each row which is active by default.
This mode can be changed in ``src/Editor/ST.hpp`` and ``src/Editor/MT.hpp`` for single- and multi-threaded operation, respectively.

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

We recommend redirecting the output to a file when outputting JSON. JSON fragments can be assembled into a valid JSON object by using the helper scrip ``./eval2json input_file(s) > output_file``. The JSON file can then be converted to CSV using ``json2csv csv_file json_file(s)``, the ``json2csv`` binary can be compiled using CMake.
``bio_plot.py`` produces most plots, ``bio_qtm_plot.py`` the comparison plots for the QTM heuristic and ``bio_stats.py`` produces various statistics.
``social_table.py`` produces the comparison table for the social networks.
``cluster_analysis.py`` contains the code for comparing the solutions on the social network instances, this script might need to be adjusted for the actual path to the solutions.
This needs the current version of NetworKit.
The ``run_qtm.py`` script contains the code for executing the QTM heuristic on all given graphs.
This script outputs the CSV that is needed for ``bio_qtm_plot.py``.
QTM is available in the NetworKit fork at https://github.com/michitux/networkit/tree/upstream/qtm/networkit (note that this fork unfortunately does not contain the features required for ``cluster_analysis.py``).
All scripts but ``cluster_analysis.py`` use an argument parser that will also print a help listing all required and optional parameters when run without arguments.

### HACKING

In the main source directory ``src/`` contains the files for configuration (``config.hpp``), command line parsing (``main.cpp``), setting up the individual experiments (``Run_impl.hpp``), templates used by the build system (``*.tpp``) and some general utility. Components are in subdirectories named after their type.

The code relies on templates instead of inheritance/polymorphism as we observed a 35% increase in running time using the latter. However, when simply adding components, the interface and the existing components should serve as sufficient example on how the templates are intended to be used. The more messy ones (implementation of ``Run_impl``) shouldn't need to be touched.
Don't forget to add a new component to ``src/choices.hpp`` and ``src/config.hpp``; not adding them will lead to build failures or the build process ignoring the new components.

- Conventions
	- Every function that is a loop or recursion and the callbacks called by them return a boolean signaling whether it should continue executing. Receiving ``true`` from a callback means the function should stop looping resp. recursing and return ``true`` as well. Currently these functions are:
		* ``Editor::<any>.edit``
		* ``Finder::<any>.find`` and their ``…_near`` variants with their callback parameter
		* Helper functions ``Finder::for_all_edges_ordered`` and ``Finder::for_all_edges_unordered`` and their lambda parameter
