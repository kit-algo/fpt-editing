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

algo_order = ["Base", "No Undo", "No Redundancy", "Lower Bound", "Skip Conversion", "Least Editable", "Single"]
algo_colors = [cm.Dark2(i) for i, v in enumerate(algo_order)]
thread_order = [1, 2, 4, 7, 14, 28]
thread_colors = [cm.plasma(i/len(thread_order)) for i in range(len(thread_order))]

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

def graph_time_selector(data, desired_time):
    selector = False

    for g, t in desired_time.items():
        selector = ((data.Graph == g) & (data['Time [s]'] == t)) | selector

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

    fig, ax = plt.subplots(figsize=(4, 3))
    sns.boxplot(x="Graph", y=measure, hue="Threads", data=plot_data, ax=ax, hue_order=thread_order, palette=thread_colors, showfliers=showfliers)
    fig.tight_layout()
    if logy:
        ax.set_yscale('log')
    for patch in ax.artists:
        patch.set_edgecolor(patch.get_facecolor())

    return fig

def max_k_table(data, file=sys.stdout):
    st_max_k = data[data.Threads == 1].groupby(["Graph"]).max().k
    st_time = data[(data.Threads == 1) & graph_k_selector(data, st_max_k)].groupby(["Graph"]).min()['Time [s]']

    mt_max_k = data[data.Threads == 28].groupby(["Graph"]).max().k
    mt_k_selector = graph_k_selector(data, mt_max_k)
    mt_data = data[(data.Threads == 28) & mt_k_selector].groupby(["Graph"]).min()
    mt_time = mt_data['Time [s]']

    mt_full_data = data[(data.Threads == 28) & mt_k_selector & graph_time_selector(data, mt_time)]    
    print("Best permutation for 28 Threads per Graph:")
    print(mt_full_data[['Graph', 'Permutation']].to_string())

    solved = mt_data['Solved']

    df = pd.DataFrame(collections.OrderedDict([
        (('Graph', 'Name'), mt_data.index),
        (('Graph', 'n'), mt_data.n),
        (('Graph', 'm'), mt_data.m),
        (('Solved', ''), solved),
        (('1 Thread', 'k'), st_max_k),
        (('1 Thread', 'Time [s]'), st_time),
        (('28 Threads', 'k'), mt_max_k),
        (('28 Threads', 'Time [s]'), mt_time)
        ]))

    df.sort_values(by=('1 Thread', 'Time [s]'), inplace=True)
    print(df.to_latex(index=False, formatters={('Solved', '') : lambda x : 'Yes' if x else 'No'}, float_format=lambda x : "{:.2f}".format(x), na_rep=" "), file=file)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create plots out of the result data.")
    parser.add_argument("csv", help="The CSV input file")
    parser.add_argument("output_dir", help="The output directory where plots shall be written")

    args = parser.parse_args()

    df = pd.read_csv(args.csv)

    df_st_4 = df[(df.Threads == 1) & (df.l == 4)]

    for g in df_st_4.Graph.unique():
        g_df = df_st_4[df_st_4.Graph == g]

        fig = my_boxplot(g_df, "Time [s]")
        fig.savefig("{}/{}-times.pdf".format(args.output_dir, g))
        plt.close(fig)

        fig = my_boxplot(g_df, "Calls")
        fig.savefig("{}/{}-calls.pdf".format(args.output_dir, g))
        plt.close(fig)

        fig = my_boxplot(g_df, "Scaling Factor Time", logy=False, showfliers=False)
        fig.savefig("{}/{}-scaling_time.pdf".format(args.output_dir, g))
        plt.close(fig)

        fig = my_boxplot(g_df, "Scaling Factor Calls", logy=False, showfliers=False)
        fig.savefig("{}/{}-scaling_calls.pdf".format(args.output_dir, g))
        plt.close(fig)

    mt_plot_data = df[(df.Algorithm == "Single") & (df.l == 4) & (df.Graph != "jazz") & (df.Threads < 128)]

    fig = threading_max_k_all_graphs(mt_plot_data, "Speedup", logy=False)
    fig.savefig("{}/mt_speedup.pdf".format(args.output_dir))
    plt.close(fig)

    fig = threading_max_k_all_graphs(mt_plot_data, "Efficiency", logy=False)
    fig.savefig("{}/mt_efficiency.pdf".format(args.output_dir))
    plt.close(fig)

    fig = threading_max_k_all_graphs(mt_plot_data, "Time [s]", logy=True)
    fig.savefig("{}/mt_time.pdf".format(args.output_dir))
    plt.close(fig)

    fig = threading_max_k_all_graphs(mt_plot_data, "Calls", logy=True)
    fig.savefig("{}/mt_calls.pdf".format(args.output_dir))
    plt.close(fig)

    max_k_data = df[(df.Algorithm == "Single") & (df.l == 4) & (df.Graph != "jazz")]

    with open("{}/max_k.tex".format(args.output_dir), "w") as f:
        max_k_table(max_k_data, f)

    print("Fallback percentage:")
    df['Fallback %'] = df['Fallbacks'] / (df['Fallbacks'] + df['Single']) * 100
    #print(df[(df.Graph != 'jazz') & (df.Algorithm == 'Single') & (df.l == 4)].groupby(['Graph', 'k']).max()['Fallback %'].to_string(float_format=lambda x : "{:.2f}".format(x)))
    print(df[(df.Graph != 'jazz') & (df.Algorithm == 'Single') & (df.l == 4)].groupby(['Graph', 'k']).max()['Fallback %'].to_string())
