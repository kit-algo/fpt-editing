#!/usr/bin/python3

from networkit import *
import glob
import copy
import argparse
import re


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create GraphML files for visualization.")
    parser.add_argument("orig", help="The original graph")
    parser.add_argument("out_path", help="The output file path")
    parser.add_argument("edited", help="The edited graphs", nargs="+")

    args = parser.parse_args()

    orig = readGraph(args.orig, Format.METIS)

    solutions = [readGraph(p, Format.METIS) for p in args.edited]

    combined = copy.copy(orig)

    for sol in solutions:
        for (u, v) in sol.edges():
            if not combined.hasEdge(u, v):
                combined.addEdge(u, v)

    combined.indexEdges()

    edge_attributes = {}
    node_attributes = {}

    for sol_path, sol in zip(args.edited, solutions):
        sol_name = re.search('w(\d+)', sol_path)[1]

        edits = graph.EdgeEditDifference(orig, sol).run().getEdits()

        edge_contained = [False for i in range(combined.upperEdgeIdBound())]
        combined.forEdges(lambda u, v, w, eid : edge_contained.__setitem__(eid, sol.hasEdge(u, v)))

        edge_attributes['{}_contained'.format(sol_name)] = edge_contained

        edge_status = [0 for i in range(combined.upperEdgeIdBound())]

        for e in edits:
            eid = combined.edgeId(e.u, e.v)
            edge_contained[eid] = True

            if e.type == dynamic.GraphEvent.EDGE_REMOVAL:
                edge_status[eid] = -1
            elif e.type == dynamic.GraphEvent.EDGE_ADDITION:
                edge_status[eid] = 1
            else:
                raise RuntimeError("unexpected event {}".format(e))

        node_attributes['{}_clustering'.format(sol_name)] = components.ConnectedComponents(sol).run().getPartition()

        edge_attributes['{}_status'.format(sol_name)] = edge_status

    graphio.GraphMLWriter().write(combined, args.out_path, edgeAttributes=edge_attributes, nodeAttributes=node_attributes)
