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

#include "dags.h"
#include "flow.h"
#include "function.h"
#include "qfcc.h"
#include "set.h"
#include "statements.h"
#include "symtab.h"

static flowloop_t *free_loops;
static flownode_t *free_nodes;
static flowgraph_t *free_graphs;

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

static int
is_variable (daglabel_t *var)
{
	operand_t  *o;

	if (!var)
		return 0;

	o = var->op;
	while (o->op_type == op_alias)
		o = o->o.alias;

	if (o->op_type == op_temp)
		return 1;
	if (o->op_type != op_symbol)
		return 0;
	if (o->o.symbol->sy_type == sy_var)
		return 1;
	//FIXME functions? (some are variable)
	return 0;
}

static int
count_operand (operand_t *op)
{
	daglabel_t *var;

	if (!op)
		return 0;
	if (op->op_type == op_label)
		return 0;

	var = operand_label (op);
	// daglabels are initialized with number == 0, and any global daglabel
	// used by a function will always have a number >= 0 after flow analysis,
	// and local daglabels will always be 0 before flow analysis, so use -1
	// to indicate the variable has been counted.
	if (is_variable (var) && var->number != -1) {
		var->number = -1;
		return 1;
	}
	return 0;
}

static void
add_operand (function_t *func, operand_t *op)
{
	daglabel_t *var;

	if (!op)
		return;
	if (op->op_type == op_label)
		return;

	var = operand_label (op);
	// If the daglabel number is still -1, then the daglabel has not yet been
	// added to the list of variables referenced by the function.
	if (is_variable (var) && var->number == -1) {
		var->number = func->num_vars++;
		func->vars[var->number] = var;
	}
}

void
flow_build_vars (function_t *func)
{
	sblock_t   *sblock;
	statement_t *s;
	int         num_vars;

	for (num_vars = 0, sblock = func->sblock; sblock; sblock = sblock->next) {
		for (s = sblock->statements; s; s = s->next) {
			num_vars += count_operand (s->opa);
			num_vars += count_operand (s->opb);
			num_vars += count_operand (s->opc);
		}
	}
	if (!num_vars)
		return;

	func->vars = malloc (num_vars * sizeof (daglabel_t *));

	func->num_vars = 0;	// incremented by add_operand
	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		for (s = sblock->statements; s; s = s->next) {
			add_operand (func, s->opa);
			add_operand (func, s->opb);
			add_operand (func, s->opc);
		}
	}
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
			if (pred)
				set_assign (work, graph->nodes[pred->member]->dom);
			for (pred = set_next (pred); pred; pred = set_next (pred))
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
flow_build_graph (sblock_t *sblock)
{
	flowgraph_t *graph;
	flownode_t *node;
	sblock_t   *sb;
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
