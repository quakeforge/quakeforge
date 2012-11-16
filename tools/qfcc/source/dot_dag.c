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
#include "statements.h"
#include "symtab.h"
#include "type.h"

static int print_count;

static void
print_node_def (dstring_t *dstr, dagnode_t *node, int recurse)
{
	if (!node->a && (node->b || node->c)) {
		dasprintf (dstr, "  \"dagnode_%p\" [label=\"bad node\"];\n", node);
		return;
	}
	dasprintf (dstr, "  \"dagnode_%p\" [%slabel=\"%s\"];\n", node,
			   node->a ? "" : "shape=none,", daglabel_string (node->label));
	if (recurse) {
		if (node->a)
			print_node_def (dstr, node->a, 1);
		if (node->b)
			print_node_def (dstr, node->b, 1);
		if (node->c)
			print_node_def (dstr, node->c, 1);
	}
}

static void
print_root_nodes (dstring_t *dstr, dagnode_t *dag)
{
	dasprintf (dstr, "  subgraph roots_%p {", dag);
	dasprintf (dstr, "    rank=same;");
	for (; dag; dag = dag->next)
		print_node_def (dstr, dag, 0);
	dasprintf (dstr, "  }\n");
}

static void
print_child_nodes (dstring_t *dstr, dagnode_t *dag)
{
	for (; dag; dag = dag->next) {
		if (!dag->a && (dag->b || dag->c))
			continue;
		if (dag->a)
			print_node_def (dstr, dag->a, 1);
		if (dag->b)
			print_node_def (dstr, dag->b, 1);
		if (dag->c)
			print_node_def (dstr, dag->c, 1);
	}
}

static void
print_node (dstring_t *dstr, dagnode_t *node)
{
	if (node->print_count == print_count)
		return;
	node->print_count = print_count;
	if (node->a) {
		dasprintf (dstr, "  \"dagnode_%p\" -> \"dagnode_%p\" [label=a];\n",
				   node, node->a);
		print_node (dstr, node->a);
	}
	if (node->b) {
		dasprintf (dstr, "  \"dagnode_%p\" -> \"dagnode_%p\" [label=b];\n",
				   node, node->b);
		print_node (dstr, node->b);
	}
	if (node->c) {
		dasprintf (dstr, "  \"dagnode_%p\" -> \"dagnode_%p\" [label=c];\n",
				   node, node->c);
		print_node (dstr, node->c);
	}
	if (node->next)
		dasprintf (dstr,
				   "  \"dagnode_%p\" -> \"dagnode_%p\" [style=dashed];\n",
				   node, node->next);
	if (node->identifiers) {
		daglabel_t *id;

		dasprintf (dstr, "  \"dagnode_%p\" -> \"dagid_%p\" "
						 "[style=dashed,dir=none];\n", node, node);
		dasprintf (dstr, "  \"dagid_%p\" [shape=none,label=<\n", node);
		dasprintf (dstr, "    <table border=\"0\" cellborder=\"1\" "
						 "cellspacing=\"0\">\n");
		dasprintf (dstr, "      <tr>\n");
		dasprintf (dstr, "        <td>");
		for (id = node->identifiers; id; id = id->next)
			dasprintf (dstr, "%s%s", daglabel_string(id), id->next ? " " : "");
		dasprintf (dstr, "        </td>");
		dasprintf (dstr, "      </tr>\n");
		dasprintf (dstr, "    </table>>];\n");
	}
}

void
print_dag (dstring_t *dstr, dagnode_t *dag)
{
	dasprintf (dstr, "  subgraph cluster_dag_%p {", dag);
	print_count++;
	print_root_nodes (dstr, dag);
	print_child_nodes (dstr, dag);
	for (; dag; dag = dag->next)
		print_node (dstr, dag);
	dasprintf (dstr, "  }\n");
}
