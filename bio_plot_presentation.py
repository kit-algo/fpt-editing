#!/usr/bin/python3

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
import argparse
from collections import defaultdict

from plot_functions import solved_instances_over_measure_plot, plot_speedup_per_instance_for_one_algorithm, plot_solutions

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create plots out of the result data.")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("fpt_gurobi_csv", help="The FPT Gurobi comparison CSV input file")
    #parser.add_argument("qtm_csv", help="The QTM CSV input file")
    parser.add_argument("gurobi_csv", help="The Gurobi CSV input file")
    parser.add_argument("output_dir", help="The output directory where plots shall be written")
    parser.add_argument('--min-k', type=int, help="The minimum value of k to use, default: 10", default=10)
    parser.add_argument('--time-limit', type=int, help="The maximum running time to use in seconds, default: 1000", default=1000)

    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    fpt_gurobi_df = pd.read_csv(args.fpt_gurobi_csv)
    gurobi_df = pd.read_csv(args.gurobi_csv)

    max_ks = df.groupby('Graph').max().k
    larger_k_names = max_ks[max_ks >= args.min_k].index

    filtered_df = df[(df.Permutation < 4) & df.Graph.isin(larger_k_names) & (df['Total Time [s]'] <= args.time_limit)]

    filtered_gurobi_fpt_df = fpt_gurobi_df[(fpt_gurobi_df.Permutation < 4) & fpt_gurobi_df.Graph.isin(larger_k_names) &  (fpt_gurobi_df['Total Time [s]'] <= args.time_limit)]

    gurobi_filtered_df = gurobi_df[(gurobi_df.Permutation < 4) & gurobi_df.Graph.isin(larger_k_names)]

    #my_runtime_plot(filtered_df, 'Time [s]')
    #my_performanceplot(filtered_df, 'Time [s]', False)
    df_st_4 = filtered_df[(filtered_df.MT == False) & (filtered_df.l == 4) & filtered_df.Algorithm.str.contains('-LS-')]

    df_st_best_first = filtered_gurobi_fpt_df[~filtered_gurobi_fpt_df.MT & (filtered_gurobi_fpt_df.Algorithm == "FPT-LS-MP") & (~filtered_gurobi_fpt_df['All Solutions'])]

    gurobi_st = gurobi_filtered_df[gurobi_filtered_df.MT == False]
    gurobi_st_best = gurobi_st[gurobi_st.Algorithm == 'ILP-S-R-C4']

    df_mt = filtered_gurobi_fpt_df[(filtered_gurobi_fpt_df.l == 4) & (filtered_gurobi_fpt_df.Threads == 16) & (filtered_gurobi_fpt_df.Algorithm == 'FPT-LS-MP') & (~filtered_gurobi_fpt_df['All Solutions'])]
    gurobi_mt = gurobi_filtered_df[(gurobi_filtered_df.Threads == 16) & (gurobi_filtered_df.Algorithm == 'ILP-S-R-C4')]

    for dfs, path in [
    # Plot 1: ST FPT variants
            (df_st_4, '{}/bio_times_st_min_k_{}.pdf'.format(args.output_dir, args.min_k)),
    # Plot 2: ST Gurobi vs. best FPT
            (pd.concat([df_st_best_first, gurobi_st_best]), '{}/bio_times_gurobi_st_min_k_{}.pdf'.format(args.output_dir, args.min_k)),
    # Plot 3: MT vs. ST best Gurobi/FPT
            (pd.concat([df_st_best_first, gurobi_st_best, df_mt, gurobi_mt]), '{}/bio_times_gurobi_mt_min_k_{}.pdf'.format(args.output_dir, args.min_k)),
            ]:

        #fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True, figsize=(10, 4))
        fig, ax = plt.subplots(1, 1, figsize=(6, 4))
        solved_instances_over_measure_plot(dfs, 'Total Time [s]', ax)
        #solved_instances_over_measure_plot(df_st_4, 'Calls', ax2)
        ax.set_ylabel('Solved (Graphs $\\times$ Node Id Permutations)')
        ax.legend()
        fig.tight_layout()
        fig.savefig(path)

#    fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True, figsize=(10, 4))
#    plot_speedup_per_instance_for_one_algorithm(filtered_df[filtered_df.Algorithm == "LocalSearchLB-Most"], ax1)
#    ax1.set_title('LocalSearchLB-Most')
#    plot_speedup_per_instance_for_one_algorithm(filtered_df[filtered_df.Algorithm == "LocalSearchLB-Most Pruned"], ax2)
#    ax2.set_title('LocalSearchLB-Most Pruned')
#    ax1.legend(title='Threads')
#    ax1.set_ylabel('Speedup')
#    fig.tight_layout()
#    fig.savefig('{}/bio_speedup_min_k_{}.pdf'.format(args.output_dir, args.min_k))

    #qtm_df = pd.read_csv(args.qtm_csv)

#    fig = plot_max_k_for_all_algorithms(df, qtm_df)
#    fig.tight_layout()
#    fig.savefig('{}/bio_max_k.pdf'.format(args.output_dir))
#
#    fig = plot_solutions(df)
#    fig.tight_layout()
#    fig.savefig('{}/bio_solutions.pdf'.format(args.output_dir))
