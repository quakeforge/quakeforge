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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/pr_comp.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "compat.h"

static hashtab_t *opcode_table;

VISIBLE const pr_ushort_t pr_type_size[ev_type_count] = {
	1,			// ev_void
	1,			// ev_string
	1,			// ev_float
	3,			// ev_vector
	1,			// ev_entity
	1,			// ev_field
	1,			// ev_func
	1,			// ev_pointer
	4,			// ev_quat
	1,			// ev_integer
	1,			// ev_uinteger
	0,			// ev_short        value in opcode
	2,			// ev_double
	0,			// ev_invalid      not a valid/simple type
};

VISIBLE const char * const pr_type_name[ev_type_count] = {
	"void",
	"string",
	"float",
	"vector",
	"entity",
	"field",
	"function",
	"pointer",
	"quaternion",
	"integer",
	"uinteger",
	"short",
	"double",
	"invalid",
};

// default format is "%Ga, %Gb, %gc"
// V  global_string, contents, void
// G  global_string, contents
// g  global_string, no contents
// s  as short
// O  address + short
// P  function parameter
// F  function (must come before any P)
// R  return value
// E  entity + field (%Eab)
//
// a  operand a
// b  operand b
// c  operand c
// x  place holder for P (padding)
// 0-7 parameter index (for P)
VISIBLE const opcode_t pr_opcodes[] = {
	{"<DONE>", "done", OP_DONE, false,	// OP_DONE is actually the same as
	 ev_entity, ev_field, ev_void,		// OP_RETURN, the types are bogus
	 PROG_ID_VERSION,
	 "%Va",
	},

	{"*", "mul.d", OP_MUL_D, false,
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	{"*", "mul.f", OP_MUL_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	{"*", "mul.v", OP_MUL_V, false,
	 ev_vector, ev_vector, ev_float,
	 PROG_ID_VERSION,
	},
	{"*", "mul.fv", OP_MUL_FV, false,
	 ev_float, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	{"*", "mul.vf", OP_MUL_VF, false,
	 ev_vector, ev_float, ev_vector,
	 PROG_ID_VERSION,
	},
	{"*", "mul.dv", OP_MUL_DV, false,
	 ev_double, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	{"*", "mul.vd", OP_MUL_VD, false,
	 ev_vector, ev_double, ev_vector,
	 PROG_ID_VERSION,
	},
	{"*", "mul.q", OP_MUL_Q, false,
	 ev_quat, ev_quat, ev_quat,
	 PROG_VERSION,
	},
	{"*", "mul.fq", OP_MUL_FQ, false,
	 ev_float, ev_quat, ev_quat,
	 PROG_VERSION,
	},
	{"*", "mul.qf", OP_MUL_QF, false,
	 ev_quat, ev_float, ev_quat,
	 PROG_VERSION,
	},
	{"*", "mul.dq", OP_MUL_DQ, false,
	 ev_double, ev_quat, ev_quat,
	 PROG_VERSION,
	},
	{"*", "mul.qd", OP_MUL_QD, false,
	 ev_quat, ev_double, ev_quat,
	 PROG_VERSION,
	},
	{"*", "mul.qv", OP_MUL_QV, false,
	 ev_quat, ev_vector, ev_vector,
	 PROG_VERSION,
	},

	{"~", "conj.q", OP_CONJ_Q, false,
	 ev_quat, ev_invalid, ev_quat,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	{"/", "div.f", OP_DIV_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	{"/", "div.d", OP_DIV_D, false,
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	{"%", "rem.d", OP_REM_D, false,
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	{"%%", "mod.d", OP_MOD_D, false,
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},

	{"+", "add.d", OP_ADD_D, false,
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	{"+", "add.f", OP_ADD_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	{"+", "add.v", OP_ADD_V, false,
	 ev_vector, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	{"+", "add.q", OP_ADD_Q, false,
	 ev_quat, ev_quat, ev_quat,
	 PROG_VERSION,
	},
	{"+", "add.s", OP_ADD_S, false,
	 ev_string, ev_string, ev_string,
	 PROG_VERSION,
	},

	{"-", "sub.d", OP_SUB_D, false,
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	{"-", "sub.f", OP_SUB_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	{"-", "sub.v", OP_SUB_V, false,
	 ev_vector, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	{"-", "sub.q", OP_SUB_Q, false,
	 ev_quat, ev_quat, ev_quat,
	 PROG_VERSION,
	},

	{"==", "eq.d", OP_EQ_D, false,
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	{"==", "eq.f", OP_EQ_F, false,
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	{"==", "eq.v", OP_EQ_V, false,
	 ev_vector, ev_vector, ev_integer,
	 PROG_ID_VERSION,
	},
	{"==", "eq.q", OP_EQ_Q, false,
	 ev_quat, ev_quat, ev_integer,
	 PROG_VERSION,
	},
	{"==", "eq.s", OP_EQ_S, false,
	 ev_string, ev_string, ev_integer,
	 PROG_ID_VERSION,
	},
	{"==", "eq.e", OP_EQ_E, false,
	 ev_entity, ev_entity, ev_integer,
	 PROG_ID_VERSION,
	},
	{"==", "eq.fn", OP_EQ_FN, false,
	 ev_func, ev_func, ev_integer,
	 PROG_ID_VERSION,
	},

	{"!=", "ne.d", OP_NE_D, false,
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	{"!=", "ne.f", OP_NE_F, false,
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	{"!=", "ne.v", OP_NE_V, false,
	 ev_vector, ev_vector, ev_integer,
	 PROG_ID_VERSION,
	},
	{"!=", "ne.q", OP_NE_Q, false,
	 ev_quat, ev_quat, ev_integer,
	 PROG_VERSION,
	},
	{"!=", "ne.s", OP_NE_S, false,
	 ev_string, ev_string, ev_integer,
	 PROG_ID_VERSION,
	},
	{"!=", "ne.e", OP_NE_E, false,
	 ev_entity, ev_entity, ev_integer,
	 PROG_ID_VERSION,
	},
	{"!=", "ne.fn", OP_NE_FN, false,
	 ev_func, ev_func, ev_integer,
	 PROG_ID_VERSION,
	},

	{"<=", "le.d", OP_LE_D,	false,
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	{"<=", "le.f", OP_LE_F,	false,
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	{">=", "ge.d", OP_GE_D,	false,
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	{">=", "ge.f", OP_GE_F,	false,
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	{"<=", "le.s", OP_LE_S,	false,
	 ev_string, ev_string, ev_integer,
	 PROG_VERSION,
	},
	{">=", "ge.s", OP_GE_S,	false,
	 ev_string, ev_string, ev_integer,
	 PROG_VERSION,
	},
	{"<", "lt.d", OP_LT_D,	false,
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	{"<", "lt.f", OP_LT_F,	false,
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	{">", "gt.d", OP_GT_D,	false,
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	{">", "gt.f", OP_GT_F,	false,
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	{"<", "lt.s", OP_LT_S,	false,
	 ev_string, ev_string, ev_integer,
	 PROG_VERSION,
	},
	{">", "gt.s", OP_GT_S,	false,
	 ev_string, ev_string, ev_integer,
	 PROG_VERSION,
	},

	{".", "load.f", OP_LOAD_F, false,
	 ev_entity, ev_field, ev_float,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",//FIXME %E more flexible?
	},
	{".", "load.d", OP_LOAD_D, false,
	 ev_entity, ev_field, ev_double,
	 PROG_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	{".", "load.v", OP_LOAD_V, false,
	 ev_entity, ev_field, ev_vector,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	{".", "load.q", OP_LOAD_Q, false,
	 ev_entity, ev_field, ev_quat,
	 PROG_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	{".", "load.s", OP_LOAD_S, false,
	 ev_entity, ev_field, ev_string,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	{".", "load.ent", OP_LOAD_ENT, false,
	 ev_entity, ev_field, ev_entity,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	{".", "load.fld", OP_LOAD_FLD, false,
	 ev_entity, ev_field, ev_field,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	{".", "load.fn", OP_LOAD_FN, false,
	 ev_entity, ev_field, ev_func,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	{".", "load.i", OP_LOAD_I, false,
	 ev_entity, ev_field, ev_integer,
	 PROG_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	{".", "load.p", OP_LOAD_P, false,
	 ev_entity, ev_field, ev_pointer,
	 PROG_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},

	{".", "loadb.d", OP_LOADB_D, false,
	 ev_pointer, ev_integer, ev_double,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.f", OP_LOADB_F, false,
	 ev_pointer, ev_integer, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.v", OP_LOADB_V, false,
	 ev_pointer, ev_integer, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.q", OP_LOADB_Q, false,
	 ev_pointer, ev_integer, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.s", OP_LOADB_S, false,
	 ev_pointer, ev_integer, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.ent", OP_LOADB_ENT, false,
	 ev_pointer, ev_integer, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.fld", OP_LOADB_FLD, false,
	 ev_pointer, ev_integer, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.fn", OP_LOADB_FN, false,
	 ev_pointer, ev_integer, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.i", OP_LOADB_I, false,
	 ev_pointer, ev_integer, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	{".", "loadb.p", OP_LOADB_P, false,
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},

	{".", "loadbi.d", OP_LOADBI_D, false,
	 ev_pointer, ev_short, ev_double,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.f", OP_LOADBI_F, false,
	 ev_pointer, ev_short, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.v", OP_LOADBI_V, false,
	 ev_pointer, ev_short, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.q", OP_LOADBI_Q, false,
	 ev_pointer, ev_short, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.s", OP_LOADBI_S, false,
	 ev_pointer, ev_short, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.ent", OP_LOADBI_ENT, false,
	 ev_pointer, ev_short, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.fld", OP_LOADBI_FLD, false,
	 ev_pointer, ev_short, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.fn", OP_LOADBI_FN, false,
	 ev_pointer, ev_short, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.i", OP_LOADBI_I, false,
	 ev_pointer, ev_short, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	{".", "loadbi.p", OP_LOADBI_P, false,
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},

	{"&", "address", OP_ADDRESS, false,
	 ev_entity, ev_field, ev_pointer,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},

	{"&", "address", OP_ADDRESS_VOID, false,
	 ev_void, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.d", OP_ADDRESS_D, false,
	 ev_double, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.f", OP_ADDRESS_F, false,
	 ev_float, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.v", OP_ADDRESS_V, false,
	 ev_vector, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.q", OP_ADDRESS_Q, false,
	 ev_quat, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.s", OP_ADDRESS_S, false,
	 ev_string, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.ent", OP_ADDRESS_ENT, false,
	 ev_entity, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.fld", OP_ADDRESS_FLD, false,
	 ev_field, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.fn", OP_ADDRESS_FN, false,
	 ev_func, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.i", OP_ADDRESS_I, false,
	 ev_integer, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"&", "address.p", OP_ADDRESS_P, false,
	 ev_pointer, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	{"&", "lea", OP_LEA, false,
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "(%Ga + %Gb), %gc",
	},
	{"&", "leai", OP_LEAI, false,
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "(%Ga + %sb), %gc",
	},

	{"<CONV>", "conv.if", OP_CONV_IF, false,
	 ev_integer, ev_invalid, ev_float,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"<CONV>", "conv.fi", OP_CONV_FI, false,
	 ev_float, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"<CONV>", "conv.id", OP_CONV_ID, false,
	 ev_integer, ev_invalid, ev_double,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"<CONV>", "conv.di", OP_CONV_DI, false,
	 ev_double, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"<CONV>", "conv.fd", OP_CONV_FD, false,
	 ev_float, ev_invalid, ev_double,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"<CONV>", "conv.df", OP_CONV_DF, false,
	 ev_double, ev_invalid, ev_float,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	{"=", "store.d", OP_STORE_D, true,
	 ev_double, ev_double, ev_invalid,
	 PROG_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.f", OP_STORE_F, true,
	 ev_float, ev_float, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.v", OP_STORE_V, true,
	 ev_vector, ev_vector, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.q", OP_STORE_Q, true,
	 ev_quat, ev_quat, ev_invalid,
	 PROG_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.s", OP_STORE_S, true,
	 ev_string, ev_string, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.ent", OP_STORE_ENT, true,
	 ev_entity, ev_entity, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.fld", OP_STORE_FLD, true,
	 ev_field, ev_field, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.fn", OP_STORE_FN, true,
	 ev_func, ev_func, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.i", OP_STORE_I, true,
	 ev_integer, ev_integer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, %gb",
	},
	{"=", "store.p", OP_STORE_P, true,
	 ev_pointer, ev_pointer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, %gb",
	},

	{".=", "storep.d", OP_STOREP_D, true,
	 ev_double, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.f", OP_STOREP_F, true,
	 ev_float, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.v", OP_STOREP_V, true,
	 ev_vector, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.q", OP_STOREP_Q, true,
	 ev_quat, ev_pointer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.s", OP_STOREP_S, true,
	 ev_string, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.ent", OP_STOREP_ENT, true,
	 ev_entity, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.fld", OP_STOREP_FLD, true,
	 ev_field, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.fn", OP_STOREP_FN, true,
	 ev_func, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.i", OP_STOREP_I, true,
	 ev_integer, ev_pointer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, *%Gb",
	},
	{".=", "storep.p", OP_STOREP_P, true,
	 ev_pointer, ev_pointer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, *%Gb",
	},

	{".=", "storeb.d", OP_STOREB_D, true,
	 ev_double, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.f", OP_STOREB_F, true,
	 ev_float, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.v", OP_STOREB_V, true,
	 ev_vector, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.q", OP_STOREB_Q, true,
	 ev_quat, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.s", OP_STOREB_S, true,
	 ev_string, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.ent", OP_STOREB_ENT, true,
	 ev_entity, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.fld", OP_STOREB_FLD, true,
	 ev_field, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.fn", OP_STOREB_FN, true,
	 ev_func, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.i", OP_STOREB_I, true,
	 ev_integer, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	{".=", "storeb.p", OP_STOREB_P, true,
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},

	{".=", "storebi.d", OP_STOREBI_D, true,
	 ev_double, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.f", OP_STOREBI_F, true,
	 ev_float, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.v", OP_STOREBI_V, true,
	 ev_vector, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.q", OP_STOREBI_Q, true,
	 ev_quat, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.s", OP_STOREBI_S, true,
	 ev_string, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.ent", OP_STOREBI_ENT, true,
	 ev_entity, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.fld", OP_STOREBI_FLD, true,
	 ev_field, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.fn", OP_STOREBI_FN, true,
	 ev_func, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.i", OP_STOREBI_I, true,
	 ev_integer, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	{".=", "storebi.p", OP_STOREBI_P, true,
	 ev_pointer, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},

	{"<RETURN>", "return", OP_RETURN, false,
	 ev_void, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ra",
	},

	{"<RETURN_V>", "return", OP_RETURN_V, false,
	 ev_invalid, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "",
	},

	{"!", "not.d", OP_NOT_D, false,
	 ev_double, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"!", "not.f", OP_NOT_F, false,
	 ev_float, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	{"!", "not.v", OP_NOT_V, false,
	 ev_vector, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	{"!", "not.q", OP_NOT_Q, false,
	 ev_quat, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"!", "not.s", OP_NOT_S, false,
	 ev_string, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	{"!", "not.ent", OP_NOT_ENT, false,
	 ev_entity, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	{"!", "not.fn", OP_NOT_FN, false,
	 ev_func, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	{"!", "not.p", OP_NOT_P, false,
	 ev_pointer, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	{"<IF>", "if", OP_IF, false,
	 ev_integer, ev_short, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	{"<IFNOT>", "ifnot", OP_IFNOT, false,
	 ev_integer, ev_short, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	{"<IFBE>", "ifbe", OP_IFBE, true,
	 ev_integer, ev_short, ev_invalid,
	 PROG_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	{"<IFB>", "ifb", OP_IFB, true,
	 ev_integer, ev_short, ev_invalid,
	 PROG_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	{"<IFAE>", "ifae", OP_IFAE, true,
	 ev_integer, ev_short, ev_invalid,
	 PROG_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	{"<IFA>", "ifa", OP_IFA, true,
	 ev_integer, ev_short, ev_invalid,
	 PROG_VERSION,
	 "%Ga branch %sb (%Ob)",
	},

// calls returns REG_RETURN
	{"<CALL0>", "call0", OP_CALL0, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa ()",
	},
	{"<CALL1>", "call1", OP_CALL1, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x)",
	},
	{"<CALL2>", "call2", OP_CALL2, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x)",
	},
	{"<CALL3>", "call3", OP_CALL3, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x)",
	},
	{"<CALL4>", "call4", OP_CALL4, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x)",
	},
	{"<CALL5>", "call5", OP_CALL5, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x)",
	},
	{"<CALL6>", "call6", OP_CALL6, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x)",
	},
	{"<CALL7>", "call7", OP_CALL7, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x, %P6x)",
	},
	{"<CALL8>", "call8", OP_CALL8, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x, %P6x, %P7x)",
	},
	{"<RCALL1>", "rcall1", OP_RCALL1, false,
	 ev_func, ev_void, ev_invalid,
	 PROG_VERSION,
	 "%Fa (%P0b)",
	},
	{"<RCALL2>", "rcall2", OP_RCALL2, false,
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c)",
	},
	{"<RCALL3>", "rcall3", OP_RCALL3, false,
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x)",
	},
	{"<RCALL4>", "rcall4", OP_RCALL4, false,
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x)",
	},
	{"<RCALL5>", "rcall5", OP_RCALL5, false,
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x)",
	},
	{"<RCALL6>", "rcall6", OP_RCALL6, false,
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x)",
	},
	{"<RCALL7>", "rcall7", OP_RCALL7, false,
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x, %P6x)",
	},
	{"<RCALL8>", "rcall8", OP_RCALL8, false,
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x, %P6x, %P7x)",
	},

	{"<STATE>", "state", OP_STATE, false,
	 ev_float, ev_func, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %Gb",
	},

	{"<STATE>", "state.f", OP_STATE_F, false,
	 ev_float, ev_func, ev_float,
	 PROG_VERSION,
	 "%Ga, %Gb, %Gc",
	},

	{"<GOTO>", "goto", OP_GOTO, false,
	 ev_short, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "branch %sa (%Oa)",
	},
	{"<JUMP>", "jump", OP_JUMP, false,
	 ev_integer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<JUMPB>", "jumpb", OP_JUMPB, false,
	 ev_void, ev_integer, ev_invalid,
	 PROG_VERSION,
	 "%Ga[%Gb]",
	},

	{"&&", "and.f", OP_AND, false,
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	{"||", "or.f", OP_OR, false,
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},

	{"<<", "shl.f", OP_SHL_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},
	{">>", "shr.f", OP_SHR_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},
	{"<<", "shl.i", OP_SHL_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{">>", "shr.i", OP_SHR_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{">>", "shr.u", OP_SHR_U, false,
	 ev_uinteger, ev_integer, ev_uinteger,
	 PROG_VERSION,
	},

	{"&", "bitand", OP_BITAND, false,
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	{"|", "bitor", OP_BITOR, false,
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},

	{"+", "add.i", OP_ADD_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"-", "sub.i", OP_SUB_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"*", "mul.i", OP_MUL_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"/", "div.i", OP_DIV_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"%", "rem.i", OP_REM_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"%%", "mod.i", OP_MOD_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"&", "bitand.i", OP_BITAND_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"|", "bitor.i", OP_BITOR_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},

	{"%", "rem.f", OP_REM_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},

	{"%%", "mod.f", OP_MOD_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},

	{">=", "ge.i", OP_GE_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"<=", "le.i", OP_LE_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{">", "gt.i", OP_GT_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"<", "lt.i", OP_LT_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},

	{"&&", "and.i", OP_AND_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"||", "or.i", OP_OR_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"!", "not.i", OP_NOT_I, false,
	 ev_integer, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"==", "eq.i", OP_EQ_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"!=", "ne.i", OP_NE_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},

	{">=", "ge.u", OP_GE_U, false,
	 ev_uinteger, ev_uinteger, ev_integer,
	 PROG_VERSION,
	},
	{"<=", "le.u", OP_LE_U, false,
	 ev_uinteger, ev_uinteger, ev_integer,
	 PROG_VERSION,
	},
	{">", "gt.u", OP_GT_U, false,
	 ev_uinteger, ev_uinteger, ev_integer,
	 PROG_VERSION,
	},
	{"<", "lt.u", OP_LT_U, false,
	 ev_uinteger, ev_uinteger, ev_integer,
	 PROG_VERSION,
	},

	{"^", "bitxor.f", OP_BITXOR_F, false,
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},
	{"~", "bitnot.f", OP_BITNOT_F, false,
	 ev_float, ev_invalid, ev_float,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	{"^", "bitxor.i", OP_BITXOR_I, false,
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	{"~", "bitnot.i", OP_BITNOT_I, false,
	 ev_integer, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	{">=", "ge.p", OP_GE_P, false,
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	{"<=", "le.p", OP_LE_P, false,
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	{">", "gt.p", OP_GT_P, false,
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	{"<", "lt.p", OP_LT_P, false,
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	{"==", "eq.p", OP_EQ_P, false,
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	{"!=", "ne.p", OP_NE_P, false,
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},

	{"<MOVE>", "movei", OP_MOVEI, true,
	 ev_void, ev_short, ev_void,
	 PROG_VERSION,
	 "%Ga, %sb, %gc",
	},
	{"<MOVEP>", "movep", OP_MOVEP, true,
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %Gb, %Gc",
	},
	{"<MOVEP>", "movepi", OP_MOVEPI, true,
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %sb, %Gc",
	},
	{"<MEMSET>", "memseti", OP_MEMSETI, true,
	 ev_integer, ev_short, ev_void,
	 PROG_VERSION,
	 "%Ga, %sb, %gc",
	},
	{"<MEMSETP>", "memsetp", OP_MEMSETP, true,
	 ev_integer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %Gb, %Gc",
	},
	{"<MEMSETP>", "memsetpi", OP_MEMSETPI, true,
	 ev_integer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %sb, %Gc",
	},

	{"<PUSH>", "push.s", OP_PUSH_S, false,
	 ev_string, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<PUSH>", "push.f", OP_PUSH_F, false,
	 ev_float, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<PUSH>", "push.v", OP_PUSH_V, false,
	 ev_vector, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<PUSH>", "push.ent", OP_PUSH_ENT, false,
	 ev_entity, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<PUSH>", "push.fld", OP_PUSH_FLD, false,
	 ev_field, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<PUSH>", "push.fn", OP_PUSH_FN, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<PUSH>", "push.p", OP_PUSH_P, false,
	 ev_pointer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<PUSH>", "push.q", OP_PUSH_Q, false,
	 ev_quat, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	{"<PUSH>", "push.i", OP_PUSH_I, false,
	 ev_integer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},

	{"<PUSH>", "pushb.s", OP_PUSHB_S, false,
	 ev_pointer, ev_integer, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<PUSH>", "pushb.f", OP_PUSHB_F, false,
	 ev_pointer, ev_integer, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<PUSH>", "pushb.v", OP_PUSHB_V, false,
	 ev_pointer, ev_integer, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<PUSH>", "pushb.ent", OP_PUSHB_ENT, false,
	 ev_pointer, ev_integer, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<PUSH>", "pushb.fld", OP_PUSHB_FLD, false,
	 ev_pointer, ev_integer, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<PUSH>", "pushb.fn", OP_PUSHB_FN, false,
	 ev_pointer, ev_integer, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<PUSH>", "pushb.p", OP_PUSHB_P, false,
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<PUSH>", "pushb.q", OP_PUSHB_Q, false,
	 ev_pointer, ev_integer, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<PUSH>", "pushb.i", OP_PUSHB_I, false,
	 ev_pointer, ev_integer, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},

	{"<PUSH>", "pushbi.s", OP_PUSHBI_S, false,
	 ev_pointer, ev_short, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<PUSH>", "pushbi.f", OP_PUSHBI_F, false,
	 ev_pointer, ev_short, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<PUSH>", "pushbi.v", OP_PUSHBI_V, false,
	 ev_pointer, ev_short, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<PUSH>", "pushbi.ent", OP_PUSHBI_ENT, false,
	 ev_pointer, ev_short, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<PUSH>", "pushbi.fld", OP_PUSHBI_FLD, false,
	 ev_pointer, ev_short, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<PUSH>", "pushbi.fn", OP_PUSHBI_FN, false,
	 ev_pointer, ev_short, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<PUSH>", "pushbi.p", OP_PUSHBI_P, false,
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<PUSH>", "pushbi.q", OP_PUSHBI_Q, false,
	 ev_pointer, ev_short, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<PUSH>", "pushbi.i", OP_PUSHBI_I, false,
	 ev_pointer, ev_short, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},

	{"<POP>", "pop.s", OP_POP_S, false,
	 ev_string, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	{"<POP>", "pop.f", OP_POP_F, false,
	 ev_float, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	{"<POP>", "pop.v", OP_POP_V, false,
	 ev_vector, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	{"<POP>", "pop.ent", OP_POP_ENT, false,
	 ev_entity, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	{"<POP>", "pop.fld", OP_POP_FLD, false,
	 ev_field, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	{"<POP>", "pop.fn", OP_POP_FN, false,
	 ev_func, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	{"<POP>", "pop.p", OP_POP_P, false,
	 ev_pointer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	{"<POP>", "pop.q", OP_POP_Q, false,
	 ev_quat, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	{"<POP>", "pop.i", OP_POP_I, false,
	 ev_integer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},

	{"<POP>", "popb.s", OP_POPB_S, false,
	 ev_pointer, ev_integer, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<POP>", "popb.f", OP_POPB_F, false,
	 ev_pointer, ev_integer, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<POP>", "popb.v", OP_POPB_V, false,
	 ev_pointer, ev_integer, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<POP>", "popb.ent", OP_POPB_ENT, false,
	 ev_pointer, ev_integer, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<POP>", "popb.fld", OP_POPB_FLD, false,
	 ev_pointer, ev_integer, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<POP>", "popb.fn", OP_POPB_FN, false,
	 ev_pointer, ev_integer, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<POP>", "popb.p", OP_POPB_P, false,
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<POP>", "popb.q", OP_POPB_Q, false,
	 ev_pointer, ev_integer, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	{"<POP>", "popb.i", OP_POPB_I, false,
	 ev_pointer, ev_integer, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},

	{"<POP>", "popbi.s", OP_POPBI_S, false,
	 ev_pointer, ev_short, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<POP>", "popbi.f", OP_POPBI_F, false,
	 ev_pointer, ev_short, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<POP>", "popbi.v", OP_POPBI_V, false,
	 ev_pointer, ev_short, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<POP>", "popbi.ent", OP_POPBI_ENT, false,
	 ev_pointer, ev_short, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<POP>", "popbi.fld", OP_POPBI_FLD, false,
	 ev_pointer, ev_short, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<POP>", "popbi.fn", OP_POPBI_FN, false,
	 ev_pointer, ev_short, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<POP>", "popbi.p", OP_POPBI_P, false,
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<POP>", "popbi.q", OP_POPBI_Q, false,
	 ev_pointer, ev_short, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	{"<POP>", "popbi.i", OP_POPBI_I, false,
	 ev_pointer, ev_short, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},

	// end of table
	{0},
};


static uintptr_t
opcode_get_hash (const void *op, void *unused)
{
	return ((opcode_t *)op)->opcode;
}

static int
opcode_compare (const void *_opa, const void *_opb, void *unused)
{
	opcode_t   *opa = (opcode_t *)_opa;
	opcode_t   *opb = (opcode_t *)_opb;

	return opa->opcode == opb->opcode;
}

opcode_t *
PR_Opcode (pr_short_t opcode)
{
	opcode_t	op;

	op.opcode = opcode;
	return Hash_FindElement (opcode_table, &op);
}

VISIBLE void
PR_Opcode_Init (void)
{
	const opcode_t *op;

	if (opcode_table) {
		// already initialized
		return;
	}
	opcode_table = Hash_NewTable (1021, 0, 0, 0);
	Hash_SetHashCompare (opcode_table, opcode_get_hash, opcode_compare);

	for (op = pr_opcodes; op->name; op++) {
		Hash_AddElement (opcode_table, (void *) op);
	}
}

static inline void
check_branch (progs_t *pr, dstatement_t *st, opcode_t *op, short offset)
{
	pr_int_t    address = st - pr->pr_statements;

	address += offset;
	if (address < 0 || (pr_uint_t) address >= pr->progs->numstatements)
		PR_Error (pr, "PR_Check_Opcodes: invalid branch (statement %ld: %s)",
				  (long)(st - pr->pr_statements), op->opname);
}

static int
is_vector_parameter_store (progs_t *pr, dstatement_t *st,
						   unsigned short operand)
{
	int         i;

	if (st->op != OP_STORE_V)
		return 0;
	if (operand != st->a)
		return 0;
	for (i = 0; i < MAX_PARMS; i++)
		if (st->b == pr->pr_params[i] - pr->pr_globals)
			return 1;
	return 0;
}

#define ISDENORM(x) ((x) && !((x) & 0x7f800000))

static inline void
check_global (progs_t *pr, dstatement_t *st, opcode_t *op, etype_t type,
			  unsigned short operand, int check_denorm)
{
	const char *msg;
	pr_def_t   *def;

	switch (type) {
		case ev_short:
			break;
		case ev_invalid:
			if (operand) {
				msg = "non-zero global index in invalid operand";
				goto error;
			}
			break;
		default:
			if (operand + (unsigned) pr_type_size[type]
				> pr->progs->numglobals) {
				if (operand >= pr->progs->numglobals
					|| !is_vector_parameter_store (pr, st, operand)) {
					msg = "out of bounds global index";
					goto error;
				}
			}
			if (type != ev_float || !check_denorm)
				break;
			if (!ISDENORM (G_INT (pr, operand))
				|| G_UINT(pr, operand) == 0x80000000)
				break;
			if ((def = PR_GlobalAtOfs (pr, operand))
				&& (def->type & ~DEF_SAVEGLOBAL) != ev_float) {
				// FTEqcc uses store.f parameters of most types :/
				break;
			}
			if (!pr->denorm_found) {
				pr->denorm_found = 1;
				if (pr_boundscheck->int_val) {
					Sys_Printf ("DENORMAL floats detected, these progs might "
								"not work. Good luck.\n");
					return;
				}
				msg = "DENORMAL float detected. These progs are probably "
					"using qccx arrays and integers. If just simple arrays "
					"are being used, then they should work, but if "
					"internal.qc is used, they most definitely will NOT. To"
					"allow these progs to be used, set pr_boundscheck to 1.";
				goto error;
			}
			break;
	}
	return;
error:
	PR_PrintStatement (pr, st, 0);
	PR_Error (pr, "PR_Check_Opcodes: %s (statement %ld: %s)", msg,
			  (long)(st - pr->pr_statements), op->opname);
}

static void
check_global_size (progs_t *pr, dstatement_t *st, opcode_t *op,
				   unsigned short size, unsigned short operand)
{
	const char *msg;
	if (operand + size > pr->progs->numglobals) {
		msg = "out of bounds global index";
		goto error;
	}
	return;
error:
	PR_PrintStatement (pr, st, 0);
	PR_Error (pr, "PR_Check_Opcodes: %s (statement %ld: %s)", msg,
			  (long)(st - pr->pr_statements), op->opname);
}

int
PR_Check_Opcodes (progs_t *pr)
{
	opcode_t   *op;
	dstatement_t *st;
	int         state_ok = 0;
	int         pushpop_ok = 0;
	pr_uint_t   i;

	if (pr->globals.time && pr->globals.self && pr->fields.nextthink != -1
		&& pr->fields.think != -1 && pr->fields.frame != -1) {
		state_ok = 1;
	}
	if (pr->globals.stack) {
		pushpop_ok = 1;
	}

	//FIXME need to decide if I really want to always do static bounds checking
	// the only problem is that it slows progs load a little, but it's the only
	// way to check for qccx' evil
	if (0 && !pr_boundscheck->int_val) {
		for (i = 0, st = pr->pr_statements; i < pr->progs->numstatements;
			 st++, i++) {
			op = PR_Opcode (st->op);
			if (!op) {
				PR_Error (pr, "PR_Check_Opcodes: unknown opcode %d at "
						  "statement %ld", st->op,
						  (long)(st - pr->pr_statements));
			}
			if ((st->op == OP_STATE || st->op == OP_STATE_F) && !state_ok) {
				PR_Error (pr, "PR_Check_Opcodes: %s used with missing fields "
						  "or globals", op->opname);
			}
			if ((strequal(op->name, "<PUSH>") || strequal(op->name, "<POP>"))
				&& !pushpop_ok) {
				PR_Error (pr, "PR_Check_Opcodes: %s used with missing .stack "
						  "globals", op->opname);
			}
		}
	} else {
		for (i = 0, st = pr->pr_statements; i < pr->progs->numstatements;
			 st++, i++) {
			op = PR_Opcode (st->op);
			if (!op) {
				PR_Error (pr, "PR_Check_Opcodes: unknown opcode %d at "
						  "statement %ld", st->op,
						  (long)(st - pr->pr_statements));
			}
			switch (st->op) {
				case OP_IF:
				case OP_IFNOT:
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_branch (pr, st, op, st->b);
					break;
				case OP_GOTO:
					check_branch (pr, st, op, st->a);
					break;
				case OP_DONE:
				case OP_RETURN:
					check_global (pr, st, op, ev_integer, st->a, 1);
					check_global (pr, st, op, ev_void, st->b, 0);
					check_global (pr, st, op, ev_void, st->c, 0);
					break;
				case OP_RCALL1:
					check_global (pr, st, op, ev_void, st->c, 1);
				case OP_RCALL2:
				case OP_RCALL3:
				case OP_RCALL4:
				case OP_RCALL5:
				case OP_RCALL6:
				case OP_RCALL7:
				case OP_RCALL8:
					if (st->op > OP_RCALL1)
						check_global (pr, st, op, ev_integer, st->c, 1);
					check_global (pr, st, op, ev_integer, st->b, 1);
					check_global (pr, st, op, ev_func, st->a, 1);
					break;
				case OP_STATE:
				case OP_STATE_F:
					if (!state_ok) {
						PR_Error (pr, "PR_Check_Opcodes: %s used with missing "
								  "fields or globals", op->opname);
					}
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_global (pr, st, op, op->type_b, st->b, 1);
					check_global (pr, st, op, op->type_c, st->c, 1);
					break;
				case OP_MOVEI:
					check_global_size (pr, st, op, st->b, st->a);
					check_global_size (pr, st, op, st->b, st->c);
					break;
				case OP_MEMSETI:
					check_global_size (pr, st, op, st->b, st->c);
					break;
				case OP_PUSHB_F:
				case OP_PUSHB_S:
				case OP_PUSHB_ENT:
				case OP_PUSHB_FLD:
				case OP_PUSHB_FN:
				case OP_PUSHB_I:
				case OP_PUSHB_P:
				case OP_PUSHB_V:
				case OP_PUSHB_Q:
				case OP_PUSHBI_F:
				case OP_PUSHBI_S:
				case OP_PUSHBI_ENT:
				case OP_PUSHBI_FLD:
				case OP_PUSHBI_FN:
				case OP_PUSHBI_I:
				case OP_PUSHBI_P:
				case OP_PUSHBI_V:
				case OP_PUSHBI_Q:
					// op->type_c is used for selecting the operator during
					// compilation, but is invalid when running
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_global (pr, st, op, op->type_b, st->b, 1);
					check_global (pr, st, op, ev_invalid, st->c, 1);
					break;
				case OP_POP_F:
				case OP_POP_FLD:
				case OP_POP_ENT:
				case OP_POP_S:
				case OP_POP_FN:
				case OP_POP_I:
				case OP_POP_P:
				case OP_POP_V:
				case OP_POP_Q:
					// don't want to check for denormal floats, otherwise
					// OP_POP_* could use the defualt rule
					check_global (pr, st, op, op->type_a, st->a, 0);
					check_global (pr, st, op, ev_invalid, st->b, 1);
					check_global (pr, st, op, ev_invalid, st->c, 1);
					break;
				case OP_POPB_F:
				case OP_POPB_S:
				case OP_POPB_ENT:
				case OP_POPB_FLD:
				case OP_POPB_FN:
				case OP_POPB_I:
				case OP_POPB_P:
				case OP_POPB_V:
				case OP_POPB_Q:
				case OP_POPBI_F:
				case OP_POPBI_S:
				case OP_POPBI_ENT:
				case OP_POPBI_FLD:
				case OP_POPBI_FN:
				case OP_POPBI_I:
				case OP_POPBI_P:
				case OP_POPBI_V:
				case OP_POPBI_Q:
					// op->type_c is used for selecting the operator during
					// compilation, but is invalid when running
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_global (pr, st, op, op->type_b, st->b, 1);
					check_global (pr, st, op, ev_invalid, st->c, 1);
					break;
				default:
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_global (pr, st, op, op->type_b, st->b,
								  op->opcode != OP_STORE_F);
					check_global (pr, st, op, op->type_c, st->c, 0);
					break;
			}
		}
	}
	return 1;
}
