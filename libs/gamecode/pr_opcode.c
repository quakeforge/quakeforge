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

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/hash.h"
#include "QF/pr_comp.h"

hashtab_t *opcode_table;

opcode_t    pr_opcodes[] = {
	{"<DONE>", "done", OP_DONE, -1, false, ev_entity, ev_field, ev_void, PROG_ID_VERSION},

	{"*", "mul.f", OP_MUL_F, 2, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"*", "mul.v", OP_MUL_V, 2, false, ev_vector, ev_vector, ev_float, PROG_ID_VERSION},
	{"*", "mul.fv", OP_MUL_FV, 2, false, ev_float, ev_vector, ev_vector, PROG_ID_VERSION},
	{"*", "mul.vf", OP_MUL_VF, 2, false, ev_vector, ev_float, ev_vector, PROG_ID_VERSION},

	{"/", "div.f", OP_DIV_F, 2, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},

	{"+", "add.f", OP_ADD_F, 3, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"+", "add.v", OP_ADD_V, 3, false, ev_vector, ev_vector, ev_vector, PROG_ID_VERSION},
	{"+", "add.s", OP_ADD_S, 3, false, ev_string, ev_string, ev_string, PROG_VERSION},

	{"-", "sub.f", OP_SUB_F, 3, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"-", "sub.v", OP_SUB_V, 3, false, ev_vector, ev_vector, ev_vector, PROG_ID_VERSION},

	{"==", "eq.f", OP_EQ_F, 4, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"==", "eq.v", OP_EQ_V, 4, false, ev_vector, ev_vector, ev_integer, PROG_ID_VERSION},
	{"==", "eq.s", OP_EQ_S, 4, false, ev_string, ev_string, ev_integer, PROG_ID_VERSION},
	{"==", "eq.e", OP_EQ_E, 4, false, ev_entity, ev_entity, ev_integer, PROG_ID_VERSION},
	{"==", "eq.fnc", OP_EQ_FNC, 4, false, ev_func, ev_func, ev_integer, PROG_ID_VERSION},

	{"!=", "ne.f", OP_NE_F, 4, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"!=", "ne.v", OP_NE_V, 4, false, ev_vector, ev_vector, ev_integer, PROG_ID_VERSION},
	{"!=", "ne.s", OP_NE_S, 4, false, ev_string, ev_string, ev_integer, PROG_ID_VERSION},
	{"!=", "ne.e", OP_NE_E, 4, false, ev_entity, ev_entity, ev_integer, PROG_ID_VERSION},
	{"!=", "ne.fnc", OP_NE_FNC, 4, false, ev_func, ev_func, ev_integer, PROG_ID_VERSION},

	{"<=", "le", OP_LE, 4, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{">=", "ge", OP_GE, 4, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"<=", "le.s", OP_LE_S, 4, false, ev_string, ev_string, ev_integer, PROG_VERSION},
	{">=", "ge.s", OP_GE_S, 4, false, ev_string, ev_string, ev_integer, PROG_VERSION},
	{"<", "lt", OP_LT, 4, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{">", "gt", OP_GT, 4, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"<", "lt.s", OP_LT_S, 4, false, ev_string, ev_string, ev_integer, PROG_VERSION},
	{">", "gt.s", OP_GT_S, 4, false, ev_string, ev_string, ev_integer, PROG_VERSION},

	{".", "load.f", OP_LOAD_F, 1, false, ev_entity, ev_field, ev_float, PROG_ID_VERSION},
	{".", "load.v", OP_LOAD_V, 1, false, ev_entity, ev_field, ev_vector, PROG_ID_VERSION},
	{".", "load.s", OP_LOAD_S, 1, false, ev_entity, ev_field, ev_string, PROG_ID_VERSION},
	{".", "load.ent", OP_LOAD_ENT, 1, false, ev_entity, ev_field, ev_entity, PROG_ID_VERSION},
	{".", "load.fld", OP_LOAD_FLD, 1, false, ev_entity, ev_field, ev_field, PROG_ID_VERSION},
	{".", "load.fnc", OP_LOAD_FNC, 1, false, ev_entity, ev_field, ev_func, PROG_ID_VERSION},

	{".", "address", OP_ADDRESS, 1, false, ev_entity, ev_field, ev_pointer, PROG_ID_VERSION},

	{"=", "store.f", OP_STORE_F, 5, true, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"=", "store.v", OP_STORE_V, 5, true, ev_vector, ev_vector, ev_vector, PROG_ID_VERSION},
	{"=", "store.s", OP_STORE_S, 5, true, ev_string, ev_string, ev_string, PROG_ID_VERSION},
	{"=", "store.ent", OP_STORE_ENT, 5, true, ev_entity, ev_entity, ev_entity, PROG_ID_VERSION},
	{"=", "store.fld", OP_STORE_FLD, 5, true, ev_field, ev_field, ev_field, PROG_ID_VERSION},
	{"=", "store.fnc", OP_STORE_FNC, 5, true, ev_func, ev_func, ev_func, PROG_ID_VERSION},

	{"=", "storep.f", OP_STOREP_F, 5, true, ev_pointer, ev_float, ev_float, PROG_ID_VERSION},
	{"=", "storep.v", OP_STOREP_V, 5, true, ev_pointer, ev_vector, ev_vector, PROG_ID_VERSION},
	{"=", "storep.s", OP_STOREP_S, 5, true, ev_pointer, ev_string, ev_string, PROG_ID_VERSION},
	{"=", "storep.ent", OP_STOREP_ENT, 5, true, ev_pointer, ev_entity, ev_entity, PROG_ID_VERSION},
	{"=", "storep.fld", OP_STOREP_FLD, 5, true, ev_pointer, ev_field, ev_field, PROG_ID_VERSION},
	{"=", "storep.fnc", OP_STOREP_FNC, 5, true, ev_pointer, ev_func, ev_func, PROG_ID_VERSION},

	{"<RETURN>", "return", OP_RETURN, -1, false, ev_void, ev_void, ev_void, PROG_ID_VERSION},

	{"!", "not.f", OP_NOT_F, -1, false, ev_float, ev_void, ev_integer, PROG_ID_VERSION},
	{"!", "not.v", OP_NOT_V, -1, false, ev_vector, ev_void, ev_integer, PROG_ID_VERSION},
	{"!", "not.s", OP_NOT_S, -1, false, ev_string, ev_void, ev_integer, PROG_ID_VERSION},
	{"!", "not.ent", OP_NOT_ENT, -1, false, ev_entity, ev_void, ev_integer, PROG_ID_VERSION},
	{"!", "not.fnc", OP_NOT_FNC, -1, false, ev_func, ev_void, ev_integer, PROG_ID_VERSION},

	{"<IF>", "if", OP_IF, -1, false, ev_integer, ev_integer, ev_void, PROG_ID_VERSION},
	{"<IFNOT>", "ifnot", OP_IFNOT, -1, false, ev_integer, ev_integer, ev_void, PROG_ID_VERSION},

// calls returns REG_RETURN
	{"<CALL0>", "call0", OP_CALL0, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL1>", "call1", OP_CALL1, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL2>", "call2", OP_CALL2, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL3>", "call3", OP_CALL3, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL4>", "call4", OP_CALL4, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL5>", "call5", OP_CALL5, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL6>", "call6", OP_CALL6, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL7>", "call7", OP_CALL7, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL8>", "call8", OP_CALL8, -1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},

	{"<STATE>", "state", OP_STATE, -1, false, ev_float, ev_float, ev_void, PROG_ID_VERSION},

	{"<GOTO>", "goto", OP_GOTO, -1, false, ev_float, ev_void, ev_void, PROG_ID_VERSION},

	{"&&", "and", OP_AND, 6, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"||", "or", OP_OR, 6, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},

	{"&", "bitand", OP_BITAND, 2, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"|", "bitor", OP_BITOR, 2, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},

	{"+", "add.i", OP_ADD_I, 3, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"-", "sub.i", OP_SUB_I, 3, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"*", "mul.i", OP_MUL_I, 2, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"/", "div.i", OP_DIV_I, 2, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"&", "bitand.i", OP_BITAND_I, 2, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"|", "bitor.i", OP_BITOR_I, 2, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{">=", "ge.i", OP_GE_I, 4, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"<=", "le.i", OP_LE_I, 4, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{">", "gt.i", OP_GT_I, 4, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"<", "lt.i", OP_LT_I, 4, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"&&", "and.i", OP_AND_I, 6, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"||", "or.i", OP_OR_I, 6, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"!", "not.i", OP_NOT_I, -1, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"==", "eq.i", OP_EQ_I, 4, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"!=", "ne.i", OP_NE_I, 4, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"=", "store.i", OP_STORE_I, 5, true, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"=", "storep.i", OP_STOREP_I, 5, true, ev_pointer, ev_integer, ev_integer, PROG_VERSION},
	{".", "load.i", OP_LOAD_I, 1, false, ev_entity, ev_field, ev_integer, PROG_VERSION},

	{"^", "bitxor.f", OP_BITXOR_F, 2, false, ev_float, ev_float, ev_float, PROG_VERSION},
	{"^", "bitxor.i", OP_BITXOR_I, 2, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"~", "bitnot.f", OP_BITNOT_F, -1, false, ev_float, ev_void, ev_float, PROG_VERSION},
	{"~", "bitnot.i", OP_BITNOT_I, -1, false, ev_integer, ev_void, ev_integer, PROG_VERSION},
	{0},
};

static const char *
get_key (void *_op, void *unused)
{
	opcode_t *op = (opcode_t *)_op;
	static char rep[4];
	char *r = rep;

	*r++ = (op->opcode & 0x7f) + 2;
	*r++ = ((op->opcode >> 7) & 0x7f) + 2;
	*r++ = ((op->opcode >> 14) & 0x3) + 2;
	*r = 0;
	return rep;
}

opcode_t *
PR_Opcode (short opcode)
{
	opcode_t op;
	char rep[100];

	op.opcode = opcode;
	strcpy (rep, get_key (&op, 0));
	return Hash_Find (opcode_table, rep);
}

void
PR_Opcode_Init (void)
{
	opcode_t *op;

	opcode_table = Hash_NewTable (1021, get_key, 0, 0);

	for (op = pr_opcodes; op->name; op++) {
		Hash_Add (opcode_table, op);
	}
}
