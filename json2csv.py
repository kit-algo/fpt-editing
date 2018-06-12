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
         'ST-Edit-Redundant-Skip-Center_4-First-Updated-Matrix' : 'Skip Conversion',
         'ST-Edit-Redundant-Skip-Center_4-Least-Updated-Matrix' : 'Least Editable',
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


        exp_dict = dict()

        g = experiment['graph']

        gm = re.match("(graphs|data)/(.*).permutate.*\.n([0-9]*)\..*$", g)
        exp_dict["Graph"] = gm[2]
        exp_dict["Permutation"] = int(gm[3])

        a = experiment['algo']
        if not a in algos:
            continue

        g_path = g.replace(gm[1], "{}/data".format(json_dir))
        with open(g_path, 'r') as f:
            n, m, _ = f.readline().strip().split(maxsplit=3)

        exp_dict["n"] = int(n)
        exp_dict["m"] = int(m)

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

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert results json file to csv.")
    parser.add_argument("json", help="The JSON input file.")
    parser.add_argument("csv", help="The CSV output file.")

    args = parser.parse_args()

    df = load_json(args.json)

    df['Scaling Factor Time'] = df.apply(get_scaling, axis=1, df=df, measure="Time [s]")
    df['Scaling Factor Calls'] = df.apply(get_scaling, axis=1, df=df, measure="Calls")
    df['Speedup'] = df.apply(get_speedup, axis=1, df=df)
    df['Efficiency'] = df['Speedup']/df['Threads']

    df.to_csv(args.csv)
