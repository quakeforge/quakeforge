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

typedef enum param_qual_e {
	pq_const,
	pq_in,
	pq_out,
	pq_inout,
} param_qual_t;

typedef struct function_s function_t;
typedef struct typeeval_s typeeval_t;

typedef struct gentype_s {
	const char *name;
	// earlier types have priority over later types. null if compute valid
	const type_t **valid_types;
} gentype_t;

typedef struct genparam_s {
	const char *name;
	const type_t *fixed_type;
	typeeval_t *compute;
	int         gentype;	// index into function's list of types
	param_qual_t qual;
	bool        is_reference;
	unsigned    tag;
} genparam_t;

typedef struct genfunc_s {
	struct genfunc_s *next;
	const char *name;
	gentype_t  *types;
	int         num_types;
	int         num_params;
	genparam_t *params;
	genparam_t *ret_type;
	const expr_t *expr;				///< inline or intrinsic
	bool        can_inline;
} genfunc_t;

typedef enum {
	mf_simple,
	mf_overload,
	mf_generic,
} mf_type_e;

/** Internal representation of a function.
*/
typedef struct function_s {
	struct function_s  *next;
	const char         *o_name;
	int                 builtin;	///< if non 0, call an internal function
	int                 code;		///< first statement
	int                 id;
	int                 function_num;
	int                 line_info;
	int                 params_start;///< relative to locals space. 0 for v6p
	pr_string_t         s_file;		///< source file with definition
	pr_string_t         s_name;		///< name of function in output
	const type_t       *type;		///< function's type without aliases
	int                 temp_reg;	///< base register to use for temp defs
	int                 temp_num;	///< number for next temp var
	struct def_s       *temp_defs[MAX_DEF_SIZE];///< freed temp vars (by size)
	struct def_s       *def;		///< output def holding function number
	symbol_t           *sym;		///< internal symbol for this function
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
	symtab_t           *parameters;	///< Root scope symbol table
	symtab_t           *locals;		///< Actual local variables
	struct defspace_s  *arguments;	///< Space for called function arguments
	///@}
	symtab_t           *label_scope;
	struct reloc_s     *refs;		///< relocation targets for this function
	expr_t             *var_init;
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
	const expr_t       *exprs;

	const expr_t     *(*return_imp) (function_t *func, const expr_t *val);
	const expr_t       *return_val;
	const expr_t       *return_label;
} function_t;

/** Represent an overloading of a function.

	Every function, whether overloaded or not, has a meta-function
*/
typedef struct metafunc_s {
	struct metafunc_s *next;
	const char *name;				///< source level name of function
	const char *full_name;			///< progs name of function, with type
									///< encoding
	const type_t *type;				///< type of this function
	rua_loc_t   loc;				///< source location of the function
	mf_type_e   meta_type;			///< is this function overloaded
	function_t *func;
	genfunc_t  *genfunc;
	const expr_t *state_expr;
	const expr_t *expr;				///< inline or intrinsic
	bool        can_inline;
} metafunc_t;

extern function_t *current_func;

/** Representation of a function parameter.
	\note	The first two fields match the first two fields of keywordarg_t
			in method.h
*/
typedef struct param_s {
	struct param_s *next;
	const char *selector;
	const type_t *type;
	const expr_t *type_expr;
	const char *name;
	param_qual_t qual;
	attribute_t *attributes;
} param_t;

typedef struct expr_s expr_t;
typedef struct symbol_s symbol_t;
typedef struct symtab_s symtab_t;

genfunc_t *add_generic_function (genfunc_t *genfunc);

param_t *new_param (const char *selector, const type_t *type, const char *name);
param_t *new_generic_param (const expr_t *type_expr, const char *name);
param_t *param_append_identifiers (param_t *params, struct symbol_s *idents,
								   const type_t *type);
param_t *reverse_params (param_t *params);
param_t *append_params (param_t *params, param_t *more_params);
param_t *copy_params (param_t *params);
const type_t *parse_params (const type_t *return_type, param_t *params);

param_t *check_params (param_t *params);

enum storage_class_e : unsigned;
struct defspace_s;
int value_too_large (const type_t *val_type) __attribute__((pure));
function_t *make_function (symbol_t *sym, const char *nice_name,
						struct defspace_s *space,
						enum storage_class_e storage);
symbol_t *function_symbol (specifier_t spec, rua_ctx_t *ctx);
const expr_t *find_function (const expr_t *fexpr, const expr_t *params,
							 rua_ctx_t *ctx);
function_t *begin_function (specifier_t spec, const char *nicename,
							symtab_t *parent, rua_ctx_t *ctx);
void build_code_function (specifier_t spec, const expr_t *state_expr,
						  expr_t *statements, rua_ctx_t *ctx);
void build_builtin_function (specifier_t spec, const char *ext_name,
							 const expr_t *bi_val);
void build_intrinsic_function (specifier_t spec, const expr_t *intrinsic,
							   rua_ctx_t *ctx);
void emit_function (function_t *f, expr_t *e);
void clear_functions (void);

void add_ctor_expr (const expr_t *expr);
void emit_ctor (rua_ctx_t *ctx);

///@}

#endif//__function_h
