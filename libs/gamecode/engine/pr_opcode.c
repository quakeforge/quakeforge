/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
static const char rcsid[] =
	"$Id$";

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
#include "QF/progs.h"

hashtab_t *opcode_table;

opcode_t    pr_opcodes[] = {
	{"<DONE>", "done", OP_DONE, false, ev_entity, ev_field, ev_void, PROG_ID_VERSION},

	{"*", "mul.f",  OP_MUL_F,  false, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"*", "mul.v",  OP_MUL_V,  false, ev_vector, ev_vector, ev_float, PROG_ID_VERSION},
	{"*", "mul.fv", OP_MUL_FV, false, ev_float, ev_vector, ev_vector, PROG_ID_VERSION},
	{"*", "mul.vf", OP_MUL_VF, false, ev_vector, ev_float, ev_vector, PROG_ID_VERSION},

	{"/", "div.f", OP_DIV_F, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},

	{"+", "add.f", OP_ADD_F, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"+", "add.v", OP_ADD_V, false, ev_vector, ev_vector, ev_vector, PROG_ID_VERSION},
	{"+", "add.s", OP_ADD_S, false, ev_string, ev_string, ev_string, PROG_VERSION},

	{"-", "sub.f", OP_SUB_F, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"-", "sub.v", OP_SUB_V, false, ev_vector, ev_vector, ev_vector, PROG_ID_VERSION},

	{"==", "eq.f",   OP_EQ_F,   false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"==", "eq.v",   OP_EQ_V,   false, ev_vector, ev_vector, ev_integer, PROG_ID_VERSION},
	{"==", "eq.s",   OP_EQ_S,   false, ev_string, ev_string, ev_integer, PROG_ID_VERSION},
	{"==", "eq.e",   OP_EQ_E,   false, ev_entity, ev_entity, ev_integer, PROG_ID_VERSION},
	{"==", "eq.fnc", OP_EQ_FNC, false, ev_func, ev_func, ev_integer, PROG_ID_VERSION},

	{"!=", "ne.f",   OP_NE_F,   false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"!=", "ne.v",   OP_NE_V,   false, ev_vector, ev_vector, ev_integer, PROG_ID_VERSION},
	{"!=", "ne.s",   OP_NE_S,   false, ev_string, ev_string, ev_integer, PROG_ID_VERSION},
	{"!=", "ne.e",   OP_NE_E,   false, ev_entity, ev_entity, ev_integer, PROG_ID_VERSION},
	{"!=", "ne.fnc", OP_NE_FNC, false, ev_func, ev_func, ev_integer, PROG_ID_VERSION},

	{"<=", "le.f", OP_LE,   false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{">=", "ge.f", OP_GE,   false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"<=", "le.s", OP_LE_S, false, ev_string, ev_string, ev_integer, PROG_VERSION},
	{">=", "ge.s", OP_GE_S, false, ev_string, ev_string, ev_integer, PROG_VERSION},
	{"<",  "lt.f", OP_LT,   false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{">",  "gt.f", OP_GT,   false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"<",  "lt.s", OP_LT_S, false, ev_string, ev_string, ev_integer, PROG_VERSION},
	{">",  "gt.s", OP_GT_S, false, ev_string, ev_string, ev_integer, PROG_VERSION},

	{".", "load.f",   OP_LOAD_F,   false, ev_entity, ev_field, ev_float, PROG_ID_VERSION},
	{".", "load.v",   OP_LOAD_V,   false, ev_entity, ev_field, ev_vector, PROG_ID_VERSION},
	{".", "load.s",   OP_LOAD_S,   false, ev_entity, ev_field, ev_string, PROG_ID_VERSION},
	{".", "load.ent", OP_LOAD_ENT, false, ev_entity, ev_field, ev_entity, PROG_ID_VERSION},
	{".", "load.fld", OP_LOAD_FLD, false, ev_entity, ev_field, ev_field, PROG_ID_VERSION},
	{".", "load.fnc", OP_LOAD_FNC, false, ev_entity, ev_field, ev_func, PROG_ID_VERSION},
	{".", "load.i",   OP_LOAD_I,   false, ev_entity, ev_field, ev_integer, PROG_VERSION},
	{".", "load.p",   OP_LOAD_P,   false, ev_entity, ev_field, ev_pointer, PROG_VERSION},

	{".", "loadb.f",   OP_LOADB_F,   false, ev_pointer, ev_integer, ev_float, PROG_VERSION},
	{".", "loadb.v",   OP_LOADB_V,   false, ev_pointer, ev_integer, ev_vector, PROG_VERSION},
	{".", "loadb.s",   OP_LOADB_S,   false, ev_pointer, ev_integer, ev_string, PROG_VERSION},
	{".", "loadb.ent", OP_LOADB_ENT, false, ev_pointer, ev_integer, ev_entity, PROG_VERSION},
	{".", "loadb.fld", OP_LOADB_FLD, false, ev_pointer, ev_integer, ev_field, PROG_VERSION},
	{".", "loadb.fnc", OP_LOADB_FNC, false, ev_pointer, ev_integer, ev_func, PROG_VERSION},
	{".", "loadb.i",   OP_LOADB_I,   false, ev_pointer, ev_integer, ev_integer, PROG_VERSION},
	{".", "loadb.p",   OP_LOADB_P,   false, ev_pointer, ev_integer, ev_pointer, PROG_VERSION},

	{".", "loadbi.f",   OP_LOADBI_F,   false, ev_pointer, ev_short, ev_float, PROG_VERSION},
	{".", "loadbi.v",   OP_LOADBI_V,   false, ev_pointer, ev_short, ev_vector, PROG_VERSION},
	{".", "loadbi.s",   OP_LOADBI_S,   false, ev_pointer, ev_short, ev_string, PROG_VERSION},
	{".", "loadbi.ent", OP_LOADBI_ENT, false, ev_pointer, ev_short, ev_entity, PROG_VERSION},
	{".", "loadbi.fld", OP_LOADBI_FLD, false, ev_pointer, ev_short, ev_field, PROG_VERSION},
	{".", "loadbi.fnc", OP_LOADBI_FNC, false, ev_pointer, ev_short, ev_func, PROG_VERSION},
	{".", "loadbi.i",   OP_LOADBI_I,   false, ev_pointer, ev_short, ev_integer, PROG_VERSION},
	{".", "loadbi.p",   OP_LOADBI_P,   false, ev_pointer, ev_short, ev_pointer, PROG_VERSION},

	{"&", "address", OP_ADDRESS, false, ev_entity, ev_field, ev_pointer, PROG_ID_VERSION},

	{"&", "address.f",   OP_ADDRESS_F,   false, ev_float, ev_void, ev_pointer, PROG_VERSION},
	{"&", "address.v",   OP_ADDRESS_V,   false, ev_vector, ev_void, ev_pointer, PROG_VERSION},
	{"&", "address.s",   OP_ADDRESS_S,   false, ev_string, ev_void, ev_pointer, PROG_VERSION},
	{"&", "address.ent", OP_ADDRESS_ENT, false, ev_entity, ev_void, ev_pointer, PROG_VERSION},
	{"&", "address.fld", OP_ADDRESS_FLD, false, ev_field, ev_void, ev_pointer, PROG_VERSION},
	{"&", "address.fnc", OP_ADDRESS_FNC, false, ev_func, ev_void, ev_pointer, PROG_VERSION},
	{"&", "address.i",   OP_ADDRESS_I,   false, ev_integer, ev_void, ev_pointer, PROG_VERSION},
	{"&", "address.p",   OP_ADDRESS_P,   false, ev_pointer, ev_void, ev_pointer, PROG_VERSION},

	{"&", "lea",  OP_LEA,  false, ev_pointer, ev_integer, ev_pointer, PROG_VERSION},
	{"&", "leai", OP_LEAI, false, ev_pointer, ev_short, ev_pointer, PROG_VERSION},

	{"=", "store.f",   OP_STORE_F,   true, ev_float, ev_float, ev_void, PROG_ID_VERSION},
	{"=", "store.v",   OP_STORE_V,   true, ev_vector, ev_vector, ev_void, PROG_ID_VERSION},
	{"=", "store.s",   OP_STORE_S,   true, ev_string, ev_string, ev_void, PROG_ID_VERSION},
	{"=", "store.ent", OP_STORE_ENT, true, ev_entity, ev_entity, ev_void, PROG_ID_VERSION},
	{"=", "store.fld", OP_STORE_FLD, true, ev_field, ev_field, ev_void, PROG_ID_VERSION},
	{"=", "store.fnc", OP_STORE_FNC, true, ev_func, ev_func, ev_void, PROG_ID_VERSION},
	{"=", "store.i",   OP_STORE_I,   true, ev_integer, ev_integer, ev_void, PROG_VERSION},

	{"=", "storep.f",   OP_STOREP_F,   true, ev_float, ev_pointer, ev_void, PROG_ID_VERSION},
	{"=", "storep.v",   OP_STOREP_V,   true, ev_vector, ev_pointer, ev_void, PROG_ID_VERSION},
	{"=", "storep.s",   OP_STOREP_S,   true, ev_string, ev_pointer, ev_void, PROG_ID_VERSION},
	{"=", "storep.ent", OP_STOREP_ENT, true, ev_entity, ev_pointer, ev_void, PROG_ID_VERSION},
	{"=", "storep.fld", OP_STOREP_FLD, true, ev_field, ev_pointer, ev_void, PROG_ID_VERSION},
	{"=", "storep.fnc", OP_STOREP_FNC, true, ev_func, ev_pointer, ev_void, PROG_ID_VERSION},
	{"=", "storep.i",   OP_STOREP_I,   true, ev_integer, ev_pointer, ev_void, PROG_VERSION},

	{"=", "storeb.f",   OP_STOREB_F,   true, ev_float, ev_pointer, ev_integer, PROG_VERSION},
	{"=", "storeb.v",   OP_STOREB_V,   true, ev_vector, ev_pointer, ev_integer, PROG_VERSION},
	{"=", "storeb.s",   OP_STOREB_S,   true, ev_string, ev_pointer, ev_integer, PROG_VERSION},
	{"=", "storeb.ent", OP_STOREB_ENT, true, ev_entity, ev_pointer, ev_integer, PROG_VERSION},
	{"=", "storeb.fld", OP_STOREB_FLD, true, ev_field, ev_pointer, ev_integer, PROG_VERSION},
	{"=", "storeb.fnc", OP_STOREB_FNC, true, ev_func, ev_pointer, ev_integer, PROG_VERSION},
	{"=", "storeb.i",   OP_STOREB_I,   true, ev_integer, ev_pointer, ev_integer, PROG_VERSION},
	{"=", "storeb.p",   OP_STOREB_P,   true, ev_pointer, ev_pointer, ev_integer, PROG_VERSION},

	{"=", "storebi.f",   OP_STOREBI_F,   true, ev_float, ev_pointer, ev_short, PROG_VERSION},
	{"=", "storebi.v",   OP_STOREBI_V,   true, ev_vector, ev_pointer, ev_short, PROG_VERSION},
	{"=", "storebi.s",   OP_STOREBI_S,   true, ev_string, ev_pointer, ev_short, PROG_VERSION},
	{"=", "storebi.ent", OP_STOREBI_ENT, true, ev_entity, ev_pointer, ev_short, PROG_VERSION},
	{"=", "storebi.fld", OP_STOREBI_FLD, true, ev_field, ev_pointer, ev_short, PROG_VERSION},
	{"=", "storebi.fnc", OP_STOREBI_FNC, true, ev_func, ev_pointer, ev_short, PROG_VERSION},
	{"=", "storebi.i",   OP_STOREBI_I,   true, ev_integer, ev_pointer, ev_short, PROG_VERSION},
	{"=", "storebi.p",   OP_STOREBI_P,   true, ev_pointer, ev_pointer, ev_short, PROG_VERSION},

	{"<RETURN>", "return", OP_RETURN, false, ev_void, ev_void, ev_void, PROG_ID_VERSION},

	{"!", "not.f",   OP_NOT_F,   false, ev_float, ev_void, ev_integer, PROG_ID_VERSION},
	{"!", "not.v",   OP_NOT_V,   false, ev_vector, ev_void, ev_integer, PROG_ID_VERSION},
	{"!", "not.s",   OP_NOT_S,   false, ev_string, ev_void, ev_integer, PROG_ID_VERSION},
	{"!", "not.ent", OP_NOT_ENT, false, ev_entity, ev_void, ev_integer, PROG_ID_VERSION},
	{"!", "not.fnc", OP_NOT_FNC, false, ev_func, ev_void, ev_integer, PROG_ID_VERSION},

	{"<IF>",    "if",    OP_IF,    false, ev_integer, ev_void, ev_void, PROG_ID_VERSION},
	{"<IFNOT>", "ifnot", OP_IFNOT, false, ev_integer, ev_void, ev_void, PROG_ID_VERSION},
	{"<IFBE>",  "ifbe",  OP_IFBE,  true, ev_integer, ev_integer, ev_void, PROG_VERSION},
	{"<IFB>",   "ifb",   OP_IFB,   true, ev_integer, ev_integer, ev_void, PROG_VERSION},
	{"<IFAE>",  "ifae",  OP_IFAE,  true, ev_integer, ev_integer, ev_void, PROG_VERSION},
	{"<IFA>",   "ifa",   OP_IFA,   true, ev_integer, ev_integer, ev_void, PROG_VERSION},

// calls returns REG_RETURN
	{"<CALL0>", "call0", OP_CALL0, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL1>", "call1", OP_CALL1, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL2>", "call2", OP_CALL2, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL3>", "call3", OP_CALL3, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL4>", "call4", OP_CALL4, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL5>", "call5", OP_CALL5, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL6>", "call6", OP_CALL6, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL7>", "call7", OP_CALL7, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},
	{"<CALL8>", "call8", OP_CALL8, false, ev_func, ev_void, ev_void, PROG_ID_VERSION},

	{"<STATE>", "state", OP_STATE, false, ev_float, ev_float, ev_void, PROG_ID_VERSION},

	{"<GOTO>",  "goto",  OP_GOTO,  false, ev_integer, ev_void, ev_void, PROG_ID_VERSION},
	{"<JUMP>",  "jump",  OP_JUMP,  false, ev_integer, ev_void, ev_void, PROG_VERSION},
	{"<JUMPB>", "jumpb", OP_JUMPB, false, ev_pointer, ev_integer, ev_void, PROG_VERSION},

	{"&&", "and", OP_AND, false, ev_float, ev_float, ev_integer, PROG_ID_VERSION},
	{"||", "or",  OP_OR, false,  ev_float, ev_float, ev_integer, PROG_ID_VERSION},

	{"<<", "shl.f", OP_SHL_F, false, ev_float, ev_float, ev_float, PROG_VERSION},
	{">>", "shr.f", OP_SHR_F, false, ev_float, ev_float, ev_float, PROG_VERSION},
	{"<<", "shl.i", OP_SHL_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{">>", "shr.i", OP_SHR_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},

	{"&", "bitand", OP_BITAND, false, ev_float, ev_float, ev_float, PROG_ID_VERSION},
	{"|", "bitor",  OP_BITOR,  false, ev_float, ev_float, ev_float, PROG_ID_VERSION},

	{"+", "add.i", OP_ADD_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"-", "sub.i", OP_SUB_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"*", "mul.i", OP_MUL_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"/", "div.i", OP_DIV_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"%", "mod_i", OP_MOD_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"%", "mod.f", OP_MOD_F, false, ev_float, ev_float, ev_float, PROG_VERSION},
	{"&", "bitand.i", OP_BITAND_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"|", "bitor.i",  OP_BITOR_I,  false, ev_integer, ev_integer, ev_integer, PROG_VERSION},

	{">=", "ge.i", OP_GE_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"<=", "le.i", OP_LE_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{">",  "gt.i", OP_GT_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"<",  "lt.i", OP_LT_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},

	{">=", "ge.ui", OP_GE_UI, false, ev_uinteger, ev_uinteger, ev_integer, PROG_VERSION},
	{"<=", "le.ui", OP_LE_UI, false, ev_uinteger, ev_uinteger, ev_integer, PROG_VERSION},
	{">",  "gt.ui", OP_GT_UI, false, ev_uinteger, ev_uinteger, ev_integer, PROG_VERSION},
	{"<",  "lt.ui", OP_LT_UI, false, ev_uinteger, ev_uinteger, ev_integer, PROG_VERSION},

	{"&&", "and.i", OP_AND_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"||", "or.i",  OP_OR_I,  false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"!",  "not.i", OP_NOT_I, false, ev_integer, ev_void, ev_integer, PROG_VERSION},
	{"==", "eq.i",  OP_EQ_I,  false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"!=", "ne.i",  OP_NE_I,  false, ev_integer, ev_integer, ev_integer, PROG_VERSION},

	{"^", "bitxor.f", OP_BITXOR_F, false, ev_float, ev_float, ev_float, PROG_VERSION},
	{"~", "bitnot.f", OP_BITNOT_F, false, ev_float, ev_void, ev_float, PROG_VERSION},
	{"^", "bitxor.i", OP_BITXOR_I, false, ev_integer, ev_integer, ev_integer, PROG_VERSION},
	{"~", "bitnot.i", OP_BITNOT_I, false, ev_integer, ev_void, ev_integer, PROG_VERSION},
	{0},
};


static unsigned long
opcode_get_hash (void *op, void *unused)
{
	return ((opcode_t *)op)->opcode;
}

static int
opcode_compare (void *_opa, void *_opb, void *unused)
{
	opcode_t   *opa = (opcode_t *)_opa;
	opcode_t   *opb = (opcode_t *)_opb;

	return opa->opcode == opb->opcode;
}

opcode_t *
PR_Opcode (short opcode)
{
	opcode_t	op;

	op.opcode = opcode;
	return Hash_FindElement (opcode_table, &op);
}

void
PR_Opcode_Init (void)
{
	opcode_t   *op;

	opcode_table = Hash_NewTable (1021, 0, 0, 0);
	Hash_SetHashCompare (opcode_table, opcode_get_hash, opcode_compare);

	for (op = pr_opcodes; op->name; op++) {
		Hash_AddElement (opcode_table, op);
	}
}

static inline void
check_branch (progs_t *pr, dstatement_t *st, opcode_t *op, short offset)
{
	int         address = st - pr->pr_statements;
	
	address += offset;
	if (address < 0 || address >= pr->progs->numstatements)
		PR_Error (pr, "PR_Check_Opcodes: invalid branch (statement %ld: %s)\n",
				  (long)(st - pr->pr_statements), op->opname);
}

static inline void
check_global (progs_t *pr, dstatement_t *st, opcode_t *op, etype_t type,
			  unsigned short operand)
{
	switch (type) {
		case ev_short:
			break;
		case ev_void:
			//FIXME need better "not used flags"
			if (operand && st->op != OP_RETURN && st->op != OP_DONE)
				PR_Error (pr, "PR_Check_Opcodes: non-zero global index in "
						  "void operand (statement %ld: %s)\n",
						  (long)(st - pr->pr_statements), op->opname);
			break;
		default:
			if (operand >= pr->progs->numglobals)
				PR_Error (pr, "PR_Check_Opcodes: out of bounds global index "
						  "(statement %ld: %s)\n",
						  (long)(st - pr->pr_statements), op->opname);
			break;
	}
}

void
PR_Check_Opcodes (progs_t *pr)
{
	opcode_t   *op;
	dstatement_t *st;

	for (st = pr->pr_statements;
		 st - pr->pr_statements < pr->progs->numstatements;
		 st++) {
		op = PR_Opcode (st->op);
		if (!op) {
			PR_Error (pr,
					  "PR_Check_Opcodes: unknown opcode %d at statement %ld\n",
					  st->op, (long)(st - pr->pr_statements));
		}
		switch (st->op) {
			case OP_IF:
			case OP_IFNOT:
				check_global (pr, st, op, op->type_a, st->a);
				check_branch (pr, st, op, st->b);
				break;
			case OP_GOTO:
				check_branch (pr, st, op, st->a);
				break;
			default:
				check_global (pr, st, op, op->type_a, st->a);
				check_global (pr, st, op, op->type_b, st->b);
				check_global (pr, st, op, op->type_c, st->c);
				break;
		}
	}
}
