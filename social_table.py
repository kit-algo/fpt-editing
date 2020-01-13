#!/usr/bin/python3

import argparse
import collections
import pandas as pd
import numpy as np

def get_max_k_time(df):
    df = df.copy()
    graph_groups = df.groupby('Graph')
    df['max_k'] = graph_groups.k.transform(np.max)
    max_k_df = df[df.k == df.max_k]
    return max_k_df.groupby('Graph')[['k', 'Total Time [s]']].min()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate the results table of the social instances")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("gurobi_csv", help="The Gurobi CSV input file")
    parser.add_argument("gurobi_fpt_comparison_csv",
                        help="The FPT results to compare to Gurobi")
    parser.add_argument(
        '--time-limit',
        type=int,
        help="The maximum running time to use in seconds, default: 1000",
        default=1000)

    args = parser.parse_args()

    df = pd.read_csv(args.csv)

    filtered_df = df[df['Total Time [s]'] <= args.time_limit]

    gurobi_df = pd.read_csv(args.gurobi_csv)
    filtered_gurobi_df = gurobi_df[~gurobi_df.Algorithm.str.contains('Heuristic')]

    gurobi_fpt_df = pd.read_csv(args.gurobi_fpt_comparison_csv)
    filtered_gurobi_fpt_df = gurobi_fpt_df[gurobi_fpt_df['Total Time [s]'] <= args.time_limit]

    fpt_algo = 'FPT-LS-MP'
    ilp_algo = 'ILP-S-R-C4'
    all_solutions = False

    fpt_data = filtered_gurobi_fpt_df[(filtered_gurobi_fpt_df.Algorithm == fpt_algo) & (filtered_gurobi_fpt_df['All Solutions'] == all_solutions)]
    ilp_data = filtered_gurobi_df[filtered_gurobi_df.Algorithm == ilp_algo]

    general_data = fpt_data.groupby('Graph')[['n', 'm']].first()
    solved_data = ilp_data.groupby('Graph')['Solved'].any()
    fpt_st_data = get_max_k_time(fpt_data[~fpt_data.MT])
    fpt_mt_data = get_max_k_time(fpt_data[fpt_data.MT])
    ilp_st_data = get_max_k_time(ilp_data[~ilp_data.MT])
    ilp_mt_data = get_max_k_time(ilp_data[ilp_data.MT])

    df = pd.DataFrame(collections.OrderedDict([
        (('', '', 'Graph'), general_data.index),
        (('', '', 'n'), general_data.n),
        (('', '', 'm'), general_data.m),
        #(('', '', 'Solved'), solved_data),
        (('FPT', '1 core', 'k'), fpt_st_data.k),
        (('FPT', '1 core', 'Time [s]'), fpt_st_data['Total Time [s]']),
        (('FPT', '16 cores', 'k'), fpt_mt_data.k),
        (('FPT', '16 cores', 'Time [s]'), fpt_mt_data['Total Time [s]']),
        (('ILP', '1 core', 'k'), ilp_st_data.k),
        (('ILP', '1 core', 'Time [s]'), ilp_st_data['Total Time [s]']),
        (('ILP', '16 cores', 'k'), ilp_mt_data.k),
        (('ILP', '16 cores', 'Time [s]'), ilp_mt_data['Total Time [s]']),
        ]))

    df.sort_values(by=('FPT', '1 core', 'Time [s]'), inplace=True)
    print(df.to_latex(index=False, formatters=
                      {('', '', 'Solved') : lambda x : 'Yes' if x else 'No'},
                      float_format=lambda x : "{:.2f}".format(x), na_rep=" "))
