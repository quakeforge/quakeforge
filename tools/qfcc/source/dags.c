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
#include <ctype.h>

#include "qfalloca.h"

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/set.h"

#include "tools/qfcc/include/dags.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/dot.h"
#include "tools/qfcc/include/flow.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

ALLOC_STATE (daglabel_t, labels);
ALLOC_STATE (dagnode_t, nodes);
ALLOC_STATE (dag_t, dags);

static daglabel_t *daglabel_chain;

static void dagnode_set_edges (dag_t *dag, dagnode_t *n, statement_t *s);
static void dag_live_aliases(operand_t *op);

static int
op_is_identifier (operand_t *op)
{
	if (!op)
		return 0;
	if (op->op_type == op_label)
		return 0;
	if (op->op_type == op_value)
		return 0;
	if (op->op_type == op_temp)
		return 1;
	if (op->op_type != op_def)
		return 0;
	return 1;
}

static int
op_is_constant (operand_t *op)
{
	if (!op)
		return 0;
	if (op->op_type == op_label)
		return 1;
	if (op->op_type == op_value)
		return 1;
	if (op->op_type == op_label)
		return op->def->constant;
	return 0;
}

static int
op_is_temp (operand_t *op)
{
	if (!op)
		return 0;
	return op->op_type == op_temp;
}

static bool
op_is_alias (operand_t *op)
{
	if (!op) {
		return false;
	}
	if (op->op_type == op_alias) {
		return true;
	}
	if (op->op_type == op_temp) {
		return !!op->tempop.alias;
	}
	if (op->op_type == op_def) {
		return !!op->def->alias;
	}
	return false;
}

static int __attribute__((pure))
op_alias_offset (operand_t *op)
{
	if (!op_is_alias (op)) {
		internal_error (op->expr, "not an alias op");
	}
	if (op->op_type == op_temp) {
		return op->tempop.offset;
	}
	if (op->op_type == op_def) {
		return op->def->offset;
	}
	internal_error (op->expr, "eh? how?");
}

static operand_t * __attribute__((pure))
unalias_op (operand_t *op)
{
	if (!op_is_alias (op)) {
		internal_error (op->expr, "not an alias op");
	}
	if (op->op_type == op_alias) {
		return op->alias;
	}
	if (op->op_type == op_temp) {
		return op->tempop.alias;
	}
	if (op->op_type == op_def) {
		auto def = op->def;
		return def_operand (def->alias, def->alias->type, op->expr);
	}
	internal_error (op->expr, "eh? how?");
}

static void
flush_daglabels (void)
{
	while (daglabel_chain) {
		operand_t  *op;

		if ((op = daglabel_chain->op)) {
			if (op->op_type == op_def)
				op->def->daglabel = 0;
			else if (op->op_type == op_temp)
				op->tempop.daglabel = 0;
			else if (op->op_type == op_value)
				op->value->daglabel = 0;
			else if (op->op_type == op_label)
				op->label->daglabel = 0;
			else
				internal_error (op->expr, "unexpected operand type");
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
	node->reachable = set_new ();
	node->number = dag->num_nodes;
	set_add (dag->roots, node->number);	// nodes start out as root nodes
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
#if 0
	if (label->op->type) {
		dstring_appendstr (str, label->op->type->encoding);
	}
#endif
	return quote_string (str->str);
}

static daglabel_t *
opcode_label (dag_t *dag, const char *opcode, const expr_t *expr)
{
	daglabel_t *label;

	label = new_label (dag);
	label->opcode = opcode;
	label->expr = expr;
	return label;
}

static __attribute__((pure)) dagnode_t *
dag_node (operand_t *op)
{
	def_t      *def;
	dagnode_t  *node = 0;

	if (!op)
		return 0;
	if (op->op_type == op_def) {
		def = op->def;
		if (def->daglabel)
			node = def->daglabel->dagnode;
	} else if (op->op_type == op_temp) {
		if (op->tempop.daglabel)
			node = op->tempop.daglabel->dagnode;
	} else if (op->op_type == op_value) {
		if (op->value->daglabel)
			node = op->value->daglabel->dagnode;
	} else if (op->op_type == op_label) {
		if (op->label->daglabel)
			node = op->label->daglabel->dagnode;
	}
	return node;
}

static dagnode_t *leaf_node (dag_t *dag, operand_t *op, const expr_t *expr);

static daglabel_t *
operand_label (dag_t *dag, operand_t *op)
{
	def_t      *def = 0;
	ex_value_t *val = 0;
	daglabel_t *label;

	if (!op)
		return 0;

	if (op->op_type == op_temp) {
		//while (op->tempop.alias)
		//	op = op->tempop.alias;
		if (op->tempop.daglabel)
			return op->tempop.daglabel;
		label = new_label (dag);
		label->op = op;
		op->tempop.daglabel = label;
		if (op->tempop.alias) {
			if (!dag_node (op->tempop.alias)) {
				leaf_node (dag, op->tempop.alias, op->expr);
			}
		}
	} else if (op->op_type == op_def) {
		def = op->def;
		if (def->daglabel)
			return def->daglabel;
		label = new_label (dag);
		label->op = op;
		def->daglabel = label;
	} else if (op->op_type == op_value) {
		val = op->value;
		if (val->daglabel)
			return val->daglabel;
		label = new_label (dag);
		label->op = op;
		val->daglabel = label;
	} else if (op->op_type == op_label) {
		if (op->label->daglabel)
			return op->label->daglabel;
		label = new_label (dag);
		label->op = op;
		op->label->daglabel = label;
	} else {
		internal_error (op->expr, "unexpected operand type: %s", op_type_names[op->op_type]);
	}
	return label;
}

static dagnode_t *
leaf_node (dag_t *dag, operand_t *op, const expr_t *expr)
{
	daglabel_t *label;
	dagnode_t  *node;

	if (!op)
		return 0;
	node = new_node (dag);
	node->vtype = op->type;
	label = operand_label (dag, op);
	label->dagnode = node;
	label->expr = expr;
	node->label = label;
	return node;
}

static int
dagnode_deref_match (const dagnode_t *n, const dagnode_t *search)
{
	int         i;
	auto children = search->children;

	for (i = 0; i < 2; i++) {
		if (n->children[i] != children[i + 1])
			return 0;
	}
	return 1;
}

static int
dagnode_match (const dagnode_t *n, const dagnode_t *search)
{
	int         i;
	auto op = search->label;

	if (n->killed)
		return 0;
	if (!strcmp (op->opcode, "load")
		&& n->label->opcode && !strcmp (n->label->opcode, "store"))
		return dagnode_deref_match (n, search);
	if (n->label->opcode != op->opcode)
		return 0;
	for (i = 0; i < 3; i++) {
		if (n->children[i] != search->children[i]) {
			return 0;
		}
		if (n->type == st_alias) {
			if (n->types[i] != search->types[i]) {
				return 0;
			}
			if (n->offset != search->offset) {
				return 0;
			}
		}
	}
	return 1;
}

static dagnode_t *
dagnode_search (dag_t *dag, const dagnode_t *search)
{
	int         i;

	for (i = 0; i < dag->num_nodes; i++)
		if (dagnode_match (dag->nodes[i], search))
			return dag->nodes[i];
	return 0;
}

static void
dagnode_set_reachable (dag_t *dag, dagnode_t *node)
{
	for (set_iter_t *edge_iter = set_first (node->edges); edge_iter;
		 edge_iter = set_next (edge_iter)) {
		dagnode_t  *r = dag->nodes[edge_iter->element];
		// The other node is directly reachable
		set_add (node->reachable, r->number);
		// All nodes reachable by the other node are indirectly reachable
		// from this node.
		set_union (node->reachable, r->reachable);
	}
}

static dagnode_t *
dag_make_child (dag_t *dag, operand_t *op, statement_t *s)
{
	dagnode_t  *node = dag_node (op);
	dagnode_t  *killer = 0;

	if (node && (node->killed || s->type == st_address)) {
		// If the node has been killed, then a new node is needed
		// taking the address of a variable effectively kills the node it's
		// attached to. FIXME should this be for only when the variable is
		// in the attached identifiers list and is not the node's label?
		killer = node->killed;
		node = 0;
	}

	if (!node && op_is_temp (op) && op_is_alias (op)) {
		operand_t  *uop = unalias_op (op);
		if (uop != op) {
			if (!(node = dag_node (uop))) {
				node = leaf_node (dag, uop, s->expr);
			}
			node->label->live = 1;
			dagnode_t *n;
			dagnode_t search = {
				.type = st_alias,
				.label = opcode_label (dag, save_string ("alias"), s->expr),
				.children = { node },
				.types = { op->type },
				.offset = op_alias_offset (op),
			};
			if (!(n = dagnode_search (dag, &search))) {
				n = new_node (dag);
				n->type = st_alias;
				n->label = search.label;
				n->children[0] = node;
				n->types[0] = op->type;
				n->offset = search.offset;
				set_remove (dag->roots, node->number);
				set_add (node->parents, n->number);
				dagnode_set_edges (dag, n, s);
				dagnode_set_reachable (dag, n);
			}
			node = n;
		}
	}

	if (!node) {
		// No valid node found (either first reference to the value,
		// or the value's node was killed).
		node = leaf_node (dag, op, s->expr);
		if (node && dag->killer_node >= 0) {
			set_add (node->edges, dag->killer_node);
		}
	}
	if (killer) {
		// When an operand refers to a killed node, it must be
		// evaluated AFTER the killing node has been evaluated.
		set_add (node->edges, killer->number);
		// If killer is set, then node is guaranteed to be a new node
		// and thus does not have any parents, so no need to worry about
		// updating the reachable sets of any parent nodes.
		dagnode_set_reachable (dag, node);
	}
	return node;
}

static void
dag_make_children (dag_t *dag, statement_t *s,
				   operand_t *operands[FLOW_OPERANDS],
				   dagnode_t *children[3])
{
	int         i;

	flow_analyze_statement (s, 0, 0, 0, operands);
	if (!operands[0] && s->def) {
		auto op = s->def;
		while (op && op->op_type == op_pseudo) {
			op = op->next;
		}
		//FIXME hopefully only one non-pseudo op
		operands[0] = op;
	}
	for (i = 0; i < 3; i++) {
		operand_t  *op = operands[i + 1];
		children[i] = dag_make_child (dag, op, s);
	}
}

static void
dagnode_add_children (dag_t *dag, dagnode_t *n, operand_t *operands[4],
					  dagnode_t *children[3])
{
	int         i;

	for (i = 0; i < 3; i++) {
		dagnode_t  *child = children[i];
		if ((n->children[i] = child)) {
			n->types[i] = operands[i + 1]->type;
			set_remove (dag->roots, child->number);
			set_add (child->parents, n->number);
		}
	}
	if (operands[0]) {
		n->vtype = operands[0]->type;
	}
}

static int
dagnode_tempop_set_edges_visit (tempop_t *tempop, void *_node)
{
	dagnode_t  *node = (dagnode_t *) _node;
	daglabel_t *label;

	label = tempop->daglabel;
	if (label && label->dagnode) {
		set_add (node->edges, label->dagnode->number);
		label->live = 1;
	}
	return 0;
}

static int
dagnode_def_set_edges_visit (def_t *def, void *_node)
{
	dagnode_t  *node = (dagnode_t *) _node;
	daglabel_t *label;

	label = def->daglabel;
	if (label && label->dagnode) {
		set_add (node->edges, label->dagnode->number);
		label->live = 1;
	}
	return 0;
}

static int
dag_find_node (def_t *def, void *_daglabel)
{
	daglabel_t **daglabel = (daglabel_t **) _daglabel;
	if (def->daglabel && def->daglabel->dagnode) {
		*daglabel = def->daglabel;
		return def->daglabel->dagnode->number + 1;	// ensure never 0
	}
	return 0;
}

static void
dagnode_set_edges (dag_t *dag, dagnode_t *n, statement_t *s)
{
	int         i;

	for (i = 0; i < 3; i++) {
		dagnode_t  *child = n->children[i];
		if (child && n != child)
			set_add (n->edges, child->number);
	}
	if (n->type == st_flow)
		return;
	for (i = 0; i < 3; i++) {
		dagnode_t  *child = n->children[i];
		if (child) {
			if (child->label->op) {
				dagnode_t  *node = child->label->dagnode;
				operand_t  *op = child->label->op;
				if (node != child && node != n) {
					set_add (node->edges, n->number);
				}
				if (op->op_type == op_value
					&& op->value->lltype == ev_ptr
					&& op->value->v.pointer.def) {
					def_visit_all (op->value->v.pointer.def, 1,
								   dagnode_def_set_edges_visit, n);
				}
				if (op->op_type == op_def
					&& (op->def->alias || op->def->alias_defs)) {
					def_visit_all (op->def, 1,
								   dagnode_def_set_edges_visit, n);
				}
				if (op->op_type == op_temp
					&& (op->tempop.alias || op->tempop.alias_ops)) {
					tempop_visit_all (&op->tempop, 1,
									  dagnode_tempop_set_edges_visit, n);
				}
			}
			if (n != child)
				set_add (n->edges, child->number);
		}
	}
	for (operand_t *use = s->use; use; use = use->next) {
		if (use->op_type == op_pseudo) {
			continue;
		}
		auto u = dag_make_child (dag, use, s);
		u->label->live = 1;
		dag_live_aliases (use);
		set_add (n->edges, u->number);
	}
	if (n->type == st_func) {
		const char *num_params = 0;
		int         first_param = 0;
		function_t *func = dag->flownode->graph->func;
		flowvar_t **flowvars = func->vars;

		if (!strcmp (n->label->opcode, "call")) {
			// nothing to do
		} else if (!strncmp (n->label->opcode, "rcall", 5)) {
			num_params = n->label->opcode + 6;
			first_param = 2;
		} else if (!strncmp (n->label->opcode, "call", 4)) {
			num_params = n->label->opcode + 5;
		} else if (!strcmp (n->label->opcode, "return") && n->children[0]) {
			daglabel_t *label = n->children[0]->label;
			if (!label->op) {
				set_iter_t *lab_i;
				for (lab_i = set_first (n->children[0]->identifiers); lab_i;
					 lab_i = set_next (lab_i)) {
					label = dag->labels[lab_i->element];
				}
			}
			label->live = 1;
		}
		for (int i = 0; i < s->num_use; i++) {
			udchain_t   ud = func->ud_chains[s->first_use + i];
			flowvar_t  *var = func->vars[ud.var];
			if (var->op->op_type == op_pseudo) {
				continue;
			}
			daglabel_t *l = operand_label (dag, var->op);
			if (l) {
				l->live = 1;
			}
		}
		// ensure all operations on global variables are completed before
		// the st_func statement executes
		for (set_iter_t *g = set_first (func->global_vars); g;
			 g = set_next (g)) {
			flowvar_t  *var = flowvars[g->element];
			dagnode_t  *gn = dag_node (var->op);
			if (gn) {
				set_add (n->edges, gn->number);
				set_remove (gn->edges, n->number);
			}
		}
		if (num_params && isdigit ((byte) *num_params)) {
			for (i = first_param; i < *num_params - '0'; i++) {
				flowvar_t  *var = flowvars[i + 1];
				def_t      *param_def = var->op->def;
				daglabel_t *daglabel;
				int         param_node;

				// FIXME hopefully only the one alias :P
				param_node = def_visit_all (param_def, 0, dag_find_node,
											&daglabel);
				if (!param_node) {
					bug (n->label->expr, ".param_%d not set for %s", i,
						 n->label->opcode);
					continue;
				}
				daglabel->live = 1;
				set_add (n->edges, param_node - 1);
			}
		}
	}
}

static int
dag_tempop_kill_aliases_visit (tempop_t *tempop, void *_l)
{
	daglabel_t *l = (daglabel_t *) _l;
	dagnode_t  *node = l->dagnode;
	daglabel_t *label;

	if (tempop == &l->op->tempop)
		return 0;
	label = tempop->daglabel;
	if (label && label->dagnode && !label->dagnode->killed) {
		set_add (node->edges, label->dagnode->number);
		set_remove (node->edges, node->number);
		label->dagnode->killed = node;
	}
	return 0;
}

static int
dag_def_kill_aliases_visit (def_t *def, void *_l)
{
	daglabel_t *l = (daglabel_t *) _l;
	dagnode_t  *node = l->dagnode;
	daglabel_t *label;

	if (def == l->op->def)
		return 0;
	label = def->daglabel;
	if (label && label->dagnode && !label->dagnode->killed) {
		set_add (node->edges, label->dagnode->number);
		set_remove (node->edges, node->number);
		label->dagnode->killed = node;
	}
	return 0;
}

static void
dag_kill_aliases (daglabel_t *l)
{
	operand_t  *op = l->op;

	if (op->op_type == op_temp) {
		if (op->tempop.alias || op->tempop.alias_ops) {
			tempop_visit_all (&op->tempop, 1,
							  dag_tempop_kill_aliases_visit, l);
		}
	} else if (op->op_type == op_def) {
		if (op->def->alias || op->def->alias_defs) {
			def_visit_all (op->def, 1, dag_def_kill_aliases_visit, l);
		}
	} else {
		internal_error (op->expr, "rvalue assignment?");
	}
}

static int
dag_tempop_live_aliases (tempop_t *tempop, void *_t)
{

	if (tempop != _t && tempop->daglabel)
		tempop->daglabel->live = 1;
	return 0;
}

static int
dag_def_live_aliases (def_t *def, void *_d)
{

	if (def != _d && def->daglabel)
		def->daglabel->live = 1;
	return 0;
}

static void
dag_live_aliases(operand_t *op)
{
	// FIXME it would be better to propogate the aliasing
	if (op->op_type == op_temp
		&& (op->tempop.alias || op->tempop.alias_ops)) {
		tempop_visit_all (&op->tempop, 1, dag_tempop_live_aliases,
						  &op->tempop);
	}
	if (op->op_type == op_def
		&& (op->def->alias || op->def->alias_defs)) {
		def_visit_all (op->def, 1, dag_def_live_aliases, op->def);
	}
}

static int
dagnode_attach_label (dag_t *dag, dagnode_t *n, daglabel_t *l)
{
	if (!l->op)
		internal_error (0, "attempt to attach operator label to dagnode "
						"identifiers");
	if (!op_is_identifier (l->op))
		internal_error (l->op->expr,
						"attempt to attach non-identifer label to dagnode "
						"identifiers");
	if (l->dagnode) {
		dagnode_t  *node = l->dagnode;
		set_remove (node->identifiers, l->number);

		// If the target node (n) is reachable by the label's node or its
		// parents, then attaching the label's node to the target node would
		// cause the label's node to be written before it used.
		set_t      *reachable = set_new ();
		set_assign (reachable, node->reachable);
		for (set_iter_t *node_iter = set_first (node->parents); node_iter;
			 node_iter = set_next (node_iter)) {
			dagnode_t  *p = dag->nodes[node_iter->element];
			set_union (reachable, p->reachable);
		}
		int         is_reachable = set_is_member (reachable, n->number);
		set_delete (reachable);
		if (is_reachable) {
			return 0;
		}

		// this assignment to the variable must come after any previous uses,
		// which includes itself and its parents
		set_add (n->edges, node->number);
		set_union (n->edges, node->parents);
		// nodes never need edges to themselves, but n might be one of node's
		// parents
		set_remove (n->edges, n->number);
		dagnode_set_reachable (dag, n);
	}
	l->live = 0;	// remove live forcing on assignment
	l->dagnode = n;
	set_add (n->identifiers, l->number);
	dag_kill_aliases (l);
	if (n->label->op) {
		dag_live_aliases (n->label->op);
	}
	return 1;
}

static int
dag_tempop_alias_live (tempop_t *tempop, void *_live_vars)
{
	set_t      *live_vars = (set_t *) _live_vars;
	if (!tempop->flowvar)
		return 0;
	return set_is_member (live_vars, tempop->flowvar->number);
}

static int
dag_def_alias_live (def_t *def, void *_live_vars)
{
	set_t      *live_vars = (set_t *) _live_vars;
	if (!def->flowvar)
		return 0;
	return set_is_member (live_vars, def->flowvar->number);
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
		if (l->live)		// label forced live (probably via an alias)
			continue;
		var = flow_get_var (l->op);
		if (!var)
			continue;
		if (set_is_member (dag->flownode->global_vars, var->number))
			continue;
		if (l->op->op_type == op_def
			&& def_visit_all (l->op->def, 1, dag_def_alias_live, live_vars))
			continue;
		if (l->op->op_type == op_temp
			&& tempop_visit_all (&l->op->tempop, 1, dag_tempop_alias_live,
								 live_vars))
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
		dag_sort_visit (dag, visited, node_iter->element, topo);
	node->topo = *topo;
	dag->topo[(*topo)++] = node_index;
}

static void
dag_sort_nodes (dag_t *dag)
{
	set_iter_t *root_iter;
	set_t      *visited = set_new ();
	int         topo = 0;
	int        *tmp_topo;

	if (dag->topo)
		free (dag->topo);
	dag->topo = alloca (dag->num_nodes * sizeof (int));
	for (root_iter = set_first (dag->roots); root_iter;
		 root_iter = set_next (root_iter))
		dag_sort_visit (dag, visited, root_iter->element, &topo);
	set_delete (visited);
	tmp_topo = malloc (topo * sizeof (int));
	memcpy (tmp_topo, dag->topo, topo * sizeof (int));
	dag->topo = tmp_topo;
	dag->num_topo = topo;
}

static void
dag_kill_nodes (dag_t *dag, dagnode_t *n, bool is_call)
{
	int         i;
	dagnode_t  *node;

	for (i = 0; i < dag->num_nodes; i++) {
		node = dag->nodes[i];
		if (node->killed) {
			//the node is already killed
			continue;
		}
		if (node == n->children[1])	{
			// assume the pointer does not point to itself. This should be
			// reasonable because without casting, only a void pointer can
			// point to itself (the required type is recursive).
			continue;
		}
		if (!is_call && op_is_constant (node->label->op)) {
			// While constants in the Quake VM can be changed via a pointer,
			// doing so would cause much more fun than a simple
			// mis-optimization would, so consider them safe from pointer
			// operations.
			continue;
		}
		if (!is_call && op_is_temp (node->label->op)) {
			// Assume that the pointer cannot point to a temporary variable.
			//  This is reasonable as there is no programmer access to temps.
			continue;
		}
		node->killed = n;
	}
	n->killed = 0;
	dag->killer_node = n->number;
}

static __attribute__((pure)) int
dag_count_ops (operand_t *op)
{
	int count = 0;
	for (; op; op = op->next) {
		if (op->op_type == op_pseudo) {
			continue;
		}
		count++;
	}
	return count;
}

static void
dag_free_set (set_t **set)
{
	set_delete (*set);
}

static bool
dag_check_overlap (statement_t *s, dagnode_t *node, daglabel_t *label,
				   flownode_t *flownode)
{
	int start = s->number + 1;
	int end = flownode->first_statement + flownode->num_statements;

	auto nvar = flow_get_var (node->label->op);
	__attribute__((cleanup(dag_free_set))) set_t *def = set_new ();
	set_add_range (def, start, end - start);
	set_intersection (def, nvar->define);

	auto d = set_first (def);
	if (!d) {
		// not set in this flow node
		return false;
	}

	auto lvar = flow_get_var (label->op);
	__attribute__((cleanup(dag_free_set))) set_t *use = set_new ();
	set_add_range (use, start, end - start);
	set_intersection (use, lvar->use);

	bool overlap = false;
	for (auto u = set_first (use); u; u = set_next (u)) {
		if (u->element > d->element) {
			overlap = true;
			set_del_iter (u);
			break;
		}
	}
	set_del_iter (d);
	return overlap;
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
	int         num_aux = 0;
	int         num_nodes;
	int         num_lables;
	set_t      *live_vars = set_new ();

	flush_daglabels ();

	// count the number of statements so the number of nodes and labels can be
	// guessed
	for (s = block->statements; s; s = s->next) {
		num_statements++;
		num_aux += dag_count_ops (s->use);
		num_aux += dag_count_ops (s->def);
		num_aux += op_is_alias (s->opa);
		num_aux += op_is_alias (s->opb);
		num_aux += op_is_alias (s->opc);
	}

	set_assign (live_vars, flownode->live_vars.out);

	dag = new_dag ();
	dag->flownode = flownode;
	// at most FLOW_OPERANDS per statement + aux
	num_nodes = num_statements * FLOW_OPERANDS + num_aux;
	dag->nodes = alloca (num_nodes * sizeof (dagnode_t));
	// at most FLOW_OPERANDS per statement, + return + params + aux
	num_lables = num_statements * (FLOW_OPERANDS + 1 + 8) + num_aux;
	dag->labels = alloca (num_lables * sizeof (daglabel_t));
	dag->roots = set_new ();
	dag->killer_node = -1;

	// actual dag creation
	for (s = block->statements; s; s = s->next) {
		operand_t  *operands[FLOW_OPERANDS];
		dagnode_t  *n = 0, *children[3] = {0, 0, 0};
		daglabel_t *op, *lx;

		dag_make_children (dag, s, operands, children);
		op = opcode_label (dag, s->opcode, s->expr);
		n = children[0];
		if (s->type != st_assign && s->type != st_move) {
			dagnode_t search = {
				.label = op,
				.children = { children[0], children[1], children[2] },
			};
			if (!(n = dagnode_search (dag, &search))) {
				n = new_node (dag);
				n->type = s->type;
				n->label = op;
				dagnode_add_children (dag, n, operands, children);
				dagnode_set_edges (dag, n, s);
				dagnode_set_reachable (dag, n);
			}
		}
		lx = operand_label (dag, operands[0]);
		if (lx && lx->dagnode != n) {
			lx->expr = s->expr;
			if (!dagnode_attach_label (dag, n, lx)) {
				// attempting to attach the label to the node would create
				// a dependency cycle in the dag, so a new node needs to be
				// created for the source operand
				if (s->type == st_assign) {
					n = leaf_node (dag, operands[1], s->expr);
					dagnode_attach_label (dag, n, lx);
				} else {
					internal_error (s->expr, "unexpected failure to attach"
									" label to node");
				}
			}
			if (n->type == st_none && op_is_identifier (n->label->op)) {
				// if the attached variable has a use after the variable in
				// the leaf node has a define in the block, then force the
				// attached variable to be live. Takes care of code similar
				// to `while (count--) {...}`.
				if (dag_check_overlap (s, n, lx, flownode)) {
					lx->live = 1;
				}
			}
		}
		if (n->type == st_ptrassign) {
			dag_kill_nodes (dag, n, false);
		}
		if (s->type == st_func && strcmp (s->opcode, "call") == 0) {
			dag_kill_nodes (dag, n, true);
		}
	}

	nodes = malloc (dag->num_nodes * sizeof (dagnode_t *));
	memcpy (nodes, dag->nodes, dag->num_nodes * sizeof (dagnode_t *));
	dag->nodes = nodes;
	labels = malloc (dag->num_labels * sizeof (daglabel_t *));
	memcpy (labels, dag->labels, dag->num_labels * sizeof (daglabel_t *));
	dag->labels = labels;
#if 0
	if (options.block_dot.dags) {
		flownode->dag = dag;
		dump_dot ("raw-dags", flownode->graph, dump_dot_flow_dags);
	}
#endif
	dag_remove_dead_vars (dag, live_vars);
	dag_sort_nodes (dag);
	set_delete (live_vars);
	return dag;
}

static statement_t *
build_statement (const char *opcode, operand_t **operands, const expr_t *expr)
{
	int         i;
	operand_t  *op;
	statement_t *st = new_statement (st_none, opcode, expr);

	for (i = 0; i < 3; i++) {
		if ((op = operands[i])) {
			while (op->op_type == op_alias)
				op = op->alias;
			if (op->op_type == op_temp) {
				while (op->tempop.alias)
					op = op->tempop.alias;
				op->tempop.users++;
			}
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
fix_op_type (operand_t *op, const type_t *type)
{
	if (op && op->op_type != op_label && op->type != type)
		op = alias_operand (type, op, op->expr);
	return op;
}

static operand_t *
make_operand (dag_t *dag, sblock_t *block, const dagnode_t *dagnode, int index)
{
	operand_t  *op;

	op = dagnode->children[index]->value;
	op = fix_op_type (op, dagnode->types[index]);
	return op;
}

static operand_t *
generate_moves (dag_t *dag, sblock_t *block, dagnode_t *dagnode)
{
	set_iter_t *var_iter;
	daglabel_t  *var;
	operand_t   *operands[3] = {0, 0, 0};
	statement_t *st;
	operand_t   *dst;

	operands[0] = make_operand (dag, block, dagnode, 0);
	operands[1] = make_operand (dag, block, dagnode, 1);
	dst = operands[0];
	for (var_iter = set_first (dagnode->identifiers); var_iter;
		 var_iter = set_next (var_iter)) {
		var = dag->labels[var_iter->element];
		operands[2] = var->op;
		dst = operands[2];
		st = build_statement ("move", operands, var->expr);
		sblock_add_statement (block, st);
	}
	return dst;
}

static operand_t *
generate_moveps (dag_t *dag, sblock_t *block, dagnode_t *dagnode)
{
	set_iter_t *var_iter;
	daglabel_t  *var;
	operand_t   *operands[3] = {0, 0, 0};
	statement_t *st;
	operand_t   *dst = 0;
	int          offset = 0;
	def_t       *dstDef;

	operands[0] = make_operand (dag, block, dagnode, 0);
	operands[1] = make_operand (dag, block, dagnode, 1);
	if (dagnode->children[2]) {
		operands[2] = make_operand (dag, block, dagnode, 2);
		st = build_statement ("movep", operands, dagnode->label->expr);
		sblock_add_statement (block, st);
		if ((var_iter = set_first (dagnode->identifiers))) {
			var = dag->labels[var_iter->element];
			dst = var->op;
			set_del_iter (var_iter);
		}
	} else {
		for (var_iter = set_first (dagnode->identifiers); var_iter;
			 var_iter = set_next (var_iter)) {
			var = dag->labels[var_iter->element];
			dst = var->op;
			auto type = dst->def->type;
			dstDef = dst->def;
			if (dstDef->alias) {
				offset = dstDef->offset;
				dstDef = dstDef->alias;
			}
			operands[2] = value_operand (new_pointer_val (offset, type, dstDef, 0),
										 operands[1]->expr);
			st = build_statement ("movep", operands, var->expr);
			sblock_add_statement (block, st);
		}
	}
	return dst;
}

static operand_t *
generate_memsets (dag_t *dag, sblock_t *block, dagnode_t *dagnode)
{
	set_iter_t *var_iter;
	daglabel_t  *var;
	operand_t   *operands[3] = {0, 0, 0};
	statement_t *st;
	operand_t   *dst;

	operands[0] = make_operand (dag, block, dagnode, 0);
	operands[1] = make_operand (dag, block, dagnode, 1);
	dst = operands[0];
	for (var_iter = set_first (dagnode->identifiers); var_iter;
		 var_iter = set_next (var_iter)) {
		var = dag->labels[var_iter->element];
		operands[2] = var->op;
		dst = operands[2];
		st = build_statement ("memset", operands, var->expr);
		sblock_add_statement (block, st);
	}
	return dst;
}

static operand_t *
generate_memsetps (dag_t *dag, sblock_t *block, dagnode_t *dagnode)
{
	set_iter_t *var_iter;
	daglabel_t  *var;
	operand_t   *operands[3] = {0, 0, 0};
	statement_t *st;
	operand_t   *dst = 0;
	int          offset = 0;
	def_t       *dstDef;

	operands[0] = make_operand (dag, block, dagnode, 0);
	operands[1] = make_operand (dag, block, dagnode, 1);
	if (dagnode->children[2]) {
		operands[2] = make_operand (dag, block, dagnode, 2);
		st = build_statement ("memsetp", operands, dagnode->label->expr);
		sblock_add_statement (block, st);
	} else {
		for (var_iter = set_first (dagnode->identifiers); var_iter;
			 var_iter = set_next (var_iter)) {
			var = dag->labels[var_iter->element];
			dst = var->op;
			auto type = dst->def->type;
			dstDef = dst->def;
			if (dstDef->alias) {
				offset = dstDef->offset;
				dstDef = dstDef->alias;
			}
			operands[2] = value_operand (new_pointer_val (offset, type, dstDef, 0),
										 operands[1]->expr);
			st = build_statement ("memsetp", operands, var->expr);
			sblock_add_statement (block, st);
		}
	}
	return dst;
}

static operand_t *
generate_assignments (dag_t *dag, sblock_t *block, operand_t *src,
					  set_iter_t *var_iter, const type_t *type)
{
	statement_t *st;
	operand_t   *dst = 0;
	operand_t   *operands[3] = {0, 0, 0};
	daglabel_t  *var;

	if (is_structural (type) || type->width > 4) {
		operands[0] = fix_op_type (src, type);
		operands[1] = short_operand (type_size (type), src->expr);
		for ( ; var_iter; var_iter = set_next (var_iter)) {
			var = dag->labels[var_iter->element];
			operands[2] = fix_op_type (var->op, type);
			if (!dst)
				dst = operands[2];

			st = build_statement ("move", operands, var->expr);
			sblock_add_statement (block, st);
		}
	} else {
		operands[2] = fix_op_type (src, type);
		for ( ; var_iter; var_iter = set_next (var_iter)) {
			var = dag->labels[var_iter->element];
			operands[0] = fix_op_type (var->op, type);
			if (!dst)
				dst = operands[0];

			st = build_statement ("assign", operands, var->expr);
			sblock_add_statement (block, st);
		}
	}
	return dst;
}

static operand_t *
generate_call (dag_t *dag, sblock_t *block, dagnode_t *dagnode)
{
	set_iter_t *var_iter;
	daglabel_t  *var = 0;
	operand_t   *operands[3] = {0, 0, 0};
	statement_t *st;
	operand_t   *dst = nullptr;
	const type_t *type = dagnode->vtype;

	operands[0] = make_operand (dag, block, dagnode, 0);
	if (dagnode->children[1]) {
		operands[1] = make_operand (dag, block, dagnode, 1);
	}
	if ((var_iter = set_first (dagnode->identifiers))) {
		var = dag->labels[var_iter->element];
		operands[2] = var->op;
		var_iter = set_next (var_iter);
		dst = operands[2];
		st = build_statement ("call", operands, var->expr);
		sblock_add_statement (block, st);
		generate_assignments (dag, block, operands[2], var_iter, type);
	}
	if (!var) {
		if (dagnode->children[2]) {
			// void call or return value ignored, still have to call
			operands[2] = make_operand (dag, block, dagnode, 2);
		} else {
			operands[2] = temp_operand (type, dagnode->label->expr);
			dst = operands[2];
		}
		st = build_statement ("call", operands, dagnode->label->expr);
		sblock_add_statement (block, st);
	}
	return dst;
}

static void
dag_gencode (dag_t *dag, sblock_t *block, dagnode_t *dagnode)
{
	operand_t  *operands[3] = {0, 0, 0};
	operand_t  *dst = 0;
	statement_t *st;
	set_iter_t *var_iter;
	int         i;
	const type_t *type;

	switch (dagnode->type) {
		case st_none:
			if (!dagnode->label->op)
				internal_error (dagnode->label->expr,
								"non-leaf label in leaf node");
			dst = dagnode->label->op;
			if ((var_iter = set_first (dagnode->identifiers))) {
				type = dst->type;
				dst = generate_assignments (dag, block, dst, var_iter, type);
			}
			break;
		case st_alias:
			if (!dagnode->children[0] || !dagnode->children[0]->value
				|| !dagnode->types[0]
				|| dagnode->children[1] || dagnode->children[2]) {
				internal_error (dagnode->label->expr, "invalid alias node");
			}
			auto value = dagnode->children[0]->value;
			int offset = 0;
			if (op_is_alias (value)) {
				//FIXME? this check shouldn't be necessary, but aliased defs
				//stay as aliased defs in the dag. I'm not sure this is the
				//right fix, or even in the right direction
				if (value->op_type == op_def) {
					offset = op_alias_offset (value);
				}
				value = unalias_op (value);
			}
			dst = offset_alias_operand (dagnode->types[0], dagnode->offset + offset,
										value, dagnode->label->expr);
			if ((var_iter = set_first (dagnode->identifiers))) {
				type = dst->type;
				dst = generate_assignments (dag, block, dst, var_iter, type);
			}
			break;
		case st_address:
		case st_expr:
			operands[0] = make_operand (dag, block, dagnode, 0);
			if (dagnode->children[1])
				operands[1] = make_operand (dag, block, dagnode, 1);
			type = dagnode->vtype;
			if (!(var_iter = set_first (dagnode->identifiers))) {
				operands[2] = temp_operand (type, dagnode->label->expr);
			} else {
				daglabel_t *var = dag->labels[var_iter->element];

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
			internal_error (dagnode->label->expr, "unexpected assignment node");
		case st_ptrassign:
			operands[2] = make_operand (dag, block, dagnode, 0);
			operands[0] = make_operand (dag, block, dagnode, 2);
			if (dagnode->children[1])
				operands[1] = make_operand (dag, block, dagnode, 1);
			st = build_statement (dagnode->label->opcode, operands,
								  dagnode->label->expr);
			sblock_add_statement (block, st);
			// the source location is suitable for use in other nodes
			dst = operands[2];
			break;
		case st_move:
			dst = generate_moves (dag, block, dagnode);
			break;
		case st_ptrmove:
			dst = generate_moveps (dag, block, dagnode);
			break;
		case st_memset:
			dst = generate_memsets (dag, block, dagnode);
			break;
		case st_ptrmemset:
			dst = generate_memsetps (dag, block, dagnode);
			break;
		case st_func:
			if (!strcmp (dagnode->label->opcode, "call")) {
				dst = generate_call (dag, block, dagnode);
				break;
			}
			// fallthrough
		case st_state:
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
			if (dagnode->children[2])
				operands[2] = make_operand (dag, block, dagnode, 2);
			st = build_statement (dagnode->label->opcode, operands,
								  dagnode->label->expr);
			sblock_add_statement (block, st);
			break;
	}
	dagnode->value = dst;
}

void
dag_remove_dead_nodes (dag_t *dag)
{
	int         added_root;
	set_iter_t *root_i, *child_i;
	dagnode_t  *node, *child;

	do {
		added_root = 0;
		for (root_i = set_first (dag->roots); root_i;
			 root_i = set_next (root_i)) {
			node = dag->nodes[root_i->element];
			// only st_none (leaf nodes), st_expr and st_move can become
			// dead nodes (no live vars attached).
			if (node->type != st_none && node->type != st_expr
				&& node->type != st_move)
				continue;
			if (!set_is_empty (node->identifiers))
				continue;
			// MOVEP with a variable destination pointer is never dead
			if (node->type == st_move && node->children[2])
				continue;
			set_remove (dag->roots, node->number);
			for (child_i = set_first (node->edges); child_i;
				 child_i = set_next (child_i)) {
				child = dag->nodes[child_i->element];
				if (!set_is_member (child->parents, node->number))
					continue;	// not really a child (dependency edge)
				set_remove (child->parents, node->number);
				if (set_is_empty (child->parents)) {
					set_add (dag->roots, child->number);
					added_root = 1;
				}
			}
		}
	} while (added_root);

	// clean up any stray edges that point to removed nodes
	for (int i = 0; i < dag->num_nodes; i++) {
		node = dag->nodes[i];
		for (child_i = set_first (node->edges); child_i;
			 child_i = set_next (child_i)) {
			child = dag->nodes[child_i->element];
			if (!set_is_member (dag->roots, child->number)
				&& set_is_empty (child->parents)) {
				set_remove (node->edges, child->number);
			}
		}
	}
	dag_sort_nodes (dag);
}

void
dag_generate (dag_t *dag, sblock_t *block)
{
	int         i;

	for (i = 0; i < dag->num_topo; i++)
		dag_gencode (dag, block, dag->nodes[dag->topo[i]]);
}
