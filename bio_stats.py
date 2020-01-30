#!/usr/bin/python3

import argparse
import pandas as pd
import numpy as np
from plot_functions import algo_order

def print_solved_graphs(df, all_graphs):
    solved_df = df[df.Solved]

    unique_graphs = solved_df.groupby(['Algorithm', 'MT', 'All Solutions']).Graph.unique()

    algo_solved = list()
    for (algo, mt, all_solutions), graphs in unique_graphs.iteritems():
        algoname = algo

        if algoname in algo_order:
            algo_solved.append((algoname, graphs))

    algo_solved.sort(key=lambda x : len(x[1]))

    any_st_solved_graphs = solved_df[~solved_df.MT].Graph.unique()
    any_mt_solved_graphs = solved_df[solved_df.MT].Graph.unique()

    previous_algo = None
    previous_solved = None
    for algo, algo_solved_graphs in algo_solved:
        print("Algorithm {} solved {} graphs".format(algo, len(algo_solved_graphs)))
        if previous_algo is not None:
            num_new_solved = len([g for g in algo_solved_graphs if g not in previous_solved])
            print("Algorithm {} solved {} graphs not solved by {}".format(algo, num_new_solved, previous_algo))
            num_old_solved = len(previous_solved) - len(algo_solved_graphs) + num_new_solved
            print("Algorithm {} solved {} graphs not solved by {}".format(previous_algo, num_old_solved, algo))
            num_not_solved = len([g for g in any_st_solved_graphs if g not in algo_solved_graphs])
            print("There are {} graphs not solved by {} but by some other single-threaded algorithm".format(num_not_solved, algo))
            num_not_solved = len([g for g in any_mt_solved_graphs if g not in algo_solved_graphs])
            print("There are {} graphs not solved by {} but by some other multi-threaded algorithm".format(num_not_solved, algo))

            if "MT" in algo:
                num_additional_to_st = len([g for g in algo_solved_graphs if g not in any_st_solved_graphs])
                print("{} solved {} graphs in addition to the single-threaded solved graphs".format(algo, num_additional_to_st))

        previous_algo = algo
        previous_solved = algo_solved_graphs

    any_solved_graphs = solved_df.Graph.unique()
    print("{} graphs, any solved: {}, st solved {}, mt solved {}".format(len(all_graphs), len(any_solved_graphs), len(any_st_solved_graphs), len(any_mt_solved_graphs)))
    num_mt_unsolved = len([g for g in all_graphs if g not in any_mt_solved_graphs])
    print("In total, {} graphs were not solved by any -MT-algorithm".format(num_mt_unsolved))
    num_unsolved = len([g for g in all_graphs if g not in any_solved_graphs])
    print("In total, {} graphs were not solved by any algorithm".format(num_unsolved))

def print_percentile_improvement(df, base_algo, comparison_algo, min_time):
    solved_time_calls_base_min = df[df['Total Time [s]', base_algo] >= min_time]
    print("Of {} instances where {} needed at least {} seconds, {} was faster".format(len(solved_time_calls_base_min), base_algo, min_time, comparison_algo))
    for measure in ['Calls', 'Total Time [s]']:
        print(measure)
        solved_measure_base_min = solved_time_calls_base_min[measure].copy()
        assert(len(solved_measure_base_min) == len(solved_time_calls_base_min))

        # Restrict to graphs solved by both algorithms
        if measure == 'Calls' and solved_measure_base_min[comparison_algo].hasnans:
            solved_measure_base_min = solved_measure_base_min[solved_measure_base_min[comparison_algo].notna()]
            print("Restricting to {} instances solved by both algorithms".format(len(solved_measure_base_min)))
        else:
            solved_measure_base_min[comparison_algo].fillna(args.time_limit * 100, inplace=True)

        speedup = (solved_measure_base_min[base_algo] / solved_measure_base_min[comparison_algo]).to_numpy()
        percentiles = [0, 0.1, 1, 5, 10, 25, 50, 75, 90, 95, 99, 99.9, 100]
        speedup_percentiles = np.percentile(speedup, percentiles)
        for p, s in zip(percentiles, speedup_percentiles):
            print("  on {:.1f}% of the instances {:.2f} faster".format(100-p, s))

        #for speedup in [1, 1.1, 1.5, 2, 5, 8, 10, 20, 50, 80, 100, 200, 500, 800, 1000]:
        #    num_faster = sum(solved_measure_base_min[comparison_algo] * speedup <= solved_measure_base_min[base_algo])
        #    fraction_faster = num_faster / len(solved_measure_base_min)
        #    print(" {} times on {} ({:.2%}) instances ".format(speedup, num_faster, fraction_faster))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Calculate various statistics of the bio dataset")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("gurobi_csv", help="The Gurobi CSV input file")
    parser.add_argument("gurobi_fpt_comparison_csv",
                        help="The FPT results to compare to Gurobi")
    parser.add_argument('--min-k', type=int,
                        help="The minimum value of k to use, default: 10",
                        default=10)
    parser.add_argument(
        '--time-limit',
        type=int,
        help="The maximum running time to use in seconds, default: 1000",
        default=1000)

    args = parser.parse_args()

    df = pd.read_csv(args.csv)

    max_ks = df.groupby('Graph').max().k
    larger_k_names = max_ks[max_ks >= args.min_k].index

    num_graphs = len(df['Graph'].unique())
    num_trivial = num_graphs - len(larger_k_names)
    print("{} graphs, {} need less than {} edits, remaining: {} graphs".format(num_graphs, num_trivial, args.min_k, num_graphs - num_trivial))

    trivial_df = df[~df.Graph.isin(larger_k_names) & ~df.MT]
    max_trivial_time = trivial_df['Total Time [s]'].max()
    print("Those graph requiring less than {} edits need at maximum {} seconds for any of the algorithms {}".format(
        args.min_k,
        max_trivial_time,
        ", ".join(trivial_df.Algorithm.unique())
    ))
    print("Algorithm FPT-LS-MP needs at maximum {} seconds".format(trivial_df[trivial_df.Algorithm == 'FPT-LS-MP']['Total Time [s]'].max()))
    already_solved_graphs = trivial_df[trivial_df.Solved & (trivial_df.k == 0)].Graph.unique()
    print("{} of them require no edits at all".format(len(already_solved_graphs)))

    df_filter = df.Graph.isin(larger_k_names) & (df['Total Time [s]'] <=
                                                 args.time_limit)
    filtered_df = df[df_filter].copy()
    filtered_df.loc[filtered_df['All Solutions'], 'Algorithm'] += "-All"
    filtered_df.loc[filtered_df.MT, 'Algorithm'] += "-MT"

    print("For the FPT algorithms")
    print_solved_graphs(filtered_df, larger_k_names)

    gurobi_df = pd.read_csv(args.gurobi_csv)
    filtered_gurobi_df = gurobi_df[gurobi_df.Graph.isin(larger_k_names) & ~gurobi_df.Algorithm.str.contains('Heuristic') & (gurobi_df.Algorithm != 'ILP-S-R-C4-H')].copy()

    filtered_gurobi_df.loc[filtered_gurobi_df.MT, 'Algorithm'] += "-MT"

    print("For Gurobi")
    print_solved_graphs(filtered_gurobi_df, larger_k_names)

    fpt_solved_graphs = filtered_df[filtered_df.Solved].Graph.unique()
    gurobi_solved_graphs = filtered_gurobi_df[filtered_gurobi_df.Solved].Graph.unique()
    fpt_but_not_gurobi_solved_graphs = [g for g in fpt_solved_graphs if not g in gurobi_solved_graphs]
    gurobi_but_not_fpt_solved_graphs = [g for g in gurobi_solved_graphs if not g in fpt_solved_graphs]

    print("Gurobi solved {} graphs FPT did not solve. FPT solved {} graphs Gurobi did not solve.".format(len(gurobi_but_not_fpt_solved_graphs), len(fpt_but_not_gurobi_solved_graphs)))

    gurobi_fpt_df = pd.read_csv(args.gurobi_fpt_comparison_csv)
    filtered_gurobi_fpt_df = gurobi_fpt_df[
        gurobi_fpt_df.Graph.isin(larger_k_names)
        & (gurobi_fpt_df['Total Time [s]'] <= args.time_limit)].copy()

    filtered_gurobi_fpt_df.loc[filtered_gurobi_fpt_df['All Solutions'], 'Algorithm'] += '-All'
    filtered_gurobi_fpt_df.loc[filtered_gurobi_fpt_df.MT, 'Algorithm'] += '-MT'

    print("Gurobi FPT comparison")
    print_solved_graphs(filtered_gurobi_fpt_df, larger_k_names)

    additional_comparison_df = filtered_df[(filtered_df.Threads == 16) & (
        filtered_df.Algorithm == "FPT-LS-MP-All-MT") & (filtered_df.Permutation < 4)]
    assert(len(additional_comparison_df) > 0)

    final_comparison_df = pd.concat([
        filtered_gurobi_fpt_df,
        filtered_gurobi_df[(filtered_gurobi_df.Algorithm == "ILP-S-R-C4") | (filtered_gurobi_df.Algorithm == "ILP-S-R-C4-MT")],
        additional_comparison_df
    ])

    print("Final comparison")
    print_solved_graphs(final_comparison_df, larger_k_names)

    print("Final comparison, only first solution")
    final_comparison_df_first_only = final_comparison_df[~final_comparison_df['All Solutions']]
    print_solved_graphs(final_comparison_df_first_only, larger_k_names)


    gurobi_indexed = filtered_gurobi_df[filtered_gurobi_df.Solved].set_index(['Graph', 'Permutation', 'Algorithm'])
    assert(not gurobi_indexed.index.duplicated().any())
    gurobi_indexed_unstacked = gurobi_indexed.unstack()
    for base_algo, comparison_algo in [
            ('ILP-B', 'ILP-S'),
            ('ILP-S', 'ILP-S-R'),
            ('ILP-S-R', 'ILP-S-R-C4')
            ]:
        print_percentile_improvement(gurobi_indexed_unstacked, base_algo, comparison_algo, 0)

    final_comparison_indexed = final_comparison_df[final_comparison_df.Solved].set_index(['Graph', 'Permutation', 'Algorithm'])
    assert(not final_comparison_indexed.index.duplicated().any())
    final_comparison_unstacked = final_comparison_indexed.unstack()
    for base_algo, comparison_algo, min_time in [
            ('ILP-S-R-C4', 'FPT-LS-MP', 0),
            ('ILP-S-R-C4', 'FPT-LS-MP-All', 0),
            ('ILP-S-R-C4', 'ILP-S-R-C4-MT', 0),
            ('FPT-LS-MP-All', 'FPT-LS-MP', 0),
            ('FPT-LS-MP', 'FPT-LS-MP-MT', 0),
            ('FPT-LS-MP-All', 'FPT-LS-MP-All-MT', 0),
            ('FPT-LS-MP-All-MT', 'FPT-LS-MP-MT', 0),
            ('ILP-S-R-C4', 'FPT-LS-MP-MT', 0),
            ('ILP-S-R-C4', 'FPT-LS-MP-All-MT', 0),
            ]:
        print_percentile_improvement(final_comparison_unstacked, base_algo, comparison_algo, min_time)

    solved_indexed = filtered_df[filtered_df.Solved & ~filtered_df.MT].set_index(['Graph', 'Permutation', 'Algorithm'])
    assert(not solved_indexed.index.duplicated().any())

    solved_calls_time = solved_indexed[['Calls', 'Total Time [s]']].unstack()
    print(solved_calls_time)
    # Now, every algorithm is a column with the time of that algorithm
    solved_time = solved_calls_time['Total Time [s]']

    algo_indices = {}

    for algo in solved_time.columns:
        algo_indices[algo] = set(solved_time[algo].sort_values()[:10000].index)

    for algo1, algo2 in [
            ('FPT-G-F-All', 'FPT-LS-F-All'),
            ('FPT-MD-F-All', 'FPT-LS-F-All'),
            ('FPT-G-MP-All', 'FPT-LS-MP-All'),
            ('FPT-MD-MP-All', 'FPT-LS-MP-All'),
            ('FPT-LS-F-All', 'FPT-LS-MP-All'),
            ('FPT-G-F-All', 'FPT-G-MP-All'),
            ('FPT-MD-F-All', 'FPT-MD-MP-All'),
            ('FPT-G-F-All', 'FPT-LS-MP-All'),
            ('FPT-MD-F-All', 'FPT-LS-MP-All')
            ]:
        intersection = len(algo_indices[algo1].intersection(algo_indices[algo2]))
        algo1_len = len(algo_indices[algo1])
        algo2_len = len(algo_indices[algo2])
        print("Fastest 10000 graphs of {} and {} overlap in {} out of {} instances".format(algo1, algo2, intersection, algo1_len + algo2_len - intersection))

    for base_algo, comparison_algo, min_time in [
            ('FPT-LS-M-All', 'FPT-LS-MP-All', 0),
            ('FPT-LS-M-All', 'FPT-LS-MP-All', 10),
            ('FPT-LS-F-All', 'FPT-LS-MP-All', 0),
            ('FPT-LS-F-All', 'FPT-LS-MP-All', 10),
            ('FPT-U-MP-All', 'FPT-G-MP-All', 0),
            ('FPT-G-MP-All', 'FPT-MD-MP-All', 0),
            ('FPT-MD-MP-All', 'FPT-LS-MP-All', 0),
            ('FPT-LS-MP-All', 'FPT-LP-MP-All', 0),
            ('FPT-LP-MP-All', 'FPT-LS-MP-All', 0),
            ('FPT-LS-MP-All', 'FPT-U-MP-All', 0),
            ('FPT-U-MP-All', 'FPT-LS-MP-All', 0),
            ('FPT-U-MP-All', 'FPT-LS-MP-All', 1),
            ('FPT-MD-MP-All', 'FPT-LP-MP-All', 0)
            ]:

        solved_base = solved_time[~solved_time[base_algo].isna()]
        solved_comparison_solved = sum(~(solved_base[comparison_algo].isna()))
        output = "Of {} instances solved by {}, {} solved {} instances. ".format(len(solved_base), base_algo, comparison_algo, solved_comparison_solved)
        for a in [base_algo, comparison_algo]:
            output += "{} solved ".format(a)
            for time_limit in [1, 2, 5, 10, 20, 50, 100, 500, 1000]:
                a_solved = sum(solved_base[a] <= time_limit)
                output += "{} instances ({:.2%}) in {}, ".format(a_solved, a_solved/len(solved_base), time_limit)
            output += "seconds. "
        print(output)

        print_percentile_improvement(solved_calls_time, base_algo, comparison_algo, min_time)

    example_graph = 'cost_matrix_component_nr_575_size_91_cutoff_10.0'
    example_fpt_df = filtered_gurobi_fpt_df[(filtered_gurobi_fpt_df.Graph == example_graph) & filtered_gurobi_fpt_df.MT & (filtered_gurobi_fpt_df.Permutation == 0)]
    assert(len(example_fpt_df.Algorithm.unique()) == 1)
    assert(example_fpt_df.Solved.any())
    assert(len(example_fpt_df.k.unique()) == len(example_fpt_df))

    example_gurobi_df = filtered_gurobi_df[(filtered_gurobi_df.Graph == example_graph) & filtered_gurobi_df.MT & (filtered_gurobi_df.Permutation == 0) & (filtered_gurobi_df.Algorithm == 'ILP-S-R-C4-MT')]
    assert(len(example_gurobi_df) == 1)

    example_k = example_fpt_df[example_fpt_df.Solved].k.iloc[0]
    last_calls = example_fpt_df[example_fpt_df.Solved].Calls.iloc[0]
    second_last_calls = example_fpt_df[example_fpt_df.k == (example_k - 1)].Calls.iloc[0]
    print("Graph {}, k {}, {} needs {} calls total (last two k: {}, {}, max: {}), {} needs {} calls".format(example_graph, example_k, example_fpt_df.Algorithm.iloc[0], example_fpt_df.Calls.sum(), second_last_calls, last_calls, example_fpt_df.Calls.max(), example_gurobi_df.Algorithm.iloc[0], example_gurobi_df.Calls.sum()))
