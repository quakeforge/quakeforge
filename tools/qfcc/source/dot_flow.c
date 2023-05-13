/*
	dot_flow.c

	"emit" flow graphs to dot (graphvis).

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/11/01

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/quakeio.h"
#include "QF/set.h"
#include "QF/va.h"

#include "tools/qfcc/include/dags.h"
#include "tools/qfcc/include/flow.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"

typedef struct {
	void (*node) (dstring_t *, flowgraph_t *, flownode_t *, int);
	void (*edge) (dstring_t *, flowgraph_t *, flowedge_t *, int);
	void (*extra) (dstring_t *, flowgraph_t *, int);
} flow_print_t;

typedef struct {
	const char *type;
	flow_print_t *print;
} flow_dot_t;

static void
print_flow_node (dstring_t *dstr, flowgraph_t *graph, flownode_t *node,
				 int level)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*s\"fn_%p\" [label=\"%d (%d)\"];\n", indent, "",
			   node, node->id, node->dfn);
}

static void
print_flow_edge (dstring_t *dstr, flowgraph_t *graph, flowedge_t *edge,
				 int level)
{
	int         indent = level * 2 + 2;
	int         edge_num = edge - graph->edges;
	flownode_t *t, *h;
	const char *style;
	const char *dir;
	int         weight;

	t = graph->nodes[edge->tail];
	h = graph->nodes[edge->head];
	if (t->dfn >= h->dfn) {
		flownode_t *temp;
		temp = h;
		h = t;
		t = temp;

		dir = "dir=back,";
		style = "dashed";
		weight = 0;
	} else if (set_is_member (graph->dfst, edge_num)) {
		dir = "";
		style = "bold";
		weight = 10;
	} else {
		dir = "";
		style = "solid";
		weight = 0;
	}
	dasprintf (dstr, "%*s", indent, "");
	dasprintf (dstr, "fn_%p -> ", t);
	dasprintf (dstr, "fn_%p [%sstyle=%s,weight=%d", h, dir, style, weight);
	dasprintf (dstr, "];\n");
}

static void
print_flow_node_dag (dstring_t *dstr, flowgraph_t *graph, flownode_t *node,
					 int level)
{
	if (node->dag)
		print_dag (dstr, node->dag, va (0, "%d (%d)", node->id, node->dfn));
	else
		print_flow_node (dstr, graph, node, level);
}

static void
print_flow_edge_dag (dstring_t *dstr, flowgraph_t *graph, flowedge_t *edge,
					 int level)
{
	int         indent = level * 2 + 2;
	int         edge_num = edge - graph->edges;
	flownode_t *t, *h;
	const char *tpref;
	const char *hpref;
	const char *style;
	const char *dir;
	int         weight;

	t = graph->nodes[edge->tail];
	h = graph->nodes[edge->head];
	if (t->dfn >= h->dfn) {
		flownode_t *temp;
		temp = h;
		h = t;
		t = temp;

		tpref = "enter";
		hpref = "leave";
		dir = "dir=back,";
		style = "dashed";
		weight = 0;
	} else if (set_is_member (graph->dfst, edge_num)) {
		tpref = "leave";
		hpref = "enter";
		dir = "";
		style = "bold";
		weight = 10;
	} else {
		tpref = "leave";
		hpref = "enter";
		dir = "";
		style = "solid";
		weight = 0;
	}
	dasprintf (dstr, "%*s", indent, "");
	if (t->dag)
		dasprintf (dstr, "dag_%s_%p -> ", tpref, t->dag);
	else
		dasprintf (dstr, "fn_%p -> ", t);
	if (h->dag)
		dasprintf (dstr, "dag_%s_%p [%sstyle=%s,weight=%d",
				   hpref, h->dag, dir, style, weight);
	else
		dasprintf (dstr, "fn_%p [%sstyle=%s,weight=%d",
				   h, dir, style, weight);
	if (t->dag)
		dasprintf (dstr, ",ltail=cluster_dag_%p", t->dag);
	if (h->dag)
		dasprintf (dstr, ",lhead=cluster_dag_%p", h->dag);
	dasprintf (dstr, "];\n");
}

static void
print_flow_node_live (dstring_t *dstr, flowgraph_t *graph, flownode_t *node,
					  int level)
{
	int         indent = level * 2 + 2;
	set_t      *use = node->live_vars.use;
	set_t      *def = node->live_vars.def;
	set_t      *in = node->live_vars.in;
	set_t      *out = node->live_vars.out;

	dasprintf (dstr, "%*sfn_%p [label=\"", indent, "", node);
	dasprintf (dstr, "use: %s\\n", set_as_string (use));
	dasprintf (dstr, "def: %s\\n", set_as_string (def));
	dasprintf (dstr, "in: %s\\n", set_as_string (in));
	dasprintf (dstr, "out: %s", set_as_string (out));
	dasprintf (dstr, "\"];\n");
}

static void
print_flow_vars (dstring_t *dstr, flowgraph_t *graph, int level)
{
	int         indent = level * 2 + 2;
	int         i;
	flowvar_t  *var;

	dasprintf (dstr, "%*sfv_%p [shape=none,label=<\n", indent, "", graph);
	dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
					 "cellspacing=\"0\">\n", indent + 2, "");
	dasprintf (dstr, "%*s<tr><td colspan=\"3\">flow vars</td></tr>\n",
			   indent + 4, "");
	dasprintf (dstr, "%*s<tr><td>#</td><td>name</td>"
							"<td>addr</td><td>define</td></tr>\n",
			   indent + 4, "");
	for (i = 0; i < graph->func->num_vars; i++) {
		var = graph->func->vars[i];
		dasprintf (dstr, "%*s<tr><td>%d</td><td>%s</td>"
								"<td>%d</td><td>%s</td></tr>\n",
				   indent + 4, "",
				   var->number, html_string(operand_string (var->op)),
				   var->flowaddr,
				   set_as_string (var->define));
	}
	dasprintf (dstr, "%*s</table>>];\n", indent + 2, "");
}

static void
print_flow_node_reaching (dstring_t *dstr, flowgraph_t *graph,
						  flownode_t *node, int level)
{
	int         indent = level * 2 + 2;
	int         reach;
	set_t      *gen = node->reaching_defs.gen;
	set_t      *kill = node->reaching_defs.kill;
	set_t      *in = node->reaching_defs.in;
	set_t      *out = node->reaching_defs.out;

	reach = gen && kill && in && out;

	if (reach) {
		dasprintf (dstr, "%*sfn_%p [label=\"", indent, "", node);
		dasprintf (dstr, "gen: %s\\n", set_as_string (gen));
		dasprintf (dstr, "kill: %s\\n", set_as_string (kill));
		dasprintf (dstr, "in: %s\\n", set_as_string (in));
		dasprintf (dstr, "out: %s\"];\n", set_as_string (out));
	} else {
		print_flow_node (dstr, graph, node, level);
	}
}

static void
print_flow_node_statements (dstring_t *dstr, flowgraph_t *graph,
							flownode_t *node, int level)
{
	if (node->sblock) {
		dot_sblock (dstr, node->sblock, node->id);
	} else {
		print_flow_node (dstr, graph, node, level);
	}
}

static void
print_flow_edge_statements (dstring_t *dstr, flowgraph_t *graph,
							flowedge_t *edge, int level)
{
	int         indent = level * 2 + 2;
	int         edge_num = edge - graph->edges;
	flownode_t *h, *t;
	const char *hpre = "fn_", *tpre = "fn_";
	const char *style;
	const char *dir;
	int         weight;

	t = graph->nodes[edge->tail];
	h = graph->nodes[edge->head];
	if (t->dfn >= h->dfn) {
		flownode_t *temp;
		temp = h;
		h = t;
		t = temp;

		dir = "dir=back,";
		style = "dashed";
		weight = 0;
	} else if (set_is_member (graph->dfst, edge_num)) {
		dir = "";
		style = "bold";
		weight = 10;
	} else {
		dir = "";
		style = "solid";
		weight = 0;
	}
	if (t->sblock) {
		tpre = "sb_";
		t = (flownode_t *) t->sblock;
	}
	if (h->sblock) {
		hpre = "sb_";
		h = (flownode_t *) h->sblock;
	}
	dasprintf (dstr, "%*s", indent, "");
	dasprintf (dstr, "%s%p -> ", tpre, t);
	dasprintf (dstr, "%s%p [%sstyle=%s,weight=%d", hpre, h, dir, style,
			   weight);
	dasprintf (dstr, "];\n");
}

static flow_print_t null_print[] = {
	{	print_flow_node,	print_flow_edge },
	{ 0 }
};
static flow_print_t dag_print[] = {
	{	print_flow_node_dag,	print_flow_edge_dag},
	{ 0 }
};
static flow_print_t live_print[] = {
	{	print_flow_node_live,	print_flow_edge, 	print_flow_vars},
	{	print_flow_node_statements,	print_flow_edge_statements},
	{ 0 }
};
static flow_print_t reaching_print[] = {
	{	print_flow_node_reaching,	print_flow_edge},
	{ 0 }
};
static flow_print_t statements_print[] = {
	{	print_flow_node_statements,	print_flow_edge_statements},
	{ 0 }
};

static flow_dot_t flow_dot_methods[] = {
	{"",			null_print},
	{"dag",			dag_print},
	{"live",		live_print},
	{"reaching",	reaching_print},
	{"statements",	statements_print},
};

static void
print_flowgraph (flow_dot_t *method, flowgraph_t *graph, const char *filename)
{
	int         i;
	dstring_t  *dstr = dstring_newstr();

	dasprintf (dstr, "digraph flowgraph_%s_%p {\n", method->type, graph);
	dasprintf (dstr, "  graph [label=\"%s\"];\n", quote_string (filename));
	dasprintf (dstr, "  layout=dot;\n");
	dasprintf (dstr, "  clusterrank=local;\n");
	dasprintf (dstr, "  rankdir=TB;\n");
	dasprintf (dstr, "  compound=true;\n");
	for (flow_print_t *print = method->print; print->node; print++) {
		for (i = 0; i < graph->num_nodes; i++) {
			print->node (dstr, graph, graph->nodes[i], 0);
		}
		for (i = 0; i < graph->num_edges; i++) {
			if ((int) graph->edges[i].head >= graph->num_nodes
				|| (int) graph->edges[i].tail >= graph->num_nodes)
				continue;		// dummy node
			print->edge (dstr, graph, &graph->edges[i], 0);
		}
		if (print->extra)
			print->extra (dstr, graph, 0);
	}
	dasprintf (dstr, "}\n");

	if (filename) {
		QFile      *file;

		file = Qopen (filename, "wt");
		Qwrite (file, dstr->str, dstr->size - 1);
		Qclose (file);
	} else {
		fputs (dstr->str, stdout);
	}
	dstring_delete (dstr);
}

void
dump_dot_flow (void *g, const char *filename)
{
	print_flowgraph (&flow_dot_methods[0], (flowgraph_t *) g, filename);
}

void
dump_dot_flow_dags (void *g, const char *filename)
{
	print_flowgraph (&flow_dot_methods[1], (flowgraph_t *) g, filename);
}

void
dump_dot_flow_live (void *g, const char *filename)
{
	print_flowgraph (&flow_dot_methods[2], (flowgraph_t *) g, filename);
}

void
dump_dot_flow_reaching (void *g, const char *filename)
{
	print_flowgraph (&flow_dot_methods[3], (flowgraph_t *) g, filename);
}

void
dump_dot_flow_statements (void *g, const char *filename)
{
	print_flowgraph (&flow_dot_methods[4], (flowgraph_t *) g, filename);
}
