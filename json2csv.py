#!/usr/bin/python3

import math
import json
import pandas as pd
import re
import argparse
import os

algos = {'MT-Edit-Redundant-Skip-Center_Edits_Sparse_4-Single_Heur-Matrix' : 'Single Sparse',
         'ST-Edit-Redundant-Normal-Center_4-First-Basic-Matrix' : 'Lower Bound (LB No-Update)',
         'ST-Edit-Redundant-Normal-Center_4-First-No-Matrix' : 'No Redundancy',
         'ST-Edit-Redundant-Skip-Center_4-First-Basic-Matrix' : 'Skip Conversion (LB No-Update)',
         'ST-Edit-Redundant-Skip-Center_4-Least-Basic-Matrix' : 'Least Editable (LB No-Update)',
         #'ST-Edit-Redundant-Skip-Center_4-Single_Heur-Matrix' : 'Single',
         #'ST-Edit-Redundant-Skip-Center_5-Single_Heur-Matrix' : 'Single',
         'ST-Edit-Redundant-Skip-Center_Edits_Sparse_4-Single_Heur-Matrix' : 'Single Sparse',
         'ST-Edit-Redundant-Skip-Center_Edits_Sparse_5-Single_Heur-Matrix' : 'Single Sparse',
         'MT-Edit-Redundant-Skip-Center_4-Single_Heur-Matrix' : 'Single',
         'ST-Edit-None-Normal-Center_4-First-No-Matrix' : 'Base',
         'ST-Edit-Redundant-Normal-Center_4-First-Updated-Matrix' : 'Lower Bound',
         'ST-Edit-Redundant-Normal-Center_4-First-Min_Deg-Matrix' : 'Lower Bound',
         'ST-Edit-Redundant-Skip-Center_4-First-Updated-Matrix' : 'Skip Conversion',
         'ST-Edit-Redundant-Skip-Center_4-Least-Updated-Matrix' : 'Least Editable',
         'ST-Edit-Redundant-Skip-Center_4-Least-Min_Deg-Matrix' : 'Least Editable',
         'ST-Edit-Redundant-Skip-Center_4-Most-Min_Deg-Matrix' : 'Most Forbidden',
         'ST-Edit-Redundant-Skip-Center_4-Most_Pruned-Min_Deg-Matrix' : 'Early Pruning',
         'ST-Edit-Redundant-Skip-Center_4-Single_Most-Min_Deg-Matrix' : 'Single (Most Forbidden)',
         #'ST-Edit-Redundant-Skip-Center_4-Single_Heur-Matrix' : 'Single',
         #'ST-Edit-Redundant-Skip-Center_5-Single_Heur-Matrix' : 'Single',
         'ST-Edit-Redundant-Skip-Center_4-Single_Heur-Matrix' : 'Single',
         'ST-Edit-Redundant-Skip-Center_5-Single_Heur-Matrix' : 'Single',
         'ST-Edit-Undo-Normal-Center_4-First-No-Matrix' : 'No Undo'}

def load_json(jsonfile):
    with open(jsonfile, "r") as f:
        data = json.load(f)

    json_dir = os.path.abspath(os.path.dirname(jsonfile))

    df_data = []

    for experiment in data:
        if not 'k' in experiment or not 'time' in experiment['results'] or not 'solved' in experiment['results']:
            continue

        a = experiment['algo']
        if not a in algos:
            continue

        exp_dict = dict()
        exp_dict["Algorithm"] = algos[a]
        exp_dict["l"] = 4 if "4" in a else 5
        exp_dict['MT'] = ('MT' in a)

        g = experiment['graph']

        exp_dict["Graph"], _ = os.path.splitext(os.path.basename(g))
        exp_dict["Permutation"] = experiment['permutation']

        exp_dict["n"] = experiment['n']
        exp_dict["m"] = experiment['m']

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

def get_scaling(row, df, measure):
    if row is None or not 'k' in row:
        return math.nan

    key = (row['Graph'], row['Permutation'], row['Algorithm'], row['MT'], row['Threads'], row['l'], row['k'] - 1)

    if key in df.groups:
        previous = df.get_group(key)

        if len(previous) > 1:
            print("Found too many rows: {}".format(previous))
            return math.nan

        return row[measure] / previous[measure].values[0]

    return math.nan

def get_speedup(row, df):
    if row is None or not 'k' in row:
        return math.nan

    st_row = df.get_group((row['Graph'], row['Permutation'], row['Algorithm'], False, 1, row['l'], row['k']))

    if len(st_row) == 0:
        return math.nan

    if not len(st_row) == 1:
        raise RuntimeError("Found too many rows: {}".format(st_row))

    return st_row['Time [s]'].values[0] / row['Time [s]']

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert results json file to csv.")
    parser.add_argument("json", help="The JSON input file.")
    parser.add_argument("csv", help="The CSV output file.")

    args = parser.parse_args()

    df = load_json(args.json)

    grouped_df = df.groupby(['Graph', 'Permutation', 'Algorithm', 'MT', 'Threads', 'l', 'k'])

    df['Scaling Factor Time'] = df.apply(get_scaling, axis=1, df=grouped_df, measure="Time [s]")
    df['Scaling Factor Calls'] = df.apply(get_scaling, axis=1, df=grouped_df, measure="Calls")
    df['Speedup'] = df.apply(get_speedup, axis=1, df=grouped_df)
    df['Efficiency'] = df['Speedup']/df['Threads']

    df.to_csv(args.csv)
