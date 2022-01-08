/*
	function.h

	QC function support code

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/05/08

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

#ifndef __function_h
#define __function_h

/** \defgroup qfcc_function Internal function structures.
	\ingroup qfcc
*/
///@{

#include "QF/progs/pr_comp.h"
#include "QF/progs/pr_debug.h"

#include "def.h"

/** Represent an overloading of a function.

	Every function, whether overloaded or not, has an entry in the overloaded
	function database.
*/
typedef struct overloaded_function_s {
	struct overloaded_function_s *next;
	const char *name;				///< source level name of function
	const char *full_name;			///< progs name of function, with type
									///< encoding
	const struct type_s *type;		///< type of this function
	int         overloaded;			///< is this function overloaded
	string_t    file;				///< source file of the function
	int         line;				///< source line of this function
} overloaded_function_t;

/** Internal representation of a function.
*/
typedef struct function_s {
	struct function_s  *next;
	int                 builtin;	///< if non 0, call an internal function
	int                 code;		///< first statement
	int                 function_num;
	int                 line_info;
	int                 local_defs;
	string_t            s_file;		///< source file with definition
	string_t            s_name;		///< name of function in output
	const struct type_s *type;		///< function's type without aliases
	int                 temp_num;	///< number for next temp var
	struct def_s       *temp_defs[4];	///< freed temp vars (by size)
	struct def_s       *def;		///< output def holding function number
	struct symbol_s    *sym;		///< internal symbol for this function
	/** Root scope symbol table of the function.

		Sub-scope symbol tables are not directly accessible, but all defs
		created in the function's local data space are recorded in the root
		scope symbol table's defspace.
	*/
	struct symtab_s    *symtab;
	struct symtab_s    *label_scope;
	struct reloc_s     *refs;		///< relocation targets for this function
	struct expr_s      *var_init;
	const char         *name;		///< nice name for __PRETTY_FUNCTION__
	struct sblock_s    *sblock;		///< initial node of function's code
	struct flowgraph_s *graph;		///< the function's flow graph
	/** Array of pointers to all variables referenced by the function's code.

		This permits ready mapping of (function specific) variable number to
		variable in the flow analyzer.
	*/
	struct flowvar_s  **vars;
	int                 num_vars;	///< total number of variables referenced
	struct set_s       *global_vars;///< set indicating which vars are global
	struct statement_s **statements;
	int                 num_statements;
	int                 pseudo_addr;///< pseudo address space for flow analysis
	struct pseudoop_s  *pseudo_ops;///< pseudo operands used by this function
} function_t;

extern function_t *current_func;

/** Representation of a function parameter.
	\note	The first two fields match the first two fields of keywordarg_t
			in method.h
*/
typedef struct param_s {
	struct param_s *next;
	const char *selector;
	struct type_s *type;
	const char *name;
	struct symbol_s *symbol;	//FIXME what is this for?
} param_t;

struct expr_s;
struct symbol_s;
struct symtab_s;

param_t *new_param (const char *selector, struct type_s *type,
					const char *name);
param_t *param_append_identifiers (param_t *params, struct symbol_s *idents,
								   struct type_s *type);
param_t *reverse_params (param_t *params);
param_t *append_params (param_t *params, param_t *more_params);
param_t *copy_params (param_t *params);
struct type_s *parse_params (struct type_s *type, param_t *params);
param_t *check_params (param_t *params);

enum storage_class_e;
struct defspace_s;
void make_function (struct symbol_s *sym, const char *nice_name,
					struct defspace_s *space, enum storage_class_e storage);
struct symbol_s *function_symbol (struct symbol_s *sym,
								  int overload, int create);
struct expr_s *find_function (struct expr_s *fexpr, struct expr_s *params);
function_t *new_function (const char *name, const char *nice_name);
void add_function (function_t *f);
function_t *begin_function (struct symbol_s *sym, const char *nicename,
							struct symtab_s *parent, int far,
							enum storage_class_e storage);
function_t *build_code_function (struct symbol_s *fsym,
								 struct expr_s *state_expr,
								 struct expr_s *statements);
function_t *build_builtin_function (struct symbol_s *sym,
									struct expr_s *bi_val, int far,
									enum storage_class_e storage);
void finish_function (function_t *f);
void emit_function (function_t *f, struct expr_s *e);
int function_parms (function_t *f, byte *parm_size);
void clear_functions (void);

///@}

#endif//__function_h
