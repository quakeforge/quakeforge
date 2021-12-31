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

typedef int16_t pr_short_t;
typedef uint16_t pr_ushort_t;
typedef int32_t pr_int_t;
typedef uint32_t pr_uint_t;
typedef pr_uint_t func_t;
typedef pr_int_t string_t;
typedef pr_uint_t pointer_t;

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
	OP_DONE,
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,

	OP_EQ_F,
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FN,

	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FN,

	OP_LE_F,
	OP_GE_F,
	OP_LT_F,
	OP_GT_F,

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FN,

	OP_ADDRESS,

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FN,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,
	OP_STOREP_FLD,
	OP_STOREP_FN,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FN,
	OP_IF,
	OP_IFNOT,
	OP_CALL0,
	OP_CALL1,
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,
	OP_GOTO,
	OP_AND,
	OP_OR,

	OP_BITAND,
	OP_BITOR,	// end of v6 opcodes

	OP_ADD_S,
	OP_LE_S,
	OP_GE_S,
	OP_LT_S,
	OP_GT_S,

	OP_ADD_I,
	OP_SUB_I,
	OP_MUL_I,
	OP_DIV_I,
	OP_BITAND_I,
	OP_BITOR_I,
	OP_GE_I,
	OP_LE_I,
	OP_GT_I,
	OP_LT_I,
	OP_AND_I,
	OP_OR_I,
	OP_NOT_I,
	OP_EQ_I,
	OP_NE_I,
	OP_STORE_I,
	OP_STOREP_I,
	OP_LOAD_I,

	OP_CONV_IF,
	OP_CONV_FI,

	OP_BITXOR_F,
	OP_BITXOR_I,
	OP_BITNOT_F,
	OP_BITNOT_I,

	OP_SHL_F,
	OP_SHR_F,
	OP_SHL_I,
	OP_SHR_I,

	OP_REM_F,
	OP_REM_I,

	OP_LOADB_F,
	OP_LOADB_V,
	OP_LOADB_S,
	OP_LOADB_ENT,
	OP_LOADB_FLD,
	OP_LOADB_FN,
	OP_LOADB_I,
	OP_LOADB_P,

	OP_STOREB_F,
	OP_STOREB_V,
	OP_STOREB_S,
	OP_STOREB_ENT,
	OP_STOREB_FLD,
	OP_STOREB_FN,
	OP_STOREB_I,
	OP_STOREB_P,

	OP_ADDRESS_VOID,
	OP_ADDRESS_F,
	OP_ADDRESS_V,
	OP_ADDRESS_S,
	OP_ADDRESS_ENT,
	OP_ADDRESS_FLD,
	OP_ADDRESS_FN,
	OP_ADDRESS_I,
	OP_ADDRESS_P,

	OP_LEA,

	OP_IFBE,
	OP_IFB,
	OP_IFAE,
	OP_IFA,

	OP_JUMP,
	OP_JUMPB,

	OP_LT_U,
	OP_GT_U,
	OP_LE_U,
	OP_GE_U,

	OP_LOADBI_F,
	OP_LOADBI_V,
	OP_LOADBI_S,
	OP_LOADBI_ENT,
	OP_LOADBI_FLD,
	OP_LOADBI_FN,
	OP_LOADBI_I,
	OP_LOADBI_P,

	OP_STOREBI_F,
	OP_STOREBI_V,
	OP_STOREBI_S,
	OP_STOREBI_ENT,
	OP_STOREBI_FLD,
	OP_STOREBI_FN,
	OP_STOREBI_I,
	OP_STOREBI_P,

	OP_LEAI,

	OP_LOAD_P,
	OP_STORE_P,
	OP_STOREP_P,
	OP_NOT_P,
	OP_EQ_P,
	OP_NE_P,
	OP_LE_P,
	OP_GE_P,
	OP_LT_P,
	OP_GT_P,

	OP_MOVEI,
	OP_MOVEP,
	OP_MOVEPI,

	OP_SHR_U,

	OP_STATE_F,

	OP_ADD_Q,
	OP_SUB_Q,
	OP_MUL_Q,
	OP_MUL_QF,
	OP_MUL_FQ,
	OP_MUL_QV,
	OP_CONJ_Q,
	OP_NOT_Q,
	OP_EQ_Q,
	OP_NE_Q,
	OP_STORE_Q,
	OP_STOREB_Q,
	OP_STOREBI_Q,
	OP_STOREP_Q,
	OP_LOAD_Q,
	OP_LOADB_Q,
	OP_LOADBI_Q,
	OP_ADDRESS_Q,

	OP_RCALL0,
	OP_RCALL1,
	OP_RCALL2,
	OP_RCALL3,
	OP_RCALL4,
	OP_RCALL5,
	OP_RCALL6,
	OP_RCALL7,
	OP_RCALL8,

	OP_RETURN_V,

	OP_PUSH_S,
	OP_PUSH_F,
	OP_PUSH_V,
	OP_PUSH_ENT,
	OP_PUSH_FLD,
	OP_PUSH_FN,
	OP_PUSH_P,
	OP_PUSH_Q,
	OP_PUSH_I,
	OP_PUSH_D,

	OP_PUSHB_S,
	OP_PUSHB_F,
	OP_PUSHB_V,
	OP_PUSHB_ENT,
	OP_PUSHB_FLD,
	OP_PUSHB_FN,
	OP_PUSHB_P,
	OP_PUSHB_Q,
	OP_PUSHB_I,
	OP_PUSHB_D,

	OP_PUSHBI_S,
	OP_PUSHBI_F,
	OP_PUSHBI_V,
	OP_PUSHBI_ENT,
	OP_PUSHBI_FLD,
	OP_PUSHBI_FN,
	OP_PUSHBI_P,
	OP_PUSHBI_Q,
	OP_PUSHBI_I,
	OP_PUSHBI_D,

	OP_POP_S,
	OP_POP_F,
	OP_POP_V,
	OP_POP_ENT,
	OP_POP_FLD,
	OP_POP_FN,
	OP_POP_P,
	OP_POP_Q,
	OP_POP_I,
	OP_POP_D,

	OP_POPB_S,
	OP_POPB_F,
	OP_POPB_V,
	OP_POPB_ENT,
	OP_POPB_FLD,
	OP_POPB_FN,
	OP_POPB_P,
	OP_POPB_Q,
	OP_POPB_I,
	OP_POPB_D,

	OP_POPBI_S,
	OP_POPBI_F,
	OP_POPBI_V,
	OP_POPBI_ENT,
	OP_POPBI_FLD,
	OP_POPBI_FN,
	OP_POPBI_P,
	OP_POPBI_Q,
	OP_POPBI_I,
	OP_POPBI_D,

	OP_ADD_D,
	OP_SUB_D,
	OP_MUL_D,
	OP_MUL_QD,
	OP_MUL_DQ,
	OP_MUL_VD,
	OP_MUL_DV,
	OP_DIV_D,
	OP_REM_D,
	OP_GE_D,
	OP_LE_D,
	OP_GT_D,
	OP_LT_D,
	OP_NOT_D,
	OP_EQ_D,
	OP_NE_D,
	OP_CONV_FD,
	OP_CONV_DF,
	OP_CONV_ID,
	OP_CONV_DI,
	OP_STORE_D,
	OP_STOREB_D,
	OP_STOREBI_D,
	OP_STOREP_D,
	OP_LOAD_D,
	OP_LOADB_D,
	OP_LOADBI_D,
	OP_ADDRESS_D,

	OP_MOD_I,
	OP_MOD_F,
	OP_MOD_D,

	OP_MEMSETI,
	OP_MEMSETP,
	OP_MEMSETPI,
} pr_opcode_e;
#define OP_BREAK 0x8000

typedef struct opcode_s {
	const char *name;
	const char *opname;
	qboolean    right_associative;
	etype_t     type_a, type_b, type_c;
	unsigned int min_version;
	const char *fmt;
} opcode_t;

extern const opcode_t pr_opcodes[];
const opcode_t *PR_Opcode (pr_short_t opcode) __attribute__((const));
void PR_Opcode_Init (void);	// idempotent

typedef struct dstatement_s {
	pr_opcode_e op:16;
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
#define	PROG_VERSION	PROG_VERSION_ENCODE(0,fff,00a)

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
