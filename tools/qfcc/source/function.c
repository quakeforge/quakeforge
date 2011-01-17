/*
	function.c

	QC function support code

	Copyright (C) 2002 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/5/7

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/va.h"

#include "qfcc.h"

#include "codespace.h"
#include "debug.h"
#include "def.h"
#include "defspace.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "immediate.h"
#include "opcodes.h"
#include "options.h"
#include "reloc.h"
#include "symtab.h"
#include "type.h"

static param_t *free_params;
static function_t *free_functions;
static hashtab_t *overloaded_functions;
static hashtab_t *function_map;

static const char *
ol_func_get_key (void *_f, void *unused)
{
	overloaded_function_t *f = (overloaded_function_t *) _f;
	return f->full_name;
}

static const char *
func_map_get_key (void *_f, void *unused)
{
	overloaded_function_t *f = (overloaded_function_t *) _f;
	return f->name;
}

param_t *
new_param (const char *selector, type_t *type, const char *name)
{
	param_t    *param;

	ALLOC (4096, param_t, params, param);
	param->next = 0;
	param->selector = selector;
	param->type = type;
	param->name = name;

	return param;
}

param_t *
param_append_identifiers (param_t *params, symbol_t *idents, type_t *type)
{
	param_t   **p = &params;

	while (*p)
		p = &(*p)->next;
	if (!idents) {
		*p = new_param (0, 0, 0);
		p = &(*p)->next;
	}
	while (idents) {
		idents->type = type;
		*p = new_param (0, type, idents->name);
		(*p)->symbol = idents;
		p = &(*p)->next;
		idents = idents->next;
	}
	return params;
}

param_t *
_reverse_params (param_t *params, param_t *next)
{
	param_t    *p = params;
	if (params->next)
		p = _reverse_params (params->next, params);
	params->next = next;
	return p;
}

param_t *
reverse_params (param_t *params)
{
	if (!params)
		return 0;
	return _reverse_params (params, 0);
}

param_t *
copy_params (param_t *params)
{
	param_t    *n_parms = 0, **p = &n_parms;

	while (params) {
		*p = new_param (params->selector, params->type, params->name);
		params = params->next;
		p = &(*p)->next;
	}
	return n_parms;
}

type_t *
parse_params (type_t *type, param_t *parms)
{
	param_t    *p;
	type_t      new;

	memset (&new, 0, sizeof (new));
	new.type = ev_func;
	new.t.func.type = type;
	new.t.func.num_params = 0;

	for (p = parms; p; p = p->next) {
		if (new.t.func.num_params > MAX_PARMS) {
			error (0, "too many params");
			return type;
		}
		if (!p->selector && !p->type && !p->name) {
			if (p->next) {
				error (0, "internal error");
				abort ();
			}
			new.t.func.num_params = -(new.t.func.num_params + 1);
		} else if (p->type) {
			new.t.func.param_types[new.t.func.num_params] = p->type;
			new.t.func.num_params++;
		}
	}
	//print_type (&new); puts("");
	return find_type (&new);
}

overloaded_function_t *
get_function (const char *name, type_t *type, int overload, int create)
{
	const char *full_name;
	overloaded_function_t *func;

	if (!overloaded_functions) {
		overloaded_functions = Hash_NewTable (1021, ol_func_get_key, 0, 0);
		function_map = Hash_NewTable (1021, func_map_get_key, 0, 0);
	}

	name = save_string (name);

	full_name = save_string (va ("%s|%s", name, encode_params (type)));

	func = Hash_Find (overloaded_functions, full_name);
	if (func) {
		if (func->type != type) {
			error (0, "can't overload on return types");
			return func;
		}
		return func;
	}

	if (!create)
		return 0;

	func = Hash_Find (function_map, name);
	if (func) {
		if (!overload && !func->overloaded) {
			expr_t     *e = new_expr ();
			e->line = func->line;
			e->file = func->file;
			warning (0, "creating overloaded function %s without @overload",
					 full_name);
			warning (e, "(previous function is %s)", func->full_name);
		}
		overload = 1;
	}

	func = calloc (1, sizeof (overloaded_function_t));
	func->name = name;
	func->full_name = full_name;
	func->type = type;
	func->overloaded = overload;
	func->file = pr.source_file;
	func->line = pr.source_line;

	Hash_Add (overloaded_functions, func);
	Hash_Add (function_map, func);
	return func;
}

def_t *
get_function_def (const char *name, struct type_s *type,
							 scope_t *scope, storage_class_t storage,
							 int overload, int create)
{
	overloaded_function_t *func;
	
	func = get_function (name, type, overload, create);

	if (func && func->overloaded)
		name = func->full_name;
	return get_def (type, name, scope, storage);
}

// NOTE sorts the list in /reverse/ order
static int
func_compare (const void *a, const void *b)
{
	overloaded_function_t *fa = *(overloaded_function_t **) a;
	overloaded_function_t *fb = *(overloaded_function_t **) b;
	type_t     *ta = fa->type;
	type_t     *tb = fb->type;
	int         na = ta->t.func.num_params;
	int         nb = tb->t.func.num_params;
	int         ret, i;

	if (na < 0)
		na = ~na;
	if (nb < 0)
		nb = ~nb;
	if (na != nb)
		return nb - na;
	if ((ret = (fb->type->t.func.num_params - fa->type->t.func.num_params)))
		return ret;
	for (i = 0; i < na && i < nb; i++)
		if (ta->t.func.param_types[i] != tb->t.func.param_types[i])
			return (long)(tb->t.func.param_types[i] - ta->t.func.param_types[i]);
	return 0;
}

expr_t *
find_function (expr_t *fexpr, expr_t *params)
{
	expr_t     *e;
	int         i, j, func_count, parm_count, reported = 0;
	overloaded_function_t *f, dummy, *best = 0;
	type_t      type;
	void      **funcs, *dummy_p = &dummy;

	if (fexpr->type != ex_symbol)
		return fexpr;

	memset (&type, 0, sizeof (type));

	for (e = params; e; e = e->next) {
		if (e->type == ex_error)
			return e;
		type.t.func.num_params++;
	}
	if (type.t.func.num_params > MAX_PARMS)
		return fexpr;
	for (i = 0, e = params; e; i++, e = e->next) {
		type.t.func.param_types[type.t.func.num_params - 1 - i] = get_type (e);
		if (e->type == ex_error)
			return e;
	}
	funcs = Hash_FindList (function_map, fexpr->e.symbol->name);
	if (!funcs)
		return fexpr;
	for (func_count = 0; funcs[func_count]; func_count++)
		;
	if (func_count < 2) {
		free (funcs);
		return fexpr;
	}
	type.t.func.type = ((overloaded_function_t *) funcs[0])->type->t.func.type;
	dummy.type = find_type (&type);

	qsort (funcs, func_count, sizeof (void *), func_compare);
	dummy.full_name = save_string (va ("%s|%s", fexpr->e.symbol->name,
									   encode_params (&type)));
	dummy_p = bsearch (&dummy_p, funcs, func_count, sizeof (void *),
					   func_compare);
	if (dummy_p) {
		f = (overloaded_function_t *) *(void **) dummy_p;
		if (f->overloaded)
			fexpr->e.symbol->name = f->full_name;
		free (funcs);
		return fexpr;
	}
	for (i = 0; i < func_count; i++) {
		f = (overloaded_function_t *) funcs[i];
		parm_count = f->type->t.func.num_params;
		if ((parm_count >= 0 && parm_count != type.t.func.num_params)
			|| (parm_count < 0 && ~parm_count > type.t.func.num_params)) {
			funcs[i] = 0;
			continue;
		}
		if (parm_count < 0)
			parm_count = ~parm_count;
		for (j = 0; j < parm_count; j++) {
			if (!type_assignable (f->type->t.func.param_types[j],
								  type.t.func.param_types[j])) {
				funcs[i] = 0;
				break;
			}
		}
		if (j < parm_count)
			continue;
	}
	for (i = 0; i < func_count; i++) {
		f = (overloaded_function_t *) funcs[i];
		if (f) {
			if (!best) {
				best = f;
			} else {
				if (!reported) {
					reported = 1;
					error (fexpr, "unable to disambiguate %s",
						   dummy.full_name);
					error (fexpr, "possible match: %s", best->full_name);
				}
				error (fexpr, "possible match: %s", f->full_name);
			}
		}
	}
	if (reported)
		return fexpr;
	if (best) {
		if (best->overloaded)
			fexpr->e.symbol->name = best->full_name;
		free (funcs);
		return fexpr;
	}
	error (fexpr, "unable to find function matching %s", dummy.full_name);
	free (funcs);
	return fexpr;
}

static void
check_function (def_t *func, param_t *params)
{
	param_t    *p;
	int         i;

	if (!type_size (func->type->t.func.type)) {
		error (0, "return type is an incomplete type");
		func->type->t.func.type = &type_void;//FIXME
	}
	if (type_size (func->type->t.func.type) > type_size (&type_param)) {
		error (0, "return value too large to be passed by value");
		func->type->t.func.type = &type_void;//FIXME
	}
	for (p = params, i = 0; p; p = p->next, i++) {
		if (!p->selector && !p->type && !p->name)
			continue;					// ellipsis marker
		if (!p->type)
			continue;					// non-param selector
		if (!type_size (p->type))
			error (0, "parameter %d (‘%s’) has incomplete type",
				   i + 1, p->name);
		if (type_size (p->type) > type_size (&type_param))
			error (0, "param %d (‘%s’) is too large to be passed by value",
				   i + 1, p->name);
	}
}

void
build_scope (function_t *f, def_t *func, param_t *params)
{
	int         i;
	def_t      *def;
	param_t    *p;
	def_t      *args = 0;
	int         parm_ofs[MAX_PARMS];

	check_function (func, params);

	f->scope = new_scope (sc_params, new_defspace (), pr.scope);

	if (func->type->t.func.num_params < 0) {
		args = get_def (&type_va_list, ".args", f->scope, st_local);
		args->used = 1;
		def_initialized (args);
	}

	for (p = params, i = 0; p; p = p->next) {
		if (!p->selector && !p->type && !p->name)
			continue;					// ellipsis marker
		if (!p->type)
			continue;					// non-param selector
		def = get_def (p->type, p->name, f->scope, st_local);
		parm_ofs[i] = def->ofs;
		if (i > 0 && parm_ofs[i] < parm_ofs[i - 1]) {
			error (0, "bad parm order");
			abort ();
		}
		//printf ("%s%s %d\n", p == params ? "" : "    ", p->name, def->ofs);
		def->used = 1;				// don't warn for unused params
		def_initialized (def);	// params are assumed to be initialized
		i++;
	}

	if (args) {
		while (i < MAX_PARMS) {
			def = get_def (&type_vector, 0, f->scope, st_local);//XXX param
			def->used = 1;
			i++;
		}
	}
}

function_t *
new_function (def_t *func, const char *nice_name)
{
	function_t	*f;

	ALLOC (1024, function_t, functions, f);
	f->s_name = ReuseString (func->name);
	f->s_file = pr.source_file;
	f->def = func;
	if (!(f->name = nice_name))
		f->name = f->def->name;
	return f;
}

void
add_function (function_t *f)
{
	*pr.func_tail = f;
	pr.func_tail = &f->next;
	f->function_num = pr.num_functions++;
	if (options.code.debug)
		f->aux = new_auxfunction ();
}

function_t *
begin_function (def_t *def, const char *nicename, param_t *params)
{
	function_t *func;

	if (def->constant)
		error (0, "%s redefined", def->name);
	func = new_function (def, nicename);
	if (!def->external) {
		add_function (func);
		reloc_def_func (func, def->ofs);
	}
	func->code = pr.code->size;
	if (options.code.debug && func->aux) {
		pr_lineno_t *lineno = new_lineno ();
		func->aux->source_line = def->line;
		func->aux->line_info = lineno - pr.linenos;
		func->aux->local_defs = pr.num_locals;
		func->aux->return_type = def->type->t.func.type->type;

		lineno->fa.func = func->aux - pr.auxfunctions;
	}
	build_scope (func, def, params);
	return func;
}

function_t *
build_code_function (function_t *f, expr_t *state_expr, expr_t *statements)
{
	build_function (f);
	if (state_expr) {
		state_expr->next = statements;
		emit_function (f, state_expr);
	} else {
		emit_function (f, statements);
	}
	finish_function (f);
	return f;
}

function_t *
build_builtin_function (def_t *def, expr_t *bi_val, param_t *params)
{
	function_t *f;

	if (def->type->type != ev_func) {
		error (bi_val, "%s is not a function", def->name);
		return 0;
	}
	if (def->constant) {
		error (bi_val, "%s redefined", def->name);
		return 0;
	}
	if (bi_val->type != ex_integer && bi_val->type != ex_float) {
		error (bi_val, "invalid constant for = #");
		return 0;
	}
	if (def->external)
		return 0;

	f = new_function (def, 0);
	add_function (f);

	f->builtin = bi_val->type == ex_integer ? bi_val->e.integer_val
											: (int)bi_val->e.float_val;
	reloc_def_func (f, def->ofs);
	build_function (f);
	finish_function (f);

	// for debug info
	build_scope (f, f->def, params);
	flush_scope (f->scope, 1);
	return f;
}

void
build_function (function_t *f)
{
	f->def->constant = 1;
	f->def->nosave = 1;
	f->def->initialized = 1;
	G_FUNCTION (f->def->ofs) = f->function_num;
}

void
finish_function (function_t *f)
{
	if (f->aux) {
		def_t *def;
		f->aux->function = f->function_num;
		if (f->scope) {
			for (def = f->scope->head; def; def = def->def_next) {
				if (def->name) {
					def_to_ddef (def, new_local (), 0);
					f->aux->num_locals++;
				}
			}
		}
	}
}

void
emit_function (function_t *f, expr_t *e)
{
	int         last_is_label = 0;
	dstatement_t *s;
//#define DUMP_EXPR
#ifdef DUMP_EXPR
	printf (" %s =\n", f->def->name);
#endif

	if (f->aux)
		lineno_base = f->aux->source_line;

	while (f->var_init) {
		emit_expr (f->var_init);
		f->var_init = f->var_init->next;
	}

	//FIXME current_scope = f->scope;
	while (e) {
#ifdef DUMP_EXPR
		printf ("%d ", pr.source_line);
		print_expr (e);
		puts("");
#endif
		last_is_label = (e->type == ex_label);
		emit_expr (e);
		e = e->next;
	}
	s = &pr.code->code[pr.code->size - 1];
	if (last_is_label
		|| !(s->op == op_return->opcode
			 || (op_return_v && s->op == op_return_v->opcode))) {
		if (!options.traditional && op_return_v)
			emit_statement (0, op_return_v, 0, 0, 0);
		else
			emit_statement (0, op_done, 0, 0, 0);
	}
	//FIXME flush_scope (current_scope, 0);
	//FIXME current_scope = pr.scope;
	reset_tempdefs ();

#ifdef DUMP_EXPR
	puts ("");
#endif
}

int
function_parms (function_t *f, byte *parm_size)
{
	int         count, i;

	if (f->def->type->t.func.num_params >= 0)
		count = f->def->type->t.func.num_params;
	else
		count = -f->def->type->t.func.num_params - 1;

	for (i = 0; i < count; i++)
		parm_size[i] = type_size (f->def->type->t.func.param_types[i]);
	return f->def->type->t.func.num_params;
}
