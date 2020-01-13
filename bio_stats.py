#!/usr/bin/python3

import argparse
import pandas as pd

def print_solved_graphs(df, all_graphs):
    solved_df = df[df.Solved]

    unique_graphs = solved_df.groupby(['Algorithm', 'MT', 'All Solutions']).Graph.unique()

    algo_solved = list()
    for (algo, mt, all_solutions), graphs in unique_graphs.iteritems():
        algoname = algo
        if mt:
            algoname += "-MT"
        if all_solutions:
            algoname += "-All"

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

    trivial_df = df[~df.Graph.isin(larger_k_names)]
    max_trivial_time = trivial_df['Total Time [s]'].max()
    print("Those graph requiring less than {} edits need at maximum {} seconds for any of the algorithms {}".format(
        args.min_k,
        max_trivial_time,
        ", ".join(trivial_df.Algorithm.unique())
    ))
    already_solved_graphs = trivial_df[trivial_df.Solved & (trivial_df.k == 0)].Graph.unique()
    print("{} of them require no edits at all".format(len(already_solved_graphs)))

    df_filter = df.Graph.isin(larger_k_names) & (df['Total Time [s]'] <=
                                                 args.time_limit)
    filtered_df = df[df_filter]

    print("For the FPT algorithms")
    print_solved_graphs(filtered_df, larger_k_names)

    gurobi_df = pd.read_csv(args.gurobi_csv)
    filtered_gurobi_df = gurobi_df[gurobi_df.Graph.isin(larger_k_names) & ~gurobi_df.Algorithm.str.contains('Heuristic') & (gurobi_df.Algorithm != 'ILP-S-R-C4-H')]

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
        & (gurobi_fpt_df['Total Time [s]'] <= args.time_limit)]

    print("Gurobi FPT comparison")
    print_solved_graphs(filtered_gurobi_fpt_df, larger_k_names)

    additional_comparison_df = filtered_df[(filtered_df.Threads == 16) & (
        filtered_df.Algorithm == "FPT-LS-MP") & (filtered_df.Permutation < 4)]

    final_comparison_df = pd.concat([
        filtered_gurobi_fpt_df,
        filtered_gurobi_df[(filtered_gurobi_df.Algorithm == "ILP-S-R-C4")],
        additional_comparison_df
    ])

    print("Final comparison")
    print_solved_graphs(final_comparison_df, larger_k_names)

    print("Final comparison, only first solution")
    final_comparison_df_first_only = final_comparison_df[~final_comparison_df['All Solutions']]
    print_solved_graphs(final_comparison_df_first_only, larger_k_names)


    example_graph = 'cost_matrix_component_nr_575_size_91_cutoff_10.0'
    example_fpt_df = filtered_gurobi_fpt_df[(filtered_gurobi_fpt_df.Graph == example_graph) & filtered_gurobi_fpt_df.MT & (filtered_gurobi_fpt_df.Permutation == 0)]
    assert(len(example_fpt_df.Algorithm.unique()) == 1)
    assert(example_fpt_df.Solved.any())
    assert(len(example_fpt_df.k.unique()) == len(example_fpt_df))

    example_gurobi_df = filtered_gurobi_df[(filtered_gurobi_df.Graph == example_graph) & filtered_gurobi_df.MT & (filtered_gurobi_df.Permutation == 0) & (filtered_gurobi_df.Algorithm == 'ILP-S-R-C4')]
    assert(len(example_gurobi_df) == 1)

    example_k = example_fpt_df[example_fpt_df.Solved].k.iloc[0]
    last_calls = example_fpt_df[example_fpt_df.Solved].Calls.iloc[0]
    second_last_calls = example_fpt_df[example_fpt_df.k == (example_k - 1)].Calls.iloc[0]
    print("Graph {}, k {}, {} needs {} calls total (last two k: {}, {}, max: {}), {} needs {} calls".format(example_graph, example_k, example_fpt_df.Algorithm.iloc[0], example_fpt_df.Calls.sum(), second_last_calls, last_calls, example_fpt_df.Calls.max(), example_gurobi_df.Algorithm.iloc[0], example_gurobi_df.Calls.sum()))
