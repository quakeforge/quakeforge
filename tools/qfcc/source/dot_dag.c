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

#include <QF/dstring.h>
#include <QF/quakeio.h>
#include <QF/va.h>

#include "dags.h"
#include "set.h"
#include "statements.h"
#include "symtab.h"
#include "type.h"

static void
print_node_def (dstring_t *dstr, dagnode_t *node)
{
	dasprintf (dstr, "  \"dagnode_%p\" [%slabel=\"%s (%d)\"];\n", node,
			   node->type != st_none ? "" : "shape=none,",
			   daglabel_string (node->label), node->topo);
}

static void
print_root_nodes (dstring_t *dstr, dag_t *dag)
{
	set_iter_t *node_iter;
	dasprintf (dstr, "    subgraph roots_%p {", dag);
	dasprintf (dstr, "      rank=same;");
	for (node_iter = set_first (dag->roots); node_iter;
		 node_iter = set_next (node_iter)) {
		dagnode_t  *node = dag->nodes[node_iter->member];
		print_node_def (dstr, node);
		dasprintf (dstr, "      dag_%p ->dagnode_%p [style=invis];\n", dag,
				   node);
	}
	dasprintf (dstr, "    }\n");
}

static void
print_child_nodes (dstring_t *dstr, dag_t *dag)
{
	int         i;
	dagnode_t  *node;

	for (i = 0; i < dag->num_nodes; i++) {
		node = dag->nodes[i];
		if (!set_is_empty (node->parents))
			print_node_def (dstr, node);
	}
}

static void
print_node (dstring_t *dstr, dag_t *dag, dagnode_t *node)
{
	int         i;

	for (i = 0; i < 3; i++) {
		if (node->children[i]) {
			dasprintf (dstr,
					   "  \"dagnode_%p\" -> \"dagnode_%p\" [label=%c];\n",
					   node, node->children[i], i + 'a');
		}
	}
	if (!set_is_empty (node->identifiers)) {
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
			id = dag->labels[id_iter->member];
			dasprintf (dstr, " %s", daglabel_string(id));
		}
		dasprintf (dstr, "        </td>");
		dasprintf (dstr, "      </tr>\n");
		dasprintf (dstr, "    </table>>];\n");
	}
}

void
print_dag (dstring_t *dstr, dag_t *dag)
{
	int         i;
	dasprintf (dstr, "  subgraph cluster_dag_%p {\n", dag);
	dasprintf (dstr, "    dag_%p [label=\"\", style=invis];\n", dag);
	print_root_nodes (dstr, dag);
	print_child_nodes (dstr, dag);
	for (i = 0; i < dag->num_nodes; i++)
		print_node (dstr, dag, dag->nodes[i]);
	dasprintf (dstr, "  }\n");
}
