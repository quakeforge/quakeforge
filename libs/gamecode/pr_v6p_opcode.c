/*
	pr_v6p_opcodes.c

	Opcode table and checking for v6+ progs.

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

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
#include "QF/progs.h"
#include "QF/sys.h"

#include "QF/progs/pr_comp.h"

#include "compat.h"

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
// M  addressing mode, contents
// m  addressing mode, no contents
//      takes operand (a,b,c,o (opcode)) and right shift(hex). always masked
//        by 3
//      %Mc5 -> contents, operand c, shift right 5 bits
//      %mo2 -> no contents, opcode, shift right 2 bits
//      %mo0 -> no contents, opcode, no shift
//      always uses a and b for the address calculation (Ruamoko convention))
//
// a  operand a
// b  operand b
// c  operand c
// o  opcode
// x  place holder for P (padding)
// 0-7 parameter index (for P)
VISIBLE const v6p_opcode_t pr_v6p_opcodes[] = {
	// OP_DONE_v6p is actually the same as OP_RETURN_v6p, the types are bogus
	[OP_DONE_v6p] = {"done", "done",
	 ev_entity, ev_field, ev_void,
	 PROG_ID_VERSION,
	 "%Va",
	},

	[OP_MUL_D_v6p] = {"mul", "mul.d",
	 ev_double, ev_double, ev_double,
	 PROG_V6P_VERSION,
	},
	[OP_MUL_F_v6p] = {"mul", "mul.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_MUL_V_v6p] = {"vdot", "mul.v",
	 ev_vector, ev_vector, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_MUL_FV_v6p] = {"scale", "mul.fv",
	 ev_float, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_MUL_VF_v6p] = {"scale", "mul.vf",
	 ev_vector, ev_float, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_MUL_DV_v6p] = {"mul", "mul.dv",
	 ev_double, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_MUL_VD_v6p] = {"mul", "mul.vd",
	 ev_vector, ev_double, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_MUL_Q_v6p] = {"mul", "mul.q",
	 ev_quaternion, ev_quaternion, ev_quaternion,
	 PROG_V6P_VERSION,
	},
	[OP_MUL_FQ_v6p] = {"mul", "mul.fq",
	 ev_float, ev_quaternion, ev_quaternion,
	 PROG_V6P_VERSION,
	},
	[OP_MUL_QF_v6p] = {"mul", "mul.qf",
	 ev_quaternion, ev_float, ev_quaternion,
	 PROG_V6P_VERSION,
	},
	[OP_MUL_DQ_v6p] = {"mul", "mul.dq",
	 ev_double, ev_quaternion, ev_quaternion,
	 PROG_V6P_VERSION,
	},
	[OP_MUL_QD_v6p] = {"mul", "mul.qd",
	 ev_quaternion, ev_double, ev_quaternion,
	 PROG_V6P_VERSION,
	},
	[OP_MUL_QV_v6p] = {"mul", "mul.qv",
	 ev_quaternion, ev_vector, ev_vector,
	 PROG_V6P_VERSION,
	},

	[OP_CONJ_Q_v6p] = {"conj", "conj.q",
	 ev_quaternion, ev_invalid, ev_quaternion,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},

	[OP_DIV_F_v6p] = {"div", "div.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_DIV_D_v6p] = {"div", "div.d",
	 ev_double, ev_double, ev_double,
	 PROG_V6P_VERSION,
	},
	[OP_REM_D_v6p] = {"rem", "rem.d",
	 ev_double, ev_double, ev_double,
	 PROG_V6P_VERSION,
	},
	[OP_MOD_D_v6p] = {"mod", "mod.d",
	 ev_double, ev_double, ev_double,
	 PROG_V6P_VERSION,
	},

	[OP_ADD_D_v6p] = {"add", "add.d",
	 ev_double, ev_double, ev_double,
	 PROG_V6P_VERSION,
	},
	[OP_ADD_F_v6p] = {"add", "add.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_ADD_V_v6p] = {"add", "add.v",
	 ev_vector, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_ADD_Q_v6p] = {"add", "add.q",
	 ev_quaternion, ev_quaternion, ev_quaternion,
	 PROG_V6P_VERSION,
	},
	[OP_ADD_S_v6p] = {"add", "add.s",
	 ev_string, ev_string, ev_string,
	 PROG_V6P_VERSION,
	},

	[OP_SUB_D_v6p] = {"sub", "sub.d",
	 ev_double, ev_double, ev_double,
	 PROG_V6P_VERSION,
	},
	[OP_SUB_F_v6p] = {"sub", "sub.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_SUB_V_v6p] = {"sub", "sub.v",
	 ev_vector, ev_vector, ev_vector,
	 PROG_ID_VERSION,
	},
	[OP_SUB_Q_v6p] = {"sub", "sub.q",
	 ev_quaternion, ev_quaternion, ev_quaternion,
	 PROG_V6P_VERSION,
	},

	[OP_EQ_D_v6p] = {"eq", "eq.d",
	 ev_double, ev_double, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_EQ_F_v6p] = {"eq", "eq.f",
	 ev_float, ev_float, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_EQ_V_v6p] = {"eq", "eq.v",
	 ev_vector, ev_vector, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_EQ_Q_v6p] = {"eq", "eq.q",
	 ev_quaternion, ev_quaternion, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_EQ_S_v6p] = {"eq", "eq.s",
	 ev_string, ev_string, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_EQ_E_v6p] = {"eq", "eq.e",
	 ev_entity, ev_entity, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_EQ_FN_v6p] = {"eq", "eq.fn",
	 ev_func, ev_func, ev_int,
	 PROG_ID_VERSION,
	},

	[OP_NE_D_v6p] = {"ne", "ne.d",
	 ev_double, ev_double, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_NE_F_v6p] = {"ne", "ne.f",
	 ev_float, ev_float, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_NE_V_v6p] = {"ne", "ne.v",
	 ev_vector, ev_vector, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_NE_Q_v6p] = {"ne", "ne.q",
	 ev_quaternion, ev_quaternion, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_NE_S_v6p] = {"cmp", "ne.s",
	 ev_string, ev_string, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_NE_E_v6p] = {"ne", "ne.e",
	 ev_entity, ev_entity, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_NE_FN_v6p] = {"ne", "ne.fn",
	 ev_func, ev_func, ev_int,
	 PROG_ID_VERSION,
	},

	[OP_LE_D_v6p] = {"le", "le.d",
	 ev_double, ev_double, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LE_F_v6p] = {"le", "le.f",
	 ev_float, ev_float, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_GE_D_v6p] = {"ge", "ge.d",
	 ev_double, ev_double, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_GE_F_v6p] = {"ge", "ge.f",
	 ev_float, ev_float, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_LE_S_v6p] = {"le", "le.s",
	 ev_string, ev_string, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_GE_S_v6p] = {"ge", "ge.s",
	 ev_string, ev_string, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LT_D_v6p] = {"lt", "lt.d",
	 ev_double, ev_double, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LT_F_v6p] = {"lt", "lt.f",
	 ev_float, ev_float, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_GT_D_v6p] = {"gt", "gt.d",
	 ev_double, ev_double, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_GT_F_v6p] = {"gt", "gt.f",
	 ev_float, ev_float, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_LT_S_v6p] = {"lt", "lt.s",
	 ev_string, ev_string, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_GT_S_v6p] = {"gt", "gt.s",
	 ev_string, ev_string, ev_int,
	 PROG_V6P_VERSION,
	},

	[OP_LOAD_F_v6p] = {"load", "load.f",
	 ev_entity, ev_field, ev_float,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",//FIXME %E more flexible?
	},
	[OP_LOAD_D_v6p] = {"load", "load.d",
	 ev_entity, ev_field, ev_double,
	 PROG_V6P_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_V_v6p] = {"load", "load.v",
	 ev_entity, ev_field, ev_vector,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_Q_v6p] = {"load", "load.q",
	 ev_entity, ev_field, ev_quaternion,
	 PROG_V6P_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_S_v6p] = {"load", "load.s",
	 ev_entity, ev_field, ev_string,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_ENT_v6p] = {"load", "load.ent",
	 ev_entity, ev_field, ev_entity,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_FLD_v6p] = {"load", "load.fld",
	 ev_entity, ev_field, ev_field,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_FN_v6p] = {"load", "load.fn",
	 ev_entity, ev_field, ev_func,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_I_v6p] = {"load", "load.i",
	 ev_entity, ev_field, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},
	[OP_LOAD_P_v6p] = {"load", "load.p",
	 ev_entity, ev_field, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},

	[OP_LOADB_D_v6p] = {"load", "loadb.d",
	 ev_ptr, ev_int, ev_double,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_F_v6p] = {"load", "loadb.f",
	 ev_ptr, ev_int, ev_float,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_V_v6p] = {"load", "loadb.v",
	 ev_ptr, ev_int, ev_vector,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_Q_v6p] = {"load", "loadb.q",
	 ev_ptr, ev_int, ev_quaternion,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_S_v6p] = {"load", "loadb.s",
	 ev_ptr, ev_int, ev_string,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_ENT_v6p] = {"load", "loadb.ent",
	 ev_ptr, ev_int, ev_entity,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_FLD_v6p] = {"load", "loadb.fld",
	 ev_ptr, ev_int, ev_field,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_FN_v6p] = {"load", "loadb.fn",
	 ev_ptr, ev_int, ev_func,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_I_v6p] = {"load", "loadb.i",
	 ev_ptr, ev_int, ev_int,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},
	[OP_LOADB_P_v6p] = {"load", "loadb.p",
	 ev_ptr, ev_int, ev_ptr,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb), %gc",
	},

	[OP_LOADBI_D_v6p] = {"load", "loadbi.d",
	 ev_ptr, ev_short, ev_double,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_F_v6p] = {"load", "loadbi.f",
	 ev_ptr, ev_short, ev_float,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_V_v6p] = {"load", "loadbi.v",
	 ev_ptr, ev_short, ev_vector,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_Q_v6p] = {"load", "loadbi.q",
	 ev_ptr, ev_short, ev_quaternion,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_S_v6p] = {"load", "loadbi.s",
	 ev_ptr, ev_short, ev_string,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_ENT_v6p] = {"load", "loadbi.ent",
	 ev_ptr, ev_short, ev_entity,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_FLD_v6p] = {"load", "loadbi.fld",
	 ev_ptr, ev_short, ev_field,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_FN_v6p] = {"load", "loadbi.fn",
	 ev_ptr, ev_short, ev_func,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_I_v6p] = {"load", "loadbi.i",
	 ev_ptr, ev_short, ev_int,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},
	[OP_LOADBI_P_v6p] = {"load", "loadbi.p",
	 ev_ptr, ev_short, ev_ptr,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb), %gc",
	},

	[OP_ADDRESS_v6p] = {"lea", "address",
	 ev_entity, ev_field, ev_ptr,
	 PROG_ID_VERSION,
	 "%Ga.%Gb(%Ec), %gc",
	},

	[OP_ADDRESS_VOID_v6p] = {"lea", "address",
	 ev_void, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_D_v6p] = {"lea", "address.d",
	 ev_double, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_F_v6p] = {"lea", "address.f",
	 ev_float, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_V_v6p] = {"lea", "address.v",
	 ev_vector, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_Q_v6p] = {"lea", "address.q",
	 ev_quaternion, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_S_v6p] = {"lea", "address.s",
	 ev_string, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_ENT_v6p] = {"lea", "address.ent",
	 ev_entity, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_FLD_v6p] = {"lea", "address.fld",
	 ev_field, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_FN_v6p] = {"lea", "address.fn",
	 ev_func, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_I_v6p] = {"lea", "address.i",
	 ev_int, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_ADDRESS_P_v6p] = {"lea", "address.p",
	 ev_ptr, ev_invalid, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},

	[OP_LEA_v6p] = {"lea", "lea",
	 ev_ptr, ev_int, ev_ptr,
	 PROG_V6P_VERSION,
	 "(%Ga + %Gb), %gc",
	},
	[OP_LEAI_v6p] = {"lea", "leai",
	 ev_ptr, ev_short, ev_ptr,
	 PROG_V6P_VERSION,
	 "(%Ga + %sb), %gc",
	},

	[OP_CONV_IF_v6p] = {"conv", "conv.if",
	 ev_int, ev_invalid, ev_float,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_FI_v6p] = {"conv", "conv.fi",
	 ev_float, ev_invalid, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_ID_v6p] = {"conv", "conv.id",
	 ev_int, ev_invalid, ev_double,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_DI_v6p] = {"conv", "conv.di",
	 ev_double, ev_invalid, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_FD_v6p] = {"conv", "conv.fd",
	 ev_float, ev_invalid, ev_double,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_CONV_DF_v6p] = {"conv", "conv.df",
	 ev_double, ev_invalid, ev_float,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},

	[OP_STORE_D_v6p] = {"assign", "store.d",
	 ev_double, ev_double, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_F_v6p] = {"assign", "store.f",
	 ev_float, ev_float, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_V_v6p] = {"assign", "store.v",
	 ev_vector, ev_vector, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_Q_v6p] = {"assign", "store.q",
	 ev_quaternion, ev_quaternion, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_S_v6p] = {"assign", "store.s",
	 ev_string, ev_string, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_ENT_v6p] = {"assign", "store.ent",
	 ev_entity, ev_entity, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_FLD_v6p] = {"assign", "store.fld",
	 ev_field, ev_field, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_FN_v6p] = {"assign", "store.fn",
	 ev_func, ev_func, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_I_v6p] = {"assign", "store.i",
	 ev_int, ev_int, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga, %gb",
	},
	[OP_STORE_P_v6p] = {"assign", "store.p",
	 ev_ptr, ev_ptr, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga, %gb",
	},

	[OP_STOREP_D_v6p] = {"store", "storep.d",
	 ev_double, ev_ptr, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_F_v6p] = {"store", "storep.f",
	 ev_float, ev_ptr, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_V_v6p] = {"store", "storep.v",
	 ev_vector, ev_ptr, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_Q_v6p] = {"store", "storep.q",
	 ev_quaternion, ev_ptr, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_S_v6p] = {"store", "storep.s",
	 ev_string, ev_ptr, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_ENT_v6p] = {"store", "storep.ent",
	 ev_entity, ev_ptr, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_FLD_v6p] = {"store", "storep.fld",
	 ev_field, ev_ptr, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_FN_v6p] = {"store", "storep.fn",
	 ev_func, ev_ptr, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_I_v6p] = {"store", "storep.i",
	 ev_int, ev_ptr, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga, *%Gb",
	},
	[OP_STOREP_P_v6p] = {"store", "storep.p",
	 ev_ptr, ev_ptr, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga, *%Gb",
	},

	[OP_STOREB_D_v6p] = {"store", "storeb.d",
	 ev_double, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_F_v6p] = {"store", "storeb.f",
	 ev_float, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_V_v6p] = {"store", "storeb.v",
	 ev_vector, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_Q_v6p] = {"store", "storeb.q",
	 ev_quaternion, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_S_v6p] = {"store", "storeb.s",
	 ev_string, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_ENT_v6p] = {"store", "storeb.ent",
	 ev_entity, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_FLD_v6p] = {"store", "storeb.fld",
	 ev_field, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_FN_v6p] = {"store", "storeb.fn",
	 ev_func, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_I_v6p] = {"store", "storeb.i",
	 ev_int, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},
	[OP_STOREB_P_v6p] = {"store", "storeb.p",
	 ev_ptr, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %Gc)",
	},

	[OP_STOREBI_D_v6p] = {"store", "storebi.d",
	 ev_double, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_F_v6p] = {"store", "storebi.f",
	 ev_float, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_V_v6p] = {"store", "storebi.v",
	 ev_vector, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_Q_v6p] = {"store", "storebi.q",
	 ev_quaternion, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_S_v6p] = {"store", "storebi.s",
	 ev_string, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_ENT_v6p] = {"store", "storebi.ent",
	 ev_entity, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_FLD_v6p] = {"store", "storebi.fld",
	 ev_field, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_FN_v6p] = {"store", "storebi.fn",
	 ev_func, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_I_v6p] = {"store", "storebi.i",
	 ev_int, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},
	[OP_STOREBI_P_v6p] = {"store", "storebi.p",
	 ev_ptr, ev_ptr, ev_short,
	 PROG_V6P_VERSION,
	 "%Ga, *(%Gb + %sc)",
	},

	[OP_RETURN_v6p] = {"return", "return",
	 ev_void, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ra",
	},

	[OP_RETURN_V_v6p] = {"return", "return",
	 ev_invalid, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "",
	},

	[OP_NOT_D_v6p] = {"not", "not.d",
	 ev_double, ev_invalid, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_F_v6p] = {"not", "not.f",
	 ev_float, ev_invalid, ev_int,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_V_v6p] = {"not", "not.v",
	 ev_vector, ev_invalid, ev_int,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_Q_v6p] = {"not", "not.q",
	 ev_quaternion, ev_invalid, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_S_v6p] = {"not", "not.s",
	 ev_string, ev_invalid, ev_int,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_ENT_v6p] = {"not", "not.ent",
	 ev_entity, ev_invalid, ev_int,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_FN_v6p] = {"not", "not.fn",
	 ev_func, ev_invalid, ev_int,
	 PROG_ID_VERSION,
	 "%Ga, %gc",
	},
	[OP_NOT_P_v6p] = {"not", "not.p",
	 ev_ptr, ev_invalid, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},

	[OP_IF_v6p] = {"ifnz", "if",
	 ev_int, ev_short, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFNOT_v6p] = {"ifz", "ifnot",
	 ev_int, ev_short, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFBE_v6p] = {"ifbe", "ifbe",
	 ev_int, ev_short, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFB_v6p] = {"ifb", "ifb",
	 ev_int, ev_short, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFAE_v6p] = {"ifae", "ifae",
	 ev_int, ev_short, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga branch %sb (%Ob)",
	},
	[OP_IFA_v6p] = {"ifa", "ifa",
	 ev_int, ev_short, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga branch %sb (%Ob)",
	},

// calls returns REG_RETURN
	[OP_CALL0_v6p] = {"call0", "call0",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa ()",
	},
	[OP_CALL1_v6p] = {"call1", "call1",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x)",
	},
	[OP_CALL2_v6p] = {"call2", "call2",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x)",
	},
	[OP_CALL3_v6p] = {"call3", "call3",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x)",
	},
	[OP_CALL4_v6p] = {"call4", "call4",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x)",
	},
	[OP_CALL5_v6p] = {"call5", "call5",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x)",
	},
	[OP_CALL6_v6p] = {"call6", "call6",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x)",
	},
	[OP_CALL7_v6p] = {"call7", "call7",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x, %P6x)",
	},
	[OP_CALL8_v6p] = {"call8", "call8",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "%Fa (%P0x, %P1x, %P2x, %P3x, %P4x, %P5x, %P6x, %P7x)",
	},
	[OP_RCALL0_v6p] = {"rcall0", 0,
	 ev_invalid, ev_invalid, ev_invalid,
	 ~0,	// not a valid instruction
	 0,
	},
	[OP_RCALL1_v6p] = {"rcall1", "rcall1",
	 ev_func, ev_void, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Fa (%P0b)",
	},
	[OP_RCALL2_v6p] = {"rcall2", "rcall2",
	 ev_func, ev_void, ev_void,
	 PROG_V6P_VERSION,
	 "%Fa (%P0b, %P1c)",
	},
	[OP_RCALL3_v6p] = {"rcall3", "rcall3",
	 ev_func, ev_void, ev_void,
	 PROG_V6P_VERSION,
	 "%Fa (%P0b, %P1c, %P2x)",
	},
	[OP_RCALL4_v6p] = {"rcall4", "rcall4",
	 ev_func, ev_void, ev_void,
	 PROG_V6P_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x)",
	},
	[OP_RCALL5_v6p] = {"rcall5", "rcall5",
	 ev_func, ev_void, ev_void,
	 PROG_V6P_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x)",
	},
	[OP_RCALL6_v6p] = {"rcall6", "rcall6",
	 ev_func, ev_void, ev_void,
	 PROG_V6P_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x)",
	},
	[OP_RCALL7_v6p] = {"rcall7", "rcall7",
	 ev_func, ev_void, ev_void,
	 PROG_V6P_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x, %P6x)",
	},
	[OP_RCALL8_v6p] = {"rcall8", "rcall8",
	 ev_func, ev_void, ev_void,
	 PROG_V6P_VERSION,
	 "%Fa (%P0b, %P1c, %P2x, %P3x, %P4x, %P5x, %P6x, %P7x)",
	},

	[OP_STATE_v6p] = {"state", "state",
	 ev_float, ev_func, ev_invalid,
	 PROG_ID_VERSION,
	 "%Ga, %Gb",
	},

	[OP_STATE_F_v6p] = {"state", "state.f",
	 ev_float, ev_func, ev_float,
	 PROG_V6P_VERSION,
	 "%Ga, %Gb, %Gc",
	},

	[OP_GOTO_v6p] = {"jump", "goto",
	 ev_short, ev_invalid, ev_invalid,
	 PROG_ID_VERSION,
	 "branch %sa (%Oa)",
	},
	[OP_JUMP_v6p] = {"jump", "jump",
	 ev_int, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_JUMPB_v6p] = {"jump", "jump",
	 ev_void, ev_int, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga[%Gb]",
	},

	[OP_AND_v6p] = {"and", "and.f",
	 ev_float, ev_float, ev_int,
	 PROG_ID_VERSION,
	},
	[OP_OR_v6p] = {"or", "or.f",
	 ev_float, ev_float, ev_int,
	 PROG_ID_VERSION,
	},

	[OP_SHL_F_v6p] = {"shl", "shl.f",
	 ev_float, ev_float, ev_float,
	 PROG_V6P_VERSION,
	},
	[OP_SHR_F_v6p] = {"shr", "shr.f",
	 ev_float, ev_float, ev_float,
	 PROG_V6P_VERSION,
	},
	[OP_SHL_I_v6p] = {"shl", "shl.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_SHR_I_v6p] = {"shr", "shr.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_SHR_U_v6p] = {"shr", "shr.u",
	 ev_uint, ev_int, ev_uint,
	 PROG_V6P_VERSION,
	},

	[OP_BITAND_v6p] = {"bitand", "bitand.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},
	[OP_BITOR_v6p] = {"bitor", "bitor.f",
	 ev_float, ev_float, ev_float,
	 PROG_ID_VERSION,
	},

	[OP_ADD_I_v6p] = {"add", "add.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_SUB_I_v6p] = {"sub", "sub.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_MUL_I_v6p] = {"mul", "mul.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_DIV_I_v6p] = {"div", "div.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_REM_I_v6p] = {"rem", "rem.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_MOD_I_v6p] = {"mod", "mod.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_BITAND_I_v6p] = {"bitand", "bitand.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_BITOR_I_v6p] = {"bitor", "bitor.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},

	[OP_REM_F_v6p] = {"rem", "rem.f",
	 ev_float, ev_float, ev_float,
	 PROG_V6P_VERSION,
	},

	[OP_MOD_F_v6p] = {"mod", "mod.f",
	 ev_float, ev_float, ev_float,
	 PROG_V6P_VERSION,
	},

	[OP_GE_I_v6p] = {"ge", "ge.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LE_I_v6p] = {"le", "le.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_GT_I_v6p] = {"gt", "gt.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LT_I_v6p] = {"lt", "lt.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},

	[OP_AND_I_v6p] = {"and", "and.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_OR_I_v6p] = {"or", "or.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_NOT_I_v6p] = {"not", "not.i",
	 ev_int, ev_invalid, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_EQ_I_v6p] = {"eq", "eq.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_NE_I_v6p] = {"ne", "ne.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},

	[OP_GE_U_v6p] = {"ge", "ge.u",
	 ev_uint, ev_uint, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LE_U_v6p] = {"le", "le.u",
	 ev_uint, ev_uint, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_GT_U_v6p] = {"gt", "gt.u",
	 ev_uint, ev_uint, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LT_U_v6p] = {"lt", "lt.u",
	 ev_uint, ev_uint, ev_int,
	 PROG_V6P_VERSION,
	},

	[OP_BITXOR_F_v6p] = {"bitxor", "bitxor.f",
	 ev_float, ev_float, ev_float,
	 PROG_V6P_VERSION,
	},
	[OP_BITNOT_F_v6p] = {"bitnot", "bitnot.f",
	 ev_float, ev_invalid, ev_float,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},
	[OP_BITXOR_I_v6p] = {"bitxor", "bitxor.i",
	 ev_int, ev_int, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_BITNOT_I_v6p] = {"bitnot", "bitnot.i",
	 ev_int, ev_invalid, ev_int,
	 PROG_V6P_VERSION,
	 "%Ga, %gc",
	},

	[OP_GE_P_v6p] = {"ge", "ge.p",
	 ev_ptr, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LE_P_v6p] = {"le", "le.p",
	 ev_ptr, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_GT_P_v6p] = {"gt", "gt.p",
	 ev_ptr, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_LT_P_v6p] = {"lt", "lt.p",
	 ev_ptr, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_EQ_P_v6p] = {"eq", "eq.p",
	 ev_ptr, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	},
	[OP_NE_P_v6p] = {"ne", "ne.p",
	 ev_ptr, ev_ptr, ev_int,
	 PROG_V6P_VERSION,
	},

	[OP_MOVEI_v6p] = {"move", "movei",
	 ev_void, ev_short, ev_void,
	 PROG_V6P_VERSION,
	 "%Ga, %sb, %gc",
	},
	[OP_MOVEP_v6p] = {"movep", "movep",
	 ev_ptr, ev_int, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %Gb, %Gc",
	},
	[OP_MOVEPI_v6p] = {"movep", "movepi",
	 ev_ptr, ev_short, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %sb, %Gc",
	},
	[OP_MEMSETI_v6p] = {"memset", "memseti",
	 ev_int, ev_short, ev_void,
	 PROG_V6P_VERSION,
	 "%Ga, %sb, %gc",
	},
	[OP_MEMSETP_v6p] = {"memsetp", "memsetp",
	 ev_int, ev_int, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %Gb, %Gc",
	},
	[OP_MEMSETPI_v6p] = {"memsetp", "memsetpi",
	 ev_int, ev_short, ev_ptr,
	 PROG_V6P_VERSION,
	 "%Ga, %sb, %Gc",
	},

	[OP_PUSH_S_v6p] = {"push", "push.s",
	 ev_string, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_F_v6p] = {"push", "push.f",
	 ev_float, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_V_v6p] = {"push", "push.v",
	 ev_vector, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_ENT_v6p] = {"push", "push.ent",
	 ev_entity, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_FLD_v6p] = {"push", "push.fld",
	 ev_field, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_FN_v6p] = {"push", "push.fn",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_P_v6p] = {"push", "push.p",
	 ev_ptr, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_Q_v6p] = {"push", "push.q",
	 ev_quaternion, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_I_v6p] = {"push", "push.i",
	 ev_int, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},
	[OP_PUSH_D_v6p] = {"push", "push.d",
	 ev_double, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%Ga",
	},

	[OP_PUSHB_S_v6p] = {"push", "pushb.s",
	 ev_ptr, ev_int, ev_string,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_F_v6p] = {"push", "pushb.f",
	 ev_ptr, ev_int, ev_float,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_V_v6p] = {"push", "pushb.v",
	 ev_ptr, ev_int, ev_vector,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_ENT_v6p] = {"push", "pushb.ent",
	 ev_ptr, ev_int, ev_entity,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_FLD_v6p] = {"push", "pushb.fld",
	 ev_ptr, ev_int, ev_field,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_FN_v6p] = {"push", "pushb.fn",
	 ev_ptr, ev_int, ev_func,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_P_v6p] = {"push", "pushb.p",
	 ev_ptr, ev_int, ev_ptr,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_Q_v6p] = {"push", "pushb.q",
	 ev_ptr, ev_int, ev_quaternion,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_I_v6p] = {"push", "pushb.i",
	 ev_ptr, ev_int, ev_int,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_PUSHB_D_v6p] = {"push", "pushb.d",
	 ev_ptr, ev_int, ev_double,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},

	[OP_PUSHBI_S_v6p] = {"push", "pushbi.s",
	 ev_ptr, ev_short, ev_string,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_F_v6p] = {"push", "pushbi.f",
	 ev_ptr, ev_short, ev_float,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_V_v6p] = {"push", "pushbi.v",
	 ev_ptr, ev_short, ev_vector,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_ENT_v6p] = {"push", "pushbi.ent",
	 ev_ptr, ev_short, ev_entity,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_FLD_v6p] = {"push", "pushbi.fld",
	 ev_ptr, ev_short, ev_field,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_FN_v6p] = {"push", "pushbi.fn",
	 ev_ptr, ev_short, ev_func,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_P_v6p] = {"push", "pushbi.p",
	 ev_ptr, ev_short, ev_ptr,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_Q_v6p] = {"push", "pushbi.q",
	 ev_ptr, ev_short, ev_quaternion,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_I_v6p] = {"push", "pushbi.i",
	 ev_ptr, ev_short, ev_int,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_PUSHBI_D_v6p] = {"push", "pushbi.d",
	 ev_ptr, ev_short, ev_double,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},

	[OP_POP_S_v6p] = {"pop", "pop.s",
	 ev_string, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_F_v6p] = {"pop", "pop.f",
	 ev_float, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_V_v6p] = {"pop", "pop.v",
	 ev_vector, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_ENT_v6p] = {"pop", "pop.ent",
	 ev_entity, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_FLD_v6p] = {"pop", "pop.fld",
	 ev_field, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_FN_v6p] = {"pop", "pop.fn",
	 ev_func, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_P_v6p] = {"pop", "pop.p",
	 ev_ptr, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_Q_v6p] = {"pop", "pop.q",
	 ev_quaternion, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_I_v6p] = {"pop", "pop.i",
	 ev_int, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},
	[OP_POP_D_v6p] = {"pop", "pop.d",
	 ev_double, ev_invalid, ev_invalid,
	 PROG_V6P_VERSION,
	 "%ga",
	},

	[OP_POPB_S_v6p] = {"pop", "popb.s",
	 ev_ptr, ev_int, ev_string,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_F_v6p] = {"pop", "popb.f",
	 ev_ptr, ev_int, ev_float,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_V_v6p] = {"pop", "popb.v",
	 ev_ptr, ev_int, ev_vector,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_ENT_v6p] = {"pop", "popb.ent",
	 ev_ptr, ev_int, ev_entity,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_FLD_v6p] = {"pop", "popb.fld",
	 ev_ptr, ev_int, ev_field,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_FN_v6p] = {"pop", "popb.fn",
	 ev_ptr, ev_int, ev_func,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_P_v6p] = {"pop", "popb.p",
	 ev_ptr, ev_int, ev_ptr,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_Q_v6p] = {"pop", "popb.q",
	 ev_ptr, ev_int, ev_quaternion,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_I_v6p] = {"pop", "popb.i",
	 ev_ptr, ev_int, ev_int,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},
	[OP_POPB_D_v6p] = {"pop", "popb.d",
	 ev_ptr, ev_int, ev_double,
	 PROG_V6P_VERSION,
	 "*(%Ga + %Gb)",
	},

	[OP_POPBI_S_v6p] = {"pop", "popbi.s",
	 ev_ptr, ev_short, ev_string,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_F_v6p] = {"pop", "popbi.f",
	 ev_ptr, ev_short, ev_float,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_V_v6p] = {"pop", "popbi.v",
	 ev_ptr, ev_short, ev_vector,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_ENT_v6p] = {"pop", "popbi.ent",
	 ev_ptr, ev_short, ev_entity,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_FLD_v6p] = {"pop", "popbi.fld",
	 ev_ptr, ev_short, ev_field,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_FN_v6p] = {"pop", "popbi.fn",
	 ev_ptr, ev_short, ev_func,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_P_v6p] = {"pop", "popbi.p",
	 ev_ptr, ev_short, ev_ptr,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_Q_v6p] = {"pop", "popbi.q",
	 ev_ptr, ev_short, ev_quaternion,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_I_v6p] = {"pop", "popbi.i",
	 ev_ptr, ev_short, ev_int,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},
	[OP_POPBI_D_v6p] = {"pop", "popbi.d",
	 ev_ptr, ev_short, ev_double,
	 PROG_V6P_VERSION,
	 "*(%Ga + %sb)",
	},

	// end of table
	[OP_MEMSETPI_v6p+1] = {0}, //XXX FIXME relies on OP_MEMSETPI_v6p being last
};

const v6p_opcode_t *
PR_v6p_Opcode (pr_ushort_t opcode)
{
	size_t opcode_count = sizeof (pr_v6p_opcodes) / sizeof (pr_v6p_opcodes[0]);
	if (opcode >= opcode_count - 1) {
		return 0;
	}
	return &pr_v6p_opcodes[opcode];
}

static inline void
check_branch (progs_t *pr, dstatement_t *st, const v6p_opcode_t *op, short offset)
{
	pr_int_t    address = st - pr->pr_statements;

	address += offset;
	if (address < 0 || (pr_uint_t) address >= pr->progs->statements.count)
		PR_Error (pr, "PR_Check_Opcodes: invalid branch (statement %ld: %s)",
				  (long)(st - pr->pr_statements), op->opname);
}

static int
is_vector_parameter_store (progs_t *pr, dstatement_t *st,
						   unsigned short operand)
{
	int         i;

	if ((pr_opcode_v6p_e) st->op != OP_STORE_V_v6p)
		return 0;
	if (operand != st->a)
		return 0;
	for (i = 0; i < PR_MAX_PARAMS; i++)
		if (st->b == pr->pr_params[i] - pr->pr_globals)
			return 1;
	return 0;
}

#define ISDENORM(x) ((x) && !((x) & 0x7f800000))

static inline void
check_global (progs_t *pr, dstatement_t *st, const v6p_opcode_t *op, etype_t type,
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
				> pr->progs->globals.count) {
				if (operand >= pr->progs->globals.count
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
				if (pr_boundscheck) {
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
check_global_size (progs_t *pr, dstatement_t *st, const v6p_opcode_t *op,
				   unsigned short size, unsigned short operand)
{
	const char *msg;
	if (operand + size > pr->progs->globals.count) {
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
PR_Check_v6p_Opcodes (progs_t *pr)
{
	const v6p_opcode_t *op;
	dstatement_t *st;
	int         state_ok = 0;
	int         pushpop_ok = 0;
	pr_uint_t   i;

	if (pr->globals.ftime && pr->globals.self && pr->fields.nextthink != -1
		&& pr->fields.think != -1 && pr->fields.frame != -1) {
		state_ok = 1;
	}
	if (pr->globals.stack) {
		pushpop_ok = 1;
	}

	//FIXME need to decide if I really want to always do static bounds checking
	// the only problem is that it slows progs load a little, but it's the only
	// way to check for qccx' evil
	if (0 && !pr_boundscheck) {
		for (i = 0, st = pr->pr_statements; i < pr->progs->statements.count;
			 st++, i++) {
			pr_opcode_v6p_e st_op = (pr_opcode_v6p_e) st->op;
			op = PR_v6p_Opcode (st_op);
			if (!op) {
				PR_Error (pr, "PR_Check_Opcodes: unknown opcode %d at "
						  "statement %ld", st_op,
						  (long)(st - pr->pr_statements));
			}
			if ((st_op == OP_STATE_v6p || st_op == OP_STATE_F_v6p)
				&& !state_ok) {
				PR_Error (pr, "PR_Check_Opcodes: %s used with missing fields "
						  "or globals", op->opname);
			}
			if ((strequal(op->name, "push") || strequal(op->name, "pop"))
				&& !pushpop_ok) {
				PR_Error (pr, "PR_Check_Opcodes: %s used with missing .stack "
						  "globals", op->opname);
			}
		}
	} else {
		for (i = 0, st = pr->pr_statements; i < pr->progs->statements.count;
			 st++, i++) {
			pr_opcode_v6p_e st_op = (pr_opcode_v6p_e) st->op;
			op = PR_v6p_Opcode (st_op);
			if (!op) {
				PR_Error (pr, "PR_Check_Opcodes: unknown opcode %d at "
						  "statement %ld", st_op,
						  (long)(st - pr->pr_statements));
			}
			switch (st_op) {
				case OP_IF_v6p:
				case OP_IFNOT_v6p:
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_branch (pr, st, op, st->b);
					break;
				case OP_GOTO_v6p:
					check_branch (pr, st, op, st->a);
					break;
				case OP_DONE_v6p:
				case OP_RETURN_v6p:
					check_global (pr, st, op, ev_int, st->a, 1);
					check_global (pr, st, op, ev_void, st->b, 0);
					check_global (pr, st, op, ev_void, st->c, 0);
					break;
				case OP_RCALL1_v6p:
					check_global (pr, st, op, ev_void, st->c, 1);
				case OP_RCALL2_v6p:
				case OP_RCALL3_v6p:
				case OP_RCALL4_v6p:
				case OP_RCALL5_v6p:
				case OP_RCALL6_v6p:
				case OP_RCALL7_v6p:
				case OP_RCALL8_v6p:
					if (st_op > OP_RCALL1_v6p)
						check_global (pr, st, op, ev_int, st->c, 1);
					check_global (pr, st, op, ev_int, st->b, 1);
					check_global (pr, st, op, ev_func, st->a, 1);
					break;
				case OP_STATE_v6p:
				case OP_STATE_F_v6p:
					if (!state_ok) {
						PR_Error (pr, "PR_Check_Opcodes: %s used with missing "
								  "fields or globals", op->opname);
					}
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_global (pr, st, op, op->type_b, st->b, 1);
					check_global (pr, st, op, op->type_c, st->c, 1);
					break;
				case OP_MOVEI_v6p:
					check_global_size (pr, st, op, st->b, st->a);
					check_global_size (pr, st, op, st->b, st->c);
					break;
				case OP_MEMSETI_v6p:
					check_global_size (pr, st, op, st->b, st->c);
					break;
				case OP_PUSHB_F_v6p:
				case OP_PUSHB_S_v6p:
				case OP_PUSHB_ENT_v6p:
				case OP_PUSHB_FLD_v6p:
				case OP_PUSHB_FN_v6p:
				case OP_PUSHB_I_v6p:
				case OP_PUSHB_P_v6p:
				case OP_PUSHB_V_v6p:
				case OP_PUSHB_Q_v6p:
				case OP_PUSHBI_F_v6p:
				case OP_PUSHBI_S_v6p:
				case OP_PUSHBI_ENT_v6p:
				case OP_PUSHBI_FLD_v6p:
				case OP_PUSHBI_FN_v6p:
				case OP_PUSHBI_I_v6p:
				case OP_PUSHBI_P_v6p:
				case OP_PUSHBI_V_v6p:
				case OP_PUSHBI_Q_v6p:
					// op->type_c is used for selecting the operator during
					// compilation, but is invalid when running
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_global (pr, st, op, op->type_b, st->b, 1);
					check_global (pr, st, op, ev_invalid, st->c, 1);
					break;
				case OP_POP_F_v6p:
				case OP_POP_FLD_v6p:
				case OP_POP_ENT_v6p:
				case OP_POP_S_v6p:
				case OP_POP_FN_v6p:
				case OP_POP_I_v6p:
				case OP_POP_P_v6p:
				case OP_POP_V_v6p:
				case OP_POP_Q_v6p:
					// don't want to check for denormal floats, otherwise
					// OP_POP__v6p* could use the defualt rule
					check_global (pr, st, op, op->type_a, st->a, 0);
					check_global (pr, st, op, ev_invalid, st->b, 1);
					check_global (pr, st, op, ev_invalid, st->c, 1);
					break;
				case OP_POPB_F_v6p:
				case OP_POPB_S_v6p:
				case OP_POPB_ENT_v6p:
				case OP_POPB_FLD_v6p:
				case OP_POPB_FN_v6p:
				case OP_POPB_I_v6p:
				case OP_POPB_P_v6p:
				case OP_POPB_V_v6p:
				case OP_POPB_Q_v6p:
				case OP_POPBI_F_v6p:
				case OP_POPBI_S_v6p:
				case OP_POPBI_ENT_v6p:
				case OP_POPBI_FLD_v6p:
				case OP_POPBI_FN_v6p:
				case OP_POPBI_I_v6p:
				case OP_POPBI_P_v6p:
				case OP_POPBI_V_v6p:
				case OP_POPBI_Q_v6p:
					// op->type_c is used for selecting the operator during
					// compilation, but is invalid when running
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_global (pr, st, op, op->type_b, st->b, 1);
					check_global (pr, st, op, ev_invalid, st->c, 1);
					break;
				default:
					check_global (pr, st, op, op->type_a, st->a, 1);
					check_global (pr, st, op, op->type_b, st->b,
								  (op - pr_v6p_opcodes) != OP_STORE_F_v6p);
					check_global (pr, st, op, op->type_c, st->c, 0);
					break;
			}
		}
	}
	return 1;
}
