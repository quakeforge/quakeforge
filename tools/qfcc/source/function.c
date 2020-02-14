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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/va.h"

#include "qfcc.h"

#include "codespace.h"
#include "debug.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
#include "expr.h"
#include "flow.h"
#include "function.h"
#include "opcodes.h"
#include "options.h"
#include "reloc.h"
#include "shared.h"
#include "statements.h"
#include "strpool.h"
#include "symtab.h"
#include "type.h"
#include "value.h"

static param_t *params_freelist;
static function_t *functions_freelist;
static hashtab_t *overloaded_functions;
static hashtab_t *function_map;

static const char *
ol_func_get_key (const void *_f, void *unused)
{
	overloaded_function_t *f = (overloaded_function_t *) _f;
	return f->full_name;
}

static const char *
func_map_get_key (const void *_f, void *unused)
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
	type_t     *new;

	new = new_type ();
	new->type = ev_func;
	new->t.func.type = type;
	new->t.func.num_params = 0;

	for (p = parms; p; p = p->next) {
		if (new->t.func.num_params > MAX_PARMS) {
			error (0, "too many params");
			return type;
		}
		if (!p->selector && !p->type && !p->name) {
			if (p->next)
				internal_error (0, 0);
			new->t.func.num_params = -(new->t.func.num_params + 1);
		} else if (p->type) {
			new->t.func.param_types[new->t.func.num_params] = p->type;
			new->t.func.num_params++;
		}
	}
	return new;
}

param_t *
check_params (param_t *params)
{
	int         num = 1;
	param_t    *p = params;
	if (!params)
		return 0;
	while (p) {
		if (p->type == &type_void) {
			if (p->name) {
				error (0, "parameter %d ('%s') has incomplete type", num,
					   p->name);
				p->type = type_default;
			} else if (num > 1 || p->next) {
				error (0, "'void' must be the only parameter");
				p->name = "void";
			} else {
				// this is a void function
				return 0;
			}
		}
		p = p->next;
	}
	return params;
}

static overloaded_function_t *
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

symbol_t *
function_symbol (symbol_t *sym, int overload, int create)
{
	const char *name = sym->name;
	overloaded_function_t *func;
	symbol_t   *s;

	func = get_function (name, sym->type, overload, create);

	if (func && func->overloaded)
		name = func->full_name;
	s = symtab_lookup (current_symtab, name);
	if ((!s || s->table != current_symtab) && create) {
		s = new_symbol (name);
		s->sy_type = sy_func;
		s->type = sym->type;
		s->params = sym->params;
		s->s.func = 0;				// function not yet defined
		symtab_addsymbol (current_symtab, s);
	}
	return s;
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
	type.type = ev_func;

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
	if (func_count < 1) {
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
		if (f->overloaded) {
			fexpr->e.symbol = symtab_lookup (current_symtab, f->full_name);
			if (!fexpr->e.symbol)
				internal_error (fexpr, "overloaded function %s not found",
								best->full_name);
		}
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
		if (best->overloaded) {
			fexpr->e.symbol = symtab_lookup (current_symtab,
											 best->full_name);
			if (!fexpr->e.symbol)
				internal_error (fexpr, "overloaded function %s not found",
								best->full_name);
		}
		free (funcs);
		return fexpr;
	}
	error (fexpr, "unable to find function matching %s", dummy.full_name);
	free (funcs);
	return fexpr;
}

static void
check_function (symbol_t *fsym)
{
	param_t    *params = fsym->params;
	param_t    *p;
	int         i;

	if (!type_size (fsym->type->t.func.type)) {
		error (0, "return type is an incomplete type");
		fsym->type->t.func.type = &type_void;//FIXME better type?
	}
	if (type_size (fsym->type->t.func.type) > type_size (&type_param)) {
		error (0, "return value too large to be passed by value (%d)",
			   type_size (&type_param));
		fsym->type->t.func.type = &type_void;//FIXME better type?
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

static void
build_scope (symbol_t *fsym, symtab_t *parent)
{
	int         i;
	param_t    *p;
	symbol_t   *args = 0;
	symbol_t   *param;
	symtab_t   *symtab;
	symtab_t   *cs = current_symtab;

	check_function (fsym);

	symtab = new_symtab (parent, stab_local);
	fsym->s.func->symtab = symtab;
	symtab->space = defspace_new (ds_virtual);
	current_symtab = symtab;

	if (fsym->type->t.func.num_params < 0) {
		args = new_symbol_type (".args", &type_va_list);
		initialize_def (args, 0, symtab->space, sc_param);
	}

	for (p = fsym->params, i = 0; p; p = p->next) {
		if (!p->selector && !p->type && !p->name)
			continue;					// ellipsis marker
		if (!p->type)
			continue;					// non-param selector
		if (!p->name) {
			error (0, "parameter name omitted");
			p->name = save_string ("");
		}
		param = new_symbol_type (p->name, p->type);
		initialize_def (param, 0, symtab->space, sc_param);
		i++;
	}

	if (args) {
		while (i < MAX_PARMS) {
			param = new_symbol_type (va (".par%d", i), &type_param);
			initialize_def (param, 0, symtab->space, sc_param);
			i++;
		}
	}
	current_symtab = cs;
}

function_t *
new_function (const char *name, const char *nice_name)
{
	function_t	*f;

	ALLOC (1024, function_t, functions, f);
	f->s_name = ReuseString (name);
	f->s_file = pr.source_file;
	if (!(f->name = nice_name))
		f->name = name;
	return f;
}

void
make_function (symbol_t *sym, const char *nice_name, defspace_t *space,
			   storage_class_t storage)
{
	reloc_t    *relocs = 0;
	if (sym->sy_type != sy_func)
		internal_error (0, "%s is not a function", sym->name);
	if (storage == sc_extern && sym->s.func)
		return;
	if (!sym->s.func) {
		sym->s.func = new_function (sym->name, nice_name);
		sym->s.func->sym = sym;
	}
	if (sym->s.func->def && sym->s.func->def->external
		&& storage != sc_extern) {
		//FIXME this really is not the right way
		relocs = sym->s.func->def->relocs;
		free_def (sym->s.func->def);
		sym->s.func->def = 0;
	}
	if (!sym->s.func->def) {
		sym->s.func->def = new_def (sym->name, sym->type, space, storage);
		reloc_attach_relocs (relocs, &sym->s.func->def->relocs);
	}
}

void
add_function (function_t *f)
{
	*pr.func_tail = f;
	pr.func_tail = &f->next;
	f->function_num = pr.num_functions++;
}

function_t *
begin_function (symbol_t *sym, const char *nicename, symtab_t *parent,
				int far)
{
	defspace_t *space;

	if (sym->sy_type != sy_func) {
		error (0, "%s is not a function", sym->name);
		sym = new_symbol_type (sym->name, &type_function);
		sym = function_symbol (sym, 1, 1);
	}
	if (sym->s.func && sym->s.func->def && sym->s.func->def->initialized) {
		error (0, "%s redefined", sym->name);
		sym = new_symbol_type (sym->name, sym->type);
		sym = function_symbol (sym, 1, 1);
	}
	space = sym->table->space;
	if (far)
		space = pr.far_data;
	make_function (sym, nicename, space, current_storage);
	if (!sym->s.func->def->external) {
		sym->s.func->def->initialized = 1;
		sym->s.func->def->constant = 1;
		sym->s.func->def->nosave = 1;
		add_function (sym->s.func);
		reloc_def_func (sym->s.func, sym->s.func->def);
	}
	sym->s.func->code = pr.code->size;

	sym->s.func->s_file = pr.source_file;
	if (options.code.debug) {
		pr_lineno_t *lineno = new_lineno ();
		sym->s.func->line_info = lineno - pr.linenos;
	}

	build_scope (sym, parent);
	return sym->s.func;
}

function_t *
build_code_function (symbol_t *fsym, expr_t *state_expr, expr_t *statements)
{
	if (fsym->sy_type != sy_func)	// probably in error recovery
		return 0;
	build_function (fsym->s.func);
	if (state_expr) {
		state_expr->next = statements;
		statements = state_expr;
	}
	emit_function (fsym->s.func, statements);
	finish_function (fsym->s.func);
	return fsym->s.func;
}

function_t *
build_builtin_function (symbol_t *sym, expr_t *bi_val, int far)
{
	int         bi;
	defspace_t *space;

	if (sym->sy_type != sy_func) {
		error (bi_val, "%s is not a function", sym->name);
		return 0;
	}
	if (sym->s.func && sym->s.func->def && sym->s.func->def->initialized) {
		error (bi_val, "%s redefined", sym->name);
		return 0;
	}
	if (!is_integer_val (bi_val) && !is_float_val (bi_val)) {
		error (bi_val, "invalid constant for = #");
		return 0;
	}
	space = sym->table->space;
	if (far)
		space = pr.far_data;
	make_function (sym, 0, space, current_storage);
	if (sym->s.func->def->external)
		return 0;

	add_function (sym->s.func);

	if (is_integer_val (bi_val))
		bi = expr_integer (bi_val);
	else
		bi = expr_float (bi_val);
	sym->s.func->builtin = bi;
	reloc_def_func (sym->s.func, sym->s.func->def);
	build_function (sym->s.func);
	finish_function (sym->s.func);

	// for debug info
	build_scope (sym, current_symtab);
	sym->s.func->symtab->space->size = 0;
	return sym->s.func;
}

void
build_function (function_t *f)
{
	//	FIXME
//	f->def->constant = 1;
//	f->def->nosave = 1;
//	f->def->initialized = 1;
//	G_FUNCTION (f->def->ofs) = f->function_num;
}

void
finish_function (function_t *f)
{
}

void
emit_function (function_t *f, expr_t *e)
{
	if (pr.error_count)
		return;
	f->code = pr.code->size;
	lineno_base = f->def->line;
	f->sblock = make_statements (e);
	if (options.code.optimize) {
		flow_data_flow (f);
	} else {
		statements_count_temps (f->sblock);
	}
	emit_statements (f->sblock);
}

int
function_parms (function_t *f, byte *parm_size)
{
	int         count, i;
	ty_func_t  *func = &f->sym->type->t.func;

	if (func->num_params >= 0)
		count = func->num_params;
	else
		count = -func->num_params - 1;

	for (i = 0; i < count; i++)
		parm_size[i] = type_size (func->param_types[i]);
	return func->num_params;
}

void
clear_functions (void)
{
	if (overloaded_functions)
		Hash_FlushTable (overloaded_functions);
	if (function_map)
		Hash_FlushTable (function_map);
}
