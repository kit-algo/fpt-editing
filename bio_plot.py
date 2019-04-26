#!/usr/bin/python3

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import gridspec, cm, ticker
import seaborn as sns
import argparse
from collections import defaultdict

import matplotlib.scale as mscale
import matplotlib.transforms as mtransforms

class CubeRootScale(mscale.ScaleBase):
    #ScaleBase class for generating cubee root scale.
    name = 'cuberoot'

    def __init__(self, axis, **kwargs):
        mscale.ScaleBase.__init__(self)

    def set_default_locators_and_formatters(self, axis):
        axis.set_major_locator(ticker.AutoLocator())
        axis.set_major_formatter(ticker.ScalarFormatter())
        axis.set_minor_locator(ticker.NullLocator())
        axis.set_minor_formatter(ticker.NullFormatter())

    def limit_range_for_scale(self, vmin, vmax, minpos):
        return  max(0., vmin), vmax

    class CubeRootTransform(mtransforms.Transform):
        input_dims = 1
        output_dims = 1
        is_separable = True

        def transform_non_affine(self, a):
            x = np.array(a)
            return np.sign(x) * (np.abs(x)**(1.0/2.0))

        def inverted(self):
            return CubeRootScale.InvertedCubeRootTransform()

    class InvertedCubeRootTransform(mtransforms.Transform):
        input_dims = 1
        output_dims = 1
        is_separable = True

        def transform(self, a):
            x = np.array(a)
            return np.sign(x) * (np.abs(x)**2)

        def inverted(self):
            return CubeRootScale.CubeRootTransform()

    def get_transform(self):
        return self.CubeRootTransform()

mscale.register_scale(CubeRootScale)

sns.set(style="whitegrid")

algo_order = ["No Optimization", "No Undo", "No Redundancy", "Skip Conversion", "GreedyLB-First", "GreedyLB-Most", "GreedyLB-Most Pruned", "LocalSearchLB-First", "LocalSearchLB-Most", "LocalSearchLB-Most Pruned"]

color_palette = sns.color_palette('bright', len(algo_order))
algo_colors = [color_palette[i] for i, v in enumerate(algo_order)]

thread_order = [1, 2, 4, 8, 16]
thread_colors = [cm.plasma(i/len(thread_order)) for i in range(len(thread_order))]

def my_performanceplot(data, measure, logy=True):
    # (graph, k, algorithm) -> [t1, t2, t3, t4, t5]

    data_grouped = data.groupby(["Graph", "k", "Algorithm"])
    num_permutations = 16 #len(data_grouped.get_group((data.Graph[0], 0, "Base")))

    for name, required_measures, summarize_function in [('min', 1, lambda x : x.min()), ('median', num_permutations / 2 + 1, lambda x : x.quantile(0.5 * (num_permutations - 1) / (len(x) - 1))), ('max', num_permutations, lambda x : x.max())]:

        ratios = defaultdict(list)

        for graph, graph_data in data.groupby(["Graph"]):
            algorithm_groups = graph_data.groupby(["Algorithm"])
            max_k = algorithm_groups.max().k

            times = dict() # algorithm -> {"k" : k, "measure" : measure}

            best_algo = None
            best_k = -1
            best_measure = np.inf

            for algorithm, k in max_k.iteritems():
                k_data = data_grouped.get_group((graph, k, algorithm))

                while len(k_data) < required_measures:
                    k = k - 1
                    k_data = data_grouped.get_group((graph, k, algorithm))

                v = summarize_function(k_data[measure])

                times[algorithm] = {measure : v, "k" : k}

                if k > best_k or (k == best_k and v < best_measure):
                    best_k = k
                    best_algo = algorithm
                    best_measure = v

            assert(not best_algo is None)

            if best_k < 10:
                continue

            for algo, values in times.items():
                reference_measure = best_measure
                if values['k'] < best_k:
                    reference_measure = summarize_function(data_grouped.get_group((graph, values['k'], best_algo))[measure])
                ratios[algo].append(1.0 - reference_measure / values[measure])

                if ('Most' in algo or algo == 'Single')  and reference_measure * 2 < values[measure]:
                    print('Graph: {}, algo: {}, {}s, best algo: {}, {}s at best k: {}'.format(graph, algo, values[measure], best_algo, best_measure, best_k))

        sorted_ratios = {algo : sorted(v) for algo, v in ratios.items()}

        pd.DataFrame(sorted_ratios).plot()
        plt.show()

def my_runtime_plot(data, measure):
    # (graph, k, algorithm) -> [t1, t2, t3, t4, t5]

    data_grouped = data.groupby(["Graph", "k", "Algorithm"])
    num_permutations = 16 #len(data_grouped.get_group((data.Graph[0], 0, "Base")))

    plot_data = list() # (Graph: graph, algo: measure)

    algorithms = data.Algorithm.unique()

    above_limit = 10000

    for graph, graph_data in data.groupby(["Graph"]):
        max_k = graph_data.k.max()
        max_k_data = graph_data[graph_data.k == max_k]

        for algo in algorithms:
            measurements = max_k_data[max_k_data.Algorithm == algo][measure]
            for v in measurements:
                plot_data.append({'Graph': graph, 'Algorithm': algo, measure: v})
            for i in range(len(measurements), 16):
                plot_data.append({'Graph': graph, 'Algorithm': algo, measure: above_limit})

    pd.DataFrame(plot_data).groupby(['Graph', 'Algorithm']).median()[measure].unstack().sort_values(by='Most Pruned').plot(logy=True)
    plt.show()

def solved_instances_over_measure_plot(data, measure, ax):
    num_graphs = len(data.groupby(['Graph', 'Permutation']))

    if measure == "Calls":
        summed_calls = data.groupby(['Graph', 'Permutation', 'Algorithm']).agg({'Solved': np.any, 'Calls': np.sum})
        summed_calls.sort_values(by='Calls', inplace=True)
        summed_calls = summed_calls[summed_calls.Solved == True]
        sorted_data = summed_calls.reset_index()
    else:
        sorted_data = data[data.Solved == True].sort_values(by=measure)

    x_plot_data=defaultdict(list)
    y_plot_data=defaultdict(list)
    num_solved=defaultdict(int)

    for algo, val in zip(sorted_data.Algorithm, sorted_data[measure]):
        if "Single" in algo:
            continue
        x_plot_data[algo].append(val)
        y_plot_data[algo].append(num_solved[algo])
        num_solved[algo] += 1
        x_plot_data[algo].append(val)
        y_plot_data[algo].append(num_solved[algo])

    min_val = np.inf
    max_val = 0
    for algo, color in zip(algo_order, algo_colors):
        if not algo in x_plot_data:
            continue
        min_val = min(x_plot_data[algo][0], min_val)
        max_val = max(x_plot_data[algo][-1], max_val)
        ax.plot(x_plot_data[algo], y_plot_data[algo], label=algo, color=color)

    ax.axhline(num_graphs, color='k')

    ax.set_xlabel(measure)
    ax.set_xscale('log')
    ax.set_xlim(min_val, max_val)

def plot_speedup_per_instance_for_one_algorithm(data, ax):
    assert(len(data.Algorithm.unique()) == 1)

    mt_data = data[(data.MT == True) & (np.isnan(data.Speedup) == False)].copy()
    grouped_data = mt_data.groupby(['Graph', 'Algorithm', 'Permutation', 'Threads'])
    mt_data['Total Calls'] = grouped_data['Calls'].transform(np.sum)
    data_for_max_k = mt_data[grouped_data['k'].transform(np.max) == mt_data.k]

    data_for_max_k.sort_values(by='Total Calls')

    for thread, color in zip(thread_order, thread_colors):
        thread_data = data_for_max_k[data_for_max_k.Threads == thread]
        ax.scatter(thread_data['Total Calls'], thread_data.Speedup, label=thread, color=color, s=2)

    ax.set_xlabel('Calls')
    ax.set_xscale('log')
    ax.set_ylim(0, 16)

def plot_max_k_for_all_algorithms(data, qtm_data):
    data_for_max_k = data.groupby(['Graph']).max()

    qtm_indexed = qtm_data.set_index('Graph')

    data_for_max_k['QTM'] = qtm_indexed

    k1_data = data_for_max_k[data_for_max_k.k == 1]
    print("hiding {} graphs with for k == 1".format(len(k1_data)))
    k1_qtm_larger = k1_data[k1_data.QTM > 1]
    for graph, qtm_k in zip(k1_qtm_larger.index, k1_qtm_larger.QTM):
        print("Found a graph with exact k = 1 and QTM k = {}".format(qtm_k))

    data_solved = data_for_max_k[data_for_max_k.Solved & (data_for_max_k.k > 1)]

    qtm_exact_graphs = data_solved[data_solved.k == data_solved.QTM]
    print("Out of {} solved graphs, QTM has {} graphs correct".format(len(data_solved), len(qtm_exact_graphs)))

    data_unsolved = data_for_max_k[data_for_max_k.Solved == False]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4), sharey=True, gridspec_kw={'width_ratios':[3, 1]})

    for title, plot_data, ax, label in [('Solved', data_solved, ax1, 'Exact'), ('Unsolved', data_unsolved, ax2, 'Lower Bound')]:
        ax.set_yscale('log', basey=2)
        ax.set_title(title)
        sorted_plot_data = plot_data.sort_values(by=['k', 'QTM'])

        if title == 'Solved':
            qtm_precise = [cm.Set1(1) if (qtm_k == k) else cm.Set1(0) for k, qtm_k in zip(sorted_plot_data.k, sorted_plot_data.QTM)]
        else:
            qtm_precise = [cm.Set1(1) if (qtm_k == k + 1) else cm.Set1(0) for k, qtm_k in zip(sorted_plot_data.k, sorted_plot_data.QTM)]

        ax.scatter(range(len(sorted_plot_data.index)), sorted_plot_data.k, label=label, s=2, color=cm.Set1(2))
        ax.scatter(range(len(sorted_plot_data.index)), sorted_plot_data.QTM, label='QTM', s=2, color=qtm_precise)
        ax.set_xlabel('Graphs')

    ax1.legend()
    ax1.set_ylabel('k')

    return fig

def plot_solutions(data):
    data_for_solutions = data[data.Solved & (data.k > 0)].groupby(['Graph']).max()
    fig, ax = plt.subplots(1, figsize=(10, 4))

    print("Max k in solutions plot: {}".format(data_for_solutions.k.max()))

    ax.set_yscale('log')

    ax.scatter(data_for_solutions.k, data_for_solutions.Solutions, s=2)
    ax.set_xlabel('k')
    ax.set_ylabel('Number of Solutions')
    ax.set_yscale('log')
    ax.set_xscale('log')

    return fig

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create plots out of the result data.")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("qtm_csv", help="The QTM CSV input file")
    parser.add_argument("output_dir", help="The output directory where plots shall be written")
    parser.add_argument('--min-k', type=int, help="The minimum value of k to use, default: 10", default=10)
    parser.add_argument('--solved-only', action="store_true", help="If only solved graphs shall be considered")

    args = parser.parse_args()

    df = pd.read_csv(args.csv)

    max_ks = df.groupby('Graph').max().k
    larger_k_names = max_ks[max_ks >= args.min_k].index

    if args.solved_only:
        solved_graphs = df[df.Solved == True].groupby('Graph').first().index
        filtered_df = df[df.Graph.isin(larger_k_names) & df.Graph.isin(solved_graphs)]
    else:
        filtered_df = df[df.Graph.isin(larger_k_names)]

    #my_runtime_plot(filtered_df, 'Time [s]')
    #my_performanceplot(filtered_df, 'Time [s]', False)
    df_st_4 = filtered_df[(filtered_df.MT == False) & (filtered_df.l == 4)]
    fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True, figsize=(10, 4))
    solved_instances_over_measure_plot(df_st_4, 'Total Time [s]', ax1)
    solved_instances_over_measure_plot(df_st_4, 'Calls', ax2)
    ax1.set_ylabel('Solved (Graphs $\\times$ Node Id Permutations)')
    ax2.legend()
    fig.tight_layout()
    fig.savefig('{}/bio_times_calls_st_min_k_{}.pdf'.format(args.output_dir, args.min_k))

    fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True, figsize=(10, 4))
    plot_speedup_per_instance_for_one_algorithm(filtered_df[filtered_df.Algorithm == "LocalSearchLB-Most"], ax1)
    ax1.set_title('LocalSearchLB-Most')
    plot_speedup_per_instance_for_one_algorithm(filtered_df[filtered_df.Algorithm == "LocalSearchLB-Most Pruned"], ax2)
    ax2.set_title('LocalSearchLB-Most Pruned')
    ax1.legend(title='Threads')
    ax1.set_ylabel('Speedup')
    fig.tight_layout()
    fig.savefig('{}/bio_speedup_min_k_{}.pdf'.format(args.output_dir, args.min_k))

    qtm_df = pd.read_csv(args.qtm_csv)

    fig = plot_max_k_for_all_algorithms(df, qtm_df)
    fig.tight_layout()
    fig.savefig('{}/bio_max_k.pdf'.format(args.output_dir))

    fig = plot_solutions(df)
    fig.tight_layout()
    fig.savefig('{}/bio_solutions.pdf'.format(args.output_dir))
