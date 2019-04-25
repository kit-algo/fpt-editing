#!/usr/bin/python3

import math
import json
import pandas as pd
import re
import matplotlib.pyplot as plt
from matplotlib import gridspec, cm, ticker
import seaborn as sns
import argparse
import numpy as np
import sys
import copy
import collections

sns.set(style="whitegrid")

algo_order = ["No Optimization", "No Undo", "No Redundancy", "Skip Conversion", "GreedyLB-First", "GreedyLB-Most", "GreedyLB-Most Pruned", "LocalSearchLB-First", "LocalSearchLB-Most", "LocalSearchLB-Most Pruned"]
unfilled_markers = [m for m, func in plt.Line2D.markers.items() if func != 'nothing' and m not in plt.Line2D.filled_markers]
unfilled_markers.remove(',')
unfilled_markers.remove('|')
unfilled_markers.remove(0)
unfilled_markers.remove(1)
unfilled_markers.remove(2)
unfilled_markers.remove(3)
algo_markers = unfilled_markers[:len(algo_order)]

color_palette = sns.color_palette('bright', len(algo_order))
algo_colors = [color_palette[i] for i, v in enumerate(algo_order)]
thread_order = [1, 2, 4, 7, 14, 28]
thread_colors = [cm.plasma(i/len(thread_order)) for i in range(len(thread_order))]

def my_single_graph_plot(data, measure, logy=True):
    fig, ax = plt.subplots(figsize=(10,4))

    algos = data.Algorithm.unique()

    for algo, color, marker in zip(algo_order, algo_colors, algo_markers):
        if not algo in algos:
            continue

        algo_data = data[data.Algorithm == algo].sort_values(by='k')

        k_perm_val = dict() # (k, perm) => val

        for p, v, k in zip(algo_data.Permutation, algo_data[measure], algo_data.k):
            if measure == 'Calls' and (k-1, p) in k_perm_val:
                v += k_perm_val[(k-1, p)]
            k_perm_val[(k, p)] = v

        s = pd.Series(k_perm_val, name=measure)
        s.index.names = ["k", "Permutation"]
        plot_data = s.reset_index()[['k', measure]]

        ax.scatter(plot_data.k, plot_data[measure], color=color, label=algo, marker=marker)
        #mean_std = plot_data.groupby('k').agg([np.mean, np.std])

        #ax.errorbar(x = mean_std.index, y = mean_std[(measure, 'mean')], yerr=mean_std[(measure, 'std')], color=color, label=algo)

    if logy:
        ax.set_yscale('log')
    ax.set_ylim(data[measure].min(), data[measure].max())

    ax.set_xlabel('k')
    ax.set_ylabel(measure)
    ax.legend()
    fig.tight_layout()

    return fig


def my_boxplot(data, measure, logy=True, showfliers=True):
    data_slow = data[data.Algorithm.isin(algo_order[:4])]
    data_fast = data[data.Algorithm.isin(algo_order[4:])]

    axes = []

    if data_slow.k.max() < data_fast.k.min():
        min_k_slow = data_slow.k.min()
        min_k_fast = data_fast.k.min()

        slow_size = data_slow.k.max() - min_k_slow
        fast_size = data_fast.k.max() - min_k_fast

        fig, (ax1, ax2) = plt.subplots(ncols=2, figsize=(12,6), sharey=True, gridspec_kw={'width_ratios': [slow_size, fast_size]})
        sns.boxplot(x="k", y=measure, hue="Algorithm", data=data_slow, ax=ax1, hue_order=algo_order[:4], palette=algo_colors[:4], showfliers=showfliers)
        ax1.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, pos : str(int(x + min_k_slow))))

        sns.boxplot(x="k", y=measure, hue="Algorithm", data=data_fast, ax=ax2, hue_order=algo_order[4:], palette=algo_colors[4:], showfliers=showfliers)
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

def graph_time_selector(data, desired_time):
    selector = False

    for g, t in desired_time.items():
        selector = ((data.Graph == g) & (data['Total Time [s]'] == t)) | selector

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

    fig, ax = plt.subplots(figsize=(5, 4))
    sns.boxplot(x="Graph", y=measure, hue="Threads", data=plot_data, ax=ax, hue_order=thread_order, palette=thread_colors, showfliers=showfliers)
    fig.tight_layout()
    if logy:
        ax.set_yscale('log')
    for patch in ax.artists:
        patch.set_edgecolor(patch.get_facecolor())

    return fig

def max_k_table(data, file=sys.stdout):
    data = data[data.Algorithm.isin(algo_order)]

    st_max_k = data[data.Threads == 1].groupby(["Graph"]).max().k
    st_k_selector = graph_k_selector(data, st_max_k)
    st_time = data[(data.Threads == 1) & st_k_selector].groupby(["Graph"]).min()['Total Time [s]']
    st_full_data = data[(data.Threads == 1) & st_k_selector & graph_time_selector(data, st_time)]
    st_algorithm = st_full_data.groupby(['Graph']).first().Algorithm

    mt_max_k = data[(data.Threads == 16)].groupby(["Graph"]).max().k
    mt_k_selector = graph_k_selector(data, mt_max_k)
    mt_data = data[(data.Threads == 16) & mt_k_selector].groupby(["Graph"]).min()
    mt_time = mt_data['Total Time [s]']
    mt_full_data = data[(data.Threads == 16) & mt_k_selector & graph_time_selector(data, mt_time)]
    mt_algorithm = mt_full_data.groupby(['Graph']).first().Algorithm

    print("Best permutation for 16 Threads per Graph:")
    print(mt_full_data[['Graph', 'Permutation', 'Algorithm']].to_string())

#    mt_extra_k = data[(data.Threads == 16) & (data['Time [s]'] > 1000)].groupby(["Graph"]).max().k
#    mt_extra_data = data[(data.Threads == 16) & (data['Time [s]'] > 1000) & graph_k_selector(data, mt_extra_k)].groupby(["Graph"]).min()
#    mt_extra_time = mt_extra_data['Time [s]']

    solved = mt_data['Solved']# | mt_extra_data['Solved']

    df = pd.DataFrame(collections.OrderedDict([
        (('Graph', 'Name'), mt_data.index),
        (('Graph', 'n'), mt_data.n),
        (('Graph', 'm'), mt_data.m),
        (('Solved', ''), solved),
        (('1 Thread', 'k'), st_max_k),
        (('1 Thread', 'Time [s]'), st_time),
        (('1 Thread', 'Algorithm'), st_algorithm),
        (('16 Threads', 'k'), mt_max_k),
        (('16 Threads', 'Time [s]'), mt_time),
        (('16 Threads', 'Algorithm'), mt_algorithm)#,
       # (('16 Threads*', 'k'), mt_extra_k),
       # (('16 Threads*', 'Time [s]'), mt_extra_time)
        ]))

    df.sort_values(by=('1 Thread', 'Time [s]'), inplace=True)
    print(df.to_latex(index=False, formatters={('Solved', '') : lambda x : 'Yes' if x else 'No', ('28 Threads*', 'k') : lambda x : str(int(x)) if not math.isnan(x) else '', ('28 Threads*', 'Time [s]') : lambda x : "{:.2f}".format(x) if not math.isnan(x) else ''}, float_format=lambda x : "{:.2f}".format(x), na_rep=" "), file=file)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create plots out of the result data.")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("output_dir", help="The output directory where plots shall be written")

    args = parser.parse_args()

    df = pd.read_csv(args.csv)

    df_st_4 = df[(df.Threads == 1) & (df.l == 4)]

    for g in df_st_4.Graph.unique():
        g_df = df_st_4[(df_st_4.Graph == g) & (df_st_4.Calls > 1)]

        fig = my_single_graph_plot(g_df, "Total Time [s]")
        fig.savefig("{}/{}-times.pdf".format(args.output_dir, g))
        plt.close(fig)

        fig = my_single_graph_plot(g_df, "Calls")
        fig.savefig("{}/{}-calls.pdf".format(args.output_dir, g))
        plt.close(fig)

#        fig = my_boxplot(g_df, "Scaling Factor Time", logy=False, showfliers=False)
#        fig.savefig("{}/{}-scaling_time.pdf".format(args.output_dir, g))
#        plt.close(fig)
#
#        fig = my_boxplot(g_df, "Scaling Factor Calls", logy=False, showfliers=False)
#        fig.savefig("{}/{}-scaling_calls.pdf".format(args.output_dir, g))
#        plt.close(fig)

#    # There are only 14 real permutations (0-13). Additional runs with 12h time limit
#    # are marked as higher permutations in the data.
#    mt_plot_data = df[(df.Algorithm == "Single") & (df.l == 4) & (df.Graph != "jazz") & (df.Permutation < 14)]
#
#    fig = threading_max_k_all_graphs(mt_plot_data, "Speedup", logy=False)
#    fig.savefig("{}/mt_speedup.pdf".format(args.output_dir))
#    plt.close(fig)
#
#    fig = threading_max_k_all_graphs(mt_plot_data, "Efficiency", logy=False)
#    fig.savefig("{}/mt_efficiency.pdf".format(args.output_dir))
#    plt.close(fig)
#
#    fig = threading_max_k_all_graphs(mt_plot_data, "Time [s]", logy=True)
#    fig.savefig("{}/mt_time.pdf".format(args.output_dir))
#    plt.close(fig)
#
#    fig = threading_max_k_all_graphs(mt_plot_data, "Calls", logy=True)
#    fig.savefig("{}/mt_calls.pdf".format(args.output_dir))
#    plt.close(fig)

    max_k_data = df[(df.l == 4) & (df.Graph != "jazz")]

    with open("{}/max_k.tex".format(args.output_dir), "w") as f:
        max_k_table(max_k_data, f)

    #print("Fallback percentage:")
    #df['Fallback %'] = df['Fallbacks'] / (df['Fallbacks'] + df['Single']) * 100
    #print(df[(df.Graph != 'jazz') & (df.Algorithm == 'Single') & (df.l == 4)].groupby(['Graph', 'k']).max()['Fallback %'].to_string(float_format=lambda x : "{:.2f}".format(x)))
    #print(df[(df.Graph != 'jazz') & (df.Algorithm == 'ARW-Single') & (df.l == 4)].groupby(['Graph', 'k']).max()['Fallback %'].to_string())
