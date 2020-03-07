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

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/set.h"
#include "QF/va.h"

#include "dags.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "dot.h"
#include "flow.h"
#include "function.h"
#include "options.h"
#include "qfcc.h"
#include "statements.h"
#include "symtab.h"
#include "type.h"

static flowvar_t *vars_freelist;
static flowloop_t *loops_freelist;
static flownode_t *nodes_freelist;
static flowgraph_t *graphs_freelist;

static struct {
	const char *name;
	operand_t   op;
} flow_params[] = {
	{".return",		{0, op_def}},
	{".param_0",	{0, op_def}},
	{".param_1",	{0, op_def}},
	{".param_2",	{0, op_def}},
	{".param_3",	{0, op_def}},
	{".param_4",	{0, op_def}},
	{".param_5",	{0, op_def}},
	{".param_6",	{0, op_def}},
	{".param_7",	{0, op_def}},
};
static const int num_flow_params = sizeof(flow_params)/sizeof(flow_params[0]);

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
	FREE (loops, loop);
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
	FREE (nodes, node);
}

static flowgraph_t *
new_graph (void)
{
	flowgraph_t *graph;
	ALLOC (256, flowgraph_t, graphs, graph);
	return graph;
}

static void __attribute__((unused))
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
	if (graph->depth_first)
		free (graph->depth_first);
	FREE (graphs, graph);
}

static __attribute__((pure)) def_t *
flowvar_get_def (flowvar_t *var)
{
	operand_t  *op = var->op;

	switch (op->op_type) {
		case op_def:
			return op->o.def;
		case op_value:
		case op_label:
			return 0;
		case op_temp:
			return op->o.tempop.def;
		case op_alias:
			internal_error (0, "unexpected alias operand");
	}
	internal_error (0, "oops, blue pill");
	return 0;
}

static int
flowvar_is_global (flowvar_t *var)
{
	def_t      *def;

	if (var->op->op_type != op_def)
		return 0;
	def = var->op->o.def;
	if (def->alias)
		def = def->alias;
	if (def->local)
		return 0;
	return 1;
}

static int
flowvar_is_param (flowvar_t *var)
{
	def_t      *def;

	if (var->op->op_type != op_def)
		return 0;
	def = var->op->o.def;
	if (def->alias)
		def = def->alias;
	if (!def->local)
		return 0;
	if (!def->param)
		return 0;
	return 1;
}

static int
flowvar_is_local (flowvar_t *var)
{
	return !(flowvar_is_global (var) || flowvar_is_param (var));
}
#if 0
static int
flowvar_is_initialized (flowvar_t *var)
{
	def_t      *def;

	if (var->op->op_type != op_def)
		return 0;
	def = var->op->o.def;
	return def->initialized;
}
#endif
flowvar_t *
flow_get_var (operand_t *op)
{
	if (!op)
		return 0;

	if (op->op_type == op_temp) {
		if (!op->o.tempop.flowvar)
			op->o.tempop.flowvar = new_flowvar ();
		return op->o.tempop.flowvar;
	}
	if (op->op_type == op_def) {
		if (!op->o.def->flowvar)
			op->o.def->flowvar = new_flowvar ();
		return op->o.def->flowvar;
	}
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
	// flowvars are initialized with number == 0, and any global flowvar
	// used by a function will always have a number >= 0 after flow analysis,
	// and local flowvars will always be 0 before flow analysis, so use -1
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

static int
get_temp_address (function_t *func, operand_t *op)
{
	operand_t  *top = op;
	if (op->o.tempop.flowaddr) {
		return op->o.tempop.flowaddr;
	}
	while (top->o.tempop.alias) {
		top = top->o.tempop.alias;
	}
	if (!top->o.tempop.flowaddr) {
		top->o.tempop.flowaddr = func->tmpaddr;
		func->tmpaddr += top->size;
	}
	op->o.tempop.flowaddr = top->o.tempop.flowaddr + op->o.tempop.offset;
	return op->o.tempop.flowaddr;
}

static void
add_operand (function_t *func, operand_t *op)
{
	flowvar_t  *var;

	if (!op)
		return;
	if (op->op_type == op_label)
		return;

	var = flow_get_var (op);
	// If the flowvar number is still -1, then the flowvar has not yet been
	// added to the list of variables referenced by the function.
	if (var && var->number == -1) {
		var->number = func->num_vars++;
		var->op = op;
		func->vars[var->number] = var;
		if (op->op_type == op_temp) {
			var->flowaddr = get_temp_address (func, op);
		} else if (flowvar_is_local (var)) {
			var->flowaddr = func->num_statements + def_offset (var->op->o.def);
		}
	}
}

static symbol_t *
param_symbol (const char *name)
{
	symbol_t   *sym;
	sym = make_symbol (name, &type_param, pr.symtab->space, sc_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	return sym;
}

static void
flow_build_statements (function_t *func)
{
	sblock_t   *sblock;
	statement_t *s;
	int         num_statements = 0;

	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		for (s = sblock->statements; s; s = s->next)
			s->number = num_statements++;
	}
	if (!num_statements)
		return;

	func->statements = malloc (num_statements * sizeof (statement_t *));
	func->num_statements = num_statements;
	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		for (s = sblock->statements; s; s = s->next)
			func->statements[s->number] = s;
	}
}

static void
flow_build_vars (function_t *func)
{
	statement_t *s;
	operand_t   *operands[4];
	int         num_vars = 0;
	int         i, j;
	set_t      *stuse;
	set_t      *stdef;
	set_iter_t *var_i;
	flowvar_t  *var;

	// first, count .return and .param_[0-7] as they are always needed
	for (i = 0; i < num_flow_params; i++) {
		flow_params[i].op.o.def = param_symbol (flow_params[i].name)->s.def;
		num_vars += count_operand (&flow_params[i].op);
	}
	// then run through the statements in the function looking for accessed
	// variables
	for (i = 0; i < func->num_statements; i++) {
		s = func->statements[i];
		flow_analyze_statement (s, 0, 0, 0, operands);
		for (j = 0; j < 4; j++)
			num_vars += count_operand (operands[j]);
	}
	if (!num_vars)
		return;

	func->vars = malloc (num_vars * sizeof (flowvar_t *));

	stuse = set_new ();
	stdef = set_new ();

	// set up pseudo address space for temp vars so accessing tmp vars
	// though aliases analyses correctly
	func->tmpaddr = func->num_statements + func->symtab->space->size;
	func->num_vars = 0;	// incremented by add_operand
	// first, add .return and .param_[0-7] as they are always needed
	for (i = 0; i < num_flow_params; i++)
		add_operand (func, &flow_params[i].op);
	// then run through the statements in the function adding accessed
	// variables
	for (i = 0; i < func->num_statements; i++) {
		s = func->statements[i];
		flow_analyze_statement (s, 0, 0, 0, operands);
		for (j = 0; j < 4; j++)
			add_operand (func, operands[j]);

		flow_analyze_statement (s, stuse, stdef, 0, 0);
		for (var_i = set_first (stdef); var_i; var_i = set_next (var_i)) {
			var = func->vars[var_i->element];
			set_add (var->define, i);
		}
		for (var_i = set_first (stuse); var_i; var_i = set_next (var_i)) {
			var = func->vars[var_i->element];
			set_add (var->use, i);
		}
	}
	func->global_vars = set_new ();
	// mark all global vars (except .return and .param_N)
	for (i = num_flow_params; i < func->num_vars; i++) {
		if (flowvar_is_global (func->vars[i]))
			set_add (func->global_vars, i);
	}
	// create dummy defs for local vars
	// defined local vars add their address in local space to the number of
	// statements in the function:
	//     ([num_statements ... num_statements+localsize])
	// with a set element for each def used in the local space
	//
	// temporary vars are pseudo allocated and their addresses are added as for
	// locals
	// add_operand takes care of setting flowaddr for both locals and temps
	for (i = 0; i < func->num_vars; i++) {
		int         j;

		var = func->vars[i];
		if (flowvar_is_global (var) || flowvar_is_param (var)) {
			continue;
		}
		for (j = 0; j < var->op->size; j++) {
			set_add (var->define, var->flowaddr + j);
		}
	}

	set_delete (stuse);
	set_delete (stdef);
}

static int
flow_tempop_kill_aliases_visit (tempop_t *tempop, void *_kill)
{
	set_t      *kill = (set_t *) _kill;
	flowvar_t  *var;
	var = tempop->flowvar;
	if (var)
		set_union (kill, var->define);
	return 0;
}

static int
flow_def_kill_aliases_visit (def_t *def, void *_kill)
{
	set_t      *kill = (set_t *) _kill;
	flowvar_t  *var;
	var = def->flowvar;
	if (var)
		set_union (kill, var->define);
	return 0;
}

static void
flow_kill_aliases (set_t *kill, flowvar_t *var, const set_t *uninit)
{
	operand_t  *op;
	set_t      *tmp;

	set_union (kill, var->define);
	op = var->op;
	tmp = set_new ();
	if (op->op_type == op_temp) {
		tempop_visit_all (&op->o.tempop, 1, flow_tempop_kill_aliases_visit,
						  tmp);
		set_difference (tmp, uninit);
	} else if (op->op_type == op_def) {
		def_visit_all (op->o.def, 1, flow_def_kill_aliases_visit, tmp);
		// don't allow aliases to kill definitions in the entry dummy block
		set_difference (tmp, uninit);
	}
	// merge the alias kills with the current def's kills
	set_union (kill, tmp);
}

static void
flow_reaching_defs (flowgraph_t *graph)
{
	int         i;
	int         changed;
	flownode_t *node;
	statement_t *st;
	set_t      *stdef = set_new ();
	set_t      *stgen = set_new ();
	set_t      *stkill = set_new ();
	set_t      *oldout = set_new ();
	set_t      *gen, *kill, *in, *out, *uninit;
	set_iter_t *var_i;
	set_iter_t *pred_i;
	flowvar_t  *var;

	// First, create out for the entry dummy node using fake statement numbers.
	kill = set_new ();
	for (i = 0; i < graph->func->num_statements; i++)
		set_add (kill, i);
	uninit = set_new ();
	for (i = 0; i < graph->func->num_vars; i++) {
		var = graph->func->vars[i];
		set_union (uninit, var->define);// do not want alias handling here
		set_difference (uninit, kill);	// remove any gens from the function
	}
	graph->nodes[graph->num_nodes]->reaching_defs.out = uninit;
	graph->nodes[graph->num_nodes]->reaching_defs.in = set_new ();
	graph->nodes[graph->num_nodes]->reaching_defs.gen = set_new ();
	graph->nodes[graph->num_nodes]->reaching_defs.kill = set_new ();

	// Calculate gen and kill for each block, and initialize in and out
	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		gen = set_new ();
		kill = set_new ();
		for (st = node->sblock->statements; st; st = st->next) {
			flow_analyze_statement (st, 0, stdef, 0, 0);
			set_empty (stgen);
			set_empty (stkill);
			for (var_i = set_first (stdef); var_i; var_i = set_next (var_i)) {
				var = graph->func->vars[var_i->element];
				flow_kill_aliases (stkill, var, uninit);
				set_remove (stkill, st->number);
				set_add (stgen, st->number);
			}

			set_difference (gen, stkill);
			set_union (gen, stgen);

			set_difference (kill, stgen);
			set_union (kill, stkill);
		}
		node->reaching_defs.gen = gen;
		node->reaching_defs.kill = kill;
		node->reaching_defs.in = set_new ();
		node->reaching_defs.out = set_new ();
	}

	changed = 1;
	while (changed) {
		changed = 0;
		// flow down the graph
		for (i = 0; i < graph->num_nodes; i++) {
			node = graph->nodes[graph->depth_first[i]];
			in = node->reaching_defs.in;
			out = node->reaching_defs.out;
			gen = node->reaching_defs.gen;
			kill = node->reaching_defs.kill;
			for (pred_i = set_first (node->predecessors); pred_i;
				 pred_i = set_next (pred_i)) {
				flownode_t *pred = graph->nodes[pred_i->element];
				set_union (in, pred->reaching_defs.out);
			}
			set_assign (oldout, out);
			set_assign (out, in);
			set_difference (out, kill);
			set_union (out, gen);
			if (!set_is_equivalent (out, oldout))
				changed = 1;
		}
	}
	set_delete (oldout);
	set_delete (stdef);
	set_delete (stgen);
	set_delete (stkill);
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
			live_set_use (stuse, use, def);
			live_set_def (stdef, use, def);
		}
		node->live_vars.use = use;
		node->live_vars.def = def;
		node->live_vars.in = set_new ();
		node->live_vars.out = set_new ();
	}
	// create in for the exit dummy block using the global vars used by the
	// function
	use = set_new ();
	set_assign (use, graph->func->global_vars);
	node = graph->nodes[graph->num_nodes + 1];
	node->live_vars.in = use;
	node->live_vars.out = set_new ();
	node->live_vars.use = set_new ();
	node->live_vars.def = set_new ();

	while (changed) {
		changed = 0;
		// flow UP the graph because live variable analysis uses information
		// from a node's successors rather than its predecessors.
		for (j = graph->num_nodes - 1; j >= 0; j--) {
			node = graph->nodes[graph->depth_first[j]];
			set_empty (tmp);
			for (succ = set_first (node->successors); succ;
				 succ = set_next (succ))
				set_union (tmp, graph->nodes[succ->element]->live_vars.in);
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
flow_uninit_scan_statements (flownode_t *node, set_t *defs, set_t *uninit)
{
	set_t      *stuse;
	set_t      *stdef;
	statement_t *st;
	set_iter_t *var_i;
	flowvar_t  *var;
	operand_t  *op;

	// defs holds only reaching definitions. make it hold only reaching
	// uninitialized definitions
	set_intersection (defs, uninit);
	stuse = set_new ();
	stdef = set_new ();
	for (st = node->sblock->statements; st; st = st->next) {
		flow_analyze_statement (st, stuse, stdef, 0, 0);
		for (var_i = set_first (stuse); var_i; var_i = set_next (var_i)) {
			var = node->graph->func->vars[var_i->element];
			if (set_is_intersecting (defs, var->define)) {
				def_t      *def = flowvar_get_def (var);
				if (def) {
					if (options.warnings.uninited_variable) {
						warning (st->expr, "%s may be used uninitialized",
								 def->name);
					}
				} else {
					bug (st->expr, "st %d, uninitialized temp %s",
						 st->number, operand_string (var->op));
				}
			}
			// avoid repeat warnings in this node
			set_difference (defs, var->define);
		}
		for (var_i = set_first (stdef); var_i; var_i = set_next (var_i)) {
			var = node->graph->func->vars[var_i->element];
			// kill any reaching uninitialized definitions for this variable
			set_difference (defs, var->define);
			if (var->op->op_type == op_temp) {
				op = var->op;
				if (op->o.tempop.alias) {
					var = op->o.tempop.alias->o.tempop.flowvar;
					if (var)
						set_difference (defs, var->define);
				}
				for (op = op->o.tempop.alias_ops; op; op = op->next) {
					var = op->o.tempop.flowvar;
					if (var)
						set_difference (defs, var->define);
				}
			}
		}
	}
	set_delete (stuse);
	set_delete (stdef);
}

static void
flow_uninitialized (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;
	flowvar_t  *var;
	set_iter_t *var_i;
	set_t      *defs;
	set_t      *uninitialized;

	uninitialized = set_new ();
	node = graph->nodes[graph->num_nodes];
	set_assign (uninitialized, node->reaching_defs.out);
	defs = set_new ();

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[graph->depth_first[i]];
		set_empty (defs);
		// collect definitions of all variables "used" in this node. use from
		// the live vars analysis is perfect for the job
		for (var_i = set_first (node->live_vars.use); var_i;
			 var_i = set_next (var_i)) {
			var = graph->func->vars[var_i->element];
			set_union (defs, var->define);
		}
		// interested in only those defintions that actually reach this node
		set_intersection (defs, node->reaching_defs.in);
		// if any of the definitions come from the entry dummy block, then
		// the statements need to be scanned in case an aliasing definition
		// kills the dummy definition before the usage, and also so the line
		// number information can be obtained from the statement.
		if (set_is_intersecting (defs, uninitialized))
			flow_uninit_scan_statements (node, defs, uninitialized);
	}
	set_delete (defs);
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
	if (options.block_dot.dags)
		dump_dot ("dags", graph, dump_dot_flow_dags);
}

static void
flow_cleanup_dags (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		dag_remove_dead_nodes (node->dag);
	}
	if (options.block_dot.dags)
		dump_dot ("cleaned-dags", graph, dump_dot_flow_dags);
}

static sblock_t *
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
			} else if (!strcmp (s->opcode, "<MOVEP>")) {
				flow_add_op_var (use, s->opc);
				if (s->opc->op_type == op_value
					&& s->opc->o.value->lltype == ev_pointer
					&& s->opc->o.value->v.pointer.def) {
					operand_t  *op;
					def_t      *alias;
					ex_pointer_t *ptr = &s->opc->o.value->v.pointer;
					alias = alias_def (ptr->def, ptr->type, ptr->val);
					op = def_operand (alias, ptr->type);
					flow_add_op_var (def, op);
					if (operands)
						operands[0] = op;
					else
						free_operand (op);
				} else {
					if (operands)
						operands[3] = s->opc;
				}
			} else {
				if (s->opc)
					flow_add_op_var (use, s->opc);
			}
			if (kill) {
				set_everything (kill);
			}
			if (operands) {
				if (!strcmp (s->opcode, "<MOVE>"))
					operands[0] = s->opc;
				operands[1] = s->opa;
				operands[2] = s->opb;
				if (strncmp (s->opcode, "<MOVE", 5))
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
flow_find_successors (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;
	sblock_t   *sb;
	statement_t *st;
	sblock_t  **target_list, **target;

	// "convert" the basic blocks connections to flow-graph connections
	for (i = 0; i < graph->num_nodes + 2; i++) {
		node = graph->nodes[i];
		set_empty (node->successors);
		set_empty (node->predecessors);
		set_empty (node->edges);
	}
	graph->num_edges = 0;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		sb = node->sblock;
		st = 0;
		if (sb->statements)
			st = (statement_t *) sb->tail;
		//NOTE: if st is null (the sblock has no statements), statement_is_*
		//will return false
		//FIXME jump/jumpb
		if (statement_is_goto (st) || statement_is_jumpb (st)) {
			// sb's next is never followed.
			target_list = statement_get_targetlist (st);
			for (target = target_list; *target; target++)
				set_add (node->successors, (*target)->flownode->id);
			free (target_list);
		} else if (statement_is_cond (st)) {
			// branch: either sb's next or the conditional statment's
			// target will be followed.
			set_add (node->successors, sb->next->flownode->id);
			target_list = statement_get_targetlist (st);
			for (target = target_list; *target; target++)
				set_add (node->successors, (*target)->flownode->id);
			free (target_list);
		} else if (statement_is_return (st)) {
			// exit from function (dead end)
			// however, make the exit dummy block the node's successor
			set_add (node->successors, graph->num_nodes + 1);
		} else {
			// there is no flow-control statement in sb, so sb's next
			// must be followed
			if (sb->next) {
				set_add (node->successors, sb->next->flownode->id);
			} else {
				bug (0, "code drops off the end of the function");
				// this shouldn't happen
				// however, make the exit dummy block the node's successor
				set_add (node->successors, graph->num_nodes + 1);
			}
		}
		graph->num_edges += set_size (node->successors);
	}
	// set the successor for the entry dummy node to the real entry node
	node = graph->nodes[graph->num_nodes];
	set_add (node->successors, 0);
	graph->num_edges += set_size (node->successors);
}

static void
flow_make_edges (flowgraph_t *graph)
{
	int         i, j;
	flownode_t *node;
	set_iter_t *succ;

	if (graph->edges)
		free (graph->edges);
	graph->edges = malloc (graph->num_edges * sizeof (flowedge_t));
	for (j = 0, i = 0; i < graph->num_nodes + 2; i++) {
		node = graph->nodes[i];
		for (succ = set_first (node->successors); succ;
			 succ = set_next (succ), j++) {
			set_add (node->edges, j);
			graph->edges[j].tail = i;
			graph->edges[j].head = succ->element;
		}
	}
}

static void
flow_find_predecessors (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;
	set_iter_t *succ;

	for (i = 0; i < graph->num_nodes + 2; i++) {
		node = graph->nodes[i];
		for (succ = set_first (node->successors); succ;
			 succ = set_next (succ)) {
			set_add (graph->nodes[succ->element]->predecessors, i);
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
			set_empty (work);
			for (pred = set_first (node->predecessors); pred;
				 pred = set_next (pred))
				set_intersection (work, graph->nodes[pred->element]->dom);
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
		unsigned    m = ss->element;
		set_del_iter (ss);
		set_remove (stack, m);
		node = graph->nodes[m];
		for (pred = set_first (node->predecessors); pred;
			 pred = set_next (pred))
			insert_loop_node (loop, pred->element, stack);
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
			if (set_is_member (node->dom, succ->element)) {
				loop = make_loop (graph, node->id, succ->element);
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
		succ = graph->edges[edge->element].head;
		if (!set_is_member (visited, succ)) {
			set_add (graph->dfst, edge->element);
			df_search (graph, visited, i, succ);
		}
	}
	node->dfn = --*i;
	graph->depth_first[node->dfn] = n;
}

static void
flow_build_dfst (flowgraph_t *graph)
{
	set_t      *visited = set_new ();
	int         i;

	// mark the dummy nodes as visited to keep them out of the dfst
	set_add (visited, graph->num_nodes);
	set_add (visited, graph->num_nodes + 1);

	if (graph->depth_first)
		free (graph->depth_first);
	if (graph->dfst)
		set_delete (graph->dfst);
	graph->depth_first = calloc (graph->num_nodes, sizeof (int));
	graph->dfst = set_new ();
	i = graph->num_nodes;
	df_search (graph, visited, &i, 0);
	set_delete (visited);
}

static int
flow_remove_unreachable_nodes (flowgraph_t *graph)
{
	int         i, j;
	flownode_t *node;

	for (i = 0, j = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		if (node->dfn < 0)	// skip over unreachable nodes
			continue;
		node->id = j;		// new node number
		graph->nodes[j++] = node;
	}
	graph->nodes[j] = graph->nodes[i];			// copy entry dummy node
	graph->nodes[j + 1] = graph->nodes[i + 1];	// copy exit dummy node

	// kill the pointers to unreachable nodes
	for (i = j; i < graph->num_nodes; i++)
		graph->nodes[i + 2] = 0;

	if (j < graph->num_nodes) {
		graph->num_nodes = j;
		return 1;
	}
	return 0;
}

static flownode_t *
flow_make_node (sblock_t *sblock, int id, function_t *func)
{
	flownode_t *node;

	node = new_node ();
	node->predecessors = set_new ();
	node->successors = set_new ();
	node->edges = set_new ();
	node->dom = set_new ();
	node->global_vars = func->global_vars;
	node->id = id;
	node->sblock = sblock;
	if (sblock)
		sblock->flownode = node;
	node->graph = func->graph;
	// Mark the node as unreachable. flow_build_dfst() will mark reachable
	// nodes with a value >= 0
	node->dfn = -1;
	return node;
}

static flowgraph_t *
flow_build_graph (function_t *func)
{
	sblock_t   *sblock = func->sblock;
	flowgraph_t *graph;
	flownode_t *node;
	sblock_t   *sb;
	int         i;
	int         pass = 0;

	graph = new_graph ();
	graph->func = func;
	func->graph = graph;
	for (sb = sblock; sb; sb = sb->next)
		graph->num_nodes++;
	// + 2 for the uninitialized dummy head block and the live dummy end block
	graph->nodes = malloc ((graph->num_nodes + 2) * sizeof (flownode_t *));
	for (i = 0, sb = sblock; sb; i++, sb = sb->next)
		graph->nodes[i] = flow_make_node (sb, i, func);
	// Create the dummy node for detecting uninitialized variables
	node = flow_make_node (0, graph->num_nodes, func);
	graph->nodes[graph->num_nodes] = node;
	// Create the dummy node for making global vars live at function exit
	node = flow_make_node (0, graph->num_nodes + 1, func);
	graph->nodes[graph->num_nodes + 1] = node;

	do {
		if (pass > 1)
			internal_error (0, "too many unreachable node passes");
		flow_find_successors (graph);
		flow_make_edges (graph);
		flow_build_dfst (graph);
		if (options.block_dot.flow)
			dump_dot (va ("flow-%d", pass), graph, dump_dot_flow);
		pass++;
	} while (flow_remove_unreachable_nodes (graph));
	flow_find_predecessors (graph);
	flow_find_dominators (graph);
	flow_find_loops (graph);
	return graph;
}

void
flow_data_flow (function_t *func)
{
	flowgraph_t *graph;

	flow_build_statements (func);
	flow_build_vars (func);
	graph = flow_build_graph (func);
	if (options.block_dot.statements)
		dump_dot ("statements", graph, dump_dot_flow_statements);
	flow_reaching_defs (graph);
	if (options.block_dot.reaching)
		dump_dot ("reaching", graph, dump_dot_flow_reaching);
	flow_live_vars (graph);
	if (options.block_dot.live)
		dump_dot ("live", graph, dump_dot_flow_live);
	flow_uninitialized (graph);
	flow_build_dags (graph);
	flow_cleanup_dags (graph);
	func->sblock = flow_generate (graph);
}
