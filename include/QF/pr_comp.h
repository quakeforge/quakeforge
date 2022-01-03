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

// this file is shared by QuakeForge and qfcc
#ifndef __QF_pr_comp_h
#define __QF_pr_comp_h

#include "QF/qtypes.h"

typedef double pr_double_t;
typedef float pr_float_t;
typedef int16_t pr_short_t;
typedef uint16_t pr_ushort_t;
typedef int32_t pr_int_t;
typedef uint32_t pr_uint_t;
typedef int64_t pr_long_t;
typedef uint64_t pr_ulong_t;
typedef pr_uint_t func_t;
typedef pr_int_t pr_string_t;
typedef pr_string_t string_t;//FIXME
typedef pr_uint_t pr_pointer_t;
typedef pr_pointer_t pointer_t;//FIXME

#define PR_VEC_TYPE(t,n,s) \
	typedef t n __attribute__ ((vector_size (s*sizeof (t))))

PR_VEC_TYPE (pr_int_t, pr_ivec2_t, 2);
PR_VEC_TYPE (pr_int_t, pr_ivec4_t, 4);
PR_VEC_TYPE (pr_uint_t, pr_uivec2_t, 2);
PR_VEC_TYPE (pr_uint_t, pr_uivec4_t, 4);
PR_VEC_TYPE (float, pr_vec2_t, 2);
PR_VEC_TYPE (float, pr_vec4_t, 4);
PR_VEC_TYPE (pr_long_t, pr_lvec2_t, 2);
PR_VEC_TYPE (pr_long_t, pr_lvec4_t, 4);
PR_VEC_TYPE (pr_ulong_t, pr_ulvec2_t, 2);
PR_VEC_TYPE (pr_ulong_t, pr_ulvec4_t, 4);
PR_VEC_TYPE (double, pr_dvec2_t, 2);
PR_VEC_TYPE (double, pr_dvec4_t, 4);

typedef enum {
	ev_void,
	ev_string,
	ev_float,
	ev_vector,
	ev_entity,
	ev_field,
	ev_func,
	ev_pointer,			// end of v6 types
	ev_quat,
	ev_integer,
	ev_uinteger,
	ev_short,			// value is embedded in the opcode
	ev_double,

	ev_invalid,			// invalid type. used for instruction checking
	ev_type_count		// not a type, gives number of types
} etype_t;

extern const pr_ushort_t pr_type_size[ev_type_count];
extern const char * const pr_type_name[ev_type_count];

#define	OFS_NULL		0
#define	OFS_RETURN		1
#define	OFS_PARM0		4		// leave 3 ofs for each parm to hold vectors
#define	OFS_PARM1		7
#define	OFS_PARM2		10
#define	OFS_PARM3		13
#define	OFS_PARM4		16
#define	OFS_PARM5		19
#define	OFS_PARM6		22
#define	OFS_PARM7		25
#define	RESERVED_OFS	28


typedef enum {
	OP_DONE_v6p,
	OP_MUL_F_v6p,
	OP_MUL_V_v6p,
	OP_MUL_FV_v6p,
	OP_MUL_VF_v6p,
	OP_DIV_F_v6p,
	OP_ADD_F_v6p,
	OP_ADD_V_v6p,
	OP_SUB_F_v6p,
	OP_SUB_V_v6p,

	OP_EQ_F_v6p,
	OP_EQ_V_v6p,
	OP_EQ_S_v6p,
	OP_EQ_E_v6p,
	OP_EQ_FN_v6p,

	OP_NE_F_v6p,
	OP_NE_V_v6p,
	OP_NE_S_v6p,
	OP_NE_E_v6p,
	OP_NE_FN_v6p,

	OP_LE_F_v6p,
	OP_GE_F_v6p,
	OP_LT_F_v6p,
	OP_GT_F_v6p,

	OP_LOAD_F_v6p,
	OP_LOAD_V_v6p,
	OP_LOAD_S_v6p,
	OP_LOAD_ENT_v6p,
	OP_LOAD_FLD_v6p,
	OP_LOAD_FN_v6p,

	OP_ADDRESS_v6p,

	OP_STORE_F_v6p,
	OP_STORE_V_v6p,
	OP_STORE_S_v6p,
	OP_STORE_ENT_v6p,
	OP_STORE_FLD_v6p,
	OP_STORE_FN_v6p,

	OP_STOREP_F_v6p,
	OP_STOREP_V_v6p,
	OP_STOREP_S_v6p,
	OP_STOREP_ENT_v6p,
	OP_STOREP_FLD_v6p,
	OP_STOREP_FN_v6p,

	OP_RETURN_v6p,
	OP_NOT_F_v6p,
	OP_NOT_V_v6p,
	OP_NOT_S_v6p,
	OP_NOT_ENT_v6p,
	OP_NOT_FN_v6p,
	OP_IF_v6p,
	OP_IFNOT_v6p,
	OP_CALL0_v6p,
	OP_CALL1_v6p,
	OP_CALL2_v6p,
	OP_CALL3_v6p,
	OP_CALL4_v6p,
	OP_CALL5_v6p,
	OP_CALL6_v6p,
	OP_CALL7_v6p,
	OP_CALL8_v6p,
	OP_STATE_v6p,
	OP_GOTO_v6p,
	OP_AND_v6p,
	OP_OR_v6p,

	OP_BITAND_v6p,
	OP_BITOR_v6p,	// end of v6 opcodes

	OP_ADD_S_v6p,
	OP_LE_S_v6p,
	OP_GE_S_v6p,
	OP_LT_S_v6p,
	OP_GT_S_v6p,

	OP_ADD_I_v6p,
	OP_SUB_I_v6p,
	OP_MUL_I_v6p,
	OP_DIV_I_v6p,
	OP_BITAND_I_v6p,
	OP_BITOR_I_v6p,
	OP_GE_I_v6p,
	OP_LE_I_v6p,
	OP_GT_I_v6p,
	OP_LT_I_v6p,
	OP_AND_I_v6p,
	OP_OR_I_v6p,
	OP_NOT_I_v6p,
	OP_EQ_I_v6p,
	OP_NE_I_v6p,
	OP_STORE_I_v6p,
	OP_STOREP_I_v6p,
	OP_LOAD_I_v6p,

	OP_CONV_IF_v6p,
	OP_CONV_FI_v6p,

	OP_BITXOR_F_v6p,
	OP_BITXOR_I_v6p,
	OP_BITNOT_F_v6p,
	OP_BITNOT_I_v6p,

	OP_SHL_F_v6p,
	OP_SHR_F_v6p,
	OP_SHL_I_v6p,
	OP_SHR_I_v6p,

	OP_REM_F_v6p,
	OP_REM_I_v6p,

	OP_LOADB_F_v6p,
	OP_LOADB_V_v6p,
	OP_LOADB_S_v6p,
	OP_LOADB_ENT_v6p,
	OP_LOADB_FLD_v6p,
	OP_LOADB_FN_v6p,
	OP_LOADB_I_v6p,
	OP_LOADB_P_v6p,

	OP_STOREB_F_v6p,
	OP_STOREB_V_v6p,
	OP_STOREB_S_v6p,
	OP_STOREB_ENT_v6p,
	OP_STOREB_FLD_v6p,
	OP_STOREB_FN_v6p,
	OP_STOREB_I_v6p,
	OP_STOREB_P_v6p,

	OP_ADDRESS_VOID_v6p,
	OP_ADDRESS_F_v6p,
	OP_ADDRESS_V_v6p,
	OP_ADDRESS_S_v6p,
	OP_ADDRESS_ENT_v6p,
	OP_ADDRESS_FLD_v6p,
	OP_ADDRESS_FN_v6p,
	OP_ADDRESS_I_v6p,
	OP_ADDRESS_P_v6p,

	OP_LEA_v6p,

	OP_IFBE_v6p,
	OP_IFB_v6p,
	OP_IFAE_v6p,
	OP_IFA_v6p,

	OP_JUMP_v6p,
	OP_JUMPB_v6p,

	OP_LT_U_v6p,
	OP_GT_U_v6p,
	OP_LE_U_v6p,
	OP_GE_U_v6p,

	OP_LOADBI_F_v6p,
	OP_LOADBI_V_v6p,
	OP_LOADBI_S_v6p,
	OP_LOADBI_ENT_v6p,
	OP_LOADBI_FLD_v6p,
	OP_LOADBI_FN_v6p,
	OP_LOADBI_I_v6p,
	OP_LOADBI_P_v6p,

	OP_STOREBI_F_v6p,
	OP_STOREBI_V_v6p,
	OP_STOREBI_S_v6p,
	OP_STOREBI_ENT_v6p,
	OP_STOREBI_FLD_v6p,
	OP_STOREBI_FN_v6p,
	OP_STOREBI_I_v6p,
	OP_STOREBI_P_v6p,

	OP_LEAI_v6p,

	OP_LOAD_P_v6p,
	OP_STORE_P_v6p,
	OP_STOREP_P_v6p,
	OP_NOT_P_v6p,
	OP_EQ_P_v6p,
	OP_NE_P_v6p,
	OP_LE_P_v6p,
	OP_GE_P_v6p,
	OP_LT_P_v6p,
	OP_GT_P_v6p,

	OP_MOVEI_v6p,
	OP_MOVEP_v6p,
	OP_MOVEPI_v6p,

	OP_SHR_U_v6p,

	OP_STATE_F_v6p,

	OP_ADD_Q_v6p,
	OP_SUB_Q_v6p,
	OP_MUL_Q_v6p,
	OP_MUL_QF_v6p,
	OP_MUL_FQ_v6p,
	OP_MUL_QV_v6p,
	OP_CONJ_Q_v6p,
	OP_NOT_Q_v6p,
	OP_EQ_Q_v6p,
	OP_NE_Q_v6p,
	OP_STORE_Q_v6p,
	OP_STOREB_Q_v6p,
	OP_STOREBI_Q_v6p,
	OP_STOREP_Q_v6p,
	OP_LOAD_Q_v6p,
	OP_LOADB_Q_v6p,
	OP_LOADBI_Q_v6p,
	OP_ADDRESS_Q_v6p,

	OP_RCALL0_v6p,
	OP_RCALL1_v6p,
	OP_RCALL2_v6p,
	OP_RCALL3_v6p,
	OP_RCALL4_v6p,
	OP_RCALL5_v6p,
	OP_RCALL6_v6p,
	OP_RCALL7_v6p,
	OP_RCALL8_v6p,

	OP_RETURN_V_v6p,

	OP_PUSH_S_v6p,
	OP_PUSH_F_v6p,
	OP_PUSH_V_v6p,
	OP_PUSH_ENT_v6p,
	OP_PUSH_FLD_v6p,
	OP_PUSH_FN_v6p,
	OP_PUSH_P_v6p,
	OP_PUSH_Q_v6p,
	OP_PUSH_I_v6p,
	OP_PUSH_D_v6p,

	OP_PUSHB_S_v6p,
	OP_PUSHB_F_v6p,
	OP_PUSHB_V_v6p,
	OP_PUSHB_ENT_v6p,
	OP_PUSHB_FLD_v6p,
	OP_PUSHB_FN_v6p,
	OP_PUSHB_P_v6p,
	OP_PUSHB_Q_v6p,
	OP_PUSHB_I_v6p,
	OP_PUSHB_D_v6p,

	OP_PUSHBI_S_v6p,
	OP_PUSHBI_F_v6p,
	OP_PUSHBI_V_v6p,
	OP_PUSHBI_ENT_v6p,
	OP_PUSHBI_FLD_v6p,
	OP_PUSHBI_FN_v6p,
	OP_PUSHBI_P_v6p,
	OP_PUSHBI_Q_v6p,
	OP_PUSHBI_I_v6p,
	OP_PUSHBI_D_v6p,

	OP_POP_S_v6p,
	OP_POP_F_v6p,
	OP_POP_V_v6p,
	OP_POP_ENT_v6p,
	OP_POP_FLD_v6p,
	OP_POP_FN_v6p,
	OP_POP_P_v6p,
	OP_POP_Q_v6p,
	OP_POP_I_v6p,
	OP_POP_D_v6p,

	OP_POPB_S_v6p,
	OP_POPB_F_v6p,
	OP_POPB_V_v6p,
	OP_POPB_ENT_v6p,
	OP_POPB_FLD_v6p,
	OP_POPB_FN_v6p,
	OP_POPB_P_v6p,
	OP_POPB_Q_v6p,
	OP_POPB_I_v6p,
	OP_POPB_D_v6p,

	OP_POPBI_S_v6p,
	OP_POPBI_F_v6p,
	OP_POPBI_V_v6p,
	OP_POPBI_ENT_v6p,
	OP_POPBI_FLD_v6p,
	OP_POPBI_FN_v6p,
	OP_POPBI_P_v6p,
	OP_POPBI_Q_v6p,
	OP_POPBI_I_v6p,
	OP_POPBI_D_v6p,

	OP_ADD_D_v6p,
	OP_SUB_D_v6p,
	OP_MUL_D_v6p,
	OP_MUL_QD_v6p,
	OP_MUL_DQ_v6p,
	OP_MUL_VD_v6p,
	OP_MUL_DV_v6p,
	OP_DIV_D_v6p,
	OP_REM_D_v6p,
	OP_GE_D_v6p,
	OP_LE_D_v6p,
	OP_GT_D_v6p,
	OP_LT_D_v6p,
	OP_NOT_D_v6p,
	OP_EQ_D_v6p,
	OP_NE_D_v6p,
	OP_CONV_FD_v6p,
	OP_CONV_DF_v6p,
	OP_CONV_ID_v6p,
	OP_CONV_DI_v6p,
	OP_STORE_D_v6p,
	OP_STOREB_D_v6p,
	OP_STOREBI_D_v6p,
	OP_STOREP_D_v6p,
	OP_LOAD_D_v6p,
	OP_LOADB_D_v6p,
	OP_LOADBI_D_v6p,
	OP_ADDRESS_D_v6p,

	OP_MOD_I_v6p,
	OP_MOD_F_v6p,
	OP_MOD_D_v6p,

	OP_MEMSETI_v6p,
	OP_MEMSETP_v6p,
	OP_MEMSETPI_v6p,
} pr_opcode_v6p_e;
#define OP_BREAK 0x8000

typedef enum {
	// 0 0000 load from [a,b] -> c
	OP_LOAD_E_1, OP_LOAD_E_2, OP_LOAD_E_3, OP_LOAD_E_4,
	OP_LOAD_B_1, OP_LOAD_B_2, OP_LOAD_B_3, OP_LOAD_B_4,
	OP_LOAD_C_1, OP_LOAD_C_2, OP_LOAD_C_3, OP_LOAD_C_4,
	OP_LOAD_D_1, OP_LOAD_D_2, OP_LOAD_D_3, OP_LOAD_D_4,
	// 0 0001 store from c -> [a, b]
	OP_STORE_A_1, OP_STORE_A_2, OP_STORE_A_3, OP_STORE_A_4, // redundant?
	OP_STORE_B_1, OP_STORE_B_2, OP_STORE_B_3, OP_STORE_B_4,
	OP_STORE_C_1, OP_STORE_C_2, OP_STORE_C_3, OP_STORE_C_4,
	OP_STORE_D_1, OP_STORE_D_2, OP_STORE_D_3, OP_STORE_D_4,
	// 0 0010 push from [a,b] to the stack
	OP_PUSH_A_1, OP_PUSH_A_2, OP_PUSH_A_3, OP_PUSH_A_4,
	OP_PUSH_B_1, OP_PUSH_B_2, OP_PUSH_B_3, OP_PUSH_B_4,
	OP_PUSH_C_1, OP_PUSH_C_2, OP_PUSH_C_3, OP_PUSH_C_4,
	OP_PUSH_D_1, OP_PUSH_D_2, OP_PUSH_D_3, OP_PUSH_D_4,
	// 0 0011 pop from the stack to [a,b]
	OP_POP_A_1, OP_POP_A_2, OP_POP_A_3, OP_POP_A_4,
	OP_POP_B_1, OP_POP_B_2, OP_POP_B_3, OP_POP_B_4,
	OP_POP_C_1, OP_POP_C_2, OP_POP_C_3, OP_POP_C_4,
	OP_POP_D_1, OP_POP_D_2, OP_POP_D_3, OP_POP_D_4,
	// 0 0100 flow control
	OP_IFNOT_A, OP_IFNOT_B, OP_IFNOT_C, OP_IFNOT_D,
	OP_IF_A,    OP_IF_B,    OP_IF_C,    OP_IF_D,
	OP_JUMP_A,  OP_JUMP_B,  OP_JUMP_C,  OP_JUMP_D,
	OP_CALL_A,  OP_CALL_B,  OP_CALL_C,  OP_CALL_D,
	// 0 0101 calls
	OP_CALL_1,   OP_CALL_2,   OP_CALL_3,   OP_CALL_4,
	OP_CALL_5,   OP_CALL_6,   OP_CALL_7,   OP_CALL_8,
	OP_RETURN_1, OP_RETURN_2, OP_RETURN_3, OP_RETURN_4,
	OP_RETURN_0, OP_WITH,  OP_STATE_ft, OP_STATE_ftt,
	// 0 0110 flow control 2
	OP_IFA_A,  OP_IFA_B,  OP_IFA_C,  OP_IFA_D,
	OP_IFBE_A, OP_IFBE_B, OP_IFBE_C, OP_IFBE_D,
	OP_IFB_A,  OP_IFB_B,  OP_IFB_C,  OP_IFB_D,
	OP_IFAE_A, OP_IFAE_B, OP_IFAE_C, OP_IFAE_D,
	// 0 0111 interpreted vector multiplication
	// C complex
	// V vector (3d)
	// Q quaternion
	OP_DOT_CC_F, OP_DOT_VV_F, OP_DOT_QQ_F, OP_CROSS_VV_F,
	OP_MUL_CC_F, OP_MUL_QV_F, OP_MUL_VQ_F, OP_MUL_QQ_F,
	OP_DOT_CC_D, OP_DOT_VV_D, OP_DOT_QQ_D, OP_CROSS_VV_D,
	OP_MUL_CC_D, OP_MUL_QV_D, OP_MUL_VQ_D, OP_MUL_QQ_D,
	// comparison
	// 0 1000 ==
	OP_EQ_I_1, OP_EQ_I_2, OP_EQ_I_3, OP_EQ_I_4,
	OP_EQ_F_1, OP_EQ_F_2, OP_EQ_F_3, OP_EQ_F_4,
	OP_EQ_L_1, OP_EQ_L_2, OP_EQ_L_3, OP_EQ_L_4,
	OP_EQ_D_1, OP_EQ_D_2, OP_EQ_D_3, OP_EQ_D_4,
	// 0 1001 <
	OP_LT_I_1, OP_LT_I_2, OP_LT_I_3, OP_LT_I_4,
	OP_LT_F_1, OP_LT_F_2, OP_LT_F_3, OP_LT_F_4,
	OP_LT_L_1, OP_LT_L_2, OP_LT_L_3, OP_LT_L_4,
	OP_LT_D_1, OP_LT_D_2, OP_LT_D_3, OP_LT_D_4,
	// 0 1010 >
	OP_GT_I_1, OP_GT_I_2, OP_GT_I_3, OP_GT_I_4,
	OP_GT_F_1, OP_GT_F_2, OP_GT_F_3, OP_GT_F_4,
	OP_GT_L_1, OP_GT_L_2, OP_GT_L_3, OP_GT_L_4,
	OP_GT_D_1, OP_GT_D_2, OP_GT_D_3, OP_GT_D_4,
	// 0 1011 convert between signed integral and double(XXX how useful as vec?)
	OP_CONV_ID_1, OP_CONV_ID_2, OP_CONV_ID_3, OP_CONV_ID_4,
	OP_CONV_DI_1, OP_CONV_DI_2, OP_CONV_DI_3, OP_CONV_DI_4,
	OP_CONV_LD_1, OP_CONV_LD_2, OP_CONV_LD_3, OP_CONV_LD_4,
	OP_CONV_DL_1, OP_CONV_DL_2, OP_CONV_DL_3, OP_CONV_DL_4,
	// comparison
	// 0 1100 !=
	OP_NE_I_1, OP_NE_I_2, OP_NE_I_3, OP_NE_I_4,
	OP_NE_F_1, OP_NE_F_2, OP_NE_F_3, OP_NE_F_4,
	OP_NE_L_1, OP_NE_L_2, OP_NE_L_3, OP_NE_L_4,
	OP_NE_D_1, OP_NE_D_2, OP_NE_D_3, OP_NE_D_4,
	// 0 1101 >=
	OP_GE_I_1, OP_GE_I_2, OP_GE_I_3, OP_GE_I_4,
	OP_GE_F_1, OP_GE_F_2, OP_GE_F_3, OP_GE_F_4,
	OP_GE_L_1, OP_GE_L_2, OP_GE_L_3, OP_GE_L_4,
	OP_GE_D_1, OP_GE_D_2, OP_GE_D_3, OP_GE_D_4,
	// 0 1110 <=
	OP_LE_I_1, OP_LE_I_2, OP_LE_I_3, OP_LE_I_4,
	OP_LE_F_1, OP_LE_F_2, OP_LE_F_3, OP_LE_F_4,
	OP_LE_L_1, OP_LE_L_2, OP_LE_L_3, OP_LE_L_4,
	OP_LE_D_1, OP_LE_D_2, OP_LE_D_3, OP_LE_D_4,
	// 0 1111 convert between signed integral sizes (XXX how useful as vec?)
	OP_CONV_IL_1, OP_CONV_IL_2, OP_CONV_IL_3, OP_CONV_IL_4,
	OP_CONV_LI_1, OP_CONV_LI_2, OP_CONV_LI_3, OP_CONV_LI_4,
	OP_CONV_uU_1, OP_CONV_uU_2, OP_CONV_uU_3, OP_CONV_uU_4,
	OP_CONV_Uu_1, OP_CONV_Uu_2, OP_CONV_Uu_3, OP_CONV_Uu_4,

	// 1 0000 c = a * b
	OP_MUL_I_1, OP_MUL_I_2, OP_MUL_I_3, OP_MUL_I_4,
	OP_MUL_F_1, OP_MUL_F_2, OP_MUL_F_3, OP_MUL_F_4,
	OP_MUL_L_1, OP_MUL_L_2, OP_MUL_L_3, OP_MUL_L_4,
	OP_MUL_D_1, OP_MUL_D_2, OP_MUL_D_3, OP_MUL_D_4,
	// 1 0001 c = a / b
	OP_DIV_I_1, OP_DIV_I_2, OP_DIV_I_3, OP_DIV_I_4,
	OP_DIV_F_1, OP_DIV_F_2, OP_DIV_F_3, OP_DIV_F_4,
	OP_DIV_L_1, OP_DIV_L_2, OP_DIV_L_3, OP_DIV_L_4,
	OP_DIV_D_1, OP_DIV_D_2, OP_DIV_D_3, OP_DIV_D_4,
	// 1 0010 c = a % b (remainder, C %)
	OP_REM_I_1, OP_REM_I_2, OP_REM_I_3, OP_REM_I_4,
	OP_REM_F_1, OP_REM_F_2, OP_REM_F_3, OP_REM_F_4,
	OP_REM_L_1, OP_REM_L_2, OP_REM_L_3, OP_REM_L_4,
	OP_REM_D_1, OP_REM_D_2, OP_REM_D_3, OP_REM_D_4,
	// 1 0011 c = a %% b (true modulo, python %)
	OP_MOD_I_1, OP_MOD_I_2, OP_MOD_I_3, OP_MOD_I_4,
	OP_MOD_F_1, OP_MOD_F_2, OP_MOD_F_3, OP_MOD_F_4,
	OP_MOD_L_1, OP_MOD_L_2, OP_MOD_L_3, OP_MOD_L_4,
	OP_MOD_D_1, OP_MOD_D_2, OP_MOD_D_3, OP_MOD_D_4,
	// 1 0100 c = a + b
	OP_ADD_I_1, OP_ADD_I_2, OP_ADD_I_3, OP_ADD_I_4,
	OP_ADD_F_1, OP_ADD_F_2, OP_ADD_F_3, OP_ADD_F_4,
	OP_ADD_L_1, OP_ADD_L_2, OP_ADD_L_3, OP_ADD_L_4,
	OP_ADD_D_1, OP_ADD_D_2, OP_ADD_D_3, OP_ADD_D_4,
	// 1 0101 c = a - b
	OP_SUB_I_1, OP_SUB_I_2, OP_SUB_I_3, OP_SUB_I_4,
	OP_SUB_F_1, OP_SUB_F_2, OP_SUB_F_3, OP_SUB_F_4,
	OP_SUB_L_1, OP_SUB_L_2, OP_SUB_L_3, OP_SUB_L_4,
	OP_SUB_D_1, OP_SUB_D_2, OP_SUB_D_3, OP_SUB_D_4,
	// 1 0110 c = a << b (string ops mixed in)
	OP_SHL_I_1, OP_SHL_I_2, OP_SHL_I_3, OP_SHL_I_4,
	OP_EQ_S,    OP_LT_S,    OP_GT_S,    OP_ADD_S,
	OP_SHL_L_1, OP_SHL_L_2, OP_SHL_L_3, OP_SHL_L_4,
	OP_CMP_S,   OP_GE_S,    OP_LE_S,    OP_NOT_S,  //OP_CMP_S doubles as NE
	// 1 0111 c = a >> b
	OP_SHR_I_1, OP_SHR_I_2, OP_SHR_I_3, OP_SHR_I_4,
	OP_SHR_u_1, OP_SHR_u_2, OP_SHR_u_3, OP_SHR_u_4,
	OP_SHR_L_1, OP_SHR_L_2, OP_SHR_L_3, OP_SHR_L_4,
	OP_SHR_U_1, OP_SHR_U_2, OP_SHR_U_3, OP_SHR_U_4,
	// 1 1000 c = a (& | ^) b or ~a (bitwise ops)
	OP_BITAND_I_1, OP_BITAND_I_2, OP_BITAND_I_3, OP_BITAND_I_4,
	OP_BITOR_I_1,  OP_BITOR_I_2,  OP_BITOR_I_3,  OP_BITOR_I_4,
	OP_BITXOR_I_1, OP_BITXOR_I_2, OP_BITXOR_I_3, OP_BITXOR_I_4,
	OP_BITNOT_I_1, OP_BITNOT_I_2, OP_BITNOT_I_3, OP_BITNOT_I_4,
	// 1 1001 < unsigned (float logic and bit ops mixed in)
	OP_LT_u_1,   OP_LT_u_2,  OP_LT_u_3,   OP_LT_u_4,
	OP_BITAND_F, OP_BITOR_F, OP_BITXOR_F, OP_BITNOT_F,
	OP_LT_U_1,   OP_LT_U_2,  OP_LT_U_3,   OP_LT_U_4,
	OP_AND_F,    OP_OR_F,    OP_XOR_F,    OP_NOT_F,
	// 1 1010 > unsigned
	OP_GT_u_1, OP_GT_u_2, OP_GT_u_3, OP_GT_u_4,
	OP_spare,  OP_NOT_D,  OP_NOT_V,  OP_NOT_Q,
	OP_GT_U_1, OP_GT_U_2, OP_GT_U_3, OP_GT_U_4,
	OP_EQ_V,   OP_EQ_Q,   OP_NE_V,   OP_NE_Q,
	// 1 1011 lea, with, etc
	OP_LEA_A,   OP_LEA_B,  OP_LEA_C,  OP_LEA_D,
	OP_LEA_E,   OP_ANY_2,  OP_ANY_3,  OP_ANY_4,
	OP_PUSHREG, OP_ALL_2,  OP_ALL_3,  OP_ALL_4,
	OP_POPREG,  OP_NONE_2, OP_NONE_3, OP_NONE_4,
	// 1 1100 c = a (&& || ^^) b or !a (logical ops (no short circuit))
	OP_AND_I_1, OP_AND_I_2, OP_AND_I_3, OP_AND_I_4,
	OP_OR_I_1,  OP_OR_I_2,  OP_OR_I_3,  OP_OR_I_4,
	OP_XOR_I_1, OP_XOR_I_2, OP_XOR_I_3, OP_XOR_I_4,
	OP_NOT_I_1, OP_NOT_I_2, OP_NOT_I_3, OP_NOT_I_4,
	// 1 1101 >= unsigned with float shifts and moves mixed in
	OP_GE_u_1, OP_GE_u_2,   OP_GE_u_3,   OP_GE_u_4,
	OP_SHL_F,  OP_MOVE_I,   OP_MOVE_P,   OP_MOVE_PI,
	OP_GE_U_1, OP_GE_U_2,   OP_GE_U_3,   OP_GE_U_4,
	OP_SHR_F,  OP_MEMSET_I, OP_MEMSET_P, OP_MEMSET_PI,
	// 1 1110 <= unsigned with scale and swizzle mixed in
	OP_LE_u_1,    OP_LE_u_2,    OP_LE_u_3,    OP_LE_u_4,
	OP_SWIZZLE_F, OP_SCALE_F_2, OP_SCALE_F_3, OP_SCALE_F_4,
	OP_LE_U_1,    OP_LE_U_2,    OP_LE_U_3,    OP_LE_U_4,
	OP_SWIZZLE_D, OP_SCALE_D_2, OP_SCALE_D_3, OP_SCALE_D_4,
	// 1 1111 convert between integral and float (XXX how useful as vec?)
	OP_CONV_IF_1, OP_CONV_IF_2, OP_CONV_IF_3, OP_CONV_IF_4,
	OP_CONV_FI_1, OP_CONV_FI_2, OP_CONV_FI_3, OP_CONV_FI_4,
	OP_CONV_FD_1, OP_CONV_FD_2, OP_CONV_FD_3, OP_CONV_FD_4,
	OP_CONV_DF_1, OP_CONV_DF_2, OP_CONV_DF_3, OP_CONV_DF_4,
} pr_opcode_e;
#define OP_A_SHIFT (9)
#define OP_B_SHIFT (11)
#define OP_C_SHIFT (13)
#define OP_A_BASE (3 << OP_A_SHIFT)
#define OP_B_BASE (3 << OP_B_SHIFT)
#define OP_C_BASE (3 << OP_C_SHIFT)
#define OP_MASK (~(OP_BREAK|OP_A_BASE|OP_B_BASE|OP_C_BASE))

typedef enum {
	OP_with_zero,
	OP_with_base,
	OP_with_stack,
	OP_with_entity,
} pr_with_e;

typedef struct v6p_opcode_s {
	const char *name;
	const char *opname;
	etype_t     type_a, type_b, type_c;
	unsigned int min_version;
	const char *fmt;
} v6p_opcode_t;

extern const v6p_opcode_t pr_v6p_opcodes[];
const v6p_opcode_t *PR_v6p_Opcode (pr_ushort_t opcode) __attribute__((const));
void PR_Opcode_Init (void);	// idempotent

typedef struct dstatement_s {
	pr_opcode_e op:16;			// will be pr_opcode_v6p_e for older progs
	pr_ushort_t a,b,c;
} GCC_STRUCT dstatement_t;

typedef struct ddef_s {
	pr_ushort_t type;			// if DEF_SAVEGLOBAL bit is set
								// the variable needs to be saved in savegames
	pr_ushort_t ofs;
	string_t    name;
} ddef_t;

typedef struct xdef_s {
	pointer_t   type;			///< pointer to type definition
	pointer_t   ofs;			///< 32-bit version of ddef_t.ofs
} xdef_t;

typedef struct pr_xdefs_s {
	pointer_t   xdefs;
	pr_int_t    num_xdefs;
} pr_xdefs_t;

typedef struct pr_def_s {
	pr_ushort_t type;
	pr_ushort_t size;			///< may not be correct
	pointer_t   ofs;
	string_t    name;
	pointer_t   type_encoding;
} pr_def_t;

typedef struct dparmsize_s {
	uint8_t     size:5;
	uint8_t     alignment:3;
} dparmsize_t;

#define	DEF_SAVEGLOBAL	(1<<15)

#define	MAX_PARMS	8

typedef struct dfunction_s {
	pr_int_t    first_statement;	// negative numbers are builtins
	pr_uint_t   parm_start;			// beginning of locals data space
	pr_uint_t   locals;				// total ints of parms + locals

	pr_uint_t   profile;			// runtime

	string_t    name;				// source function name
	string_t    file;				// source file defined in

	pr_int_t    numparms;			// -ve is varargs (1s comp of real count)
	dparmsize_t parm_size[MAX_PARMS];
} dfunction_t;

typedef union pr_type_u {
	float       float_var;
	string_t    string_var;
	func_t      func_var;
	pr_int_t    entity_var;
	float       vector_var;	// really [3], but this structure must be 32 bits
	float       quat_var;	// really [4], but this structure must be 32 bits
	pr_int_t    integer_var;
	pointer_t   pointer_var;
	pr_uint_t   uinteger_var;
} pr_type_t;

typedef struct pr_va_list_s {
	pr_int_t    count;
	pointer_t   list;			// pr_type_t
} pr_va_list_t;

#define PROG_VERSION_ENCODE(a,b,c)	\
	( (((0x##a) & 0x0ff) << 24)		\
	 |(((0x##b) & 0xfff) << 12)		\
	 |(((0x##c) & 0xfff) <<  0) )
#define	PROG_ID_VERSION	6
#define	PROG_V6P_VERSION	PROG_VERSION_ENCODE(0,fff,00a)
#define	PROG_VERSION	PROG_VERSION_ENCODE(0,fff,010)

typedef struct dprograms_s {
	pr_uint_t   version;
	pr_uint_t   crc;			// check of header file

	pr_uint_t   ofs_statements;
	pr_uint_t   numstatements;	// statement 0 is an error

	pr_uint_t   ofs_globaldefs;
	pr_uint_t   numglobaldefs;

	pr_uint_t   ofs_fielddefs;
	pr_uint_t   numfielddefs;

	pr_uint_t   ofs_functions;
	pr_uint_t   numfunctions;	// function 0 is an empty

	pr_uint_t   ofs_strings;
	pr_uint_t   numstrings;		// first string is a null string

	pr_uint_t   ofs_globals;
	pr_uint_t   numglobals;

	pr_uint_t   entityfields;
} dprograms_t;

#endif//__QF_pr_comp_h
