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

static const char *
get_key (void *_op, void *_tab)
{
	opcode_t *op = (opcode_t *)_op;
	hashtab_t **tab = (hashtab_t **)_tab;
	static char rep[100];
	char *r = rep;
	const char *s = op->name;

	if (tab == &opcode_priority_table) {
		while (*s)
			*r++ = *s++;
		*r++ = op->priority + 2;
		*r = 0;
	} else if (tab == &opcode_priority_type_table_ab) {
		while (*s)
			*r++ = *s++;
		*r++ = op->priority + 2;
		*r++ = op->type_a + 2;
		*r++ = op->type_b + 2;
		*r = 0;
	} else if (tab == &opcode_priority_type_table_abc) {
		while (*s)
			*r++ = *s++;
		*r++ = op->priority + 2;
		*r++ = op->type_a + 2;
		*r++ = op->type_b + 2;
		*r++ = op->type_c + 2;
		*r = 0;
	} else {
		abort ();
	}
	return rep;
}

opcode_t *
PR_Opcode_Find (const char *name, int priority, def_t *var_a, def_t *var_b, def_t *var_c)
{
	opcode_t op;
	hashtab_t **tab;
	char rep[100];

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
		strcpy (rep, get_key (&op, tab));
		//printf ("%s\n", rep);
		return Hash_Find (*tab, rep);
	} else {
		strcpy (rep, get_key (&op, &opcode_priority_table));
		return Hash_Find (opcode_priority_table, rep);
	}
}

void
PR_Opcode_Init_Tables (void)
{
	opcode_t *op;

	PR_Opcode_Init ();
	opcode_priority_table = Hash_NewTable (1021, get_key, 0,
										   &opcode_priority_table);
	opcode_priority_type_table_ab = Hash_NewTable (1021, get_key, 0,
												   &opcode_priority_type_table_ab);
	opcode_priority_type_table_abc = Hash_NewTable (1021, get_key, 0,
													&opcode_priority_type_table_abc);
	for (op = pr_opcodes; op->name; op++) {
		if (op->min_version > options.version)
			continue;
		if (options.version == PROG_ID_VERSION
			&& op->type_c == ev_integer)
			op->type_c = ev_float;
		Hash_Add (opcode_priority_table, op);
		Hash_Add (opcode_priority_type_table_ab, op);
		Hash_Add (opcode_priority_type_table_abc, op);
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
