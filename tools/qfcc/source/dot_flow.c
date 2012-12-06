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

#include "dags.h"
#include "flow.h"
#include "function.h"
#include "expr.h"
#include "statements.h"
#include "strpool.h"

typedef struct {
	const char *type;
	void (*print_node) (dstring_t *, flowgraph_t *, flownode_t *, int);
	void (*print_edge) (dstring_t *, flowgraph_t *, flowedge_t *, int);
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
		print_dag (dstr, node->dag, va ("%d (%d)", node->id, node->dfn));
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
	set_iter_t *var_iter;
	flowvar_t  *var;
	int         live;

	live = node->live_vars.out && !set_is_empty (node->live_vars.out);

	if (live) {
		dasprintf (dstr, "%*sfn_%p [shape=none,label=<\n", indent, "", node);
		dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
						 "cellspacing=\"0\">\n", indent + 2, "");
		dasprintf (dstr, "%*s<tr><td>%d (%d)</td></tr>\n", indent + 4, "",
				   node->id, node->dfn);
		for (var_iter = set_first (node->live_vars.out); var_iter;
			 var_iter = set_next (var_iter)) {
			var = graph->func->vars[var_iter->value];
			dasprintf (dstr, "%*s<tr><td>(%d) %s</td></tr>\n", indent + 4, "",
					   var->number, html_string(operand_string (var->op)));
		}
		dasprintf (dstr, "%*s</table>>];\n", indent + 2, "");
	} else {
		print_flow_node (dstr, graph, node, level);
	}
}

static flow_dot_t flow_dot_methods[] = {
	{"",		print_flow_node,		print_flow_edge},
	{"dag",		print_flow_node_dag,	print_flow_edge_dag},
	{"live",	print_flow_node_live,	print_flow_edge},
};

static void
print_flowgraph (flow_dot_t *method, flowgraph_t *graph, const char *filename)
{
	int         i;
	dstring_t  *dstr = dstring_newstr();

	dasprintf (dstr, "digraph flowgraph_%s_%p {\n", method->type, graph);
	dasprintf (dstr, "  layout=dot;\n");
	dasprintf (dstr, "  clusterrank=local;\n");
	dasprintf (dstr, "  rankdir=TB;\n");
	dasprintf (dstr, "  compound=true;\n");
	for (i = 0; i < graph->num_nodes; i++) {
		method->print_node (dstr, graph, graph->nodes[i], 0);
	}
	for (i = 0; i < graph->num_edges; i++) {
		method->print_edge (dstr, graph, &graph->edges[i], 0);
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
