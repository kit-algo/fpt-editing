[TOC]

### EDITOR
Centerpiece tying the other components together and implementing the main structure for FPT Edge Editing algorithms.

- Editor
Lets the finder enumerate forbidden subgraphs and gives them to the consumers. Fetches and handles the results from the consumers. All edges of the selected forbidden subgraph are edited and recursively solved in turn.

- MT
Multithreaded variant of Editor. All threads monitor the size of a shared work queue. If the size falls below a threshold, a thread noticing this gives the unhandled parts of its initial recursion to the queue.

### FINDER
Component that finds forbidden subgraphs.
What is considered a forbidden subgraph is defined by the components themselves; currently either P~4~/C~4~ or P~5~/C~5~ as indicated by the number following the algorithm name.

- Center (Center_4, Center_5)
	Tries each edge (for P~x~/C~x~, x even) resp. vertex (x odd) as the center of a forbidden subgraph. The center is expanded by simultaneously adding an edge to both sides while maintaining that the subgraph is a path (thus transforming a P~y~ to P~y+2~) until the desired length is reached. In the last expansion it is allowed to transform the path (P~y~) into a cycle (C~y+2~) instead.


### CONSUMERS
Components that use the forbidden subgraphs produced by a finder, split into three groups:
1. Selectors, choosing a forbidden subgraph for editing
2. Lower bound calculators
3. Others, that do not help the editor but produce some other result or statistic

An algorithm may be part of multiple groups

- **Selectors** (branching strategy):
	- First
		Chooses the first forbidden subgraph found

	- Most
		Chooses the unedited edge participating in most forbidden subgraphs. Includes variants for early pruning (``Most_Pruned``) and only using a single node pair (this does not actually give an advantage over early pruning, though).

	- Earlier version of a Least_Unedited selector that selects the subgraph that contains the least amount of unedited (i.e., unblocked) node pairs. In preliminary experiments, Most turned out to be superior to this.

- **Lower bound calculators**:
	- No
		pseudo calculator; returns a lower bound of 1 if at least one forbidden subgraph was found

	- Basic
		Calculates a lower bound by collecting non-overlapping forbidden subgraphs; greedy

	- Updated
	  	Updates the basic greedy bound as the graph is modified

	- Gurobi
	  	Calculates a lower bound using Gurobi with the LP relaxation. Note that this bound currently does not work with the parallel editor MT.

	- ARW
	  	Our local search lower bound that is based on the ARW independent set heuristic.

	- Min_Deg
	  	Calculates a lower bound based on the minimum degree independent set heuristic.

	- Various earlier versions of bounds that call external independent set solvers or store subgraphs explicitly for our local search that were used for preliminary experiments.

- **Other**:
(currently none)


### GRAPHS
Component that stores a graph

- Matrix
	Represents the graph as an adjacency matrix


### OPTIONS
These are not algorithm choices, but instead define which edges are considered editable

- **MODES**:
Which problem is to be solved?
	- Edit
		Edge Editing Problem (both inserting and deleting edges are allowed)
	- Delete
		Edge Deletion Problem (edges may only be deleted, not inserted)
	- Insert
		Edge Insertion Problem (edges may only be inserted, not deleted)

- **RESTRICTIONS**:
Prevent edges from being edited based on
	- None
		None of the other restrictions apply
	- Undo
		Editing an edge that is currently considered edited (thus reverting or undoing the edit) is not allowed
	- Redundant
		Editing an edge that would lead to an already explored situation is forbidden

- **CONVERSIONS**:
How is the editing an edge converting a Path to a Cycle and vice versa handled?
	- Normal
		No special treatment
	- Last
		All other edges of the forbidden subgraph are handled first
	- Skip
		The edge will not be edited
