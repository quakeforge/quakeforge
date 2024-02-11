/*
	dot_dag.c

	Output dags to dot (graphvis).

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/05/08

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
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

static void
print_node_def (dstring_t *dstr, dag_t *dag, dagnode_t *node)
{
	set_iter_t *id_iter;
	daglabel_t *id;

	dasprintf (dstr, "  \"dagnode_%p\" [%slabel=\"%s%s (%d)", node,
			   node->type != st_none ? "" : "shape=box,",
			   daglabel_string (node->label),
			   node->killed ? " k" : "",
			   node->topo);
	for (id_iter = set_first (node->identifiers); id_iter;
		 id_iter = set_next (id_iter)) {
		id = dag->labels[id_iter->element];
		dasprintf (dstr, "\\n%s", daglabel_string(id));
	}
	dasprintf (dstr, "\"];\n");
}

static void
print_root_nodes (dstring_t *dstr, dag_t *dag)
{
	set_iter_t *node_iter;
//	dasprintf (dstr, "    subgraph roots_%p {", dag);
//	dasprintf (dstr, "      rank=same;");
	for (node_iter = set_first (dag->roots); node_iter;
		 node_iter = set_next (node_iter)) {
		dagnode_t  *node = dag->nodes[node_iter->element];
		//if (set_is_empty (node->edges)) {
		//	continue;
		//}
		print_node_def (dstr, dag, node);
		dasprintf (dstr, "      dag_enter_%p -> dagnode_%p [style=invis];\n",
				   dag, node);
	}
//	dasprintf (dstr, "    }\n");
}

static void
print_child_nodes (dstring_t *dstr, dag_t *dag)
{
	int         i;
	dagnode_t  *node;

	for (i = 0; i < dag->num_nodes; i++) {
		node = dag->nodes[i];
		if (!set_is_empty (node->parents))
			print_node_def (dstr, dag, node);
	}
}

static void
print_node (dstring_t *dstr, dag_t *dag, dagnode_t *node)
{
	int         i;
	set_t      *edges = set_new ();
	set_iter_t *edge_iter;

	if (!set_is_member (dag->roots, node->number)
		&& set_is_empty (node->parents))
		return;
	set_assign (edges, node->edges);
	for (i = 0; i < 3; i++) {
		if (node->children[i]) {
			set_remove (edges, node->children[i]->number);
			dasprintf (dstr,
					   "  \"dagnode_%p\" -> \"dagnode_%p\" [label=%c];\n",
					   node, node->children[i], i + 'a');
		}
	}
	for (edge_iter = set_first (edges); edge_iter;
		 edge_iter = set_next (edge_iter)) {
		auto n = dag->nodes[edge_iter->element];
		if (n->type != st_none) {
			dasprintf (dstr,
					   "  \"dagnode_%p\" -> \"dagnode_%p\" [style=dashed];\n",
					   node, n);
		}
	}
	set_delete (edges);
	if (0) {
		for (edge_iter = set_first (node->reachable); edge_iter;
			 edge_iter = set_next (edge_iter)) {
			dasprintf (dstr,
					   "  \"dagnode_%p\" -> \"dagnode_%p\" [style=dotted];\n",
					   node, dag->nodes[edge_iter->element]);
		}
	}
	if (0 && !set_is_empty (node->identifiers)) {
		set_iter_t *id_iter;
		daglabel_t *id;

		dasprintf (dstr, "  \"dagnode_%p\" -> \"dagid_%p\" "
						 "[style=dashed,dir=none];\n", node, node);
		dasprintf (dstr, "  \"dagid_%p\" [shape=none,label=<\n", node);
		dasprintf (dstr, "    <table border=\"0\" cellborder=\"1\" "
						 "cellspacing=\"0\">\n");
		dasprintf (dstr, "      <tr>\n");
		dasprintf (dstr, "        <td>");
		for (id_iter = set_first (node->identifiers); id_iter;
			 id_iter = set_next (id_iter)) {
			id = dag->labels[id_iter->element];
			dasprintf (dstr, " %s", html_string (daglabel_string(id)));
		}
		dasprintf (dstr, "        </td>");
		dasprintf (dstr, "      </tr>\n");
		dasprintf (dstr, "    </table>>];\n");
	}
	if (set_is_empty (node->edges))
		dasprintf (dstr,
				   "  \"dagnode_%p\" -> \"dag_leave_%p\" [style=invis];\n",
				   node, dag);
}

void
print_dag (dstring_t *dstr, dag_t *dag, const char *label)
{
	int         i;

	if (label) {
		dasprintf (dstr, "  subgraph cluster_dag_%p {\n", dag);
		dasprintf (dstr, "    label=\"%s\";\n", label);
	}
	dasprintf (dstr, "    dag_enter_%p [label=\"\", style=invis];\n", dag);
	print_root_nodes (dstr, dag);
	print_child_nodes (dstr, dag);
	for (i = 0; i < dag->num_nodes; i++) {
		auto node = dag->nodes[i];
		//if (set_is_empty (node->parents) && set_is_empty (node->edges)) {
		//	continue;
		//}
		print_node (dstr, dag, node);
	}
	dasprintf (dstr, "    dag_leave_%p [label=\"\", style=invis];\n", dag);
	if (label)
		dasprintf (dstr, "  }\n");
}

void
dot_dump_dag (void *_dag, const char *filename)
{
	dag_t      *dag = _dag;
	dstring_t  *dstr = dstring_newstr();

	dasprintf (dstr, "digraph dag_%p {\n", dag);
	dasprintf (dstr, "  graph [label=\"%s\"];\n", quote_string (filename));
	dasprintf (dstr, "  layout=dot;\n");
	dasprintf (dstr, "  clusterrank=local;\n");
	dasprintf (dstr, "  rankdir=TB;\n");
	dasprintf (dstr, "  compound=true;\n");
	print_dag (dstr, dag, "");

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
