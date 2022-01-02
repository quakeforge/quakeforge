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
#include "QF/pr_comp.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "compat.h"

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
	// OP_DONE is actually the same as OP_RETURN, the types are bogus
	[OP_DONE] = {"<DONE>", "done",
	 ev_entity, ev_field, ev_void,
	 PROG_ID_VERSION,
	 "%Va",
	},

	[OP_MUL_D] = {"*", "mul.d",
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	[OP_MUL_F] = {"*", "mul.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_MUL_V] = {"*", "mul.v",
	 ev_vector, ev_vector, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_MUL_FV] = {"*", "mul.fv",
	 ev_float, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_MUL_VF] = {"*", "mul.vf",
	 ev_vector, ev_float, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_MUL_DV] = {"*", "mul.dv",
	 ev_double, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_MUL_VD] = {"*", "mul.vd",
	 ev_vector, ev_double, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_MUL_Q] = {"*", "mul.q",
	 ev_quat, ev_quat, ev_quat,
	 PROG_VERSION,
	},
	[OP_MUL_FQ] = {"*", "mul.fq",
	 ev_float, ev_quat, ev_quat,
	 PROG_VERSION,
	},
	[OP_MUL_QF] = {"*", "mul.qf",
	 ev_quat, ev_float, ev_quat,
	 PROG_VERSION,
	},
	[OP_MUL_DQ] = {"*", "mul.dq",
	 ev_double, ev_quat, ev_quat,
	 PROG_VERSION,
	},
	[OP_MUL_QD] = {"*", "mul.qd",
	 ev_quat, ev_double, ev_quat,
	 PROG_VERSION,
	},
	[OP_MUL_QV] = {"*", "mul.qv",
	 ev_quat, ev_vector, ev_vector,
	 PROG_VERSION,
	},

	[OP_CONJ_Q] = {"~", "conj.q",
	 ev_quat, ev_invalid, ev_quat,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	[OP_DIV_F] = {"/", "div.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_DIV_D] = {"/", "div.d",
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	[OP_REM_D] = {"%", "rem.d",
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	[OP_MOD_D] = {"%%", "mod.d",
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},

	[OP_ADD_D] = {"+", "add.d",
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	[OP_ADD_F] = {"+", "add.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_ADD_V] = {"+", "add.v",
	 ev_vector, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_ADD_Q] = {"+", "add.q",
	 ev_quat, ev_quat, ev_quat,
	 PROG_VERSION,
	},
	[OP_ADD_S] = {"+", "add.s",
	 ev_string, ev_string, ev_string,
	 PROG_VERSION,
	},

	[OP_SUB_D] = {"-", "sub.d",
	 ev_double, ev_double, ev_double,
	 PROG_VERSION,
	},
	[OP_SUB_F] = {"-", "sub.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_SUB_V] = {"-", "sub.v",
	 ev_vector, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_SUB_Q] = {"-", "sub.q",
	 ev_quat, ev_quat, ev_quat,
	 PROG_VERSION,
	},

	[OP_EQ_D] = {"==", "eq.d",
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	[OP_EQ_F] = {"==", "eq.f",
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_EQ_V] = {"==", "eq.v",
	 ev_vector, ev_vector, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_EQ_Q] = {"==", "eq.q",
	 ev_quat, ev_quat, ev_integer,
	 PROG_VERSION,
	},
	[OP_EQ_S] = {"==", "eq.s",
	 ev_string, ev_string, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_EQ_E] = {"==", "eq.e",
	 ev_entity, ev_entity, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_EQ_FN] = {"==", "eq.fn",
	 ev_func, ev_func, ev_integer,
	 PROG_ID_VERSION,
	},

	[OP_NE_D] = {"!=", "ne.d",
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	[OP_NE_F] = {"!=", "ne.f",
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_NE_V] = {"!=", "ne.v",
	 ev_vector, ev_vector, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_NE_Q] = {"!=", "ne.q",
	 ev_quat, ev_quat, ev_integer,
	 PROG_VERSION,
	},
	[OP_NE_S] = {"!=", "ne.s",
	 ev_string, ev_string, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_NE_E] = {"!=", "ne.e",
	 ev_entity, ev_entity, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_NE_FN] = {"!=", "ne.fn",
	 ev_func, ev_func, ev_integer,
	 PROG_ID_VERSION,
	},

	[OP_LE_D] = {"<=", "le.d",
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	[OP_LE_F] = {"<=", "le.f",
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_GE_D] = {">=", "ge.d",
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	[OP_GE_F] = {">=", "ge.f",
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_LE_S] = {"<=", "le.s",
	 ev_string, ev_string, ev_integer,
	 PROG_VERSION,
	},
	[OP_GE_S] = {">=", "ge.s",
	 ev_string, ev_string, ev_integer,
	 PROG_VERSION,
	},
	[OP_LT_D] = {"<", "lt.d",
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	[OP_LT_F] = {"<", "lt.f",
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_GT_D] = {">", "gt.d",
	 ev_double, ev_double, ev_integer,
	 PROG_VERSION,
	},
	[OP_GT_F] = {">", "gt.f",
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_LT_S] = {"<", "lt.s",
	 ev_string, ev_string, ev_integer,
	 PROG_VERSION,
	},
	[OP_GT_S] = {">", "gt.s",
	 ev_string, ev_string, ev_integer,
	 PROG_VERSION,
	},

	[OP_LOAD_F] = {".", "load.f",
	 ev_entity, ev_field, ev_float,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",//FIXME %E more flexible?
	},
	[OP_LOAD_D] = {".", "load.d",
	 ev_entity, ev_field, ev_double,
	 PROG_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_V] = {".", "load.v",
	 ev_entity, ev_field, ev_vector,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_Q] = {".", "load.q",
	 ev_entity, ev_field, ev_quat,
	 PROG_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_S] = {".", "load.s",
	 ev_entity, ev_field, ev_string,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_ENT] = {".", "load.ent",
	 ev_entity, ev_field, ev_entity,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_FLD] = {".", "load.fld",
	 ev_entity, ev_field, ev_field,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_FN] = {".", "load.fn",
	 ev_entity, ev_field, ev_func,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_I] = {".", "load.i",
	 ev_entity, ev_field, ev_integer,
	 PROG_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_P] = {".", "load.p",
	 ev_entity, ev_field, ev_pointer,
	 PROG_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},

	[OP_LOADB_D] = {".", "loadb.d",
	 ev_pointer, ev_integer, ev_double,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_F] = {".", "loadb.f",
	 ev_pointer, ev_integer, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_V] = {".", "loadb.v",
	 ev_pointer, ev_integer, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_Q] = {".", "loadb.q",
	 ev_pointer, ev_integer, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_S] = {".", "loadb.s",
	 ev_pointer, ev_integer, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_ENT] = {".", "loadb.ent",
	 ev_pointer, ev_integer, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_FLD] = {".", "loadb.fld",
	 ev_pointer, ev_integer, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_FN] = {".", "loadb.fn",
	 ev_pointer, ev_integer, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_I] = {".", "loadb.i",
	 ev_pointer, ev_integer, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_P] = {".", "loadb.p",
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %Gb), %gc",
	},

	[OP_LOADBI_D] = {".", "loadbi.d",
	 ev_pointer, ev_short, ev_double,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_F] = {".", "loadbi.f",
	 ev_pointer, ev_short, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_V] = {".", "loadbi.v",
	 ev_pointer, ev_short, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_Q] = {".", "loadbi.q",
	 ev_pointer, ev_short, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_S] = {".", "loadbi.s",
	 ev_pointer, ev_short, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_ENT] = {".", "loadbi.ent",
	 ev_pointer, ev_short, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_FLD] = {".", "loadbi.fld",
	 ev_pointer, ev_short, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_FN] = {".", "loadbi.fn",
	 ev_pointer, ev_short, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_I] = {".", "loadbi.i",
	 ev_pointer, ev_short, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_P] = {".", "loadbi.p",
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %sb), %gc",
	},

	[OP_ADDRESS] = {"&", "address",
	 ev_entity, ev_field, ev_pointer,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},

	[OP_ADDRESS_VOID] = {"&", "address",
	 ev_void, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_D] = {"&", "address.d",
	 ev_double, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_F] = {"&", "address.f",
	 ev_float, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_V] = {"&", "address.v",
	 ev_vector, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_Q] = {"&", "address.q",
	 ev_quat, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_S] = {"&", "address.s",
	 ev_string, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_ENT] = {"&", "address.ent",
	 ev_entity, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_FLD] = {"&", "address.fld",
	 ev_field, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_FN] = {"&", "address.fn",
	 ev_func, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_I] = {"&", "address.i",
	 ev_integer, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_P] = {"&", "address.p",
	 ev_pointer, ev_invalid, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	[OP_LEA] = {"&", "lea",
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "(%Ga + %Gb), %gc",
	},
	[OP_LEAI] = {"&", "leai",
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "(%Ga + %sb), %gc",
	},

	[OP_CONV_IF] = {"<CONV>", "conv.if",
	 ev_integer, ev_invalid, ev_float,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_FI] = {"<CONV>", "conv.fi",
	 ev_float, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_ID] = {"<CONV>", "conv.id",
	 ev_integer, ev_invalid, ev_double,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_DI] = {"<CONV>", "conv.di",
	 ev_double, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_FD] = {"<CONV>", "conv.fd",
	 ev_float, ev_invalid, ev_double,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_DF] = {"<CONV>", "conv.df",
	 ev_double, ev_invalid, ev_float,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	[OP_STORE_D] = {"=", "store.d",
	 ev_double, ev_double, ev_invalid,
	 PROG_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_F] = {"=", "store.f",
	 ev_float, ev_float, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_V] = {"=", "store.v",
	 ev_vector, ev_vector, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_Q] = {"=", "store.q",
	 ev_quat, ev_quat, ev_invalid,
	 PROG_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_S] = {"=", "store.s",
	 ev_string, ev_string, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_ENT] = {"=", "store.ent",
	 ev_entity, ev_entity, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_FLD] = {"=", "store.fld",
	 ev_field, ev_field, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_FN] = {"=", "store.fn",
	 ev_func, ev_func, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_I] = {"=", "store.i",
	 ev_integer, ev_integer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_P] = {"=", "store.p",
	 ev_pointer, ev_pointer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, %gb",
	},

	[OP_STOREP_D] = {".=", "storep.d",
	 ev_double, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_F] = {".=", "storep.f",
	 ev_float, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_V] = {".=", "storep.v",
	 ev_vector, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_Q] = {".=", "storep.q",
	 ev_quat, ev_pointer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_S] = {".=", "storep.s",
	 ev_string, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_ENT] = {".=", "storep.ent",
	 ev_entity, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_FLD] = {".=", "storep.fld",
	 ev_field, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_FN] = {".=", "storep.fn",
	 ev_func, ev_pointer, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_I] = {".=", "storep.i",
	 ev_integer, ev_pointer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_P] = {".=", "storep.p",
	 ev_pointer, ev_pointer, ev_invalid,
	 PROG_VERSION,
	 "%Ga, *%Gb",
	},

	[OP_STOREB_D] = {".=", "storeb.d",
	 ev_double, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_F] = {".=", "storeb.f",
	 ev_float, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_V] = {".=", "storeb.v",
	 ev_vector, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_Q] = {".=", "storeb.q",
	 ev_quat, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_S] = {".=", "storeb.s",
	 ev_string, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_ENT] = {".=", "storeb.ent",
	 ev_entity, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_FLD] = {".=", "storeb.fld",
	 ev_field, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_FN] = {".=", "storeb.fn",
	 ev_func, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_I] = {".=", "storeb.i",
	 ev_integer, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_P] = {".=", "storeb.p",
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},

	[OP_STOREBI_D] = {".=", "storebi.d",
	 ev_double, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_F] = {".=", "storebi.f",
	 ev_float, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_V] = {".=", "storebi.v",
	 ev_vector, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_Q] = {".=", "storebi.q",
	 ev_quat, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_S] = {".=", "storebi.s",
	 ev_string, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_ENT] = {".=", "storebi.ent",
	 ev_entity, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_FLD] = {".=", "storebi.fld",
	 ev_field, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_FN] = {".=", "storebi.fn",
	 ev_func, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_I] = {".=", "storebi.i",
	 ev_integer, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_P] = {".=", "storebi.p",
	 ev_pointer, ev_pointer, ev_short,
	 PROG_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},

	[OP_RETURN] = {"<RETURN>", "return",
	 ev_void, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ra",
	},

	[OP_RETURN_V] = {"<RETURN_V>", "return",
	 ev_invalid, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "",
	},

	[OP_NOT_D] = {"!", "not.d",
	 ev_double, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_F] = {"!", "not.f",
	 ev_float, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_V] = {"!", "not.v",
	 ev_vector, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_Q] = {"!", "not.q",
	 ev_quat, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_S] = {"!", "not.s",
	 ev_string, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_ENT] = {"!", "not.ent",
	 ev_entity, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_FN] = {"!", "not.fn",
	 ev_func, ev_invalid, ev_integer,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_P] = {"!", "not.p",
	 ev_pointer, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	[OP_IF] = {"<IF>", "if",
	 ev_integer, ev_short, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFNOT] = {"<IFNOT>", "ifnot",
	 ev_integer, ev_short, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFBE] = {"<IFBE>", "ifbe",
	 ev_integer, ev_short, ev_invalid,
	 PROG_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFB] = {"<IFB>", "ifb",
	 ev_integer, ev_short, ev_invalid,
	 PROG_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFAE] = {"<IFAE>", "ifae",
	 ev_integer, ev_short, ev_invalid,
	 PROG_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFA] = {"<IFA>", "ifa",
	 ev_integer, ev_short, ev_invalid,
	 PROG_VERSION,
	 "%Ga branch %sb (%Ob)",
	},

// calls returns REG_RETURN
	[OP_CALL0] = {"<CALL0>", "call0",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa ()",
	},
	[OP_CALL1] = {"<CALL1>", "call1",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x)",
	},
	[OP_CALL2] = {"<CALL2>", "call2",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x)",
	},
	[OP_CALL3] = {"<CALL3>", "call3",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x)",
	},
	[OP_CALL4] = {"<CALL4>", "call4",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x)",
	},
	[OP_CALL5] = {"<CALL5>", "call5",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x)",
	},
	[OP_CALL6] = {"<CALL6>", "call6",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x)",
	},
	[OP_CALL7] = {"<CALL7>", "call7",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x, %P6x)",
	},
	[OP_CALL8] = {"<CALL8>", "call8",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x, %P6x, %P7x)",
	},
	[OP_RCALL0] = {"<RCALL0>", 0,
	 ev_invalid, ev_invalid, ev_invalid,
	 ~0,	// not a valid instruction
	 0,
	},
	[OP_RCALL1] = {"<RCALL1>", "rcall1",
	 ev_func, ev_void, ev_invalid,
	 PROG_VERSION,
	 "%Fa (%P0b)",
	},
	[OP_RCALL2] = {"<RCALL2>", "rcall2",
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c)",
	},
	[OP_RCALL3] = {"<RCALL3>", "rcall3",
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x)",
	},
	[OP_RCALL4] = {"<RCALL4>", "rcall4",
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x)",
	},
	[OP_RCALL5] = {"<RCALL5>", "rcall5",
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x)",
	},
	[OP_RCALL6] = {"<RCALL6>", "rcall6",
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x)",
	},
	[OP_RCALL7] = {"<RCALL7>", "rcall7",
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x, %P6x)",
	},
	[OP_RCALL8] = {"<RCALL8>", "rcall8",
	 ev_func, ev_void, ev_void,
	 PROG_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x, %P6x, %P7x)",
	},

	[OP_STATE] = {"<STATE>", "state",
	 ev_float, ev_func, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %Gb",
	},

	[OP_STATE_F] = {"<STATE>", "state.f",
	 ev_float, ev_func, ev_float,
	 PROG_VERSION,
	 "%Ga, %Gb, %Gc",
	},

	[OP_GOTO] = {"<GOTO>", "goto",
	 ev_short, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "branch %sa (%Oa)",
	},
	[OP_JUMP] = {"<JUMP>", "jump",
	 ev_integer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_JUMPB] = {"<JUMPB>", "jumpb",
	 ev_void, ev_integer, ev_invalid,
	 PROG_VERSION,
	 "%Ga[%Gb]",
	},

	[OP_AND] = {"&&", "and.f",
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},
	[OP_OR] = {"||", "or.f",
	 ev_float, ev_float, ev_integer,
	 PROG_ID_VERSION,
	},

	[OP_SHL_F] = {"<<", "shl.f",
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},
	[OP_SHR_F] = {">>", "shr.f",
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},
	[OP_SHL_I] = {"<<", "shl.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_SHR_I] = {">>", "shr.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_SHR_U] = {">>", "shr.u",
	 ev_uinteger, ev_integer, ev_uinteger,
	 PROG_VERSION,
	},

	[OP_BITAND] = {"&", "bitand",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_BITOR] = {"|", "bitor",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},

	[OP_ADD_I] = {"+", "add.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_SUB_I] = {"-", "sub.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_MUL_I] = {"*", "mul.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_DIV_I] = {"/", "div.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_REM_I] = {"%", "rem.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_MOD_I] = {"%%", "mod.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_BITAND_I] = {"&", "bitand.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_BITOR_I] = {"|", "bitor.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},

	[OP_REM_F] = {"%", "rem.f",
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},

	[OP_MOD_F] = {"%%", "mod.f",
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},

	[OP_GE_I] = {">=", "ge.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_LE_I] = {"<=", "le.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_GT_I] = {">", "gt.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_LT_I] = {"<", "lt.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},

	[OP_AND_I] = {"&&", "and.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_OR_I] = {"||", "or.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_NOT_I] = {"!", "not.i",
	 ev_integer, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_EQ_I] = {"==", "eq.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_NE_I] = {"!=", "ne.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},

	[OP_GE_U] = {">=", "ge.u",
	 ev_uinteger, ev_uinteger, ev_integer,
	 PROG_VERSION,
	},
	[OP_LE_U] = {"<=", "le.u",
	 ev_uinteger, ev_uinteger, ev_integer,
	 PROG_VERSION,
	},
	[OP_GT_U] = {">", "gt.u",
	 ev_uinteger, ev_uinteger, ev_integer,
	 PROG_VERSION,
	},
	[OP_LT_U] = {"<", "lt.u",
	 ev_uinteger, ev_uinteger, ev_integer,
	 PROG_VERSION,
	},

	[OP_BITXOR_F] = {"^", "bitxor.f",
	 ev_float, ev_float, ev_float,
	 PROG_VERSION,
	},
	[OP_BITNOT_F] = {"~", "bitnot.f",
	 ev_float, ev_invalid, ev_float,
	 PROG_VERSION,
	 "%Ga, %gc",
	},
	[OP_BITXOR_I] = {"^", "bitxor.i",
	 ev_integer, ev_integer, ev_integer,
	 PROG_VERSION,
	},
	[OP_BITNOT_I] = {"~", "bitnot.i",
	 ev_integer, ev_invalid, ev_integer,
	 PROG_VERSION,
	 "%Ga, %gc",
	},

	[OP_GE_P] = {">=", "ge.p",
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	[OP_LE_P] = {"<=", "le.p",
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	[OP_GT_P] = {">", "gt.p",
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	[OP_LT_P] = {"<", "lt.p",
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	[OP_EQ_P] = {"==", "eq.p",
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},
	[OP_NE_P] = {"!=", "ne.p",
	 ev_pointer, ev_pointer, ev_integer,
	 PROG_VERSION,
	},

	[OP_MOVEI] = {"<MOVE>", "movei",
	 ev_void, ev_short, ev_void,
	 PROG_VERSION,
	 "%Ga, %sb, %gc",
	},
	[OP_MOVEP] = {"<MOVEP>", "movep",
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %Gb, %Gc",
	},
	[OP_MOVEPI] = {"<MOVEP>", "movepi",
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %sb, %Gc",
	},
	[OP_MEMSETI] = {"<MEMSET>", "memseti",
	 ev_integer, ev_short, ev_void,
	 PROG_VERSION,
	 "%Ga, %sb, %gc",
	},
	[OP_MEMSETP] = {"<MEMSETP>", "memsetp",
	 ev_integer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %Gb, %Gc",
	},
	[OP_MEMSETPI] = {"<MEMSETP>", "memsetpi",
	 ev_integer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "%Ga, %sb, %Gc",
	},

	[OP_PUSH_S] = {"<PUSH>", "push.s",
	 ev_string, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_F] = {"<PUSH>", "push.f",
	 ev_float, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_V] = {"<PUSH>", "push.v",
	 ev_vector, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_ENT] = {"<PUSH>", "push.ent",
	 ev_entity, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_FLD] = {"<PUSH>", "push.fld",
	 ev_field, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_FN] = {"<PUSH>", "push.fn",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_P] = {"<PUSH>", "push.p",
	 ev_pointer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_Q] = {"<PUSH>", "push.q",
	 ev_quat, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_I] = {"<PUSH>", "push.i",
	 ev_integer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},
	[OP_PUSH_D] = {"<PUSH>", "push.d",
	 ev_double, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%Ga",
	},

	[OP_PUSHB_S] = {"<PUSH>", "pushb.s",
	 ev_pointer, ev_integer, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_F] = {"<PUSH>", "pushb.f",
	 ev_pointer, ev_integer, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_V] = {"<PUSH>", "pushb.v",
	 ev_pointer, ev_integer, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_ENT] = {"<PUSH>", "pushb.ent",
	 ev_pointer, ev_integer, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_FLD] = {"<PUSH>", "pushb.fld",
	 ev_pointer, ev_integer, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_FN] = {"<PUSH>", "pushb.fn",
	 ev_pointer, ev_integer, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_P] = {"<PUSH>", "pushb.p",
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_Q] = {"<PUSH>", "pushb.q",
	 ev_pointer, ev_integer, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_I] = {"<PUSH>", "pushb.i",
	 ev_pointer, ev_integer, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_D] = {"<PUSH>", "pushb.d",
	 ev_pointer, ev_integer, ev_double,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},

	[OP_PUSHBI_S] = {"<PUSH>", "pushbi.s",
	 ev_pointer, ev_short, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_F] = {"<PUSH>", "pushbi.f",
	 ev_pointer, ev_short, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_V] = {"<PUSH>", "pushbi.v",
	 ev_pointer, ev_short, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_ENT] = {"<PUSH>", "pushbi.ent",
	 ev_pointer, ev_short, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_FLD] = {"<PUSH>", "pushbi.fld",
	 ev_pointer, ev_short, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_FN] = {"<PUSH>", "pushbi.fn",
	 ev_pointer, ev_short, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_P] = {"<PUSH>", "pushbi.p",
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_Q] = {"<PUSH>", "pushbi.q",
	 ev_pointer, ev_short, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_I] = {"<PUSH>", "pushbi.i",
	 ev_pointer, ev_short, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_D] = {"<PUSH>", "pushbi.d",
	 ev_pointer, ev_short, ev_double,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},

	[OP_POP_S] = {"<POP>", "pop.s",
	 ev_string, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_F] = {"<POP>", "pop.f",
	 ev_float, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_V] = {"<POP>", "pop.v",
	 ev_vector, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_ENT] = {"<POP>", "pop.ent",
	 ev_entity, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_FLD] = {"<POP>", "pop.fld",
	 ev_field, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_FN] = {"<POP>", "pop.fn",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_P] = {"<POP>", "pop.p",
	 ev_pointer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_Q] = {"<POP>", "pop.q",
	 ev_quat, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_I] = {"<POP>", "pop.i",
	 ev_integer, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},
	[OP_POP_D] = {"<POP>", "pop.d",
	 ev_double, ev_invalid, ev_invalid,
	 PROG_VERSION,
	 "%ga",
	},

	[OP_POPB_S] = {"<POP>", "popb.s",
	 ev_pointer, ev_integer, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_F] = {"<POP>", "popb.f",
	 ev_pointer, ev_integer, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_V] = {"<POP>", "popb.v",
	 ev_pointer, ev_integer, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_ENT] = {"<POP>", "popb.ent",
	 ev_pointer, ev_integer, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_FLD] = {"<POP>", "popb.fld",
	 ev_pointer, ev_integer, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_FN] = {"<POP>", "popb.fn",
	 ev_pointer, ev_integer, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_P] = {"<POP>", "popb.p",
	 ev_pointer, ev_integer, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_Q] = {"<POP>", "popb.q",
	 ev_pointer, ev_integer, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_I] = {"<POP>", "popb.i",
	 ev_pointer, ev_integer, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_D] = {"<POP>", "popb.d",
	 ev_pointer, ev_integer, ev_double,
	 PROG_VERSION,
	 "*(%Ga + %Gb)",
	},

	[OP_POPBI_S] = {"<POP>", "popbi.s",
	 ev_pointer, ev_short, ev_string,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_F] = {"<POP>", "popbi.f",
	 ev_pointer, ev_short, ev_float,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_V] = {"<POP>", "popbi.v",
	 ev_pointer, ev_short, ev_vector,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_ENT] = {"<POP>", "popbi.ent",
	 ev_pointer, ev_short, ev_entity,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_FLD] = {"<POP>", "popbi.fld",
	 ev_pointer, ev_short, ev_field,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_FN] = {"<POP>", "popbi.fn",
	 ev_pointer, ev_short, ev_func,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_P] = {"<POP>", "popbi.p",
	 ev_pointer, ev_short, ev_pointer,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_Q] = {"<POP>", "popbi.q",
	 ev_pointer, ev_short, ev_quat,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_I] = {"<POP>", "popbi.i",
	 ev_pointer, ev_short, ev_integer,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_D] = {"<POP>", "popbi.d",
	 ev_pointer, ev_short, ev_double,
	 PROG_VERSION,
	 "*(%Ga + %sb)",
	},

	// end of table
	[OP_MEMSETPI+1] = {0}, //XXX FIXME relies on OP_MEMSETPI being last
};

const opcode_t *
PR_Opcode (pr_short_t opcode)
{
	if (opcode < 0
		|| opcode >= (int) (sizeof (pr_opcodes) / sizeof (pr_opcodes[0])) - 1) {
		return 0;
	}
	return &pr_opcodes[opcode];
}

VISIBLE void
PR_Opcode_Init (void)
{
}

static inline void
check_branch (progs_t *pr, dstatement_t *st, const opcode_t *op, short offset)
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
check_global (progs_t *pr, dstatement_t *st, const opcode_t *op, etype_t type,
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
					"internal.qc is used, they most definitely will NOT. To "
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
check_global_size (progs_t *pr, dstatement_t *st, const opcode_t *op,
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
	const opcode_t *op;
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
								  (op - pr_opcodes) != OP_STORE_F);
					check_global (pr, st, op, op->type_c, st->c, 0);
					break;
			}
		}
	}
	return 1;
}
