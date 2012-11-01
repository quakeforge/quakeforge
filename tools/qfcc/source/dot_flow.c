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
#include "strpool.h"

static void
print_flow_node (dstring_t *dstr, flownode_t *node, int level)
{
	int         indent = level * 2 + 2;
	unsigned    j;

	if (node->nodes) {
		dasprintf (dstr, "%*ssubgraph \"cluster_%p\" {\n", indent, "", node);
		//dasprintf (dstr, "%*srankdir=TB;\n", indent + 2, "");
		dasprintf (dstr, "%*slabel=\"%d\";\n", indent + 2, "", node->id);
		for (j = 0; j < node->num_nodes; j++)
			print_flow_node (dstr, node->nodes[j], level + 1);
		dasprintf (dstr, "%*s}\n", indent, "");
	}
	dasprintf (dstr, "%*s\"fn_%p\" [label=\"%d\"];\n", indent, "", node,
			   node->id);
}

static void
print_flow_edges (dstring_t *dstr, flownode_t *node, int level)
{
	int         indent = level * 2 + 2;
	int         i;
	unsigned    j;

	if (node->nodes) {
		for (j = 0; j < node->num_nodes; j++)
			print_flow_edges (dstr, node->nodes[j], level + 1);
	}
	for (i = 0; i < node->num_succ; i++)
		dasprintf (dstr, "%*s\"fn_%p\" -> \"fn_%p\";\n", indent, "", node,
			   node->siblings[node->successors[i]]);
}

void
print_flowgraph (flownode_t *flow, const char *filename)
{
	unsigned    i;
	dstring_t  *dstr = dstring_newstr();

	dasprintf (dstr, "digraph flow_%p {\n", flow->siblings);
	dasprintf (dstr, "  layout=dot;\n");
	dasprintf (dstr, "  clusterrank=local;\n");
	//dasprintf (dstr, "  rankdir=TB;\n");
	dasprintf (dstr, "  compound=true;\n");
	for (i = 0; i < flow->num_siblings; i++) {
		print_flow_node (dstr, flow->siblings[i], 0);
		print_flow_edges (dstr, flow->siblings[i], 0);
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
