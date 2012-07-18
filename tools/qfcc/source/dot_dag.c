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
print_node (dstring_t *dstr, dagnode_t *node)
{
	if (node->print_count == print_count)
		return;
	node->print_count = print_count;
	if (!node->a && (node->b || node->c)) {
		dasprintf (dstr, "  \"dag_%p\" [label=\"bad node\"];\n", node);
		return;
	}
	if (node->a) {
		dasprintf (dstr, "  \"dag_%p\" -> \"dag_%p\" [label=a];\n", node,
				   node->a);
		print_node (dstr, node->a);
	}
	if (node->b) {
		dasprintf (dstr, "  \"dag_%p\" -> \"dag_%p\" [label=b];\n", node,
				   node->b);
		print_node (dstr, node->b);
	}
	if (node->c) {
		dasprintf (dstr, "  \"dag_%p\" -> \"dag_%p\" [label=c];\n", node,
				   node->c);
		print_node (dstr, node->c);
	}
	dasprintf (dstr, "  \"dag_%p\" [%slabel=\"%s\"];\n", node,
			   node->a ? "" : "shape=none,", daglabel_string (node->label));
	if (node->identifiers) {
		daglabel_t *id;

		dasprintf (dstr, "  \"dag_%p\" -> \"dagid_%p\" "
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
	print_count++;
	print_node (dstr, dag);
}
