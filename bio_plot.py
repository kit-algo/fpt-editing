#!/usr/bin/python3

import argparse
import pandas as pd
import matplotlib.pyplot as plt
from plot_functions import solved_instances_over_measure_plot, plot_speedup_per_instance_for_one_algorithm, plot_solutions

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create plots out of the result data.")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("gurobi_csv", help="The Gurobi CSV input file")
    parser.add_argument("gurobi_fpt_comparison_csv", help="The FPT results to compare to Gurobi")
    parser.add_argument("output_dir", help="The output directory where plots shall be written")
    parser.add_argument('--min-k', type=int, help="The minimum value of k to use, default: 10", default=10)
    parser.add_argument('--solved-only', action="store_true", help="If only solved graphs shall be considered")
    parser.add_argument('--time-limit', type=int, help="The maximum running time to use in seconds, default: 1000", default=1000)

    args = parser.parse_args()

    df = pd.read_csv(args.csv)

    max_ks = df.groupby('Graph').max().k
    larger_k_names = max_ks[max_ks >= args.min_k].index

    df_filter = df.Graph.isin(larger_k_names) & (df['Total Time [s]'] <= args.time_limit)

    if args.solved_only:
        solved_graphs = df[df.Solved].groupby('Graph').first().index
        filtered_df = df[df_filter & df.Graph.isin(solved_graphs)]
    else:
        filtered_df = df[df_filter]

    df_st_4 = filtered_df[(filtered_df.MT == False) & (filtered_df.l == 4)]
    fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True, figsize=(10, 4))
    solved_instances_over_measure_plot(df_st_4, 'Total Time [s]', ax1, args.time_limit)
    solved_instances_over_measure_plot(df_st_4, 'Calls', ax2)
    ax1.set_ylabel('Solved (Graphs $\\times$ Node Id Permutations)')
    ax2.legend()
    fig.tight_layout()
    fig.savefig('{}/bio_times_calls_st_min_k_{}.pdf'.format(args.output_dir, args.min_k))

    fig, ax = plt.subplots(1, 1, figsize=(6, 4))
    solved_instances_over_measure_plot(df_st_4, 'Total Time [s]', ax, args.time_limit)
    ax.set_ylabel('Solved (Graphs $\\times$ Node Id Permutations)')
    ax.legend()
    fig.tight_layout()
    fig.savefig('{}/bio_times_st_min_k_{}.pdf'.format(args.output_dir, args.min_k))

    fig = plot_solutions(df)
    fig.tight_layout()
    fig.savefig('{}/bio_solutions.pdf'.format(args.output_dir))

    gurobi_df = pd.read_csv(args.gurobi_csv)
    filtered_gurobi_df = gurobi_df[gurobi_df.Graph.isin(larger_k_names)]

    gurobi_fpt_df = pd.read_csv(args.gurobi_fpt_comparison_csv)
    filtered_gurobi_fpt_df = gurobi_fpt_df[
        gurobi_fpt_df.Graph.isin(larger_k_names) &
        (gurobi_fpt_df['Total Time [s]'] <= args.time_limit)
    ]

    additional_comparison_df = filtered_df[(filtered_df.Threads == 16) & (filtered_df.Algorithm == "FPT-LS-MP") & (filtered_df.Permutation < 4)]

    final_comparison_df = pd.concat([
        filtered_gurobi_fpt_df,
        filtered_gurobi_df[(filtered_gurobi_df.Algorithm == "ILP-S-R-C4")],
        additional_comparison_df
    ])

    fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True, figsize=(10, 4))
    ax.legend()
    solved_instances_over_measure_plot(final_comparison_df, 'Total Time [s]', ax1, args.time_limit)
    solved_instances_over_measure_plot(final_comparison_df, 'Calls', ax2, args.time_limit)
    ax1.set_ylabel('Solved (Graphs $\\times$ Node Id Permutations)')
    ax2.legend()
    fig.tight_layout()
    fig.savefig('{}/bio_times_calls_gurobi_fpt_min_k_{}.pdf'.format(args.output_dir, args.min_k))


    fig, ax = plt.subplots(1, 1, figsize=(5, 4))
    #plot_speedup_per_instance_for_one_algorithm(filtered_df[filtered_df.Algorithm == "FPT-LS-M"], ax1)
    #ax1.set_title('FPT-LS-M-All')
    plot_speedup_per_instance_for_one_algorithm(filtered_df[filtered_df.Algorithm == "FPT-LS-MP"], ax)
    ax.set_title('FPT-LS-MP-All')
    ax.legend(title='Threads')
    ax.set_ylabel('Speedup')
    fig.tight_layout()
    fig.savefig('{}/bio_speedup_min_k_{}.png'.format(args.output_dir, args.min_k), dpi=300)

    fig, ax = plt.subplots(1, 1, figsize=(5, 4))
    solved_instances_over_measure_plot(gurobi_df[gurobi_df.MT == False], 'Total Time [s]', ax, args.time_limit)
    ax.set_ylabel('Solved (Graphs $\\times$ Node Id Permutations)')
    ax.legend()
    fig.tight_layout()
    fig.savefig('{}/bio_gurobi_times_min_k_{}.pdf'.format(args.output_dir, args.min_k), dpi=300)
