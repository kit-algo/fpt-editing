library(jsonlite)
library(RSQLite)
library(ggplot2)
library(tikzDevice)
library(scales)

loadjson = function(file)
{
	# load experiment results from JSON file created by ./eval2json
	# note: jsonlite and/or R leaks memory here; on larger data sets using loaddb() is strongly recommended

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

loaddb = function(file)
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


plot.times = function(data)
{
	data = droplevels(data)

	# construct label, omit non-varying factors
	l = vector()
	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(is.null(data$label)) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
	if(min(data$threads) != max(data$threads)) {l = c(l, "Threads"); if(is.null(data$label)) {data$label = data$threads} else {data$label = paste(data$label, data$threads, sep = " / ")}}
	l = paste(l, collapse = " / ")
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
	data = droplevels(data)

	# construct label, omit non-varying factors
	l = vector()
	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(is.null(data$label)) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
	# threads should not make a difference in the number of recursive calls
	#if(min(data$threads) != max(data$threads)) {l = c(l, "Threads"); if(is.null(data$label)) {data$label = data$threads} else {data$label = paste(data$label, data$threads, sep = " / ")}}
	l = paste(l, collapse = " / ")
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

plot.mt.speedup = function(data, strip_lb = FALSE)
{
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
# TODO

plot.syn = function(data, strip_lb = FALSE)
{
#	data = subset(data, k >= max(data$k) - 10)
	data = droplevels(data)

	data$n = as.factor(as.integer(gsub("^.*components\\.([0-9]+)\\..*$", "\\1", data$group)))
	data$tk = as.factor(1.25 * as.integer(gsub("^.*\\.([0-9]+)\\.graph$", "\\1", data$group)))
	
#	l = vector()
#	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
#	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(length(as.logical(data$label)) == 0) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
#	if(min(data$results$solved) != max(data$results$solved)) {l = c(l, "Solved"); if(length(as.logical(data$label)) == 0) {data$label = data$results$solved} else {data$label = paste(data$label, data$results$solved, sep = " / ")}}
#	l = paste(l, collapse = " / ")

#	if(strip_lb)
#	{
#		levels(data$label) = gsub("LB\\\\_", "", levels(data$label))
#		data$lt = ifelse(grepl("LB", data$algo), "dashed", "solid")
#	}
#	else
#	{
#		data$lt = ifelse(grepl("LB", data$algo), "solid", "solid")
#	}

	#p = ggplot(data, aes(x = k, group = interaction(k, group, algo, results$solved), color = interaction(group, algo, results$solved, sep = ' / ')))
#	if(l == "") {p = ggplot(data, aes(x = k, group = interaction(k, group, algo, results$solved)))}
#	else {p = ggplot(data, aes(x = k, group = interaction(k, group, algo, results$solved), color = label))}
#	if(l == "") {p = ggplot(data, aes(x = k, group = interaction(group, algo)))}
#	else {
	p = ggplot(data, aes(x = k, shape = n, color = tk))
#	p = p + scale_y_log10(labels = comma) + scale_x_continuous(breaks = seq(min(data$k), max(data$k), 5), labels = function(i) {i}, minor_breaks = seq(min(data$k), max(data$k)))
	p = p + scale_y_log10(labels = comma)
	p = p + labs(x = "k", y = "Time [s]", shape = "Vertices", color = "Edits targeted")
#	p = p + geom_boxplot(aes(y = results$time), alpha = 0, outlier.alpha = 1)
	p = p + geom_point(aes(y = results$time))
	p = p + geom_point(aes(y = results$time), data = subset(data, !results$solved), color = "black", stroke = 2.5, show.legend = FALSE)
	p = p + geom_point(aes(y = results$time))
	p
}


plot.totals = function(data, no.scale = F, no.nudge = F)
{
	data = droplevels(data)
	l = vector()
	if(nlevels(data$group) > 1) {l = c(l, "Graph"); data$label = data$group}
	if(nlevels(data$algo) > 1) {l = c(l, "Algorithm"); if(length(as.logical(data$label)) == 0) {data$label = data$algo} else {data$label = paste(data$label, data$algo, sep = " / ")}}
	if(min(data$results$solved) != max(data$results$solved)) {l = c(l, "Solved"); if(length(as.logical(data$label)) == 0) {data$label = data$results$solved} else {data$label = paste(data$label, data$results$solved, sep = " / ")}}
	l = c(l, "Stat"); if(length(as.logical(data$label)) == 0) {data$label = ""} else {data$label = paste(data$label, "", sep = " / ")}
	l = paste(l, collapse = " / ")

	colors = list(calls = "black", pruned = "red", Paths = "green", Cycles = "blue")

#	p = ggplot(data, aes(x = k, group = interaction(k, group, algo, results$solved), color = interaction(algo, results$solved)))
	p = ggplot(data, aes(x = k, group = interaction(k, group, algo, results$solved)))
	if(!no.scale) {p = p + scale_y_log10(labels = comma)}
	p = p + scale_x_continuous(breaks = seq(min(data$k), max(data$k), 5), labels = function(i) {i}, minor_breaks = seq(min(data$k), max(data$k)))
	p = p + labs(x = "k", y = "Count", color = l)
	for(i in rev(names(data$results$totals)))
	{
#		if(no.nudge) {p = p + geom_boxplot(aes_string(y = paste0("results$totals$", i)), color = colors[[i]], alpha = 0, outlier.alpha = 1)}
#		else {p = p + geom_boxplot(aes_string(y = paste0("results$totals$", i)), color = colors[[i]], alpha = 0, outlier.alpha = 1, position = position_nudge(pos[[i]], 0))}
		p = p + geom_boxplot(aes_string(y = paste0("results$totals$", i), color = "sub('$', i, label)"), alpha = 0, outlier.alpha = 1)
	}
	p
}

plot.all = function(data, dir)
{
	t = theme_get()
	theme_update(plot.title = element_text(size = rel(.6)))
	unlink(dir, recursive = T)
	dir.create(dir)
	for(l in levels(data$group))
	{
		pl = paste0(dir, "/", gsub("/", "_", l))
		dir.create(pl)
		sub = subset(data, group == l)
		for(a in levels(sub$algo))
		{
			pa = paste0(pl, "/", a)
			dir.create(pa)
			suba = subset(sub, algo == a)
#			for(s in c(F, T))
#			{
#				subs = subset(suba, results$solved == s)
				s = any(suba$results$solved)
				subs = suba
				if(nrow(subs) == 0) {next}
				print(paste(l, a, s, "times"))
				ggsave(paste(gsub("/", "_", l), a, s, "times.pdf", sep = "."), plot = plot.times(subs) + ggtitle(paste("graph:", l, "algo:", a, "solved:", s)), device = "pdf", path = pa)
#				print(paste(l, s, "times"))
#				ggsave(paste(gsub("/", "_", l), s, "times.pdf", sep = "."), plot = plot.times(subs) + ggtitle(paste("graph:", l, "solved:", s)), device = "pdf", path = pl)
#				print(paste(l, a, s, "totals"))
#				if(class(try(ggsave(paste(gsub("/", "_", l), a, s, "totals.pdf", sep = "."), plot = plot.totals(subs) + ggtitle(paste("graph:", l, "algo:", a, "solved:", s)), device = "pdf", path = pa)
#				)) == "try-error") {if(class(try(ggsave(paste(gsub("/", "_", l), a, s, "totals.pdf", sep = "."), plot = plot.totals(subs, T) + ggtitle(paste("graph:", l, "algo:", a, "solved:", s)), device = "pdf", path = pa)
#				)) == "try-error") {if(class(try(ggsave(paste(gsub("/", "_", l), a, s, "totals.pdf", sep = "."), plot = plot.totals(subs, F, T) + ggtitle(paste("graph:", l, "algo:", a, "solved:", s)), device = "pdf", path = pa)
#				)) == "try-error") {if(class(try(ggsave(paste(gsub("/", "_", l), a, s, "totals.pdf", sep = "."), plot = plot.totals(subs, T, T) + ggtitle(paste("graph:", l, "algo:", a, "solved:", s)), device = "pdf", path = pa)
#				)) == "try-error") {print("failed")}}}}
#				for(i in max(subs$k):max(subs$k))
#				{
#					subk = subset(subs, k == i)
#					if(nrow(subk) == 0) {next}
#					print(paste(l, a, s, "counters", i))
#					if(class(try(ggsave(paste(gsub("/", "_", l), a, s, i, "counters.pdf", sep = "."), plot = plot.counters(subk) + ggtitle(paste("graph:", l, "algo:", a, "solved:", s, "k:", i)), device = "pdf", path = pa)
#					)) == "try-error") {if(class(try(ggsave(paste(gsub("/", "_", l), a, s, i, "counters.pdf", sep = "."), plot = plot.counters(subk, T) + ggtitle(paste("graph:", l, "algo:", a, "solved:", s, "k:", i)), device = "pdf", path = pa)
#					)) == "try-error") {print("failed")}}
#				}
#			}
		}
	}
	theme_set(t)
}

plot.eval = function(data, dir)
{
	t = theme_get()
	theme_update(legend.position="none")

#	unlink(dir, recursive = T)
#	dir.create(dir)

	levels(data$group) = gsub("grass_web\\.metis", "grassweb", levels(data$group))

	levels(data$group) = gsub("^data/", "", levels(data$group))
	levels(data$group) = gsub("\\.graph$", "", levels(data$group))
	levels(data$group) = gsub("_", "\\\\_", levels(data$group))

	levels(data$algo) = gsub("_", "\\\\_", levels(data$algo))

	for(l in levels(data$group))
	{
		print(l)
		sub = subset(data, group == l & algo %in% c("Basic-Center\\_Matrix-Matrix", "Redundant-Center\\_Matrix-Matrix", "LB\\_Basic-Center\\_Matrix-Matrix", "LB\\_Redundant-Center\\_Matrix-Matrix"))
			ggsave(paste(gsub("/", "_", l), "basic.times.tex", sep = "."), plot = plot.times(sub), device = tikz, path = dir, width = 8, height = 2)
			ggsave(paste(gsub("/", "_", l), "basic.calls.tex", sep = "."), plot = plot.calls(sub), device = tikz, path = dir, width = 8, height = 2)
		sub = subset(data, group == l & algo %in% c("Basic-Center\\_Matrix-Matrix", "Redundant-Center\\_Matrix-Matrix"))
			ggsave(paste(gsub("/", "_", l), "basic.nolb.times.tex", sep = "."), plot = plot.times(sub), device = tikz, path = dir, width = 4, height = 3)
			ggsave(paste(gsub("/", "_", l), "basic.nolb.calls.tex", sep = "."), plot = plot.calls(sub), device = tikz, path = dir, width = 4, height = 3)
		sub = subset(data, group == l & algo %in% c("LB\\_Basic-Center\\_Matrix-Matrix", "LB\\_Redundant-Center\\_Matrix-Matrix"))
			ggsave(paste(gsub("/", "_", l), "basic.lb.times.tex", sep = "."), plot = plot.times(sub), device = tikz, path = dir, width = 4, height = 3)
			ggsave(paste(gsub("/", "_", l), "basic.lb.calls.tex", sep = "."), plot = plot.calls(sub), device = tikz, path = dir, width = 4, height = 3)
		sub = subset(data, group == l & algo %in% c("LB\\_Redundant-Center\\_Matrix-Matrix", "Redundant-H\\_Triangle\\_LB\\_Center\\_Matrix2-Matrix", "Redundant-LB\\_Center\\_Matrix-Matrix", "S\\_Redundant-S\\_LB\\_Center\\_Matrix-Matrix"))
			ggsave(paste(gsub("/", "_", l), "subgraph.times.tex", sep = "."), plot = plot.times(sub), device = tikz, path = dir, width = 4, height = 3)
			ggsave(paste(gsub("/", "_", l), "subgraph.calls.tex", sep = "."), plot = plot.calls(sub), device = tikz, path = dir, width = 4, height = 3)
	}
	theme_set(t)
}

plot.eval.mt = function(data, dir)
{
	t = theme_get()
#	theme_update(legend.position="none")
	

#	unlink(dir, recursive = T)
#	dir.create(dir)

	levels(data$group) = gsub("grass_web\\.metis", "grassweb", levels(data$group))

	levels(data$group) = gsub("^data/", "", levels(data$group))
	levels(data$group) = gsub("\\.graph$", "", levels(data$group))
	levels(data$group) = gsub("_", "\\\\_", levels(data$group))

	levels(data$algo) = gsub("_", "\\\\_", levels(data$algo))

	for(l in levels(data$group))
	{
		print(l)
		sub = subset(data, group == l)
#			ggsave(paste(gsub("/", "_", l), "mt.times.tex", sep = "."), plot = plot.times(sub), device = tikz, path = dir, width = 4, height = 3)
			ggsave(paste(gsub("/", "_", l), "mt.times.tex", sep = "."), plot = plot.times(sub) + theme(legend.position = c(0.11, 0.65), legend.background = element_rect(fill = "grey92", size = 1, linetype = "solid")), device = tikz, path = dir, width = 4, height = 3)
	}
	theme_set(t)
}
