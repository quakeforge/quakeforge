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

// The maximum size of a temp def, return value, or parameter value
#define MAX_DEF_SIZE 32

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
	rua_loc_t   loc;				///< source location of the function
} overloaded_function_t;

/** Internal representation of a function.
*/
typedef struct function_s {
	struct function_s  *next;
	int                 builtin;	///< if non 0, call an internal function
	int                 code;		///< first statement
	int                 function_num;
	int                 line_info;
	int                 params_start;///< relative to locals space. 0 for v6p
	pr_string_t         s_file;		///< source file with definition
	pr_string_t         s_name;		///< name of function in output
	const struct type_s *type;		///< function's type without aliases
	int                 temp_reg;	///< base register to use for temp defs
	int                 temp_num;	///< number for next temp var
	struct def_s       *temp_defs[MAX_DEF_SIZE];///< freed temp vars (by size)
	struct def_s       *def;		///< output def holding function number
	struct symbol_s    *sym;		///< internal symbol for this function
	/** \name Local data space

		The function parameters form the root scope for the function. Its
		defspace is separate from the locals defspace so that it can be moved
		to the beginning of locals space for v6 progs, and too the end (just
		above the stack pointer on entry to the function) for Ruamoko progs.

		The locals scope is a direct child of the parameters scope, and any
		sub-scope symbol tables are not directly accessible, but all defs
		other than function call arugments created in the function's local
		data space are recorded in the root local scope symbol table's
		defspace.

		The arguments defspace is not used for v6 progs. It is used as a
		highwater allocator for the arguments to all calls made by the
		funciton, with the arguments to separate functions overlapping each
		other.

		Afther the function has been emitted, locals, arguments and possibly
		parameters will be merged into the one defspace.
	*/
	///@{
	struct symtab_s    *parameters;	///< Root scope symbol table
	struct symtab_s    *locals;		///< Actual local variables
	struct defspace_s  *arguments;	///< Space for called function arguments
	///@}
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
	struct set_s       *param_vars;	///< set indicating which vars are params
	struct set_s       *real_statements;///< actual statements for ud-chaining
	struct statement_s **statements;
	int                 num_statements;
	int                 num_ud_chains;
	struct udchain_s   *ud_chains;
	struct udchain_s   *du_chains;
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
	const struct type_s *type;
	const char *name;
	struct symbol_s *symbol;	//FIXME what is this for?
} param_t;

struct expr_s;
struct symbol_s;
struct symtab_s;

param_t *new_param (const char *selector, const struct type_s *type,
					const char *name);
param_t *param_append_identifiers (param_t *params, struct symbol_s *idents,
								   const struct type_s *type);
param_t *reverse_params (param_t *params);
param_t *append_params (param_t *params, param_t *more_params);
param_t *copy_params (param_t *params);
const struct type_s *parse_params (const struct type_s *return_type, param_t *params);

param_t *check_params (param_t *params);

enum storage_class_e;
struct defspace_s;
int value_too_large (const struct type_s *val_type) __attribute__((pure));
void make_function (struct symbol_s *sym, const char *nice_name,
					struct defspace_s *space, enum storage_class_e storage);
struct symbol_s *function_symbol (struct symbol_s *sym,
								  int overload, int create);
const struct expr_s *find_function (const struct expr_s *fexpr,
									const struct expr_s *params);
function_t *new_function (const char *name, const char *nice_name);
void add_function (function_t *f);
function_t *begin_function (struct symbol_s *sym, const char *nicename,
							struct symtab_s *parent, int far,
							enum storage_class_e storage);
function_t *build_code_function (struct symbol_s *fsym,
								 const struct expr_s *state_expr,
								 struct expr_s *statements);
function_t *build_builtin_function (struct symbol_s *sym,
									const struct expr_s *bi_val, int far,
									enum storage_class_e storage);
void emit_function (function_t *f, struct expr_s *e);
int function_parms (function_t *f, byte *parm_size);
void clear_functions (void);

///@}

#endif//__function_h
