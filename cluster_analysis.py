#!/usr/bin/python3

from networkit import *
import glob
import functools
import collections
import copy
import pandas

class HashablePartition(structures.Partition):
    @classmethod
    def fromPartition(cls, p):
        hp = cls(len(p))
        hp.setUpperBound(p.upperBound())
        for u in range(len(p)):
            hp[u] = p[u]
        return hp
    
    def __hash__(self):
        """
        Hash the partition.
        This hash value does not depend on the actually used
        partition ids. Instead, it hashes a list of new
        partition ids that are assigned in a canonical way.
        Returns
        -------
        int
                A hash value.
        """
        canonicIds = list()
        toCanonicId = dict()

        nextId = 0
        for i in range(self.numberOfElements()):
            s = self[i]
            if not s in toCanonicId:
                toCanonicId[s] = nextId
                nextId += 1

            canonicIds.append(toCanonicId[s])
        return hash(tuple(canonicIds))

class HashableGraphEvent(dynamic.GraphEvent):
    def __hash__(self):
        if self.type == dynamic.GraphEvent.TIME_STEP:
            return hash(self.type)
        else:
            return hash((self.type, self.u, self.v, self.w))

graph_names = glob.glob("data/*.graph")

input_data = []

for g_path in graph_names:
    short_name = g_path[g_path.find("/")+1:g_path.find(".")]
    print(short_name)

    solution_paths = glob.glob("{}.e.*.w*[0-9]".format(g_path))

    if not len(solution_paths):
        print("No solutions found")
        continue

    orig = graphtools.toUnweighted(readGraph(g_path, Format.METIS))

    solutions = [readGraph(p, Format.METIS) for p in solution_paths]

    edits = [[HashableGraphEvent(e.type, e.u, e.v, e.w) for e in dynamic.GraphDifference(orig, s).run().getEdits()] for s in solutions]

    assert(len(set(map(len, edits))) == 1)

    common_edits = functools.reduce(set.intersection, map(set, edits))
    common_insertions = [e for e in common_edits if e.type == dynamic.GraphEvent.EDGE_ADDITION]
    common_deletions = [e for e in common_edits if e.type == dynamic.GraphEvent.EDGE_REMOVAL]

    all_edits = functools.reduce(set.union, map(set, edits))
    all_insertions = [e for e in all_edits if e.type == dynamic.GraphEvent.EDGE_ADDITION]
    all_deletions = [e for e in all_edits if e.type == dynamic.GraphEvent.EDGE_REMOVAL]

    ccs = [HashablePartition.fromPartition(components.ConnectedComponents(g).run().getPartition()) for g in solutions]

    ccs_subsets = list(map(Partition.numberOfSubsets, ccs))

    num_different_components = len(set(ccs))

    common_edits_solution = copy.copy(orig)
    dynamic.GraphUpdater(common_edits_solution).update(common_edits)
    common_partition = components.ConnectedComponents(common_edits_solution).run().getPartition()

    all_edits_solution = copy.copy(orig)
    dynamic.GraphUpdater(all_edits_solution).update(all_edits)
    all_partition = components.ConnectedComponents(all_edits_solution).run().getPartition()


    ints = community.PartitionIntersection().calculate
    finest_clustering = functools.reduce(ints, ccs)

    input_data.append(collections.OrderedDict([
        (('', 'Graph'), short_name),
        (('', 'k'), len(edits[0])),
        (('#Solu-', 'tions'), len(edits)),
        (('#Clus-', 'terings'), num_different_components),
        (('#Clusters', 'Min'), min(ccs_subsets)),
        (('#Clusters', 'Max'), max(ccs_subsets)),
        #(('Common', 'Edits'), len(common_edits)),
        (('Common', 'Ins.'), len(common_insertions)),
        (('Common', 'Del.'), len(common_deletions)),
        (('Common', 'Clus.'), common_partition.numberOfSubsets()),
        #(('Total', 'Edits'), len(all_edits)),
        (('Total', 'Ins.'), len(all_insertions)),
        (('Total', 'Del.'), len(all_deletions)),
        (('Total', 'Clus.'), finest_clustering.numberOfSubsets())
        ]))

    print(input_data[-1])

df = pandas.DataFrame(input_data, columns=pandas.MultiIndex.from_tuples(input_data[0].keys()))
df.sort_values(by=('', 'k'), inplace=True)
print(df.to_latex(index=False))
