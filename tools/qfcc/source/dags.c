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
	if ((label->opcode && label->op) || (!label->opcode && !label->op))
		return "bad label";
	if (label->opcode)
		return label->opcode;
	return operand_string (label->op);
}

static daglabel_t *
opcode_label (const char *opcode)
{
	daglabel_t *label;

	label = new_label ();
	label->opcode = opcode;
	return label;
}

static daglabel_t *
operand_label (operand_t *op)
{
	operand_t  *o;
	symbol_t   *sym = 0;
	daglabel_t *label;

	if (!op)
		return 0;
	o = op;
	while (o->op_type == op_alias)
		o = o->o.alias;

	if (o->op_type == op_temp) {
		label = new_label ();
		label->op = op;
		o->o.tempop.daglabel = label;
	} else if (o->op_type == op_symbol) {
		sym = o->o.symbol;
		if (sym->daglabel)
			return sym->daglabel;
		label = new_label ();
		label->op = op;
		sym->daglabel = label;
	} else {
		if (o->op_type != op_value && o->op_type != op_pointer)
			debug (0, "unexpected operand type: %d", o->op_type);
		label = new_label ();
		label->op = op;
	}
	return label;
}

static dagnode_t *
leaf_node (operand_t *op)
{
	daglabel_t *label;
	dagnode_t  *node;

	if (!op)
		return 0;
	label = operand_label (op);
	node = new_node ();
	node->label = label;
	label->dagnode = node;
	return node;
}

static dagnode_t *
node (operand_t *op)
{
	operand_t  *o;
	symbol_t   *sym;

	if (!op)
		return 0;
	o = op;
	while (o->op_type == op_alias)
		o = o->o.alias;
	if (o->op_type == op_symbol) {
		sym = o->o.symbol;
		if (sym->sy_type == sy_const)
			return 0;
		if (sym->daglabel)
			return sym->daglabel->dagnode;
	} else if (o->op_type == op_temp) {
		if (o->o.tempop.daglabel)
			return o->o.tempop.daglabel->dagnode;
	}
	return 0;
}

static int
find_operands (statement_t *s, operand_t **x, operand_t **y, operand_t **z,
			   operand_t **w)
{
	int         simp = 0;

	if (s->opc) {
		*y = s->opa;
		if (s->opb) {
			// except for move, indexed pointer store, rcall2+, and state,
			// all are of the form c = a op b
			*z = s->opb;
			if (s->opcode[0] == '<' || s->opcode[0] == '.') {
				*w = s->opc;
			} else {
				*x = s->opc;
			}
		} else {
			// these are all c = op a
			*x = s->opc;
		}
	} else if (s->opb) {
		*y = s->opa;
		if (s->opcode[1] == 'I') {
			// conditional
		} else if (s->opcode[0] == '<' || s->opcode[0] == '.') {
			// pointer store or branch
			*z = s->opb;
		} else {
			// b = a
			*x = s->opb;
			simp = 1;
		}
	} else if (s->opa) {
		if (s->opcode[1] == 'G') {
		} else {
			*y = s->opa;
			if (s->opcode[1] == 'R')
				simp = 1;
		}
	}
	return simp;
}

static int
dagnode_match (const dagnode_t *n, const daglabel_t *op,
			   const dagnode_t *y, const dagnode_t *z, const dagnode_t *w)
{
	if (n->label->opcode != op->opcode)
		return 0;
	if (n->a && y && n->a->label->op != y->label->op)
		return 0;
	if (n->b && z && n->b->label->op != z->label->op)
		return 0;
	if (n->c && w && n->c->label->op != w->label->op)
		return 0;
	if ((!n->a) ^ (!y))
		return 0;
	if ((!n->c) ^ (!z))
		return 0;
	if ((!n->b) ^ (!w))
		return 0;
	return 1;
}

dagnode_t *
make_dag (const sblock_t *block)
{
	statement_t *s;
	dagnode_t   *dagnodes = 0;
	dagnode_t   *dag = 0;

	for (s = block->statements; s; s = s->next) {
		operand_t  *x = 0, *y = 0, *z = 0, *w = 0;
		dagnode_t  *n = 0, *nx, *ny, *nz, *nw;
		daglabel_t *op;
		int         simp;

		simp = find_operands (s, &x, &y, &z, &w);
		if (!(ny = node (y)))
			ny = leaf_node (y);
		if (!(nz = node (z)))
			nz = leaf_node (z);
		if (!(nw = node (w)))
			nw = leaf_node (w);
		op = opcode_label (s->opcode);
		if (simp) {
			n = ny;
		} else {
			for (n = dagnodes; n; n = n->next)
				if (dagnode_match (n, op, ny, nz, nw))
					break;
		}
		if (!n) {
			n = new_node ();
			n->label = op;
			n->a = ny;
			n->b = nz;
			n->c = nw;
			n->next = dagnodes;
			dagnodes = n;
		}
		if ((nx = node (x))) {
			daglabel_t **l;
			for (l = &nx->identifiers; *l; l = &(*l)->next) {
				if (*l == nx->label) {
					*l = (*l)->next;
					nx->label->next = 0;
					break;
				}
			}
		} else {
			nx = leaf_node (x);
		}
		if (nx) {
			nx->label->next = n->identifiers;
			n->identifiers = nx->label;
			nx->label->dagnode = n;
		}
		dag = n;
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
	return dag;
}
