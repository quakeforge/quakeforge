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

hashtab_t *opcode_table;
hashtab_t *opcode_priority_table;
hashtab_t *opcode_priority_type_table_ab;
hashtab_t *opcode_priority_type_table_abc;

opcode_t *op_done;
opcode_t *op_return;
opcode_t *op_if;
opcode_t *op_ifnot;
opcode_t *op_state;
opcode_t *op_goto;

opcode_t    pr_opcodes[] = {
	{"<DONE>", "DONE", OP_DONE, -1, false, &def_entity, &def_field, &def_void},

	{"*", "MUL_F", OP_MUL_F, 2, false, &def_float, &def_float, &def_float},
	{"*", "MUL_V", OP_MUL_V, 2, false, &def_vector, &def_vector, &def_float},
	{"*", "MUL_FV", OP_MUL_FV, 2, false, &def_float, &def_vector, &def_vector},
	{"*", "MUL_VF", OP_MUL_VF, 2, false, &def_vector, &def_float, &def_vector},

	{"/", "DIV_F", OP_DIV_F, 2, false, &def_float, &def_float, &def_float},

	{"+", "ADD_F", OP_ADD_F, 3, false, &def_float, &def_float, &def_float},
	{"+", "ADD_V", OP_ADD_V, 3, false, &def_vector, &def_vector, &def_vector},
	{"+", "ADD_S", OP_ADD_S, 3, false, &def_string, &def_string, &def_string},

	{"-", "SUB_F", OP_SUB_F, 3, false, &def_float, &def_float, &def_float},
	{"-", "SUB_V", OP_SUB_V, 3, false, &def_vector, &def_vector, &def_vector},

	{"==", "EQ_F", OP_EQ_F, 4, false, &def_float, &def_float, &def_float},
	{"==", "EQ_V", OP_EQ_V, 4, false, &def_vector, &def_vector, &def_float},
	{"==", "EQ_S", OP_EQ_S, 4, false, &def_string, &def_string, &def_float},
	{"==", "EQ_E", OP_EQ_E, 4, false, &def_entity, &def_entity, &def_float},
	{"==", "EQ_FNC", OP_EQ_FNC, 4, false, &def_function, &def_function, &def_float},

	{"!=", "NE_F", OP_NE_F, 4, false, &def_float, &def_float, &def_float},
	{"!=", "NE_V", OP_NE_V, 4, false, &def_vector, &def_vector, &def_float},
	{"!=", "NE_S", OP_NE_S, 4, false, &def_string, &def_string, &def_float},
	{"!=", "NE_E", OP_NE_E, 4, false, &def_entity, &def_entity, &def_float},
	{"!=", "NE_FNC", OP_NE_FNC, 4, false, &def_function, &def_function, &def_float},

	{"<=", "LE", OP_LE, 4, false, &def_float, &def_float, &def_float},
	{">=", "GE", OP_GE, 4, false, &def_float, &def_float, &def_float},
	{"<", "LT", OP_LT, 4, false, &def_float, &def_float, &def_float},
	{">", "GT", OP_GT, 4, false, &def_float, &def_float, &def_float},

	{".", "INDIRECT", OP_LOAD_F, 1, false, &def_entity, &def_field, &def_float},
	{".", "INDIRECT", OP_LOAD_V, 1, false, &def_entity, &def_field, &def_vector},
	{".", "INDIRECT", OP_LOAD_S, 1, false, &def_entity, &def_field, &def_string},
	{".", "INDIRECT", OP_LOAD_ENT, 1, false, &def_entity, &def_field, &def_entity},
	{".", "INDIRECT", OP_LOAD_FLD, 1, false, &def_entity, &def_field, &def_field},
	{".", "INDIRECT", OP_LOAD_FNC, 1, false, &def_entity, &def_field, &def_function},

	{".", "ADDRESS", OP_ADDRESS, 1, false, &def_entity, &def_field, &def_pointer},

	{"=", "STORE_F", OP_STORE_F, 5, true, &def_float, &def_float, &def_float},
	{"=", "STORE_V", OP_STORE_V, 5, true, &def_vector, &def_vector, &def_vector},
	{"=", "STORE_S", OP_STORE_S, 5, true, &def_string, &def_string, &def_string},
	{"=", "STORE_ENT", OP_STORE_ENT, 5, true, &def_entity, &def_entity, &def_entity},
	{"=", "STORE_FLD", OP_STORE_FLD, 5, true, &def_field, &def_field, &def_field},
	{"=", "STORE_FNC", OP_STORE_FNC, 5, true, &def_function, &def_function, &def_function},

	{"=", "STOREP_F", OP_STOREP_F, 5, true, &def_pointer, &def_float, &def_float},
	{"=", "STOREP_V", OP_STOREP_V, 5, true, &def_pointer, &def_vector, &def_vector},
	{"=", "STOREP_S", OP_STOREP_S, 5, true, &def_pointer, &def_string, &def_string},
	{"=", "STOREP_ENT", OP_STOREP_ENT, 5, true, &def_pointer, &def_entity, &def_entity},
	{"=", "STOREP_FLD", OP_STOREP_FLD, 5, true, &def_pointer, &def_field, &def_field},
	{"=", "STOREP_FNC", OP_STOREP_FNC, 5, true, &def_pointer, &def_function, &def_function},

	{"<RETURN>", "RETURN", OP_RETURN, -1, false, &def_void, &def_void, &def_void},

	{"!", "NOT_F", OP_NOT_F, -1, false, &def_float, &def_void, &def_float},
	{"!", "NOT_V", OP_NOT_V, -1, false, &def_vector, &def_void, &def_float},
	{"!", "NOT_S", OP_NOT_S, -1, false, &def_string, &def_void, &def_float},
	{"!", "NOT_ENT", OP_NOT_ENT, -1, false, &def_entity, &def_void, &def_float},
	{"!", "NOT_FNC", OP_NOT_FNC, -1, false, &def_function, &def_void, &def_float},

	{"<IF>", "IF", OP_IF, -1, false, &def_float, &def_float, &def_void},
	{"<IFNOT>", "IFNOT", OP_IFNOT, -1, false, &def_float, &def_float, &def_void},

// calls returns REG_RETURN
	{"<CALL0>", "CALL0", OP_CALL0, -1, false, &def_function, &def_void, &def_void},
	{"<CALL1>", "CALL1", OP_CALL1, -1, false, &def_function, &def_void, &def_void},
	{"<CALL2>", "CALL2", OP_CALL2, -1, false, &def_function, &def_void, &def_void},
	{"<CALL3>", "CALL3", OP_CALL3, -1, false, &def_function, &def_void, &def_void},
	{"<CALL4>", "CALL4", OP_CALL4, -1, false, &def_function, &def_void, &def_void},
	{"<CALL5>", "CALL5", OP_CALL5, -1, false, &def_function, &def_void, &def_void},
	{"<CALL6>", "CALL6", OP_CALL6, -1, false, &def_function, &def_void, &def_void},
	{"<CALL7>", "CALL7", OP_CALL7, -1, false, &def_function, &def_void, &def_void},
	{"<CALL8>", "CALL8", OP_CALL8, -1, false, &def_function, &def_void, &def_void},

	{"<STATE>", "STATE", OP_STATE, -1, false, &def_float, &def_float, &def_void},

	{"<GOTO>", "GOTO", OP_GOTO, -1, false, &def_float, &def_void, &def_void},

	{"&&", "AND", OP_AND, 6, false, &def_float, &def_float, &def_float},
	{"||", "OR", OP_OR, 6, false, &def_float, &def_float, &def_float},

	{"&", "BITAND", OP_BITAND, 2, false, &def_float, &def_float, &def_float},
	{"|", "BITOR", OP_BITOR, 2, false, &def_float, &def_float, &def_float},
};

/*
	PR_Statement

	Emits a primitive statement, returning the var it places it's value in
*/
def_t *
PR_Statement (opcode_t * op, def_t * var_a, def_t * var_b)
{
	dstatement_t	*statement;
	def_t			*var_c;

	statement = &statements[numstatements];
	numstatements++;

	statement_linenums[statement - statements] = pr_source_line;
	statement->op = op->opcode;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if (op->type_c == &def_void || op->right_associative) {
		// ifs, gotos, and assignments don't need vars allocated
		var_c = NULL;
		statement->c = 0;
	} else {	// allocate result space
		var_c = malloc (sizeof (def_t));
		memset (var_c, 0, sizeof (def_t));
		var_c->ofs = numpr_globals;
		var_c->type = op->type_c->type;

		statement->c = numpr_globals;
		numpr_globals += type_size[op->type_c->type->type];
	}

	if (op->right_associative)
		return var_a;

	return var_c;
}

static char *
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
		*r++ = op->type_a->type->type + 2;
		*r++ = op->type_b->type->type + 2;
		*r = 0;
	} else if (tab == &opcode_priority_type_table_abc) {
		while (*s)
			*r++ = *s++;
		*r++ = op->priority + 2;
		*r++ = op->type_a->type->type + 2;
		*r++ = op->type_b->type->type + 2;
		*r++ = (op->type_c->type ? op->type_c->type->type : -1) + 2;
		*r = 0;
	} else if (tab == &opcode_table) {
		*r++ = (op->opcode & 0xff) + 2;
		*r++ = ((op->opcode >> 8) & 0xff) + 2;
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
		op.type_a = var_a;
		op.type_b = var_b;
		op.type_c = var_c;
		if (op.type_c->type && op.type_c->type->type == ev_void)
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

opcode_t *
PR_Opcode (short opcode)
{
	opcode_t op;
	char rep[100];

	op.opcode = opcode;
	strcpy (rep, get_key (&op, &opcode_table));
	return Hash_Find (opcode_table, rep);
}

void
PR_Opcode_Init (void)
{
	int i;

	opcode_table = Hash_NewTable (1021, get_key, 0, &opcode_table);
	opcode_priority_table = Hash_NewTable (1021, get_key, 0,
										   &opcode_priority_table);
	opcode_priority_type_table_ab = Hash_NewTable (1021, get_key, 0,
												   &opcode_priority_type_table_ab);
	opcode_priority_type_table_abc = Hash_NewTable (1021, get_key, 0,
													&opcode_priority_type_table_abc);
	for (i = 0; i < sizeof (pr_opcodes) / sizeof (pr_opcodes[0]); i++) {
		opcode_t *op = &pr_opcodes[i];
		Hash_Add (opcode_table, op);
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
