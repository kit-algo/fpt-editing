#!/usr/bin/python3

import argparse
import pandas as pd
from plot_functions import plot_max_k_for_all_algorithms
import matplotlib.pyplot as plt

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create plots out of the result data.")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("gurobi_csv", help="The Gurobi CSV input file")
    parser.add_argument("gurobi_fpt_csv", help="The Gurobi CSV input file")
    parser.add_argument("qtm_csv", help="The QTM CSV input file")
    parser.add_argument("output_dir", help="The output directory where plots shall be written")
    parser.add_argument('--min-k', type=int, help="The minimum value of k to use, default: 10", default=10)

    args = parser.parse_args()

    df = pd.read_csv(args.csv)

    small_graph_df = df[df.Solved & (df.k < args.min_k)].groupby('Graph').first()
    small_graphs = small_graph_df.index

    fpt_gurobi_df = pd.read_csv(args.gurobi_fpt_csv)

    fpt_df = fpt_gurobi_df[(fpt_gurobi_df.Algorithm == 'FPT-LS-MP') & fpt_gurobi_df.MT & ~fpt_gurobi_df['All Solutions'] & ~fpt_gurobi_df.Graph.isin(small_graphs)]

    gurobi_df = pd.read_csv(args.gurobi_csv)

    ilp_df = gurobi_df[(gurobi_df.Algorithm == 'ILP-S-R-C4') & ~gurobi_df.Graph.isin(small_graphs)]

    ilp_heur_df = gurobi_df[(gurobi_df.Algorithm == 'Heuristic-ILP-S-R-C4') & ~gurobi_df.Graph.isin(small_graphs)]

    qtm_all = pd.read_csv(args.qtm_csv)

    qtm_large = qtm_all[~qtm_all.Graph.isin(small_graphs)]

    qtm_small = qtm_all[qtm_all.Graph.isin(small_graphs)].set_index('Graph')

    print("Small graphs")

    print("There are {} graphs that require less than {} edits".format(len(small_graphs), args.min_k))
    small_qtm_offset = qtm_small.k - small_graph_df.k
    assert(not small_qtm_offset.hasnans)
    assert(not (small_qtm_offset < 0).any())
    print("Of them, QTM solved {} exactly, {} with offset 1, {} with offset 2, {} with offset 3 and {} with offset larger than three".format(sum(small_qtm_offset == 0), sum(small_qtm_offset == 1), sum(small_qtm_offset == 2), sum(small_qtm_offset == 3), sum(small_qtm_offset > 3)))
    print(small_qtm_offset.unique())

    for name, fig in plot_max_k_for_all_algorithms(fpt_df, ilp_df, ilp_heur_df, qtm_large).items():
        fig.savefig('{}/bio_heur_{}.pgf'.format(args.output_dir, name))
        plt.close(fig)
