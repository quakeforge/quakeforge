/*
	dags.c

	DAG representation of basic blocks

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

#include "dags.h"
#include "diagnostic.h"
#include "qfcc.h"
#include "statements.h"
#include "symtab.h"

static daglabel_t *free_labels;
static dagnode_t *free_nodes;

static daglabel_t *
new_label (void)
{
	daglabel_t *label;
	ALLOC (256, daglabel_t, labels, label);
	return label;
}

static dagnode_t *
new_node (void)
{
	dagnode_t *node;
	ALLOC (256, dagnode_t, nodes, node);
	return node;
}

const char *
daglabel_string (daglabel_t *label)
{
	if ((label->opcode && label->op) || (!label->opcode || !label->op))
		return "bad label";
	if (label->opcode)
		return label->opcode;
	return operand_string (label->op);
}

static const dagnode_t *
label_node (operand_t *op)
{
	operand_t  *o = op;
	dagnode_t  *node;
	daglabel_t *label;
	symbol_t   *sym = 0;

	while (o->op_type == op_alias)
		o = o->o.alias;
	if (o->op_type != op_symbol) {
		if (o->op_type != op_value && o->op_type != op_pointer)
			debug (0, "unexpected operand type");
		goto make_new_node;
	}
	sym = o->o.symbol;
	if (sym->daglabel)
		return sym->daglabel->dagnode;

make_new_node:
	label = new_label ();
	label->op = op;
	if (sym)
		sym->daglabel = label;
	node = new_node ();
	node->label = label;
	return node;
}

dagnode_t *
make_dag (const sblock_t *block)
{
	statement_t *s;
	dagnode_t   *dag;

	for (s = block->statements; s; s = s->next) {
		dagnode_t  *x = 0, *y = 0, *z = 0;
		// c = a * b
		// c = ~a
		// c = a / b
		// c = a + b
		// c = a - b
		// c = a {==,!=,<=,>=,<,>} b
		// c = a.b
		// c = &a.b
		// c = a (convert)
		// b = a
		// b .= a
		// b.c = a
		// c = !a
		// cond a goto b
		// callN a
		// rcallN a, [b, [c]]
		// state a, b
		// state a, b, c
		// goto a
		// jump a
		// jumpb a, b
		// c = a &&/|| b
		// c = a <</>> b
		// c = a & b
		// c = a | b
		// c = a % b
		// c = a ^ b
		// c = a (move) b (count)
	}
}
