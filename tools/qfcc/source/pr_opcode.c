/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QF/hash.h>

#include "qfcc.h"

hashtab_t *opcode_priority_table;
hashtab_t *opcode_priority_type_table_ab;
hashtab_t *opcode_priority_type_table_abc;

opcode_t *op_done;
opcode_t *op_return;
opcode_t *op_if;
opcode_t *op_ifnot;
opcode_t *op_state;
opcode_t *op_goto;

statref_t *
PR_NewStatref (dstatement_t *st, int field)
{
	statref_t *ref = calloc (1, sizeof (statref_t));
	ref->statement = st;
	ref->field = field;
	return ref;
}

void
PR_AddStatementRef (def_t *def, dstatement_t *st, int field)
{
	if (def) {
		statref_t *ref = PR_NewStatref (st, field);
		ref->next = def->refs;
		def->refs = ref;
	}
}

#define ROTL(x,n) (((x)<<(n))|(x)>>(32-n))

static unsigned long
get_hash (void *_op, void *_tab)
{
	opcode_t *op = (opcode_t *)_op;
	hashtab_t **tab = (hashtab_t **)_tab;
	unsigned long hash;

	if (tab == &opcode_priority_table) {
		hash = op->priority;
	} else if (tab == &opcode_priority_type_table_ab) {
		hash = ~op->priority + ROTL(~op->type_a, 8) + ROTL(~op->type_b, 16);
	} else if (tab == &opcode_priority_type_table_abc) {
		hash = ~op->priority + ROTL(~op->type_a, 8) + ROTL(~op->type_b, 16)
			   + ROTL(~op->type_c, 24);
	} else {
		abort ();
	}
	return hash + Hash_String (op->name);;
}

static int
compare (void *_opa, void *_opb, void *_tab)
{
	opcode_t *opa = (opcode_t *)_opa;
	opcode_t *opb = (opcode_t *)_opb;
	hashtab_t **tab = (hashtab_t **)_tab;
	int cmp;

	if (tab == &opcode_priority_table) {
		cmp = opa->priority == opb->priority;
	} else if (tab == &opcode_priority_type_table_ab) {
		cmp = (opa->priority == opb->priority)
			  && (opa->type_a == opb->type_a)
			  && (opa->type_b == opb->type_b);
	} else if (tab == &opcode_priority_type_table_abc) {
		cmp = (opa->priority == opb->priority)
			  && (opa->type_a == opb->type_a)
			  && (opa->type_b == opb->type_b)
			  && (opa->type_c == opb->type_c);
	} else {
		abort ();
	}
	return cmp && !strcmp (opa->name, opb->name);
}

opcode_t *
PR_Opcode_Find (const char *name, int priority, def_t *var_a, def_t *var_b, def_t *var_c)
{
	opcode_t op;
	hashtab_t **tab;

	op.name = name;
	op.priority = priority;
	if (var_a && var_b && var_c) {
		op.type_a = var_a->type->type;
		op.type_b = var_b->type->type;
		op.type_c = var_c->type->type;
		if (op.type_c == ev_void)
			tab = &opcode_priority_type_table_ab;
		else
			tab = &opcode_priority_type_table_abc;
		return Hash_FindElement (*tab, &op);
	} else {
		return Hash_FindElement (opcode_priority_table, &op);
	}
}

void
PR_Opcode_Init_Tables (void)
{
	opcode_t *op;

	PR_Opcode_Init ();
	opcode_priority_table = Hash_NewTable (1021, 0, 0,
										   &opcode_priority_table);
	opcode_priority_type_table_ab = Hash_NewTable (1021, 0, 0,
												   &opcode_priority_type_table_ab);
	opcode_priority_type_table_abc = Hash_NewTable (1021, 0, 0,
													&opcode_priority_type_table_abc);
	Hash_SetHashCompare (opcode_priority_table, get_hash, compare);
	Hash_SetHashCompare (opcode_priority_type_table_ab, get_hash, compare);
	Hash_SetHashCompare (opcode_priority_type_table_abc, get_hash, compare);
	for (op = pr_opcodes; op->name; op++) {
		if (op->min_version > options.version)
			continue;
		if (options.version == PROG_ID_VERSION
			&& op->type_c == ev_integer)
			op->type_c = ev_float;
		Hash_AddElement (opcode_priority_table, op);
		Hash_AddElement (opcode_priority_type_table_ab, op);
		Hash_AddElement (opcode_priority_type_table_abc, op);
		if (!strcmp (op->name, "<DONE>")) {
			op_done = op;
		} else if (!strcmp (op->name, "<RETURN>")) {
			op_return = op;
		} else if (!strcmp (op->name, "<IF>")) {
			op_if = op;
		} else if (!strcmp (op->name, "<IFNOT>")) {
			op_ifnot = op;
		} else if (!strcmp (op->name, "<STATE>")) {
			op_state = op;
		} else if (!strcmp (op->name, "<GOTO>")) {
			op_goto = op;
		}
	}
}
