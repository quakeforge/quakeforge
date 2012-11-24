/*
	flow.c

	Flow graph analysis

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/10/30

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
#include "QF/va.h"

#include "dags.h"
#include "def.h"
#include "diagnostic.h"
#include "dot.h"
#include "flow.h"
#include "function.h"
#include "options.h"
#include "qfcc.h"
#include "set.h"
#include "statements.h"
#include "symtab.h"
#include "type.h"

static flowvar_t *free_vars;
static flowloop_t *free_loops;
static flownode_t *free_nodes;
static flowgraph_t *free_graphs;

static struct {
	const char *name;
	operand_t   op;
} flow_params[] = {
	{".return",		{0, op_symbol}},
	{".param_0",	{0, op_symbol}},
	{".param_1",	{0, op_symbol}},
	{".param_2",	{0, op_symbol}},
	{".param_3",	{0, op_symbol}},
	{".param_4",	{0, op_symbol}},
	{".param_5",	{0, op_symbol}},
	{".param_6",	{0, op_symbol}},
	{".param_7",	{0, op_symbol}},
};
static const int num_flow_params = sizeof(flow_params)/sizeof(flow_params[0]);

static void
dump_dot_flow (void *data, const char *fname)
{
	print_flowgraph ((flowgraph_t *) data, fname);
}

static flowvar_t *
new_flowvar (void)
{
	flowvar_t *var;
	ALLOC (256, flowvar_t, vars, var);
	var->use = set_new ();
	var->define = set_new ();
	return var;
}

static flowloop_t *
new_loop (void)
{
	flowloop_t *loop;
	ALLOC (256, flowloop_t, loops, loop);
	loop->nodes = set_new ();
	return loop;
}

static void
delete_loop (flowloop_t *loop)
{
	set_delete (loop->nodes);
	loop->next = free_loops;
	free_loops = loop;
}

static flownode_t *
new_node (void)
{
	flownode_t *node;
	ALLOC (256, flownode_t, nodes, node);
	return node;
}

static void
delete_node (flownode_t *node)
{
	if (node->predecessors)
		set_delete (node->predecessors);
	if (node->successors)
		set_delete (node->successors);
	if (node->edges)
		set_delete (node->edges);
	if (node->dom)
		set_delete (node->dom);
	node->next = free_nodes;
	free_nodes = node;
}

static flowgraph_t *
new_graph (void)
{
	flowgraph_t *graph;
	ALLOC (256, flowgraph_t, graphs, graph);
	return graph;
}

static void
delete_graph (flowgraph_t *graph)
{
	int         i;

	if (graph->nodes) {
		for (i = 0; i < graph->num_nodes; i++)
			delete_node (graph->nodes[i]);
		free (graph->nodes);
	}
	if (graph->edges)
		free (graph->edges);
	if (graph->dfst)
		set_delete (graph->dfst);
	if (graph->dfo)
		free (graph->dfo);
	graph->next = free_graphs;
	free_graphs = graph;
}

static def_t *
flowvar_get_def (flowvar_t *var)
{
	operand_t  *op = var->op;

	while (op->op_type == op_alias)
		op = op->o.alias;
	switch (op->op_type) {
		case op_symbol:
			return op->o.symbol->s.def;
		case op_value:
		case op_label:
			return 0;
		case op_temp:
			return op->o.tempop.def;
		case op_pointer:
			return op->o.value->v.pointer.def;
		case op_alias:
			internal_error (0, "oops, blue pill");
	}
	return 0;
}

static int
flowvar_is_global (flowvar_t *var)
{
	symbol_t   *sym;
	def_t      *def;

	if (var->op->op_type != op_symbol)
		return 0;
	sym = var->op->o.symbol;
	if (sym->sy_type != sy_var)
		return 0;
	def = sym->s.def;
	if (def->local)
		return 0;
	return 1;
}

static int
flowvar_is_param (flowvar_t *var)
{
	symbol_t   *sym;
	def_t      *def;

	if (var->op->op_type != op_symbol)
		return 0;
	sym = var->op->o.symbol;
	if (sym->sy_type != sy_var)
		return 0;
	def = sym->s.def;
	if (!def->local)
		return 0;
	if (!def->param)
		return 0;
	return 1;
}

static int
flowvar_is_initialized (flowvar_t *var)
{
	symbol_t   *sym;
	def_t      *def;

	if (var->op->op_type != op_symbol)
		return 0;
	sym = var->op->o.symbol;
	if (sym->sy_type != sy_var)
		return 0;
	def = sym->s.def;
	return def->initialized;
}

flowvar_t *
flow_get_var (operand_t *op)
{
	if (!op)
		return 0;

	while (op->op_type == op_alias)
		op = op->o.alias;

	if (op->op_type == op_temp) {
		if (!op->o.tempop.flowvar)
			op->o.tempop.flowvar = new_flowvar ();
		return op->o.tempop.flowvar;
	}
	if (op->op_type == op_symbol && op->o.symbol->sy_type == sy_var) {
		if (!op->o.symbol->flowvar)
			op->o.symbol->flowvar = new_flowvar ();
		return op->o.symbol->flowvar;
	}
	//FIXME functions? (some are variable) values?
	return 0;
}

static int
count_operand (operand_t *op)
{
	flowvar_t  *var;

	if (!op)
		return 0;
	if (op->op_type == op_label)
		return 0;

	var = flow_get_var (op);
	// daglabels are initialized with number == 0, and any global daglabel
	// used by a function will always have a number >= 0 after flow analysis,
	// and local daglabels will always be 0 before flow analysis, so use -1
	// to indicate the variable has been counted.
	//
	// Also, since this is the beginning of flow analysis for this function,
	// ensure the define/use sets for global vars are empty. However, as
	// checking if a var is global is too much trouble, just clear them all.
	if (var && var->number != -1) {
		set_empty (var->use);
		set_empty (var->define);
		var->number = -1;
		return 1;
	}
	return 0;
}

static void
add_operand (function_t *func, operand_t *op)
{
	flowvar_t  *var;

	if (!op)
		return;
	while (op->op_type == op_alias)
		op = op->o.alias;
	if (op->op_type == op_label)
		return;

	var = flow_get_var (op);
	// If the daglabel number is still -1, then the daglabel has not yet been
	// added to the list of variables referenced by the function.
	if (var && var->number == -1) {
		var->number = func->num_vars++;
		var->op = op;
		func->vars[var->number] = var;
	}
}

static symbol_t *
param_symbol (const char *name)
{
	symbol_t   *sym;
	sym = make_symbol (name, &type_param, pr.symtab->space, st_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	return sym;
}

void
flow_build_vars (function_t *func)
{
	sblock_t   *sblock;
	statement_t *s;
	int         num_vars = 0;
	int         num_statements = 0;
	int         i;

	// first, count .return and .param_[0-7] as they are always needed
	for (i = 0; i < num_flow_params; i++) {
		flow_params[i].op.o.symbol = param_symbol (flow_params[i].name);
		num_vars += count_operand (&flow_params[i].op);
	}
	// then run through the statements in the function looking for accessed
	// variables
	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		for (s = sblock->statements; s; s = s->next) {
			num_vars += count_operand (s->opa);
			num_vars += count_operand (s->opb);
			num_vars += count_operand (s->opc);
			s->number = num_statements++;
		}
	}
	if (num_vars) {
		func->vars = malloc (num_vars * sizeof (daglabel_t *));

		func->num_vars = 0;	// incremented by add_operand
		// first, add .return and .param_[0-7] as they are always needed
		for (i = 0; i < num_flow_params; i++)
			add_operand (func, &flow_params[i].op);
		// then run through the statements in the function adding accessed
		// variables
		for (sblock = func->sblock; sblock; sblock = sblock->next) {
			for (s = sblock->statements; s; s = s->next) {
				add_operand (func, s->opa);
				add_operand (func, s->opb);
				add_operand (func, s->opc);
			}
		}
		func->global_vars = set_new ();
		// mark all global vars (except .return and .param_N)
		for (i = num_flow_params; i < func->num_vars; i++) {
			if (flowvar_is_global (func->vars[i]))
				set_add (func->global_vars, i);
		}
	}
	if (num_statements) {
		func->statements = malloc (num_statements * sizeof (statement_t *));
		func->num_statements = num_statements;
		for (sblock = func->sblock; sblock; sblock = sblock->next) {
			for (s = sblock->statements; s; s = s->next)
				func->statements[s->number] = s;
		}
	}
}

static void
live_set_use (set_t *stuse, set_t *use, set_t *def)
{
	// the variable is used before it is defined
	set_difference (stuse, def);
	set_union (use, stuse);
}

static void
live_set_def (set_t *stdef, set_t *use, set_t *def)
{
	// the variable is defined before it is used
	set_difference (stdef, use);
	set_union (def, stdef);
}

static void
flow_live_vars (flowgraph_t *graph)
{
	int         i, j;
	flownode_t *node;
	set_t      *use;
	set_t      *def;
	set_t      *stuse = set_new ();
	set_t      *stdef = set_new ();
	set_t      *tmp = set_new ();
	set_iter_t *succ;
	statement_t *st;
	int         changed = 1;

	// first, calculate use and def for each block, and initialize the in and
	// out sets.
	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		use = set_new ();
		def = set_new ();
		for (st = node->sblock->statements; st; st = st->next) {
			flow_analyze_statement (st, stuse, stdef, 0, 0);
			if (st->type == st_func && !strncmp (st->opcode, "<RETURN", 7)) {
				set_t      *global_vars = set_new ();

				set_assign (global_vars, graph->func->global_vars);
				live_set_use (global_vars, use, def);
				set_delete (global_vars);
			}
			live_set_use (stuse, use, def);
			live_set_def (stdef, use, def);
		}
		node->live_vars.use = use;
		node->live_vars.def = def;
		node->live_vars.in = set_new ();
		node->live_vars.out = set_new ();
	}

	while (changed) {
		changed = 0;
		// flow UP the graph because live variable analysis uses information
		// from a node's successors rather than its predecessors.
		for (j = graph->num_nodes - 1; j >= 0; j--) {
			node = graph->nodes[graph->dfo[j]];
			set_empty (tmp);
			for (succ = set_first (node->successors); succ;
				 succ = set_next (succ))
				set_union (tmp, graph->nodes[succ->member]->live_vars.in);
			if (!set_is_equivalent (node->live_vars.out, tmp)) {
				changed = 1;
				set_assign (node->live_vars.out, tmp);
			}
			set_assign (node->live_vars.in, node->live_vars.out);
			set_difference (node->live_vars.in, node->live_vars.def);
			set_union (node->live_vars.in, node->live_vars.use);
		}
	}
	set_delete (stuse);
	set_delete (stdef);
	set_delete (tmp);
}

static void
flow_uninitialized (flowgraph_t *graph)
{
	set_t      *stuse;
	set_t      *stdef;
	set_t      *tmp;
	set_t      *uninit;
	set_t      *use;
	set_t      *def;
	set_t      *predecessors;
	set_iter_t *pred_iter;
	set_iter_t *var_iter;
	flownode_t *node, *pred;
	function_t *func = graph->func;
	statement_t *st;
	int         i;

	if (!graph->num_nodes)
		return;

	stuse = set_new ();
	stdef = set_new ();
	tmp = set_new ();
	uninit = set_new ();
	predecessors = set_new ();

	// in for the inital node contains parameters and global vars
	tmp = set_new ();
	for (i = 0; i < func->num_vars; i++) {
		flowvar_t  *var = func->vars[i];
		if (flowvar_is_global (var) || flowvar_is_param (var)
			|| flowvar_is_initialized (var))
			set_add (tmp, i);
	}
	// first, calculate use and def for each block, and initialize the in and
	// out sets.
	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		use = set_new ();
		def = set_new ();
		for (st = node->sblock->statements; st; st = st->next) {
			flow_analyze_statement (st, stuse, stdef, 0, 0);
			live_set_use (stuse, use, def);	// init use uses same rules as live
			set_union (def, stdef);			// for init, always def a set var
		}
		node->init_vars.use = use;
		node->init_vars.def = def;
		node->init_vars.in = set_new ();
		node->init_vars.out = set_new ();
	}
	// flow DOWN the graph in dept-first order
	node = graph->nodes[0];
	set_assign (node->init_vars.in, tmp);
	set_assign (node->init_vars.out, tmp);
	set_union (node->init_vars.out, node->init_vars.def);
	set_assign (tmp, node->init_vars.use);
	set_difference (tmp, node->init_vars.in);
	set_union (uninit, tmp);
	for (i = 1; i < graph->num_nodes; i++) {
		node = graph->nodes[graph->dfo[i]];
		set_assign (predecessors, node->predecessors);
		// not interested in back edges
		for (pred_iter = set_first (predecessors); pred_iter;
			 pred_iter = set_next (pred_iter)) {
			pred = graph->nodes[pred_iter->member];
			if (pred->dfn >= node->dfn)
				set_remove (predecessors, pred->id);
		}
		set_empty (tmp);
		pred_iter = set_first (predecessors);
		if (pred_iter) {
			pred = graph->nodes[pred_iter->member];
			set_assign (tmp, pred->init_vars.out);
			pred_iter = set_next (pred_iter);
		}
		for ( ; pred_iter; pred_iter = set_next (pred_iter)) {
			pred = graph->nodes[pred_iter->member];
			set_intersection (tmp, pred->init_vars.out);
		}
		set_assign (node->init_vars.in, tmp);
		set_assign (node->init_vars.out, tmp);
		set_union (node->init_vars.out, node->init_vars.def);
		set_assign (tmp, node->init_vars.use);
		set_difference (tmp, node->init_vars.in);
		set_union (uninit, tmp);
	}
	for (var_iter = set_first (uninit); var_iter;
		 var_iter = set_next (var_iter)) {
		expr_t  dummy;
		flowvar_t  *var = func->vars[var_iter->member];
		def_t      *def;

		if (var->op->op_type == op_temp) {
			bug (0, "uninitialized temporary: %s", operand_string (var->op));
		} else {
			def = flowvar_get_def (var);
			dummy.line = def->line;
			dummy.file = def->file;
			warning (&dummy, "%s may be used uninitialized", def->name);
		}
	}
	set_delete (stuse);
	set_delete (stdef);
	set_delete (tmp);
	set_delete (uninit);
	set_delete (predecessors);
}

static void
flow_build_dags (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		node->dag = dag_create (node);
	}
	//if (options.block_dot.dags)
	//	dump_dot ("dags", graph, dump_dot_flow);
}

void
flow_data_flow (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;
	statement_t *st;
	flowvar_t  *var;
	set_t      *stuse = set_new ();
	set_t      *stdef = set_new ();
	set_iter_t *var_i;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		for (st = node->sblock->statements; st; st = st->next) {
			flow_analyze_statement (st, stuse, stdef, 0, 0);
			for (var_i = set_first (stdef); var_i; var_i = set_next (var_i)) {
				var = graph->func->vars[var_i->member];
				set_add (var->define, st->number);
			}
			for (var_i = set_first (stuse); var_i; var_i = set_next (var_i)) {
				var = graph->func->vars[var_i->member];
				set_add (var->use, st->number);
			}
		}
	}
	flow_live_vars (graph);
	flow_uninitialized (graph);
	flow_build_dags (graph);
	if (options.block_dot.flow)
		dump_dot ("flow", graph, dump_dot_flow);
}

sblock_t *
flow_generate (flowgraph_t *graph)
{
	int         i;
	sblock_t   *code = 0;
	sblock_t  **tail = &code;

	for (i = 0; i < graph->num_nodes; i++) {
		ex_label_t *label;
		sblock_t   *block;

		flownode_t *node = graph->nodes[i];
		*tail = block = new_sblock ();
		tail = &(*tail)->next;
		// first, transfer any labels on the old node to the new
		while ((label = node->sblock->labels)) {
			node->sblock->labels = label->next;
			label->next = block->labels;
			block->labels = label;
			label->dest = block;
		}
		// generate new statements from the dag;
		dag_generate (node->dag, block);
	}
	if (options.block_dot.post)
		dump_dot ("post", code, dump_dot_sblock);
	return code;
}

int
flow_is_cond (statement_t *s)
{
	if (!s)
		return 0;
	return !strncmp (s->opcode, "<IF", 3);
}

int
flow_is_goto (statement_t *s)
{
	if (!s)
		return 0;
	return !strcmp (s->opcode, "<GOTO>");
}

int
flow_is_jumpb (statement_t *s)
{
	if (!s)
		return 0;
	return !strcmp (s->opcode, "<JUMPB>");
}

int
flow_is_return (statement_t *s)
{
	if (!s)
		return 0;
	return !strncmp (s->opcode, "<RETURN", 7);
}

sblock_t *
flow_get_target (statement_t *s)
{
	if (flow_is_cond (s))
		return s->opb->o.label->dest;
	if (flow_is_goto (s))
		return s->opa->o.label->dest;
	return 0;
}

sblock_t **
flow_get_targetlist (statement_t *s)
{
	sblock_t  **target_list;
	int         count = 0, i;
	def_t      *table = 0;
	expr_t     *e;

	if (flow_is_cond (s)) {
		count = 1;
	} else if (flow_is_goto (s)) {
		count = 1;
	} else if (flow_is_jumpb (s)) {
		table = s->opa->o.alias->o.symbol->s.def;	//FIXME check!!!
		count = table->type->t.array.size;
	}
	target_list = malloc ((count + 1) * sizeof (sblock_t *));
	target_list[count] = 0;
	if (flow_is_cond (s)) {
		target_list[0] = flow_get_target (s);
	} else if (flow_is_goto (s)) {
		target_list[0] = flow_get_target (s);
	} else if (flow_is_jumpb (s)) {
		e = table->initializer->e.block.head;	//FIXME check!!!
		for (i = 0; i < count; e = e->next, i++)
			target_list[i] = e->e.labelref.label->dest;
	}
	return target_list;
}

static void
flow_add_op_var (set_t *set, operand_t *op)
{
	flowvar_t  *var;

	if (!set)
		return;
	if (!(var = flow_get_var (op)))
		return;
	set_add (set, var->number);
}

void
flow_analyze_statement (statement_t *s, set_t *use, set_t *def, set_t *kill,
						operand_t *operands[4])
{
	int         i, start, calln = -1;

	if (use)
		set_empty (use);
	if (def)
		set_empty (def);
	if (kill)
		set_empty (kill);
	if (operands) {
		for (i = 0; i < 4; i++)
			operands[i] = 0;
	}

	switch (s->type) {
		case st_none:
			internal_error (s->expr, "not a statement");
		case st_expr:
			flow_add_op_var (def, s->opc);
			flow_add_op_var (use, s->opa);
			if (s->opb)
				flow_add_op_var (use, s->opb);
			if (operands) {
				operands[0] = s->opc;
				operands[1] = s->opa;
				operands[2] = s->opb;
			}
			break;
		case st_assign:
			flow_add_op_var (def, s->opb);
			flow_add_op_var (use, s->opa);
			if (operands) {
				operands[0] = s->opb;
				operands[1] = s->opa;
			}
			break;
		case st_ptrassign:
		case st_move:
			flow_add_op_var (use, s->opa);
			flow_add_op_var (use, s->opb);
			if (!strcmp (s->opcode, "<MOVE>")) {
				flow_add_op_var (def, s->opc);
			} else {
				if (s->opc)
					flow_add_op_var (use, s->opc);
			}
			if (kill) {
				//FIXME set of everything
			}
			if (operands) {
				operands[1] = s->opa;
				operands[2] = s->opb;
				operands[3] = s->opc;
			}
			break;
		case st_state:
			flow_add_op_var (use, s->opa);
			flow_add_op_var (use, s->opb);
			if (s->opc)
				flow_add_op_var (use, s->opc);
			//FIXME entity members
			if (operands) {
				operands[1] = s->opa;
				operands[2] = s->opb;
				operands[3] = s->opc;
			}
			break;
		case st_func:
			if (strcmp (s->opcode, "<RETURN>") == 0
				|| strcmp (s->opcode, "<DONE>") == 0) {
				flow_add_op_var (use, s->opa);
			} else if (strcmp (s->opcode, "<RETURN_V>") == 0) {
				if (use)
					set_add (use, 0);		//FIXME assumes .return location
			}
			if (strncmp (s->opcode, "<CALL", 5) == 0) {
				start = 0;
				calln = s->opcode[5] - '0';
				flow_add_op_var (use, s->opa);
			} else if (strncmp (s->opcode, "<RCALL", 6) == 0) {
				start = 2;
				calln = s->opcode[6] - '0';
				flow_add_op_var (use, s->opa);
				flow_add_op_var (use, s->opb);
				if (s->opc)
					flow_add_op_var (use, s->opc);
			}
			if (calln >= 0) {
				if (use) {
					for (i = start; i < calln; i++)
						set_add (use, i + 1);//FIXME assumes .param_N locations
				}
				if (kill)
					set_add (kill, 0);		//FIXME assumes .return location
			}
			if (operands) {
				operands[1] = s->opa;
				operands[2] = s->opb;
				operands[3] = s->opc;
			}
			break;
		case st_flow:
			if (strcmp (s->opcode, "<GOTO>") != 0) {
				flow_add_op_var (use, s->opa);
				if (strcmp (s->opcode, "<JUMPB>") == 0)
					flow_add_op_var (use, s->opb);
			}
			if (operands) {
				operands[1] = s->opa;
				operands[2] = s->opb;
			}
			break;
	}
}

static void
flow_find_predecessors (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;
	set_iter_t *succ;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		for (succ = set_first (node->successors); succ;
			 succ = set_next (succ)) {
			set_add (graph->nodes[succ->member]->predecessors, i);
		}
	}
}

static void
flow_find_dominators (flowgraph_t *graph)
{
	set_t      *work;
	flownode_t *node;
	int         i;
	set_iter_t *pred;
	int         changed;

	if (!graph->num_nodes)
		return;

	// First, create a base set for the initial state of the non-initial nodes
	work = set_new ();
	for (i = 0; i < graph->num_nodes; i++)
		set_add (work, i);

	set_add (graph->nodes[0]->dom, 0);

	// initialize dom for the non-initial nodes
	for (i = 1; i < graph->num_nodes; i++) {
		set_assign (graph->nodes[i]->dom, work);
	}

	do {
		changed = 0;
		for (i = 1; i < graph->num_nodes; i++) {
			node = graph->nodes[i];
			pred = set_first (node->predecessors);
			set_empty (work);
			for (pred = set_first (node->predecessors); pred;
				 pred = set_next (pred))
				set_intersection (work, graph->nodes[pred->member]->dom);
			set_add (work, i);
			if (!set_is_equivalent (work, node->dom))
				changed = 1;
			set_assign (node->dom, work);
		}
	} while (changed);
	set_delete (work);
}

static void
insert_loop_node (flowloop_t *loop, unsigned n, set_t *stack)
{
	if (!set_is_member (loop->nodes, n)) {
		set_add (loop->nodes, n);
		set_add (stack, n);
	}
}

static flowloop_t *
make_loop (flowgraph_t *graph, unsigned n, unsigned d)
{
	flowloop_t *loop = new_loop ();
	flownode_t *node;
	set_t      *stack = set_new ();
	set_iter_t *pred;

	loop->head = d;
	set_add (loop->nodes, d);
	insert_loop_node (loop, n, stack);
	while (!set_is_empty (stack)) {
		set_iter_t *ss = set_first (stack);
		unsigned    m = ss->member;
		set_del_iter (ss);
		set_remove (stack, m);
		node = graph->nodes[m];
		for (pred = set_first (node->predecessors); pred;
			 pred = set_next (pred))
			insert_loop_node (loop, pred->member, stack);
	}
	set_delete (stack);
	return loop;
}

static void
flow_find_loops (flowgraph_t *graph)
{
	flownode_t *node;
	set_iter_t *succ;
	flowloop_t *loop, *l;
	flowloop_t *loop_list = 0;
	int         i;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		for (succ = set_first (node->successors); succ;
			 succ = set_next (succ)) {
			if (set_is_member (node->dom, succ->member)) {
				loop = make_loop (graph, node->id, succ->member);
				for (l = loop_list; l; l = l->next) {
					if (l->head == loop->head
						&& !set_is_subset (l->nodes, loop->nodes)
						&& !set_is_subset (loop->nodes, l->nodes)) {
						set_union (l->nodes, loop->nodes);
						delete_loop (loop);
						loop = 0;
						break;
					}
				}
				if (loop) {
					loop->next = loop_list;
					loop_list = loop;
				}
			}
		}
	}
	graph->loops = loop_list;
}

static void
df_search (flowgraph_t *graph, set_t *visited, int *i, int n)
{
	flownode_t *node;
	set_iter_t *edge;
	int         succ;

	set_add (visited, n);
	node = graph->nodes[n];
	for (edge = set_first (node->edges); edge; edge = set_next (edge)) {
		succ = graph->edges[edge->member].head;
		if (!set_is_member (visited, succ)) {
			set_add (graph->dfst, edge->member);
			df_search (graph, visited, i, succ);
		}
	}
	node->dfn = --*i;
	graph->dfo[node->dfn] = n;
}

static void
flow_build_dfst (flowgraph_t *graph)
{
	set_t      *visited = set_new ();
	int         i;

	graph->dfo = malloc (graph->num_nodes * sizeof (unsigned));
	graph->dfst = set_new ();
	i = graph->num_nodes;
	df_search (graph, visited, &i, 0);
	set_delete (visited);
}

flowgraph_t *
flow_build_graph (sblock_t *sblock, function_t *func)
{
	flowgraph_t *graph;
	flownode_t *node;
	sblock_t   *sb;
	sblock_t  **target_list, **target;
	statement_t *st;
	set_iter_t *succ;
	int         i, j;

	graph = new_graph ();
	for (sb = sblock; sb; sb = sb->next)
		sb->number = graph->num_nodes++;
	graph->nodes = malloc (graph->num_nodes * sizeof (flownode_t *));
	for (sb = sblock; sb; sb = sb->next) {
		node = new_node ();
		node->predecessors = set_new ();
		node->successors = set_new ();
		node->edges = set_new ();
		node->dom = set_new ();
		node->global_vars = func->global_vars;
		node->id = sb->number;
		node->sblock = sb;
		graph->nodes[node->id] = node;
	}

	// "convert" the basic blocks connections to flow-graph connections
	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		sb = node->sblock;
		st = 0;
		if (sb->statements)
			st = (statement_t *) sb->tail;
		//NOTE: if st is null (the sblock has no statements), flow_is_* will
		//return false
		//FIXME jump/jumpb
		if (flow_is_goto (st)) {
			// sb's next is never followed.
			set_add (node->successors, flow_get_target (st)->number);
		} else if (flow_is_jumpb (st)) {
			target_list = flow_get_targetlist (st);
			for (target = target_list; *target; target++)
				set_add (node->successors, (*target)->number);
			free (target_list);
		} else if (flow_is_cond (st)) {
			// branch: either sb's next or the conditional statment's
			// target will be followed.
			set_add (node->successors, sb->next->number);
			set_add (node->successors, flow_get_target (st)->number);
		} else if (flow_is_return (st)) {
			// exit from function (dead end)
		} else {
			// there is no flow-control statement in sb, so sb's next
			// must be followed
			set_add (node->successors, sb->next->number);
		}
		graph->num_edges += set_size (node->successors);
	}
	graph->edges = malloc (graph->num_edges * sizeof (flowedge_t *));
	for (j = 0, i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		for (succ = set_first (node->successors); succ;
			 succ = set_next (succ), j++) {
			set_add (node->edges, j);
			graph->edges[j].tail = i;
			graph->edges[j].head = succ->member;
		}
	}
	flow_build_dfst (graph);
	flow_find_predecessors (graph);
	flow_find_dominators (graph);
	flow_find_loops (graph);
	return graph;
}

void
flow_del_graph (flowgraph_t *graph)
{
	delete_graph (graph);
}
