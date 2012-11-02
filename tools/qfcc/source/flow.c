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
#if 0
static void
delete_node (flownode_t *node)
{
	if (node->nodes)
		free (node->nodes);
	if (node->predecessors)
		free (node->predecessors);
	if (node->successors)
		free (node->successors);
	node->next = free_nodes;
	free_nodes = node;
}
#endif
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
flow_find_predecessors (flownode_t **node_list, unsigned num_nodes)
{
	unsigned    i, j, k;
	flownode_t *node;

	for (i = 0; i < num_nodes; i++) {
		node = node_list[i];
		for (j = 0; j < num_nodes; j++) {
			unsigned   *succ;
			flownode_t *n;

			n = node_list[j];
			for (succ = n->successors; succ - n->successors < n->num_succ;
				 succ++) {
				if (*succ == i) {
					node->num_pred++;
					break;
				}
			}
		}
		node->predecessors = malloc (node->num_pred * sizeof (flownode_t *));
		for (k = j = 0; j < num_nodes; j++) {
			unsigned   *succ;
			flownode_t *n;

			n = node_list[j];
			for (succ = n->successors; succ - n->successors < n->num_succ;
				 succ++) {
				if (*succ == i) {
					node->predecessors[k++] = j;
					break;
				}
			}
		}
	}
}

static void
flow_calc_dominators (flownode_t **node_list, unsigned num_nodes)
{
	set_t      *work;
	flownode_t *node;
	unsigned    i;
	unsigned   *pred;
	int         changed;

	if (!num_nodes)
		return;

	// First, create a base set for the initial state of the non-initial nodes
	work = set_new ();
	for (i = 0; i < num_nodes; i++)
		set_add (work, i);

	node_list[0]->dom = set_new ();
	set_add (node_list[0]->dom, 0);

	// initialize dom for the non-initial nodes
	for (i = 1; i < num_nodes; i++) {
		node_list[i]->dom = set_new ();
		set_assign (node_list[i]->dom, work);
	}

	do {
		changed = 0;
		for (i = 1; i < num_nodes; i++) {
			node = node_list[i];
			pred = node->predecessors;
			set_assign (work, node_list[*pred]->dom);
			for (pred++; pred - node->predecessors < node->num_pred; pred++)
				set_intersection (work, node_list[*pred]->dom);
			set_add (work, i);
			if (!set_is_equivalent (work, node->dom))
				changed = 1;
			set_assign (node->dom, work);
		}
	} while (changed);
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
make_loop (flownode_t **node_list, unsigned num_nodes, unsigned n, unsigned d)
{
	flowloop_t *loop = new_loop ();
	flownode_t *node;
	set_t      *stack = set_new ();
	unsigned   *pred;

	loop->head = d;
	set_add (loop->nodes, d);
	insert_loop_node (loop, n, stack);
	while (!set_is_empty (stack)) {
		set_iter_t *ss = set_first (stack);
		unsigned    m = ss->member;
		set_del_iter (ss);
		set_remove (stack, m);
		node = node_list[m];
		for (pred = node->predecessors;
			 pred - node->predecessors < node->num_pred; pred++)
			insert_loop_node (loop, *pred, stack);
	}
	set_delete (stack);
	return loop;
}

static flowloop_t *
flow_find_loops (flownode_t **node_list, unsigned num_nodes)
{
	flownode_t *node;
	unsigned   *succ;
	flowloop_t *loop, *l;
	flowloop_t *loop_list = 0;
	unsigned    i;

	for (i = 0; i < num_nodes; i++) {
		node = node_list[i];
		for (succ = node->successors; succ - node->successors < node->num_succ;
			 succ++) {
			if (set_is_member (node->dom, *succ)) {
				loop = make_loop (node_list, num_nodes, node->id, *succ);
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
	return loop_list;
}

static void
df_search (flownode_t *graph, set_t *visited, unsigned *i, unsigned n)
{
	int         j;
	flownode_t *node;

	set_add (visited, n);
	node = graph->siblings[n];
	for (j = 0; j < node->num_succ; j++) {
		if (!set_is_member (visited, node->successors[j])) {
			df_search (graph, visited, i, node->successors[j]);
		}
	}
	graph->depth_first[--*i] = n;
}

static void
flow_depth_first (flownode_t *graph)
{
	unsigned    i = graph->num_siblings;
	set_t      *visited = set_new ();

	graph->depth_first = malloc (graph->num_siblings * sizeof (unsigned));
	df_search (graph, visited, &i, 0);
}

static int
is_predecessor (unsigned m, set_t *I, flownode_t *graph)
{
	flownode_t *node = graph->siblings[m];
	int         i;

	for (i = 0; i < node->num_pred; i++)
		if (!set_is_member (I, node->predecessors[i]))
			return 0;
	return 1;
}

static set_t *
select_nodes (flownode_t *graph, set_t *G, unsigned n)
{
	set_t      *I;
	set_iter_t *m;

	I = set_new ();
	set_add (I, n);
	set_remove (G, n);
	for (m = set_first (G); m; m = set_next (m)) {
		if (m->member == n || !is_predecessor (m->member, I, graph))
			continue;
		set_remove (G, m->member);
		set_add (I, m->member);
	}
	return I;
}

static flownode_t *
flow_reduce (flownode_t *graph)
{
	set_t      *G;
	set_t     **I;
	unsigned    i, j, count = 0;
	int         k;
	flownode_t **node_list = 0;
	flownode_t *node;
	set_iter_t *m;

	if (graph->num_siblings < 2)
		return 0;

	G = set_new ();
	// Initialize G to be the set of all nodes in graph
	for (i = 0; i < graph->num_siblings; i++)
		set_add (G, i);

	// allocate space for the interval sets. There will never be more intervals
	// than nodes in graph.
	I = malloc (graph->num_siblings * sizeof (set_t *));

	for (i = 0; i < graph->num_siblings; i++) {
		unsigned m = graph->depth_first[i];
		if (!set_is_member (G, m))
			continue;
		I[count++] = select_nodes (graph, G, m);
	}

	if (count == graph->num_siblings)
		goto irreducible;

	node_list = malloc (count * sizeof (flownode_t *));
	for (i = 0; i < count; i++) {
		node = new_node ();
		node->siblings = node_list;
		node->num_siblings = count;
		node->id = i;

		node->num_nodes = set_size (I[i]);
		node->nodes = malloc (node->num_nodes * sizeof (flownode_t *));
		for (j = 0, m = set_first (I[i]); m; m = set_next (m), j++) {
			node->nodes[j] = graph->siblings[m->member];
			node->nodes[j]->region = i;
		}

		node_list[node->id] = node;
	}

	for (i = 0; i < count; i++) {
		node = node_list[i];
		set_empty (G);		// G now represents the set of successors of node
		for (j = 0; j < node->num_nodes; j++) {
			flownode_t *n = node->nodes[j];
			for (k = 0; k < n->num_succ; k++) {
				flownode_t *m = n->siblings[n->successors[k]];
				if (m->region != i && !set_is_member (G, m->region))
					set_add (G, m->region);
			}
		}
		node->num_succ = set_size (G);
		node->successors = malloc (node->num_succ * sizeof (unsigned));
		for (j = 0, m = set_first (G); m; j++, m = set_next (m))
			node->successors[j] = m->member;
	}
	flow_find_predecessors (node_list, count);
	flow_depth_first (node_list[0]);
irreducible:
	for (i = 0; i < count; i++)
		set_delete (I[i]);
	free (I);
	set_delete (G);
	if (node_list)
		return node_list[0];
	return 0;
}

void
flow_build_graph (function_t *func)
{
	sblock_t   *sblock;
	statement_t *st;
	flownode_t *node;
	flownode_t **node_list;
	unsigned    num_blocks = 0;
	unsigned    i;

	for (sblock = func->sblock; sblock; sblock = sblock->next)
		sblock->number = num_blocks++;
	func->graph = malloc (num_blocks * sizeof (sblock_t *));
	func->num_nodes = num_blocks;
	node_list = malloc (func->num_nodes * sizeof (flownode_t *));
	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		func->graph[sblock->number] = sblock;

		node = new_node ();
		node->sblocks = func->graph;
		node->siblings = node_list;
		node->num_siblings = num_blocks;
		node->id = sblock->number;
		node->num_nodes = func->num_nodes;

		node_list[node->id] = node;
	}

	// "convert" the basic blocks connections to flow-graph connections
	for (i = 0; i < num_blocks; i++) {
		node = node_list[i];
		sblock = node->sblocks[node->id];
		st = 0;
		if (sblock->statements)
			st = (statement_t *) sblock->tail;
		//FIXME jump/jumpb
		//NOTE: if st is null (the sblock has no statements), flow_is_* will
		//return false
		if (flow_is_goto (st)) {
			// sblock's next is never followed.
			node->num_succ = 1;
			node->successors = calloc (1, sizeof (unsigned));
			node->successors[0] = flow_get_target (st)->number;
		} else if (flow_is_cond (st)) {
			// branch: either sblock's next or the conditional statment's
			// target will be followed.
			node->num_succ = 2;
			node->successors = calloc (2, sizeof (unsigned));
			node->successors[0] = sblock->next->number;
			node->successors[1] = flow_get_target (st)->number;
		} else if (flow_is_return (st)) {
			// exit from function (dead end)
			node->num_succ = 0;
		} else {
			// there is no flow-control statement in sblock, so sblock's next
			// must be followed
			node->num_succ = 1;
			node->successors = calloc (1, sizeof (unsigned));
			node->successors[0] = sblock->next->number;
		}
	}
	flow_find_predecessors (node_list, num_blocks);
	flow_depth_first (node_list[0]);

	flow_calc_dominators (node_list, num_blocks);
	func->loops = flow_find_loops (node_list, num_blocks);
	func->flow = node_list[0];
	while ((node = flow_reduce (func->flow)))
		func->flow = node;
}
