#!/usr/bin/python3

import math
import json
import pandas as pd
import re
import matplotlib.pyplot as plt
from matplotlib import gridspec, cm, ticker
import seaborn as sns

algos = {'MT-Edit-Redundant-Skip-Center_Edits_Sparse_4-Single_Heur-Matrix' : 'Single',
         'ST-Edit-None-Normal-Center_4-First-No-Matrix' : 'Base',
         'ST-Edit-Redundant-Normal-Center_4-First-Basic-Matrix' : 'Lower Bound',
         'ST-Edit-Redundant-Normal-Center_4-First-No-Matrix' : 'No Redundancy',
         'ST-Edit-Redundant-Skip-Center_4-First-Basic-Matrix' : 'Skip Conversion',
         'ST-Edit-Redundant-Skip-Center_4-Least-Basic-Matrix' : 'Least Edited',
         #'ST-Edit-Redundant-Skip-Center_4-Single_Heur-Matrix' : 'Single',
         #'ST-Edit-Redundant-Skip-Center_5-Single_Heur-Matrix' : 'Single',
         'ST-Edit-Redundant-Skip-Center_Edits_Sparse_4-Single_Heur-Matrix' : 'Single',
         'ST-Edit-Redundant-Skip-Center_Edits_Sparse_5-Single_Heur-Matrix' : 'Single',
         'ST-Edit-Undo-Normal-Center_4-First-No-Matrix' : 'No Undo'}

algo_order = ["Base", "No Undo", "No Redundancy", "Lower Bound", "Skip Conversion", "Least Edited", "Single"]
algo_colors = [cm.Dark2(i) for i, v in enumerate(algo_order)]
thread_order = [1, 2, 4, 7, 14, 28]
thread_colors = [cm.plasma(i/len(thread_order)) for i in range(len(thread_order))]

def load_json(jsonfile):
    with open(jsonfile, "r") as f:
        data = json.load(f)

    df_data = []

    for experiment in data:
        if not 'k' in experiment or not 'time' in experiment['results'] or not 'solved' in experiment['results']:
            continue


        exp_dict = dict()

        g = experiment['graph']

        gm = re.match("graphs/(.*).permutate.*\.n([0-9]*)\..*$", g)
        exp_dict["Graph"] = gm[1]
        exp_dict["Permutation"] = int(gm[2])

        a = experiment['algo']
        if not a in algos:
            continue

        exp_dict["l"] = 4 if "4" in a else 5

        exp_dict["Algorithm"] = algos[a]

        exp_dict["Threads"] = experiment['threads']
        exp_dict["k"] = experiment['k']

        res = experiment['results']
        exp_dict["Solved"] = (res['solved'] == 'true')
        exp_dict["Time [s]"] = res['time']

        if "counters" in res:
            for stat, values in res['counters'].items():
                exp_dict[stat.capitalize()] = sum(values)

        df_data.append(exp_dict)

    return pd.DataFrame(df_data)

df = load_json("results.json")

def get_scaling(row, df, measure):
    if row is None or not 'k' in row:
        return math.nan

    previous = df[(df.Graph == row['Graph']) &
                  (df.Permutation == row['Permutation']) &
                  (df.Algorithm == row['Algorithm']) &
                  (df.Threads == row['Threads']) &
                  (df.l == row['l']) &
                  (df.k == (row['k'] - 1))]

    if len(previous) > 1:
        print("Found too many rows: {}".format(previous))
        return math.nan

    if len(previous) > 0:
        return row[measure] / previous[measure].values[0]

    return math.nan

def get_speedup(row, df):
    if row is None or not 'k' in row:
        return math.nan

    if row['Threads'] == 1:
        return 1

    st_row = df[(df.Graph == row['Graph']) &
                (df.Permutation == row['Permutation']) &
                (df.Algorithm == row['Algorithm']) &
                (df.Threads == 1) &
                (df.l == row['l']) &
                (df.k == row['k'])]

    if len(st_row) == 0:
        return math.nan

    if not len(st_row) == 1:
        raise RuntimeError("Found too many rows: {}".format(st_row))

    return st_row['Time [s]'].values[0] / row['Time [s]']

df['Scaling Factor Time'] = df.apply(get_scaling, axis=1, df=df, measure="Time [s]")
df['Scaling Factor Calls'] = df.apply(get_scaling, axis=1, df=df, measure="Calls")
df['Speedup'] = df.apply(get_speedup, axis=1, df=df)
df['Efficiency'] = df['Speedup']/df['Threads']

df_st_4 = df[(df.Threads == 1) & (df.l == 4)]

def my_boxplot(data, measure, logy=True, showfliers=True):
    data_slow = data[data.Algorithm.isin(algo_order[:3])]
    data_fast = data[data.Algorithm.isin(algo_order[3:])]

    axes = []

    if data_slow.k.max() < data_fast.k.min():
        min_k_slow = data_slow.k.min()
        min_k_fast = data_fast.k.min()

        slow_size = data_slow.k.max() - min_k_slow
        fast_size = data_fast.k.max() - min_k_fast

        fig, (ax1, ax2) = plt.subplots(ncols=2, figsize=(12,6), sharey=True, gridspec_kw={'width_ratios': [slow_size, fast_size]})
        sns.boxplot(x="k", y=measure, hue="Algorithm", data=data_slow, ax=ax1, hue_order=algo_order[:3], palette=algo_colors[:3], showfliers=showfliers)
        ax1.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, pos : str(int(x + min_k_slow))))

        sns.boxplot(x="k", y=measure, hue="Algorithm", data=data_fast, ax=ax2, hue_order=algo_order[3:], palette=algo_colors[3:], showfliers=showfliers)
        ax2.set_ylabel("")
        ax2.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, pos : str(int(x + min_k_fast))))

        axes.append(ax1)
        axes.append(ax2)
    else:
        fig, ax = plt.subplots(figsize=(12, 6))
        min_k = data.k.min()

        sns.boxplot(x="k", y=measure, hue="Algorithm", data=data, ax=ax, hue_order=algo_order, palette=algo_colors, showfliers=showfliers)
        ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, pos : str(int(x + min_k))))
        axes.append(ax)

    fig.tight_layout(w_pad=0.1)

    for ax in axes:
        if logy:
            ax.set_yscale('log')
        ax.xaxis.set_major_locator(ticker.MultipleLocator(5))
        for patch in ax.artists:
            patch.set_edgecolor(patch.get_facecolor())
    return fig

def graph_k_selector(data, desired_k):
    selector = False

    for g, k in desired_k.items():
        selector = ((data.Graph == g) & (data.k == k)) | selector

    return selector


def threading_boxplot(data, measure, logy=True, showfliers=True):
    fig, ax = plt.subplots(figsize=(12, 6))
    min_k = data.k.min()

    sns.boxplot(x="k", y=measure, hue="Threads", data=data, ax=ax, hue_order=thread_order, palette=thread_colors, showfliers=showfliers)

    ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, pos : str(int(x + min_k))))

    fig.tight_layout(w_pad=0.1)

    if logy:
        ax.set_yscale('log')
    ax.xaxis.set_major_locator(ticker.MultipleLocator(5))
    for patch in ax.artists:
        patch.set_edgecolor(patch.get_facecolor())

    return fig

def threading_max_k_all_graphs(data, measure, logy=False, showfliers=True):
    k_per_graph = data[data.Threads == 1].groupby(["Graph", "Threads", "Permutation"]).max().groupby("Graph").min().k

    plot_data = data[graph_k_selector(data, k_per_graph)]

    fig, ax = plt.subplots(figsize=(6, 3))
    sns.boxplot(x="Graph", y=measure, hue="Threads", data=plot_data, ax=ax, hue_order=thread_order, palette=thread_colors, showfliers=showfliers)
    fig.tight_layout()
    if logy:
        ax.set_yscale('log')
    for patch in ax.artists:
        patch.set_edgecolor(patch.get_facecolor())

    return fig

def max_k_table(data):
    st_max_k = data[data.Threads == 1].groupby(["Graph"]).max().k
    st_time = data[(data.Threads == 1) & graph_k_selector(data, st_max_k)].groupby(["Graph"]).min()['Time [s]']

    mt_max_k = data[data.Threads == 28].groupby(["Graph"]).max().k
    mt_data = data[(data.Threads == 28) & graph_k_selector(data, mt_max_k)].groupby(["Graph"]).min()
    mt_time = mt_data['Time [s]']
    solved = mt_data['Solved']

    df = pd.DataFrame({'k (st)': st_max_k, 'Time [s] (st)' : st_time, 'k (28 Threads)': mt_max_k, 'Time [s] (28 Threads)': mt_time, 'Solved': solved})
    df.sort_values(by='Time [s] (st)', inplace=True)
    print(df.to_latex())

for g in df_st_4.Graph.unique():
    g_df = df_st_4[df.Graph == g]

    fig = my_boxplot(g_df, "Time [s]")
    fig.savefig("{}.times.pdf".format(g))
    plt.close(fig)

    fig = my_boxplot(g_df, "Calls")
    fig.savefig("{}.calls.pdf".format(g))
    plt.close(fig)

    fig = my_boxplot(g_df, "Scaling Factor Time", logy=False, showfliers=False)
    fig.savefig("{}.scaling_time.pdf".format(g))
    plt.close(fig)

    fig = my_boxplot(g_df, "Scaling Factor Calls", logy=False, showfliers=False)
    fig.savefig("{}.scaling_calls.pdf".format(g))
    plt.close(fig)

mt_plot_data = df[(df.Algorithm == "Single") & (df.l == 4) & (df.Graph != "jazz")]

fig = threading_max_k_all_graphs(mt_plot_data, "Speedup", logy=False)
fig.savefig("mt_speedup.pdf")
plt.close(fig)

fig = threading_max_k_all_graphs(mt_plot_data, "Efficiency", logy=False)
fig.savefig("mt_efficiency.pdf")
plt.close(fig)

fig = threading_max_k_all_graphs(mt_plot_data, "Time [s]", logy=True)
fig.savefig("mt_time.pdf")
plt.close(fig)

fig = threading_max_k_all_graphs(mt_plot_data, "Calls", logy=True)
fig.savefig("mt_calls.pdf")
plt.close(fig)
