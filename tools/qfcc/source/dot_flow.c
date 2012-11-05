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

#include <QF/dstring.h>
#include <QF/quakeio.h>
#include <QF/va.h>

#include "dags.h"
#include "flow.h"
#include "expr.h"
#include "set.h"
#include "strpool.h"

static void
print_flow_node (dstring_t *dstr, flownode_t *node, int level)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*s\"fn_%p\" [label=\"%d (%d)\"];\n", indent, "", node,
			   node->id, node->dfn);
}

static void
print_flow_edges (dstring_t *dstr, flowgraph_t *graph, int level)
{
	int         indent = level * 2 + 2;
	int         i;
	flowedge_t *edge;
	flownode_t *t, *h;
	const char *style;
	int         weight;

	for (i = 0; i < graph->num_edges; i++) {
		edge = &graph->edges[i];
		t = graph->nodes[edge->tail];
		h = graph->nodes[edge->head];
		if (t->dfn < h->dfn) {
			style = "solid";
			weight = 0;
			if (set_is_member (graph->dfst, i)) {
				style = "bold";
				weight = 10;
			}
			dasprintf (dstr,
					   "%*s\"fn_%p\" -> \"fn_%p\" [style=%s,weight=%d];\n",
					   indent, "", t, h, style, weight);
		} else {
			dasprintf (dstr,
					   "%*s\"fn_%p\" -> \"fn_%p\" [dir=back, style=dashed];\n",
					   indent, "", h, t);
		}
	}
}

void
print_flowgraph (flowgraph_t *graph, const char *filename)
{
	int         i;
	dstring_t  *dstr = dstring_newstr();

	dasprintf (dstr, "digraph flowgraph_%p {\n", graph);
	dasprintf (dstr, "  layout=dot;\n");
	dasprintf (dstr, "  clusterrank=local;\n");
	dasprintf (dstr, "  rankdir=TB;\n");
	dasprintf (dstr, "  compound=true;\n");
	for (i = 0; i < graph->num_nodes; i++) {
		print_flow_node (dstr, graph->nodes[i], 0);
	}
	print_flow_edges (dstr, graph, 0);
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
