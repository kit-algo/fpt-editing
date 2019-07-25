#!/usr/bin/env python3

from networkit import *
import argparse
import os

parser = argparse.ArgumentParser(description="Run QTM for all supplied graphs")
parser.add_argument('output_dir')
parser.add_argument('graphs', nargs='+')

args = parser.parse_args()


print('Graph,k')

for graph_path in args.graphs:
    G = readGraph(graph_path, Format.METIS)
    G.indexEdges()

    min_k = G.numberOfEdges()
    best_result = None
    for i in range(16):
        lin = community.QuasiThresholdEditingLinear(G).run()
        qtm = community.QuasiThresholdEditingLocalMover(G, lin.getParents(), G.numberOfEdges() * 10)
        qtm.run()
        if qtm.getNumberOfEdits() < min_k:
            min_k = qtm.getNumberOfEdits()
            best_result = qtm.getQuasiThresholdGraph()

    bn = os.path.basename(graph_path)
    graph_name, _ = os.path.splitext(bn)

    writeGraph(best_result, os.path.join(args.output_dir, bn), Format.METIS)

    print("{},{}".format(graph_name, min_k))
