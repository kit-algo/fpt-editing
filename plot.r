library(jsonlite)
library(RSQLite)
library(ggplot2)
library(tikzDevice)
library(scales)

load.json = function(file)
{
	# load experiment results from JSON file created by ./eval2json
	# note: jsonlite and/or R leaks memory here; on larger data sets using load.db() is strongly recommended

	# get experiments
	data = fromJSON(file)

	# adjustments
	data$results$solved = as.logical(data$results$solved)
	data$graph = as.factor(data$graph)
	data$algo = as.factor(data$algo)

	# calculate stat totals
	data$results$totals = data.frame(matrix(unlist(lapply(data$results$counters, function(v) {return(lapply(v, sum));})), nrow = nrow(data)))
	colnames(data$results$totals) = colnames(data$results$counters)

	# assemble synthetic graphs into groups
	data$group = as.factor(gsub("\\.n[0-9]+\\.", ".", data$graph))

	data
}

load.db = function(file)
{
	# load experiment results from sqlite file created by ./json2db.py
	# note: does not load individual (i.e. per k) stat counters, only their total

	db = dbConnect(RSQLite::SQLite(), file)
	
	# get experiments
	data = data.frame(dbGetQuery(db, 'SELECT * FROM experiment ORDER BY id ASC;'))

	# recreate structure of JSON output
	data$results = data.frame(time = data$time, solved = as.logical(data$solved))
	data$results$solved = as.logical(data$solved)
	data$time = NULL
	data$solved = NULL

	# resolve foreign keys
	# - algorithms
	data$algo = as.factor(data$algo)
	levels(data$algo) = dbGetQuery(db, 'SELECT name FROM algo ORDER BY id ASC;')$name
	# - graphs
	data$graph = as.factor(data$graph)
	levels(data$graph) = dbGetQuery(db, 'SELECT name FROM graph ORDER BY id ASC;')$name

	# assemble synthetic graphs into groups
	data$group = as.factor(gsub("\\.n[0-9]+\\.", ".", data$graph))

	# calculate stat totals
	totals = dbGetQuery(db, 'SELECT experiment, stat, SUM(value) AS value FROM stats GROUP BY experiment, stat ORDER BY experiment ASC;')
	totals$stat = as.factor(totals$stat)
	levels(totals$stat) = dbGetQuery(db, 'SELECT name FROM stat_names ORDER BY id ASC;')$name
	totals2 = reshape(totals, idvar = "experiment", timevar = "stat", direction = "wide")
	totals2$experiment = NULL
	colnames(totals2) = levels(totals$stat)
	data$results$totals = totals2

	dbDisconnect(db)
	
	data
}

######################################################
# plot functions
#
# general behaviour common to all plot functions:
# - if the data contains more than one set of experiments, automatically produces a legend mentioning the variant inputs
#     usually these are group, algorithm and threads
#   otherwise no legend is created

plot.times = function(data)
{
	# plots the running time of the experiments

	data = droplevels(data)

	# construct label, omit non-varying factors
	l = vector()
	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(is.null(data$label)) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
	if(min(data$threads) != max(data$threads)) {l = c(l, "Threads"); if(is.null(data$label)) {data$label = data$threads} else {data$label = paste(data$label, data$threads, sep = " / ")}}
	l = paste(l, collapse = " / ")
	if(is.null(data$label)) {data$label = ""}
	data$label = as.factor(data$label)

	# no color needed if only plotting a single group
	if(l == "") {p = ggplot(data, aes(x = k, group = interaction(group, algo, threads)))}
	else {p = ggplot(data, aes(x = k, group = interaction(group, algo, threads), color = label))}

	p = p + scale_y_log10(labels = comma)
	p = p + labs(x = "k", y = "Time [s]", color = l)

#	p = p + geom_boxplot(aes(y = results$time, group = interaction(group, algo, threads, k)), alpha = 0, outlier.alpha = 1)
	p = p + geom_errorbar(aes(y = results$time, group = interaction(group, algo, threads, k)), stat = "boxplot", position = "dodge")
#	p = p + stat_summary(aes(y = results$time, group = interaction(group, algo, threads)), fun.y = mean, geom = "line", position = position_dodge(0.75))
	p = p + stat_summary(aes(y = results$time, group = interaction(group, algo, threads)), fun.y = mean, geom = "point", size = 0.75, position = position_dodge(0.75))

	p
}

plot.calls = function(data)
{
	# plots the number of recursive calls of the experiments

	data = droplevels(data)

	# construct label, omit non-varying factors
	l = vector()
	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(is.null(data$label)) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
	# threads should not make a difference in the number of recursive calls
	#if(min(data$threads) != max(data$threads)) {l = c(l, "Threads"); if(is.null(data$label)) {data$label = data$threads} else {data$label = paste(data$label, data$threads, sep = " / ")}}
	l = paste(l, collapse = " / ")
	if(is.null(data$label)) {data$label = ""}
	data$label = as.factor(data$label)

	# no color needed if only plotting a single group
	if(l == "") {p = ggplot(data, aes(x = k, group = interaction(group, algo)))}
	else {p = ggplot(data, aes(x = k, group = interaction(group, algo), color = label))}

	p = p + scale_y_log10(labels = comma)
	p = p + labs(x = "k", y = "Recursive calls", color = l)

#	p = p + geom_boxplot(aes(y = results$totals$calls, group = interaction(group, algo, k)), alpha = 0, outlier.alpha = 1)
#	p = p + geom_errorbar(aes(y = results$totals$calls, group = interaction(group, algo, k)), stat = "boxplot", position = "dodge")
	p = p + stat_summary(aes(y = results$totals$calls, group = interaction(group, algo)), fun.y = mean, geom = "line")
	p = p + stat_summary(aes(y = results$totals$calls, group = interaction(group, algo)), fun.y = mean, geom = "point", size = 0.75)

	p
}

plot.mt.efficiency = function(data)
{
	# plots the efficiency of the multithreaded approach

	# filter for highest k in each group present in all thread counts
	data = data[as.logical(ave(data$k, data$group, data$algo, data$threads, FUN = function(x) x == max(x))),]
	data = data[as.logical(ave(data$k, data$group, data$algo, FUN = function(x) x == min(x))),]

	data = droplevels(data)

	# construct label, omit non-varying factors
	l = vector()
	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(length(as.logical(data$label)) == 0) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
	# threads are the x axis in this plot
	#if(min(data$threads) != max(data$threads)) {l = c(l, "Threads"); if(length(as.logical(data$label)) == 0) {data$label = data$threads} else {data$label = paste(data$label, data$threads, sep = " / ")}}
	l = paste(l, collapse = " / ")
	if(is.null(data$label)) {data$label = ""}
	data$label = as.factor(data$label)

	# in each group, set results$reftime to mean(results$time) with threads == 1
	refgroups = subset(data, threads == 1)
	reftimes = aggregate(refgroups$results$time, by = list(group = refgroups$group, algo = refgroups$algo), FUN = mean)
	data = merge(data, reftimes)
	data$results$reftime = data$x
	data$x = NULL

	# no color needed if only plotting a single group
	if(l == "") {p = ggplot(data, aes(x = threads, group = interaction(group, algo)))}
	else {p = ggplot(data, aes(x = threads, group = interaction(group, algo), color = label))}

	p = p + labs(x = "Threads", y = "Efficiency", color = l)

#	p = p + geom_boxplot(aes(y = results$reftime / (results$time * threads), group = interaction(group, algo, threads, k)), alpha = 0, outlier.alpha = 1)
	p = p + geom_errorbar(aes(y = results$reftime / (results$time * threads), group = interaction(group, algo, threads, k)), stat = "boxplot", position = "dodge")
#	p = p + stat_summary(aes(y = results$reftime / (results$time * threads), group = interaction(group, algo)), fun.y = mean, geom = "line", position = position_dodge(0.75))
	p = p + stat_summary(aes(y = results$reftime / (results$time * threads), group = interaction(group, algo)), fun.y = mean, geom = "point", size = 0.75, position = position_dodge(0.75))

	p
}

######################################################
# plot generators
#
# produces multiple plots in the directory `dir'

plot.all = function(data, dir)
{
	# create individual plots for every experiment with each of the above plot functions
	# dir is the directory to which to write the plots

	# see plot.eval at the end of this file for examples on how to change ggsave's output format and size
	
	data = droplevels(data)

	t = theme_get()
	# reduce fontsize of title 
	theme_update(plot.title = element_text(size = rel(.6)))

	# mangle some names
	# adjust as needed
	#   grass_web.metis -> grassweb
	levels(data$group) = gsub("grass_web\\.metis", "grassweb", levels(data$group))
	#   graphs/foo.graph -> foo
	levels(data$group) = gsub("^graphs/", "", levels(data$group))
	levels(data$group) = gsub("\\.graph$", "", levels(data$group))

	# make underscores latex-safe
	# commented due to pdf output
#	levels(data$group) = gsub("_", "\\\\_", levels(data$group))
#	levels(data$algo) = gsub("_", "\\\\_", levels(data$algo))

	# for each group
	for(l in levels(data$group))
	{
		sub_group = subset(data, group == l)
		file_group = gsub("/", "_", l)
		# for each algorithm
		for(a in levels(sub_group$algo))
		{
			sub_algo = subset(sub_group, algo == a)
			file_algo = gsub("/", "_", a)

			# no data for this combination, skip
			if(nrow(sub_algo) == 0) {next}

			# did any experiment solve the graph?
			s = any(sub_algo$results$solved)

			# progess indicator
			print(paste(l, a, s))

			ggsave(paste(file_group, file_algo, s, "times.pdf", sep = "."), plot = plot.times(sub_algo) + ggtitle(paste("Graph:", l, "Algorithm:", a, "Solved:", s)), device = pdf, path = dir)
			ggsave(paste(file_group, file_algo, s, "calls.pdf", sep = "."), plot = plot.calls(sub_algo) + ggtitle(paste("Graph:", l, "Algorithm:", a, "Solved:", s)), device = pdf, path = dir)
			ggsave(paste(file_group, file_algo, s, "mt.efficiency.pdf", sep = "."), plot = plot.mt.efficiency(sub_algo) + ggtitle(paste("Graph:", l, "Algorithm:", a, "Solved:", s)), device = pdf, path = dir)
			# plot.times with repositioned legend (see figure 3.3.a)
			ggsave(paste(file_group, file_algo, s, "mt.times.pdf", sep = "."), plot = plot.times(sub_algo) + theme(legend.position = c(0.11, 0.65), legend.background = element_rect(fill = "grey92", size = 1, linetype = "solid")) + ggtitle(paste("Graph:", l, "Algorithm:", a, "Solved:", s)), device = pdf, path = dir)
		}
	}

	# cleanup -- restore theme
	theme_set(t)
}

######################################################
# other functions
#
# - for summary table in a paper the result of summary.table(filter.best(data)) might be useful

filter.best = function(data)
{
	# filter the best results of each group, i.e. highest k with shortest time

	data = data[as.logical(ave(data$k, data$group, FUN = function(x) x == max(x))),]
	data = data[as.logical(ave(data$results$time, data$group, FUN = function(x) x == min(x))),]

	data
}

summary.table = function(data)
{
	# reduce the data to only include the columns needed for a summary table

	data.frame(group = data$group, k = data$k, solved = data$results$solved, time = data$results$time, threads = data$threads)
}

######################################################
# onwards to uncharted err... untested territories

plot.syn = function(data)
{
	# plots for synthetic graphs

	data = droplevels(data)

	# get graph size and number of edits targeted from file name
	data$n = as.factor(as.integer(gsub("^.*components\\.([0-9]+)\\..*$", "\\1", data$group)))
	data$targetk = as.factor(1.25 * as.integer(gsub("^.*\\.([0-9]+)\\.graph$", "\\1", data$group)))

	# construct label, omit non-varying factors
	l = vector()
	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(is.null(data$label)) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
	if(min(data$threads) != max(data$threads)) {l = c(l, "Threads"); if(is.null(data$label)) {data$label = data$threads} else {data$label = paste(data$label, data$threads, sep = " / ")}}
	l = paste(l, collapse = " / ")
	if(is.null(data$label)) {data$label = ""}
	data$label = as.factor(data$label)

	# no color needed if only plotting a single group
	if(l == "") {p = ggplot(data, aes(x = k, group = interaction(group, algo, threads, targetk), shape = n, color = targetk))}
	else {p = ggplot(data, aes(x = k, group = interaction(group, algo, threads, targetk), shape = n, color = targetk))}

	p = p + scale_y_log10(labels = comma)
	p = p + labs(x = "k", y = "Time [s]", shape = "Vertices", color = "Edits targeted")

	# first actual plot for a proper legend
	p = p + geom_point(aes(y = results$time))
	# then overdraw highlight for unsolved graphs
	p = p + geom_point(aes(y = results$time), data = subset(data, !results$solved), color = "black", stroke = 2.5, show.legend = FALSE)
	# and redraw the first plot on top of the highlight
	p = p + geom_point(aes(y = results$time))

	p
}


plot.totals = function(data, no.log.scale = F)
{
	# function to plot summary counters
	# i doubt this one is useful
	# expect this one to need work

	data = droplevels(data)

	# construct label, omit non-varying factors
	l = vector()
	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(is.null(data$label)) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
	if(min(data$threads) != max(data$threads)) {l = c(l, "Threads"); if(is.null(data$label)) {data$label = data$threads} else {data$label = paste(data$label, data$threads, sep = " / ")}}
	l = c(l, "Stat"); if(is.null(data$label)) {data$label = ""} else {data$label = paste(data$label, "", sep = " / ")}
	l = paste(l, collapse = " / ")
	if(is.null(data$label)) {data$label = ""}
	data$label = as.factor(data$label)

	colors = list(calls = "black", pruned = "red", fallbacks = "green", single = "blue", skipped = "yellow")

	# no color needed if only plotting a single group
	if(l == "") {p = ggplot(data, aes(x = k, group = interaction(group, algo, threads, k)))}
	else {p = ggplot(data, aes(x = k, group = interaction(group, algo, threads, k), color = label))}

	if(!no.log.scale) {p = p + scale_y_log10(labels = comma)}
	p = p + labs(x = "k", y = "Count", color = l)

	for(i in rev(names(data$results$totals)))
	{
		p = p + geom_boxplot(aes_string(y = paste0("results$totals$", i), color = "sub('$', i, label)"), alpha = 0, outlier.alpha = 1)
	}

	p
}

plot.eval = function(data, dir)
{
	# example function to create multiple plots and write them to disk
	# dir is the directory to which the plots shall be placed

	# adjust as needed

	data = droplevels(data)
	
	t = theme_get()
	# remove legend, instead describe it in the figure caption
	theme_update(legend.position="none")

	# mangle some names
	# adjust as needed
	#   grass_web.metis -> grassweb
	levels(data$group) = gsub("grass_web\\.metis", "grassweb", levels(data$group))
	#   graphs/foo.graph -> foo
	levels(data$group) = gsub("^graphs/", "", levels(data$group))
	levels(data$group) = gsub("\\.graph$", "", levels(data$group))

	# make underscores latex-safe
	levels(data$group) = gsub("_", "\\\\_", levels(data$group))
	levels(data$algo) = gsub("_", "\\\\_", levels(data$algo))

	# for each group of graphs
	for(l in levels(data$group))
	{
		# progress indicator
		print(l)

		# .tex output
		# use via \input{file}
		# subset
		sub = subset(data, group == l & algo %in% c("Basic-Center\\_Matrix-Matrix", "Redundant-Center\\_Matrix-Matrix", "LB\\_Basic-Center\\_Matrix-Matrix", "LB\\_Redundant-Center\\_Matrix-Matrix"))
		# and create figures
		#  any "/" in the group name are replaced (no accidental directory creation fails)
		#  width and height are inces by default, 8 and 4 inches wide appear to be good values for spanning a full resp. half a page's width
		ggsave(paste(gsub("/", "_", l), "basic.times.tex", sep = "."), plot = plot.times(sub), device = tikz, path = dir, width = 8, height = 2)
		ggsave(paste(gsub("/", "_", l), "basic.calls.tex", sep = "."), plot = plot.calls(sub), device = tikz, path = dir, width = 8, height = 2)

		# .pdf output
		sub = subset(data, group == l & algo %in% c("Basic-Center\\_Matrix-Matrix", "Redundant-Center\\_Matrix-Matrix"))
		ggsave(paste(gsub("/", "_", l), "basic.nolb.times.pdf", sep = "."), plot = plot.times(sub), device = pdf, path = dir, width = 4, height = 3)
		ggsave(paste(gsub("/", "_", l), "basic.nolb.calls.pdf", sep = "."), plot = plot.calls(sub), device = pdf, path = dir, width = 4, height = 3)

		# some more plots...
		sub = subset(data, group == l & algo %in% c("LB\\_Basic-Center\\_Matrix-Matrix", "LB\\_Redundant-Center\\_Matrix-Matrix"))
		ggsave(paste(gsub("/", "_", l), "basic.lb.times.tex", sep = "."), plot = plot.times(sub), device = tikz, path = dir, width = 4, height = 3)
		ggsave(paste(gsub("/", "_", l), "basic.lb.calls.tex", sep = "."), plot = plot.calls(sub), device = tikz, path = dir, width = 4, height = 3)
		sub = subset(data, group == l & algo %in% c("LB\\_Redundant-Center\\_Matrix-Matrix", "Redundant-H\\_Triangle\\_LB\\_Center\\_Matrix2-Matrix", "Redundant-LB\\_Center\\_Matrix-Matrix", "S\\_Redundant-S\\_LB\\_Center\\_Matrix-Matrix"))
		ggsave(paste(gsub("/", "_", l), "subgraph.times.tex", sep = "."), plot = plot.times(sub), device = tikz, path = dir, width = 4, height = 3)
		ggsave(paste(gsub("/", "_", l), "subgraph.calls.tex", sep = "."), plot = plot.calls(sub), device = tikz, path = dir, width = 4, height = 3)
	}
	
	# cleanup -- restore theme
	theme_set(t)
}
