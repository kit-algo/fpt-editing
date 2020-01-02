import matplotlib.pyplot as plt
import numpy as np
from matplotlib import cm
import seaborn as sns
from collections import defaultdict

sns.set(style="whitegrid")

#algo_order = ["No Optimization", "No Undo", "No Redundancy", "Skip Conversion", "GreedyLB-First", "GreedyLB-Most", "GreedyLB-Most Pruned", "UpdatedLB-First", "UpdatedLB-Most", "UpdatedLB-Most Pruned", "LocalSearchLB-First", "LocalSearchLB-Most", "LocalSearchLB-Most Pruned"]
#algo_order = ["GreedyLB-First", "GreedyLB-Most", "GreedyLB-Most Pruned", "UpdatedLB-First", "UpdatedLB-Most", "UpdatedLB-Most Pruned", "LocalSearchLB-First", "LocalSearchLB-Most", "LocalSearchLB-Most Pruned"]
algo_order = [
    "GreedyLB-First-All", "GreedyLB-Most Pruned-All",
    "UpdatedLB-Most Pruned-All", "LocalSearchLB-First-All",
    "LocalSearchLB-Most-All", "LocalSearchLB-Most Pruned",
    "LocalSearchLB-Most Pruned-All", "LocalSearchLB-Most Pruned-MT",
    "LocalSearchLB-Most Pruned-All-MT",
    "GurobiLB-Most Pruned-All",
    "Gurobi", "Gurobi-Single",
    "Gurobi-Single-Relaxation", "Gurobi-Single-Relaxation-C4",
    "Gurobi-Single-Relaxation-C4-Heuristic-Init",
    "Gurobi-Single-Relaxation-C4-Heuristic-Init-MT"
]

algo_colors = [cm.nipy_spectral(i/len(algo_order)) for i, v in enumerate(algo_order)]

thread_order = [1, 2, 4, 8, 16]
thread_colors = [cm.plasma(i/len(thread_order)) for i in range(len(thread_order))]


def solved_instances_over_measure_plot(data, measure, ax, last_value=None):
    num_graphs = len(data.groupby(['Graph', 'Permutation']))

    if measure == "Calls":
        summed_calls = data.groupby(['Graph', 'Permutation', 'Algorithm', 'MT', 'All Solutions']).agg({'Solved': np.any, 'Calls': np.sum})
        summed_calls.sort_values(by='Calls', inplace=True)
        summed_calls = summed_calls[summed_calls.Solved == True]
        sorted_data = summed_calls.reset_index()
    else:
        sorted_data = data[data.Solved == True].sort_values(by=measure)

    x_plot_data = defaultdict(list)
    y_plot_data = defaultdict(list)
    num_solved = defaultdict(int)

    for algo, val, all_solutions, mt in zip(sorted_data.Algorithm, sorted_data[measure], sorted_data['All Solutions'], sorted_data.MT):
        if all_solutions:
            algo += "-All"
        if mt:
            algo += "-MT"
        x_plot_data[algo].append(val)
        y_plot_data[algo].append(num_solved[algo])
        num_solved[algo] += 1
        x_plot_data[algo].append(val)
        y_plot_data[algo].append(num_solved[algo])

    if last_value is not None:
        for algo in x_plot_data.keys():
            if x_plot_data[algo][-1] < last_value:
                x_plot_data[algo].append(last_value)
                y_plot_data[algo].append(num_solved[algo])
            elif x_plot_data[algo][-1] > last_value + 1:
                print("Warn: {} has a value larger than the time limit".format(algo))

    min_val = np.inf
    max_val = 0
    for algo, color in zip(algo_order, algo_colors):
        if algo not in x_plot_data:
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

    # TODO: add Gurobi data, plot both, mark where they differ the better (i.e., higher) algorithm with a symbol (i.e., symbol x for FPT is better, symbol + for Gurobi is better)
    # NOTE: Gurobi might not have data for all graphs as some terminated in external timeout or out of memory -> use a special (low) value like 1.
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
