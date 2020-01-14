import matplotlib.pyplot as plt
import numpy as np
from matplotlib import cm
import seaborn as sns
import pandas as pd
from collections import defaultdict, OrderedDict

sns.set(style="whitegrid")

#algo_order = ["No Optimization", "No Undo", "No Redundancy", "Skip Conversion", "GreedyLB-First", "GreedyLB-Most", "GreedyLB-Most Pruned", "UpdatedLB-First", "UpdatedLB-Most", "UpdatedLB-Most Pruned", "LocalSearchLB-First", "LocalSearchLB-Most", "LocalSearchLB-Most Pruned"]
#algo_order = ["GreedyLB-First", "GreedyLB-Most", "GreedyLB-Most Pruned", "UpdatedLB-First", "UpdatedLB-Most", "UpdatedLB-Most Pruned", "LocalSearchLB-First", "LocalSearchLB-Most", "LocalSearchLB-Most Pruned"]
algo_order = [
    "FPT-G-F-All", "FPT-G-MP-All",
    "FPT-U-MP-All", "FPT-LS-F-All",
    "FPT-LS-M-All", "FPT-LS-MP",
    "FPT-LS-MP-All", "FPT-LS-MP-MT",
    "FPT-LS-MP-All-MT",
    "FPT-LP-MP-All",
    "ILP", "ILP-S",
    "ILP-S-R", "ILP-S-R-C4",
    "ILP-S-R-C4-MT"
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
def plot_max_k_for_all_algorithms(fpt_df, ilp_df, ilp_heur_df, qtm_df):
    fpt_k = fpt_df.groupby('Graph').k.max()
    fpt_solved = fpt_df.groupby('Graph').Solved.any()

    m = fpt_df.groupby('Graph').m.max()

    ilp_k = ilp_df.groupby('Graph').k.max()
    ilp_solved = ilp_df.groupby('Graph').Solved.any()

    ilp_heur_k = ilp_heur_df.groupby('Graph').k.min()
    qtm_k = qtm_df.set_index('Graph')

    df = pd.DataFrame(OrderedDict([
        ('m', m),
        ('fpt_k', fpt_k),
        ('fpt_solved', fpt_solved),
        ('ilp_k', ilp_k),
        ('ilp_solved', ilp_solved),
        ('ilp_heur_k', ilp_heur_k),
        ('qtm_k', qtm_k.k)]))

    df['ilp_solved'].fillna(False, inplace=True)
    df['ilp_k'].fillna(0, inplace=True)
    df['ilp_k'] = df['ilp_k'].astype('int64')
    df['ilp_heur_k'].fillna(df.m, inplace=True)

    df['any_solved'] = df['fpt_solved'] | df['ilp_solved']
    df['fpt_bound'] = df['fpt_solved'] * df['fpt_k'] + ~df['fpt_solved'] * (df['fpt_k'] + 1)

    assert((df['fpt_bound'] >= df['fpt_k']).all())
    assert((df['fpt_bound'] <= df['fpt_k']+1).all())

    df['best_bound'] = df[['fpt_bound', 'ilp_k']].max(axis=1)

    df['qtm_exact'] = (df['best_bound'] == df['qtm_k'])
    df['ilp_heur_exact'] = (df['best_bound'] == df['ilp_heur_k'])

    print("Out of {} unsolved graphs, QTM has {} graphs correct".format(sum(~df['any_solved']), sum(df['qtm_exact'] & ~df['any_solved'])))
    print("Out of {} solved graphs, QTM has {} graphs correct".format(sum(df['any_solved']), sum(df['qtm_exact'] & df['any_solved'])))
    print("Out of {} unsolved graphs, the ILP heur has {} graphs correct".format(sum(~df['any_solved']), sum(df['ilp_heur_exact'] & ~df['any_solved'])))
    print("There are {} graphs that are only solved by FPT and {} graphs that are only solved by ILP".format(sum(df['fpt_solved'] & ~df['ilp_solved']), sum(df['ilp_solved'] & ~df['fpt_solved'])))
    print("Out of {} graphs solved only by the FPT, the ILP heur has {} graphs correct".format(sum(df['fpt_solved'] & ~df['ilp_solved']), sum(df['ilp_heur_exact'] & df['fpt_solved'] & ~df['ilp_solved'])))
    print("Out of {} graphs solved only by the FPT, QTM has {} graphs correct".format(sum(df['fpt_solved'] & ~df['ilp_solved']), sum(df['qtm_exact'] & df['fpt_solved'] & ~df['ilp_solved'])))

    qtm_ratio = (df['qtm_k'] / df['best_bound']).sort_values()
    print("Ratio QTM vs. best bound: max: {}, median: {}, last five: {}, max of all but two: {}".format(qtm_ratio.max(), qtm_ratio.median(), qtm_ratio[-5:], qtm_ratio[:-2].max()))

    df['qtm_ratio'] = df['qtm_k'] / df['best_bound']
    excluded_df = df[df.qtm_ratio >= 3]
    print('Excluding data point at k={}, qtm ratio = {}'.format(excluded_df.best_bound, excluded_df.qtm_ratio))

    df_solved = df[df.any_solved & (df.qtm_ratio < 3)]
    df_unsolved = df[~df.any_solved]

    #print(sorted(df_solved.qtm_ratio.unique()))

    result_figs = {}

    g = sns.JointGrid(x='best_bound', y='qtm_ratio', data=df_solved, height=6)
    g.plot_joint(plt.scatter, s=15, linewidth=1, marker="x")
    #g.ax_joint.collections[0].set_alpha(0.5)
    g.ax_marg_x.hist(df_solved.best_bound, bins=np.logspace(np.log10(df_solved.best_bound.min()), np.log10(df_solved.best_bound.max()), 50))
    #g.ax_marg_y.hist(df_solved.qtm_ratio, bins=np.logspace(np.log10(df_solved.qtm_ratio.min()), np.log10(df_solved.qtm_ratio.max()), 50), orientation='horizontal')
    g.ax_marg_y.hist(df_solved.qtm_ratio, bins=np.arange(df_solved.qtm_ratio.min(), df_solved.qtm_ratio.max(), 0.01), orientation='horizontal')
    #g.plot_marginals(sns.distplot, kde=False, bins=np.logspace(np.log10(0.1), np.log10(1.0), 50)
    g.ax_joint.set_xscale('log')
    g.ax_joint.set_xlim(df_solved.best_bound.min() * 0.9, df_solved.best_bound.max() * 1.1)
    #g.ax_joint.set_yscale('symlog', linthreshy=1.4, linscaley=10)
    #g.ax_joint.set_yscale('log', basey=2)
    #g.ax_joint.set_yticks([1, 1.1, 1.2, 1.3, 1.4, 2.0, 3.0])
    #g.ax_joint.set_yticklabels(['1', '1.1', '1.2', '1.3', '1.4', '2', '3'])
    g.set_axis_labels('Actual k', 'QTM k / Actual k')
    g.fig.subplots_adjust(top=0.95)
    g.fig.suptitle('Solved Graphs')
    result_figs['qtm_solved'] = g.fig

    g = sns.JointGrid(x='best_bound', y='qtm_ratio', data=df_unsolved, height=6)
    g.plot_joint(plt.scatter, s=15, linewidth=1, marker="x")
    #g.ax_joint.collections[0].set_alpha(0.5)
    g.ax_marg_x.hist(df_unsolved.best_bound, bins=np.logspace(np.log10(df_unsolved.best_bound.min()), np.log10(df_unsolved.best_bound.max()), 50))
    #g.ax_marg_y.hist(df_unsolved.qtm_ratio, bins=np.logspace(np.log10(df_unsolved.qtm_ratio.min()), np.log10(df_unsolved.qtm_ratio.max()), 50), orientation='horizontal')
    g.ax_marg_y.hist(df_unsolved.qtm_ratio, bins=np.arange(df_unsolved.qtm_ratio.min(), df_unsolved.qtm_ratio.max(), 0.02), orientation='horizontal')
    #g.plot_marginals(sns.distplot, kde=False, bins=np.logspace(np.log10(0.1), np.log10(1.0), 50)
    g.ax_joint.set_xscale('log')
    g.ax_joint.set_xlim(df_unsolved.best_bound.min() * 0.9, df_unsolved.best_bound.max() * 1.1)
    #g.ax_joint.set_yscale('symlog', linthreshy=1.6, linscaley=10)
    #g.ax_joint.set_yscale('log', basey=2)
    #g.ax_joint.set_yticks([1, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6,  2.0])
    #g.ax_joint.set_yticklabels(['1', '1.1', '1.2', '1.3', '1.4', '1.5', '1.6', '2'])
    g.set_axis_labels('Best Lower Bound', 'QTM k / Best Lower Bound')
    g.fig.subplots_adjust(top=0.95)
    g.fig.suptitle('Unsolved Graphs')
    result_figs['qtm_unsolved'] = g.fig

    return result_figs


    #fig, ax = plt.subplots(1, 1, figsize=(4, 4))
    ax.set_xscale('log')
    ax.set_yscale('symlog', linthreshy=1.4, linscaley=10)
    ax.set_yticks([1, 1.1, 1.2, 1.3, 1.4, 2.0, 3.0, 4.0])

    sns.kdeplot(df_solved.best_bound, df_solved.qtm_ratio, shade=True, ax=ax)
    ax.scatter(df_solved.best_bound, df_solved.qtm_ratio)
    plt.show()
    plt.close(fig)


    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4), sharey=True, gridspec_kw={'width_ratios':[3, 1]})

    colors = sns.color_palette('deep', 7)

    for title, plot_data, ax, label in [('Solved', df_solved, ax1, 'Exact'), ('Unsolved', df_unsolved, ax2, 'Lower Bound')]:
        ax.set_yscale('symlog', basey=10, linthreshy=10, linscaley=0.2)
        ax.set_title(title)

        plot_data.sort_values(by=['best_bound', 'qtm_k', 'ilp_heur_k'], inplace=True)
        plot_data['x'] = range(len(plot_data))

        ax.scatter(plot_data.x, plot_data.m, marker="s", facecolors='none', s=6, color=colors[6], label='m')

        ilp_better_df = plot_data[plot_data.fpt_bound < plot_data.best_bound]
        ax.scatter(ilp_better_df.x, ilp_better_df.ilp_k, color=colors[4], marker='x', label='ILP')
        ilp_plot_df = plot_data[plot_data.fpt_bound == plot_data.best_bound]
        ax.scatter(ilp_plot_df.x, ilp_plot_df.ilp_k, s=4, color=colors[4])

        fpt_better_df = plot_data[plot_data.ilp_k < plot_data.best_bound]
        ax.scatter(fpt_better_df.x, fpt_better_df.fpt_bound, color=colors[5], marker='+', s=50, Label='FPT')
        fpt_plot_df = plot_data[plot_data.ilp_k == plot_data.best_bound]
        ax.scatter(fpt_plot_df.x, fpt_plot_df.fpt_bound, s=4, color=colors[5])

        ilp_heur_exact_df = plot_data[plot_data.ilp_heur_exact]
        ax.scatter(ilp_heur_exact_df.x, ilp_heur_exact_df.ilp_heur_k, s=3, color=colors[3], label='ILP Heuristic (exact)')
        ilp_heur_inexact_df = plot_data[~plot_data.ilp_heur_exact]
        ax.scatter(ilp_heur_inexact_df.x, ilp_heur_inexact_df.ilp_heur_k, s=3, color=colors[2], label='ILP Heuristic (upper bound)')


        qtm_exact_df = plot_data[plot_data.qtm_exact]
        ax.scatter(qtm_exact_df.x, qtm_exact_df.qtm_k, s=3, color=colors[0], label='QTM (exact)')
        qtm_inexact_df = plot_data[~plot_data.qtm_exact]
        ax.scatter(qtm_inexact_df.x, qtm_inexact_df.qtm_k, s=3, color=colors[1], label='QTM (upper bound)')

        #ax.scatter(range(len(sorted_plot_data.index)), sorted_plot_data.k, label=label, s=2, color=cm.Set1(2))
        #ax.scatter(range(len(sorted_plot_data.index)), sorted_plot_data.QTM, label='QTM', s=2, color=qtm_precise)
        ax.set_xlabel('Graphs')

        ax.set_xlim(left=-5, right=len(plot_data) + 5)

    ax1.set_ylim(bottom=-2, top=df.m.max() * 1.1)
    ax1.legend()
    ax1.set_ylabel('k')

    #plt.show()

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
