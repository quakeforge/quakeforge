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
#ifndef __pr_comp_h
#define __pr_comp_h

#include "QF/qtypes.h"

typedef int	func_t;
typedef int	string_t;
typedef int pointer_t;

typedef enum {
	ev_void,
	ev_string,
	ev_float,
	ev_vector,
	ev_entity,
	ev_field,
	ev_func,
	ev_pointer,			// end of v6 types
	ev_quaternion,
	ev_integer,
	ev_uinteger,
	ev_short,			// value is embedded in the opcode
	ev_struct,
	ev_object,
	ev_class,
	ev_sel,
	ev_array,
	
	ev_type_count		// not a type, gives number of types
} etype_t;

extern int pr_type_size[ev_type_count];
extern const char *pr_type_name[ev_type_count];

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
	OP_BITOR,

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

	OP_MOD_F,
	OP_MOD_I,

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

	OP_MOVE,
	OP_MOVEP,

	OP_ADD_U,
	OP_SUB_U,
	OP_MUL_U,
	OP_DIV_U,
	OP_BITAND_U,
	OP_BITOR_U,
	OP_BITXOR_U,
	OP_BITNOT_U,
	OP_AND_U,
	OP_OR_U,
	OP_NOT_U,
	OP_EQ_U,
	OP_NE_U,
	OP_MOD_U,
	OP_SHL_U,
	OP_SHR_U,
	OP_STORE_U,
	OP_STOREB_U,
	OP_STOREBI_U,
	OP_STOREP_U,
	OP_LOAD_U,
	OP_LOADB_U,
	OP_LOADBI_U,
	OP_ADDRESS_U,

	OP_CONV_IU,
	OP_CONV_UI,

	OP_STATE_F,
} pr_opcode_e;

typedef struct opcode_s {
	const char	*name;
	const char	*opname;
	pr_opcode_e opcode;
	qboolean	right_associative;
	etype_t		type_a, type_b, type_c;
	unsigned int min_version;
	const char	*fmt;
} opcode_t;

extern opcode_t pr_opcodes[];
opcode_t *PR_Opcode (short opcode);
void PR_Opcode_Init (void);

typedef struct statement_s {
	pr_opcode_e		op:16;
	unsigned short	a,b,c;
} dstatement_t;

typedef struct ddef_s {
	unsigned short	type;		// if DEF_SAVEGLOBGAL bit is set
								// the variable needs to be saved in savegames
	unsigned short	ofs;
	int			s_name;
} ddef_t;

#define	DEF_SAVEGLOBAL	(1<<15)

#define	MAX_PARMS	8

typedef struct dfunction_s {
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;				// total ints of parms + locals
	
	int		profile;		// runtime
	
	int		s_name;
	int		s_file;			// source file defined in
	
	int		numparms;
	byte	parm_size[MAX_PARMS];
} dfunction_t;

typedef union pr_type_u {
	float	float_var;
	string_t string_var;
	func_t	func_var;
	int		entity_var;
	float	vector_var[1];		// really 3, but this structure must be 32 bits
	int		integer_var;
	pointer_t pointer_var;
	unsigned int uinteger_var;
} pr_type_t;

typedef struct pr_va_list_s {
	int     count;
	pointer_t list;				// pr_type_t
} pr_va_list_t;

#define	PROG_ID_VERSION	6
#define	PROG_VERSION	0x00fff004	// MMmmmRRR 0.fff.004 (hex)

typedef struct dprograms_s {
	unsigned int version;
	unsigned int crc;			// check of header file
	
	unsigned int ofs_statements;
	unsigned int numstatements;	// statement 0 is an error

	unsigned int ofs_globaldefs;
	unsigned int numglobaldefs;
	
	unsigned int ofs_fielddefs;
	unsigned int numfielddefs;
	
	unsigned int ofs_functions;
	int         numfunctions;	// function 0 is an empty
	
	unsigned int ofs_strings;
	int         numstrings;		// first string is a null string

	unsigned int ofs_globals;
	unsigned int numglobals;
	
	unsigned int entityfields;
} dprograms_t;

#endif // __pr_comp_h
