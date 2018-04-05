#!/usr/bin/python3


import json
import pandas as pd
import re
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import seaborn

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

def load_json(jsonfile):
    with open(jsonfile, "r") as f:
        data = json.load(f)

    df_data = []

    for experiment in data:
        if not 'k' in experiment or not 'time' in experiment['results'] or not 'solved' in experiment['results']:
            continue


        exp_dict = dict()

        g = experiment['graph']

        gm = re.match("graphs/(.*).permutate.*\.n([0-9])*\..*$", g)
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
df_st_4 = df[(df.Threads == 1) & (df.l == 4)]


def my_boxplot(data, measure):
    fig, (ax1, ax2) = plt.subplots(ncols=2, figsize=(12, 6), sharey=True)
    sns.boxplot(x="k", y="Time [s]", hue=measure, data=data[data.Algorithm.isin(algo_order[:3])], ax=ax1, hue_order=algo_order[:3], palette=algo_colors[:3])
    sns.boxplot(x="k", y="Time [s]", hue=measure, data=data[data.Algorithm.isin(algo_order[3:])], ax=ax2, hue_order=algo_order[3:], palette=algo_colors[3:])
    for ax in [ax1, ax2]:
        ax.set_yscale('log')
        for patch in ax.artists:
            patch.set_edgecolor(patch.get_facecolor())
    return fig


for g in df_st_4.Graph.unique():
    g_df = df_st_4[df.Graph == g]

    fig = my_boxplot(g_df, "Algorithm")
    fig.savefig("{}.times.pdf".format(g))

    fig = my_boxplot(g_df, "Calls")
    fig.savefig("{}.calls.pdf".format(g))
