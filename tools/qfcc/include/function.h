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

	$Id$
*/

#ifndef __function_h
#define __function_h

#include "QF/pr_comp.h"
#include "QF/pr_debug.h"

#include "def.h"

typedef struct overloaded_function_s {
	struct overloaded_function_s *next;
	const char *name;				///< source level name of function
	const char *full_name;			///< progs name of function, with type
									///< encoding
	struct type_s *type;			///< type of this function
	int         overloaded;			///< is this function overloaded
	string_t    file;				///< source file of the function
	int         line;				///< source line of this function
} overloaded_function_t;

typedef struct function_s {
	struct function_s  *next;
	pr_auxfunction_t   *aux;		///< debug info;
	int                 builtin;	///< if non 0, call an internal function
	int                 code;		///< first statement
	int                 function_num;
	string_t            s_file;		///< source file with definition
	string_t            s_name;
	int                 temp_num;	///< number for next temp var
	struct def_s       *def;
	struct symbol_s    *sym;
	struct symtab_s    *symtab;
	struct reloc_s     *refs;
	struct expr_s      *var_init;
	const char         *name;		///< nice name for __PRETTY_FUNCTION__
} function_t;

extern function_t *current_func;

typedef struct param_s {
	// the first two fields match the first two fiels of keywordarg_t in
	// method.h
	struct param_s *next;
	const char *selector;
	struct type_s *type;		//FIXME redundant
	const char *name;			//FIXME redundant
	struct symbol_s *symbol;
} param_t;

struct expr_s;
struct symbol_s;
struct symtab_s;

param_t *new_param (const char *selector, struct type_s *type,
					const char *name);
param_t *param_append_identifiers (param_t *params, struct symbol_s *idents,
								   struct type_s *type);
param_t *_reverse_params (param_t *params, param_t *next);
param_t *reverse_params (param_t *params);
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
							struct symtab_s *parent, int far);
function_t *build_code_function (struct symbol_s *fsym,
								 struct expr_s *state_expr,
								 struct expr_s *statements);
function_t *build_builtin_function (struct symbol_s *sym,
									struct expr_s *bi_val, int far);
void build_function (function_t *f);
void finish_function (function_t *f);
void emit_function (function_t *f, struct expr_s *e);
int function_parms (function_t *f, byte *parm_size);

#endif//__function_h
