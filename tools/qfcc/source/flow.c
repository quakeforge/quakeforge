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
#include "set.h"
#include "statements.h"
#include "symtab.h"

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

void
flow_build_graph (function_t *func)
{
	sblock_t   *sblock;
	statement_t *st;
	int         num_blocks = 0;

	for (sblock = func->sblock; sblock; sblock = sblock->next)
		sblock->number = num_blocks++;
	func->graph = malloc (num_blocks * sizeof (sblock_t *));
	for (sblock = func->sblock; sblock; sblock = sblock->next)
		func->graph[sblock->number] = sblock;
	func->num_nodes = num_blocks;

	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		if (sblock->statements) {
			st = (statement_t *) sblock->tail;
			//FIXME jump/jumpb
			if (flow_is_goto (st)) {
				sblock->succ = calloc (2, sizeof (sblock_t *));
				sblock->succ[0] = flow_get_target (st);
			} else if (flow_is_cond (st)) {
				sblock->succ = calloc (3, sizeof (sblock_t *));
				sblock->succ[0] = sblock->next;
				sblock->succ[1] = flow_get_target (st);
			} else if (flow_is_return (st)) {
				sblock->succ = calloc (1, sizeof (sblock_t *));
			} else {
				sblock->succ = calloc (2, sizeof (sblock_t *));
				sblock->succ[0] = sblock->next;
			}
		}
	}
	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		int         num_pred;
		sblock_t   *sb, **ss;

		for (num_pred = 0, sb = func->sblock; sb; sb = sb->next) {
			for (ss = sb->succ; *ss; ss++) {
				if (*ss == sblock) {
					num_pred++;
					break;
				}
			}
		}
		sblock->pred = calloc (num_pred + 1, sizeof (sblock_t *));
		for (num_pred = 0, sb = func->sblock; sb; sb = sb->next) {
			for (ss = sb->succ; *ss; ss++) {
				if (*ss == sblock) {
					sblock->pred[num_pred++] = sb;
					break;
				}
			}
		}
	}
}

void
flow_calc_dominators (function_t *func)
{
	set_t      *work;
	sblock_t  **pred;
	int         i;
	int         changed;

	if (!func->num_nodes)
		return;

	// First, create a base set for the initial state of the non-initial nodes
	work = set_new ();
	for (i = 0; i < func->num_nodes; i++)
		set_add (work, i);

	func->graph[0]->dom = set_new ();
	set_add (func->graph[0]->dom, 0);

	// initialize dom for the non-initial nodes
	for (i = 1; i < func->num_nodes; i++) {
		func->graph[i]->dom = set_new ();
		set_assign (func->graph[i]->dom, work);
	}

	do {
		changed = 0;
		for (i = 1; i < func->num_nodes; i++) {
			set_assign (work, func->graph[i]->pred[0]->dom);
			for (pred = func->graph[i]->pred + 1; *pred; pred++)
				set_intersection (work, (*pred)->dom);
			set_add (work, i);
			if (!set_is_equivalent (work, func->graph[i]->dom))
				changed = 1;
			set_assign (func->graph[i]->dom, work);
		}
	} while (changed);
}
