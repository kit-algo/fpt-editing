#!/usr/bin/python3

import argparse
import pandas as pd
from plot_functions import plot_max_k_for_all_algorithms

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create plots out of the result data.")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("gurobi_csv", help="The Gurobi CSV input file")
    parser.add_argument("gurobi_fpt_csv", help="The Gurobi CSV input file")
    parser.add_argument("qtm_csv", help="The QTM CSV input file")
    parser.add_argument("output_dir", help="The output directory where plots shall be written")

    args = parser.parse_args()

    df = pd.read_csv(args.csv)

    small_graph_df = df[df.Solved & (df.k < 10)].groupby('Graph').first()
    small_graphs = small_graph_df.index

    fpt_gurobi_df = pd.read_csv(args.gurobi_fpt_csv)

    fpt_df = fpt_gurobi_df[(fpt_gurobi_df.Algorithm == 'FPT-LS-MP') & fpt_gurobi_df.MT & ~fpt_gurobi_df['All Solutions']]

    gurobi_df = pd.read_csv(args.gurobi_csv)

    ilp_df = gurobi_df[gurobi_df.Algorithm == 'ILP-S-R-C4']

    ilp_heur_df = gurobi_df[gurobi_df.Algorithm == 'Heuristic-ILP-S-R-C4']

    qtm_all = pd.read_csv(args.qtm_csv)

    qtm_large = qtm_all[~qtm_all.Graph.isin(small_graphs)]

    qtm_small = qtm_all[qtm_all.Graph.isin(small_graphs)].set_index('Graph')

    print("Small graphs")

    print("There are {} graphs that require less than 10 edits".format(len(small_graphs)))
    small_qtm_offset = qtm_small.k - small_graph_df.k
    assert(not small_qtm_offset.hasnans)
    assert(not (small_qtm_offset < 0).any())
    print("Of them, QTM solved {} exactly, {} with offset 1, {} with offset 2, {} with offset 3 and {} with offset larger than three".format(sum(small_qtm_offset == 0), sum(small_qtm_offset == 1), sum(small_qtm_offset == 2), sum(small_qtm_offset == 3), sum(small_qtm_offset > 3)))
    print(small_qtm_offset.unique())

    fig = plot_max_k_for_all_algorithms(fpt_df, ilp_df, ilp_heur_df, qtm_large)
    fig.tight_layout()
    fig.savefig('{}/bio_max_k.pdf'.format(args.output_dir))
