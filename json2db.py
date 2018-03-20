#!/usr/bin/python3

# creates a database from the data of a json file produced by eval2json

# Usage: ./json2db.py input.json output.sqlite

import sqlite3
import json
import sys

def json2db(jsonfile, dbfile):
	data = json.load(open(jsonfile, "r"))

	# create tables
	db = sqlite3.connect(dbfile)
	db.executescript('CREATE TABLE algo(id integer primary key autoincrement, name text);'
		+ 'CREATE TABLE graph (id integer primary key autoincrement, name text);'
		+ 'CREATE TABLE experiment(id integer primary key autoincrement, exact bool, algo integer, graph integer, threads integer, k integer, time double, solved bool, foreign key (algo) references algo(id), foreign key (graph) references graph(id));'
		+ 'CREATE TABLE stat_names(id integer primary key autoincrement, name text);'
		+ 'CREATE TABLE stats(id integer primary key autoincrement, experiment integer, stat integer, k integer, value integer, foreign key (experiment) references experiment(id), foreign key (stat) references stat_names(id));'
	)

	# gather stat keys
	keylist = [experiment["results"]["counters"].keys() if "counters" in experiment["results"] else {} for experiment in data]
	keys = {key for l in keylist for key in l}
	valuetexts = ['("' + stat + '")' for stat in keys]
	db.execute('INSERT INTO stat_names (name) VALUES ' + ', '.join(valuetexts) + ';')

	# gather algos
	keys = {experiment["algo"] for experiment in data}
	valuetexts = ['("' + algo + '")' for algo in keys]
	db.execute('INSERT INTO algo (name) VALUES ' + ', '.join(valuetexts) + ';')

	# gather graphs
	keys = {experiment["graph"] for experiment in data}
	valuetexts = ['("' + graph + '")' for graph in keys]
	db.execute('INSERT INTO graph (name) VALUES ' + ', '.join(valuetexts) + ';')

	# insert data
	for experiment in data:
		if not 'k' in experiment or not 'time' in experiment['results'] or not 'solved' in experiment['results']: continue
		query = 'INSERT INTO experiment (exact, algo, graph, threads, k, time, solved) VALUES (' \
			+ ('1' if experiment['type'] == 'exact' else '0') + ', ' \
			+ '(SELECT id FROM algo WHERE name = "' + experiment['algo'] + '"), ' \
			+ '(SELECT id FROM graph WHERE name = "' + experiment['graph'] + '"), ' \
			+ str(experiment['threads']) + ', ' + str(experiment['k']) + ', ' + str(experiment['results']['time']) + ', ' + ('1' if experiment['results']['solved'] == 'true' else '0') + ');'
		db.execute(query)
		if "counters" in experiment["results"]:
			expid = db.execute('SELECT * FROM sqlite_sequence WHERE name = "experiment";').fetchone()[1]
			valuetexts = []
			for stat, values in experiment['results']['counters'].items():
				statid = db.execute('SELECT id FROM stat_names WHERE name = "' + stat + '";').fetchone()[0]
				valuetexts += ['(' + str(expid) + ', ' + str(statid) + ', ' + str(i) + ', ' + str(v) + ')' for i, v in enumerate(values)]
			db.execute('INSERT INTO stats (experiment, stat, k, value) VALUES ' + ', '.join(valuetexts) + ';')

	db.commit()
	db.close()

if __name__ == '__main__':
	json2db(sys.argv[1], sys.argv[2])
