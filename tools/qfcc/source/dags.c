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
#include "type.h"

static daglabel_t *free_labels;
static dagnode_t *free_nodes;
static dag_t *free_dags;

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
			else if (op->op_type == op_label)
				op->o.label->daglabel = 0;
			else
				internal_error (0, "unexpected operand type");
		}
		daglabel_chain = daglabel_chain->daglabel_chain;
	}
}

static dag_t *
new_dag (void)
{
	dag_t      *dag;
	ALLOC (256, dag_t, dags, dag);
	return dag;
}

static daglabel_t *
new_label (dag_t *dag)
{
	daglabel_t *label;
	ALLOC (256, daglabel_t, labels, label);
	label->daglabel_chain = daglabel_chain;
	daglabel_chain = label;
	label->number = dag->num_labels;
	dag->labels[dag->num_labels++] = label;
	return label;
}

static dagnode_t *
new_node (dag_t *dag)
{
	dagnode_t *node;
	ALLOC (256, dagnode_t, nodes, node);
	node->parents = set_new ();
	node->edges = set_new ();
	node->identifiers = set_new ();
	node->number = dag->num_nodes;
	dag->nodes[dag->num_nodes++] = node;
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
opcode_label (dag_t *dag, const char *opcode, expr_t *expr)
{
	daglabel_t *label;

	label = new_label (dag);
	label->opcode = opcode;
	label->expr = expr;
	return label;
}

static daglabel_t *
operand_label (dag_t *dag, operand_t *op)
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
		label = new_label (dag);
		label->op = op;
		op->o.tempop.daglabel = label;
	} else if (op->op_type == op_symbol) {
		sym = op->o.symbol;
		if (sym->daglabel)
			return sym->daglabel;
		label = new_label (dag);
		label->op = op;
		sym->daglabel = label;
	} else if (op->op_type == op_value || op->op_type == op_pointer) {
		val = op->o.value;
		if (val->daglabel)
			return val->daglabel;
		label = new_label (dag);
		label->op = op;
		val->daglabel = label;
	} else if (op->op_type == op_label) {
		if (op->o.label->daglabel)
			return op->o.label->daglabel;
		label = new_label (dag);
		label->op = op;
		op->o.label->daglabel = label;
	} else {
		internal_error (0, "unexpected operand type: %d", op->op_type);
	}
	return label;
}

static dagnode_t *
leaf_node (dag_t *dag, operand_t *op, expr_t *expr)
{
	daglabel_t *label;
	dagnode_t  *node;

	if (!op)
		return 0;
	node = new_node (dag);
	node->tl = op->type;
	label = operand_label (dag, op);
	label->dagnode = node;
	label->expr = expr;
	node->label = label;
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
			   dagnode_t *operands[3])
{
	int         i;

	if (n->label->opcode != op->opcode)
		return 0;
	for (i = 0; i < 3; i++) {
		if (n->children[i] && operands[i]
			&& n->children[i]->label->op != operands[i]->label->op )
			return 0;
		if ((!n->children[i]) ^ (!operands[i]))
			return 0;
	}
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
	if (l->dagnode) {
		dagnode_t  *node = l->dagnode;
		set_union (n->edges, node->parents);
		set_remove (n->edges, n->number);
		set_remove (node->identifiers, l->number);
	}
	l->dagnode = n;
	set_add (n->identifiers, l->number);
}

static void
dag_remove_dead_vars (dag_t *dag, set_t *live_vars)
{
	int         i;

	for (i = 0; i < dag->num_labels; i++) {
		daglabel_t *l = dag->labels[i];
		flowvar_t  *var;

		if (!l->op || !l->dagnode)
			continue;
		var = flow_get_var (l->op);
		if (!var)
			continue;
		if (!set_is_member (live_vars, var->number))
			set_remove (l->dagnode->identifiers, l->number);
	}
}

static void
dag_sort_visit (dag_t *dag, set_t *visited, int node_index, int *topo)
{
	set_iter_t *node_iter;
	dagnode_t  *node;

	if (set_is_member (visited, node_index))
		return;
	set_add (visited, node_index);
	node = dag->nodes[node_index];
	for (node_iter = set_first (node->edges); node_iter;
		 node_iter = set_next (node_iter))
		dag_sort_visit (dag, visited, node_iter->member, topo);
	node->topo = *topo;
	dag->topo[(*topo)++] = node_index;
}

static void
dag_sort_nodes (dag_t *dag)
{
	set_iter_t *root_iter;
	set_t      *visited = set_new ();
	int         topo = 0;

	dag->topo = malloc (dag->num_nodes * sizeof (int));
	for (root_iter = set_first (dag->roots); root_iter;
		 root_iter = set_next (root_iter))
		dag_sort_visit (dag, visited, root_iter->member, &topo);
	set_delete (visited);
}

dag_t *
dag_create (flownode_t *flownode)
{
	dag_t      *dag;
	sblock_t   *block = flownode->sblock;
	statement_t *s;
	dagnode_t **nodes;
	daglabel_t **labels;
	int         num_statements = 0;
	set_t      *live_vars = set_new ();

	flush_daglabels ();

	for (s = block->statements; s; s = s->next)
		num_statements++;

	set_assign (live_vars, flownode->live_vars.out);

	dag = new_dag ();
	dag->flownode = flownode;
	// at most 4 per statement
	dag->nodes = alloca (num_statements * 4 * sizeof (dagnode_t));
	// at most 3 per statement
	dag->labels = alloca (num_statements * 3 * sizeof (daglabel_t));
	dag->roots = set_new ();

	for (s = block->statements; s; s = s->next) {
		operand_t  *operands[4];
		dagnode_t  *n = 0, *children[3] = {0, 0, 0};
		daglabel_t *op, *lx;
		int         i;

		flow_analyze_statement (s, 0, 0, 0, operands);
		for (i = 0; i < 3; i++) {
			if (!(children[i] = node (operands[i + 1]))) {
				children[i] = leaf_node (dag, operands[i + 1], s->expr);
				if (children[i])
					set_add (dag->roots, children[i]->number);
			}
		}
		op = opcode_label (dag, s->opcode, s->expr);
		if (s->type == st_assign) {
			n = children[0];
		} else {
			n = 0;
			for (i = 0; i < dag->num_nodes; i++) {
				if (dagnode_match (dag->nodes[i], op, children)) {
					n = dag->nodes[i];
					break;
				}
			}
		}
		if (!n) {
			n = new_node (dag);
			n->type = s->type;
			n->label = op;
			set_add (dag->roots, n->number);
			for (i = 0; i < 3; i++) {
				dagnode_t  *child = children[i];
				n->children[i] = child;
				if (child) {
					if (child->label->op) {
						dagnode_t  *node = child->label->dagnode;
						if (node != child && node != n)
							set_add (node->edges, n->number);
					}
					set_remove (dag->roots, child->number);
					if (n != child)
						set_add (n->edges, child->number);
					set_add (child->parents, n->number);
					n->types[i] = operands[i + 1]->type;
				}
			}
		}
		lx = operand_label (dag, operands[0]);
		if (lx) {
			lx->expr = s->expr;
			dagnode_attach_label (n, lx);
		}
		if (n->type == st_func
			&& (!strncmp (n->label->opcode, "<CALL", 5)
				|| !strncmp (n->label->opcode, "<RCALL", 6))) {
			int         start, end;

			if (!strncmp (n->label->opcode, "<CALL", 5)) {
				start = 0;
				end = 0;
				if (n->label->opcode[5] != '>')
					end = n->label->opcode[5] - '0';
			} else {
				start = 2;
				end = n->label->opcode[6] - '0';
			}
			while (start < end) {
				set_add (live_vars, start + 1);
				start++;
			}
		}
	}

	nodes = malloc (dag->num_nodes * sizeof (dagnode_t *));
	memcpy (nodes, dag->nodes, dag->num_nodes * sizeof (dagnode_t *));
	dag->nodes = nodes;
	labels = malloc (dag->num_labels * sizeof (daglabel_t *));
	memcpy (labels, dag->labels, dag->num_labels * sizeof (daglabel_t *));
	dag->labels = labels;

	dag_remove_dead_vars (dag, live_vars);
	dag_sort_nodes (dag);
	set_delete (live_vars);
	return dag;
}

static statement_t *
build_statement (const char *opcode, operand_t **operands, expr_t *expr)
{
	int         i;
	operand_t  *op;
	statement_t *st = new_statement (st_none, opcode, expr);

	for (i = 0; i < 3; i++) {
		if ((op = operands[i])) {
			while (op->op_type == op_alias)
				op = op->o.alias;
			if (op->op_type == op_temp)
				op->o.tempop.users++;
		}
	}
	st->opa = operands[0];
	st->opb = operands[1];
	st->opc = operands[2];
	return st;
}
#if 0
static void
dag_calc_node_costs (dagnode_t *dagnode)
{
	int         i;

	for (i = 0; i < 3; i++)
		if (dagnode->children[i])
			dag_calc_node_costs (dagnode->children[i]);

	// if dagnode->a is null, then this is a leaf (as b and c are guaranted to
	// be null)
	if (!dagnode->children[0]) {
		// Because qc vm statements don't mix source and destination operands,
		// leaves never need temporary variables.
		dagnode->cost = 0;
	} else {
		int         different = 0;

		// a non-leaf is guaranteed to have a valid first child
		dagnode->cost = dagnode->children[0]->cost;
		for (i = 1; i < 3; i++) {
			if (dagnode->children[i]
				&& dagnode->children[i]->cost != dagnode->cost) {
				dagnode->cost = max (dagnode->cost,
									 dagnode->children[i]->cost);
				different = 1;
			}
		}
		if (!different)
			dagnode->cost += 1;
	}
}
#endif
static operand_t *
fix_op_type (operand_t *op, etype_t type)
{
	if (op && op->op_type != op_label && op->type != type)
		op = alias_operand (op, type);
	return op;
}

static operand_t *
generate_assignments (dag_t *dag, sblock_t *block, operand_t *src,
					  set_iter_t *var_iter, etype_t type)
{
	statement_t *st;
	operand_t   *dst = 0;
	operand_t   *operands[3] = {0, 0, 0};
	daglabel_t  *var;

	operands[0] = fix_op_type (src, type);
	for ( ; var_iter; var_iter = set_next (var_iter)) {
		var = dag->labels[var_iter->member];
		operands[1] = fix_op_type (var->op, type);
		if (!dst) {
			dst = operands[1];
			while (dst->op_type == op_alias)
				dst = dst->o.alias;
		}

		st = build_statement ("=", operands, var->expr);
		sblock_add_statement (block, st);
	}
	return dst;
}

static operand_t *
make_operand (dag_t *dag, sblock_t *block, const dagnode_t *dagnode, int index)
{
	operand_t  *op;

	op = dagnode->children[index]->value;
	while (op->op_type == op_alias)
		op = op->o.alias;
	op = fix_op_type (op, dagnode->types[index]);
	return op;
}

static void
dag_gencode (dag_t *dag, sblock_t *block, dagnode_t *dagnode)
{
	operand_t  *operands[3] = {0, 0, 0};
	operand_t  *dst = 0;
	statement_t *st;
	set_iter_t *var_iter;
	int         i;
	etype_t     type;

	switch (dagnode->type) {
		case st_none:
			if (!dagnode->label->op)
				internal_error (0, "non-leaf label in leaf node");
			dst = dagnode->label->op;
			if ((var_iter = set_first (dagnode->identifiers))) {
				type = low_level_type (get_type (dagnode->label->expr));
				dst = generate_assignments (dag, block, dst, var_iter, type);
			}
			break;
		case st_expr:
			operands[0] = make_operand (dag, block, dagnode, 0);
			if (dagnode->children[1])
				operands[1] = make_operand (dag, block, dagnode, 1);
			type = low_level_type (get_type (dagnode->label->expr));
			if (!(var_iter = set_first (dagnode->identifiers))) {
				operands[2] = temp_operand (get_type (dagnode->label->expr));
			} else {
				daglabel_t *var = dag->labels[var_iter->member];

				operands[2] = fix_op_type (var->op, type);
				var_iter = set_next (var_iter);
			}
			dst = operands[2];
			st = build_statement (dagnode->label->opcode, operands,
								  dagnode->label->expr);
			sblock_add_statement (block, st);
			generate_assignments (dag, block, operands[2], var_iter, type);
			break;
		case st_assign:
			internal_error (0, "unexpected assignment node");
		case st_ptrassign:
			operands[0] = make_operand (dag, block, dagnode, 0);
			operands[1] = make_operand (dag, block, dagnode, 1);
			if (dagnode->children[2])
				operands[2] = make_operand (dag, block, dagnode, 2);
			st = build_statement (dagnode->label->opcode, operands,
								  dagnode->label->expr);
			sblock_add_statement (block, st);
			break;
		case st_move:
		case st_state:
		case st_func:
			for (i = 0; i < 3; i++)
				if (dagnode->children[i])
					operands[i] = make_operand (dag, block, dagnode, i);
			st = build_statement (dagnode->label->opcode, operands,
								  dagnode->label->expr);
			sblock_add_statement (block, st);
			break;
		case st_flow:
			operands[0] = make_operand (dag, block, dagnode, 0);
			if (dagnode->children[1])
				operands[1] = make_operand (dag, block, dagnode, 1);
			st = build_statement (dagnode->label->opcode, operands,
								  dagnode->label->expr);
			sblock_add_statement (block, st);
			break;
	}
	dagnode->value = dst;
}

void
dag_generate (dag_t *dag, sblock_t *block)
{
	int         i;

	for (i = 0; i < dag->num_nodes; i++)
		dag_gencode (dag, block, dag->nodes[dag->topo[i]]);
}
