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

#include "QF/dstring.h"
#include "QF/mathlib.h"

#include "dags.h"
#include "diagnostic.h"
#include "flow.h"
#include "qfcc.h"
#include "set.h"
#include "statements.h"
#include "strpool.h"
#include "symtab.h"

static daglabel_t *free_labels;
static dagnode_t *free_nodes;

static daglabel_t *daglabel_chain;

static void
flush_daglabels (void)
{
	while (daglabel_chain) {
		operand_t  *op;

		if ((op = daglabel_chain->op)) {
			while (op->op_type == op_alias)
				op = op->o.alias;
			if (op->op_type == op_symbol)
				op->o.symbol->daglabel = 0;
			else if (op->op_type == op_temp)
				op->o.tempop.daglabel = 0;
			else if (op->op_type == op_value || op->op_type == op_pointer)
				op->o.value->daglabel = 0;
			else
				internal_error (0, "unexpected operand type");
		}
		daglabel_chain = daglabel_chain->daglabel_chain;
	}
}

static daglabel_t *
new_label (void)
{
	daglabel_t *label;
	ALLOC (256, daglabel_t, labels, label);
	label->daglabel_chain = daglabel_chain;
	daglabel_chain = label;
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
	static dstring_t *str;
	if ((label->opcode && label->op) || (!label->opcode && !label->op))
		return "bad label";
	if (label->opcode)
		return label->opcode;
	if (!str)
		str = dstring_new ();
	// operand_string might use quote_string, which returns a pointer to
	// a static variable.
	dstring_copystr (str, operand_string (label->op));
	return quote_string (str->str);
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
	symbol_t   *sym = 0;
	ex_value_t *val = 0;
	daglabel_t *label;

	if (!op)
		return 0;
	while (op->op_type == op_alias)
		op = op->o.alias;

	if (op->op_type == op_temp) {
		if (op->o.tempop.daglabel)
			return op->o.tempop.daglabel;
		label = new_label ();
		label->op = op;
		op->o.tempop.daglabel = label;
	} else if (op->op_type == op_symbol) {
		sym = op->o.symbol;
		if (sym->daglabel)
			return sym->daglabel;
		label = new_label ();
		label->op = op;
		sym->daglabel = label;
	} else if (op->op_type == op_value || op->op_type == op_pointer) {
		val = op->o.value;
		if (val->daglabel)
			return val->daglabel;
		label = new_label ();
		label->op = op;
		val->daglabel = label;
	} else {
		internal_error (0, "unexpected operand type: %d", op->op_type);
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
	symbol_t   *sym;

	if (!op)
		return 0;
	while (op->op_type == op_alias)
		op = op->o.alias;
	if (op->op_type == op_symbol) {
		sym = op->o.symbol;
		if (sym->sy_type == sy_const)
			return 0;
		if (sym->daglabel)
			return sym->daglabel->dagnode;
	} else if (op->op_type == op_temp) {
		if (op->o.tempop.daglabel)
			return op->o.tempop.daglabel->dagnode;
	}
	return 0;
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

static int
op_is_identifer (operand_t *op)
{
	while (op->op_type == op_alias)
		op = op->o.alias;
	if (op->op_type == op_pointer)
		return 1;
	if (op->op_type == op_temp)
		return 1;
	if (op->op_type != op_symbol)
		return 0;
	if (op->o.symbol->sy_type != sy_var)
		return 0;
	return 1;
}

static void
dagnode_attach_label (dagnode_t *n, daglabel_t *l)
{
	if (!l->op)
		internal_error (0, "attempt to attach operator label to dagnode "
						"identifers");
	if (!op_is_identifer (l->op))
		internal_error (0, "attempt to attach non-identifer label to dagnode "
						"identifers");
	if (n->identifiers)
		n->identifiers->prev = &l->next;
	l->next = n->identifiers;
	l->prev = &n->identifiers;
	l->dagnode = n;
	n->identifiers = l;
}

static void
daglabel_detatch (daglabel_t *l)
{
	if (l->next)
		l->next->prev = l->prev;
	*l->prev = l->next;
	l->dagnode = 0;
}

static statement_t *
build_statement (const char *opcode, operand_t *a, operand_t *b, operand_t *c,
				 expr_t *expr)
{
	if ((!a && (b || c)) || (a && !b && c))
		internal_error (0, "invalid operand combo");
	statement_t *st = new_statement (opcode, expr);
	st->opa = a;
	st->opb = b;
	st->opc = c;
	return st;
}

dagnode_t *
dag_create (const flownode_t *flownode)
{
	sblock_t    *block = flownode->sblock;
	statement_t *s;
	dagnode_t   *dagnodes = 0;
	dagnode_t  **dagtail = &dagnodes;
	dagnode_t   *d;

	flush_daglabels ();

	for (s = block->statements; s; s = s->next) {
		operand_t  *x = 0, *y = 0, *z = 0, *w = 0;
		dagnode_t  *n = 0, *ny, *nz, *nw;
		daglabel_t *op, *lx;
		int         simp;

		simp = find_operands (s, &x, &y, &z, &w);
		if (!(ny = node (y))) {
			ny = leaf_node (y);
			if (simp) {
				*dagtail = ny;
				dagtail = &ny->next;
			}
		}
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
			n->statement = s;
			n->label = op;
			n->a = ny;
			n->b = nz;
			n->c = nw;
			if (ny) {
				ny->is_child = 1;
				n->ta = y->type;
			}
			if (nz) {
				nz->is_child = 1;
				n->tb = z->type;
			}
			if (nw) {
				nw->is_child = 1;
				n->tc = w->type;
			}
			*dagtail = n;
			dagtail = &n->next;
		}
		lx = operand_label (x);
		if (lx) {
			if (lx->prev)
				daglabel_detatch (lx);
			dagnode_attach_label (n, lx);
		}
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
	for (d = dagnodes; d; d = d->next) {
		daglabel_t **l = &d->identifiers;

		while (*l) {
			if ((*l)->op->op_type == op_temp
				&& !set_is_member (flownode->live_vars.out,
								   flow_get_var ((*l)->op)->number))
				daglabel_detatch (*l);
			else
				l = &(*l)->next;
		}
	}
	while (dagnodes->is_child) {
		dagnode_t  *n = dagnodes->next;
		dagnodes->next = 0;
		dagnodes = n;
	}
	for (d = dagnodes; d && d->next; d = d->next) {
		while (d->next && d->next->is_child) {
			dagnode_t  *n = d->next->next;
			d->next->next = 0;
			d->next = n;
		}
	}
	return dagnodes;
}

static void
dag_calc_node_costs (dagnode_t *dagnode)
{
	if ((!dagnode->a && (dagnode->b || dagnode->c))
		|| (dagnode->a && !dagnode->b && dagnode->c))
		internal_error (0, "bad dag node");

	if (dagnode->a)
		dag_calc_node_costs (dagnode->a);
	if (dagnode->b)
		dag_calc_node_costs (dagnode->b);
	if (dagnode->c)
		dag_calc_node_costs (dagnode->c);

	// if dagnode->a is null, then this is a leaf (as b and c are guaranted to
	// be null)
	if (!dagnode->a) {
		// Because qc vm statements don't mix source and destination operands,
		// leaves never need temporary variables.
		dagnode->cost = 0;
	} else {
		int         different = 0;

		// a non-leaf is guaranteed to have a valid "a"
		dagnode->cost = dagnode->a->cost;
		if (dagnode->b && dagnode->b->cost != dagnode->cost) {
			dagnode->cost = max (dagnode->cost, dagnode->b->cost);
			different = 1;
		}
		if (dagnode->c && (different || dagnode->c->cost != dagnode->cost)) {
			dagnode->cost = max (dagnode->cost, dagnode->c->cost);
			different = 1;
		}
		if (!different)
			dagnode->cost += 1;
	}
}

static operand_t *
fix_op_type (operand_t *op, etype_t type)
{
	if (op && op->op_type != op_label && op->type != type)
		op = alias_operand (op, type);
	return op;
}

static operand_t *
dag_gencode (sblock_t *block, const dagnode_t *dagnode)
{
	if (!dagnode->a) {
		statement_t *st;
		daglabel_t *var = 0;
		operand_t *op = dagnode->label->op;

		if (dagnode->identifiers) {
			var = dagnode->identifiers;
		}
		for (var = dagnode->identifiers; var; var = var->next) {
			st = build_statement ("=", op, var->op, 0, 0);
			sblock_add_statement (block, st);
		}
		return op;
	} else {
		statement_t *st;
		daglabel_t *var = 0;
		operand_t  *op_a = 0;
		operand_t  *op_b = 0;
		operand_t  *op_c = 0;

		if (flow_is_cond (dagnode->statement))
			op_b = dagnode->statement->opb;

		if (dagnode->b && dagnode->c) {
			op_a = dag_gencode (block, dagnode->a);
			op_b = dag_gencode (block, dagnode->b);
			op_c = dag_gencode (block, dagnode->c);
		} else if (dagnode->b) {
			if (dagnode->a->cost < dagnode->b->cost) {
				op_b = dag_gencode (block, dagnode->b);
				op_a = dag_gencode (block, dagnode->a);
			} else {
				op_a = dag_gencode (block, dagnode->a);
				op_b = dag_gencode (block, dagnode->b);
			}
		} else {
			op_a = dag_gencode (block, dagnode->a);
		}
		if (op_a && op_b && get_type (dagnode->statement->expr)) {
			if (dagnode->identifiers) {
				op_c = dagnode->identifiers->op;
				var = dagnode->identifiers->next;
			} else {
				op_c = temp_operand (get_type (dagnode->statement->expr));
			}
		}
		op_a = fix_op_type (op_a, dagnode->ta);
		op_b = fix_op_type (op_b, dagnode->tb);
		op_c = fix_op_type (op_c, dagnode->tc);
		st = build_statement (dagnode->label->opcode, op_a, op_b, op_c,
							  dagnode->statement->expr);
		sblock_add_statement (block, st);
		while (var) {
			st = build_statement ("=", op_c, var->op, 0, 0);
			sblock_add_statement (block, st);
			var = var->next;
		}
		return op_c;
	}
	return 0;
}

void
dag_generate (sblock_t *block, const flownode_t *flownode)
{
	const dagnode_t *dag;

	dag_calc_node_costs (flownode->dag);
	for (dag = flownode->dag; dag; dag = dag->next) {
		if (!dag->a || (strcmp (dag->label->opcode, ".=") && !dag->identifiers))
			continue;
		dag_gencode (block, dag);
	}
}
