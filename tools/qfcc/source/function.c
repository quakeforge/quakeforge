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

#include "tools/qfcc/include/qfcc.h"

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/flow.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/opcodes.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

ALLOC_STATE (param_t, params);
ALLOC_STATE (function_t, functions);
static hashtab_t *overloaded_functions;
static hashtab_t *function_map;

// standardized base register to use for all locals (arguments, local defs,
// params)
#define LOCALS_REG 1
// keep the stack aligned to 8 words (32 bytes) so lvec etc can be used without
// having to do shenanigans with mixed-alignment stack frames
#define STACK_ALIGN 8

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
new_param (const char *selector, const type_t *type, const char *name)
{
	param_t    *param;

	ALLOC (4096, param_t, params, param);
	*param = (param_t) {
		.selector = selector,
		.type = find_type (type),
		.name = name,
	};
	return param;
}

param_t *
new_generic_param (const expr_t *type_expr, const char *name)
{
	param_t    *param;

	ALLOC (4096, param_t, params, param);
	*param = (param_t) {
		.type_expr = type_expr,
		.name = name,
	};
	return param;
}

param_t *
param_append_identifiers (param_t *params, symbol_t *idents, const type_t *type)
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
		p = &(*p)->next;
		idents = idents->next;
	}
	return params;
}

static param_t *
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
append_params (param_t *params, param_t *more_params)
{
	if (params) {
		param_t *p;
		for (p = params; p->next; ) {
			p = p->next;
		}
		p->next = more_params;
		return params;
	}
	return more_params;
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

const type_t *
parse_params (const type_t *return_type, param_t *parms)
{
	param_t    *p;
	type_t     *new;
	int         count = 0;

	if (return_type && is_class (return_type)) {
		error (0, "cannot return an object (forgot *?)");
		return_type = &type_id;
	}

	new = new_type ();
	new->type = ev_func;
	new->alignment = 1;
	new->width = 1;
	new->columns = 1;
	new->t.func.type = return_type;
	new->t.func.num_params = 0;

	for (p = parms; p; p = p->next) {
		if (p->type) {
			count++;
		}
	}
	if (count) {
		new->t.func.param_types = malloc (count * sizeof (type_t));
	}
	for (p = parms; p; p = p->next) {
		if (!p->selector && !p->type && !p->name) {
			if (p->next)
				internal_error (0, 0);
			new->t.func.num_params = -(new->t.func.num_params + 1);
		} else if (p->type) {
			if (is_class (p->type)) {
				error (0, "cannot use an object as a parameter (forgot *?)");
				p->type = &type_id;
			}
			auto ptype = unalias_type (p->type);
			new->t.func.param_types[new->t.func.num_params] = ptype;
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
		if (p->type && is_void(p->type)) {
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
get_function (const char *name, const type_t *type, int overload)
{
	const char *full_name;
	overloaded_function_t *func;

	name = save_string (name);

	full_name = save_string (va (0, "%s|%s", name, encode_params (type)));

	func = Hash_Find (overloaded_functions, full_name);
	if (func) {
		if (func->type != type) {
			error (0, "can't overload on return types");
			return func;
		}
		return func;
	}

	func = Hash_Find (function_map, name);
	if (func) {
		if (!overload && !func->overloaded) {
			expr_t      e = {
				.loc = func->loc,
			};
			warning (0, "creating overloaded function %s without @overload",
					 full_name);
			warning (&e, "(previous function is %s)", func->full_name);
		}
		overload = 1;
	}

	func = calloc (1, sizeof (overloaded_function_t));
	func->name = name;
	func->full_name = full_name;
	func->type = type;
	func->overloaded = overload;
	func->loc = pr.loc;

	Hash_Add (overloaded_functions, func);
	Hash_Add (function_map, func);
	return func;
}

symbol_t *
function_symbol (symbol_t *sym, int overload)
{
	const char *name = sym->name;
	overloaded_function_t *func;
	symbol_t   *s;

	func = get_function (name, unalias_type (sym->type), overload);

	if (func && func->overloaded)
		name = func->full_name;
	s = symtab_lookup (current_symtab, name);
	if (!s || s->table != current_symtab) {
		s = new_symbol (name);
		s->sy_type = sy_func;
		s->type = unalias_type (sym->type);
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
	const type_t *ta = fa->type;
	const type_t *tb = fb->type;
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

static const expr_t *
set_func_symbol (const expr_t *fexpr, overloaded_function_t *f)
{
	auto sym = symtab_lookup (current_symtab, f->full_name);
	if (!sym) {
		internal_error (fexpr, "overloaded function %s not found",
						f->full_name);
	}
	auto nf = new_expr ();
	*nf = *fexpr;
	nf->symbol = sym;
	return nf;
}

const expr_t *
find_function (const expr_t *fexpr, const expr_t *params)
{
	int         func_count, parm_count, reported = 0;
	overloaded_function_t dummy, *best = 0;
	type_t      type = {};
	void      **funcs, *dummy_p = &dummy;

	if (fexpr->type != ex_symbol)
		return fexpr;

	type.type = ev_func;
	type.t.func.num_params = params ? list_count (&params->list) : 0;
	const expr_t *args[type.t.func.num_params + 1];
	if (params) {
		list_scatter_rev (&params->list, args);
	}

	for (int i = 0; i < type.t.func.num_params; i++) {
		auto e = args[i];
		if (e->type == ex_error) {
			return e;
		}
	}
	const type_t *arg_types[type.t.func.num_params + 1];
	type.t.func.param_types = arg_types;
	for (int i = 0; i < type.t.func.num_params; i++) {
		auto e = args[i];
		type.t.func.param_types[i] = get_type (e);
	}
	funcs = Hash_FindList (function_map, fexpr->symbol->name);
	if (!funcs)
		return fexpr;
	for (func_count = 0; funcs[func_count]; func_count++)
		;
	if (func_count < 2) {
		auto f = (overloaded_function_t *) funcs[0];
		if (func_count && !f->overloaded) {
			free (funcs);
			return fexpr;
		}
	}
	type.t.func.type = ((overloaded_function_t *) funcs[0])->type->t.func.type;
	dummy.type = find_type (&type);

	qsort (funcs, func_count, sizeof (void *), func_compare);
	dummy.full_name = save_string (va (0, "%s|%s", fexpr->symbol->name,
									   encode_params (&type)));
	dummy_p = bsearch (&dummy_p, funcs, func_count, sizeof (void *),
					   func_compare);
	if (dummy_p) {
		auto f = (overloaded_function_t *) *(void **) dummy_p;
		if (f->overloaded) {
			fexpr = set_func_symbol (fexpr, f);
		}
		free (funcs);
		return fexpr;
	}
	for (int i = 0; i < func_count; i++) {
		auto f = (overloaded_function_t *) funcs[i];
		parm_count = f->type->t.func.num_params;
		if ((parm_count >= 0 && parm_count != type.t.func.num_params)
			|| (parm_count < 0 && ~parm_count > type.t.func.num_params)) {
			funcs[i] = 0;
			continue;
		}
		if (parm_count < 0)
			parm_count = ~parm_count;
		int j;
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
	for (int i = 0; i < func_count; i++) {
		auto f = (overloaded_function_t *) funcs[i];
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
			fexpr = set_func_symbol (fexpr, best);
		}
		free (funcs);
		return fexpr;
	}
	error (fexpr, "unable to find function matching %s", dummy.full_name);
	free (funcs);
	return fexpr;
}

int
value_too_large (const type_t *val_type)
{
	if ((options.code.progsversion < PROG_VERSION
		 && type_size (val_type) > type_size (&type_param))
		|| (options.code.progsversion == PROG_VERSION
			&& type_size (val_type) > MAX_DEF_SIZE)) {
		return 1;
	}
	return 0;
}

static void
check_function (symbol_t *fsym)
{
	param_t    *params = fsym->params;
	param_t    *p;
	int         i;

	if (!type_size (fsym->type->t.func.type)) {
		error (0, "return type is an incomplete type");
		//fsym->type->t.func.type = &type_void;//FIXME better type?
	}
	if (value_too_large (fsym->type->t.func.type)) {
		error (0, "return value too large to be passed by value (%d)",
			   type_size (&type_param));
		//fsym->type->t.func.type = &type_void;//FIXME better type?
	}
	for (p = params, i = 0; p; p = p->next, i++) {
		if (!p->selector && !p->type && !p->name)
			continue;					// ellipsis marker
		if (!p->type)
			continue;					// non-param selector
		if (!type_size (p->type)) {
			error (0, "parameter %d (‘%s’) has incomplete type",
				   i + 1, p->name);
		}
		if (value_too_large (p->type)) {
			error (0, "param %d (‘%s’) is too large to be passed by value",
				   i + 1, p->name);
		}
	}
}

static void
build_v6p_scope (symbol_t *fsym)
{
	int         i;
	param_t    *p;
	symbol_t   *args = 0;
	symbol_t   *param;
	symtab_t   *parameters = fsym->s.func->parameters;
	symtab_t   *locals = fsym->s.func->locals;

	if (fsym->s.func->type->t.func.num_params < 0) {
		args = new_symbol_type (".args", &type_va_list);
		initialize_def (args, 0, parameters->space, sc_param, locals);
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
		initialize_def (param, 0, parameters->space, sc_param, locals);
		i++;
	}

	if (args) {
		while (i < PR_MAX_PARAMS) {
			param = new_symbol_type (va (0, ".par%d", i), &type_param);
			initialize_def (param, 0, parameters->space, sc_param, locals);
			i++;
		}
	}
}

static void
create_param (symtab_t *parameters, symbol_t *param)
{
	defspace_t *space = parameters->space;
	def_t      *def = new_def (param->name, 0, space, sc_param);
	int         size = type_size (param->type);
	int         alignment = param->type->alignment;
	if (alignment < 4) {
		alignment = 4;
	}
	def->offset = defspace_alloc_aligned_highwater (space, size, alignment);
	def->type = param->type;
	param->s.def = def;
	param->sy_type = sy_var;
	symtab_addsymbol (parameters, param);
	if (is_vector(param->type) && options.code.vector_components)
		init_vector_components (param, 0, parameters);
}

static void
build_rua_scope (symbol_t *fsym)
{
	for (param_t *p = fsym->params; p; p = p->next) {
		symbol_t   *param;
		if (!p->selector && !p->type && !p->name) {
			// ellipsis marker
			param = new_symbol_type (".args", &type_va_list);
		} else {
			if (!p->type) {
				continue;					// non-param selector
			}
			if (!p->name) {
				error (0, "parameter name omitted");
				p->name = save_string ("");
			}
			param = new_symbol_type (p->name, p->type);
		}
		create_param (fsym->s.func->parameters, param);
		param->s.def->reg = fsym->s.func->temp_reg;;
	}
}

static void
build_scope (symbol_t *fsym, symtab_t *parent)
{
	symtab_t   *parameters;
	symtab_t   *locals;

	if (!fsym->s.func) {
		internal_error (0, "function %s not defined", fsym->name);
	}
	if (!is_func (fsym->s.func->type)) {
		internal_error (0, "function type %s not a funciton", fsym->name);
	}

	check_function (fsym);

	fsym->s.func->label_scope = new_symtab (0, stab_label);

	parameters = new_symtab (parent, stab_param);
	parameters->space = defspace_new (ds_virtual);
	fsym->s.func->parameters = parameters;

	locals = new_symtab (parameters, stab_local);
	locals->space = defspace_new (ds_virtual);
	fsym->s.func->locals = locals;

	if (options.code.progsversion == PROG_VERSION) {
		build_rua_scope (fsym);
	} else {
		build_v6p_scope (fsym);
	}
}

function_t *
new_function (const char *name, const char *nice_name)
{
	function_t	*f;

	ALLOC (1024, function_t, functions, f);
	f->s_name = ReuseString (name);
	f->s_file = pr.loc.file;
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
		sym->s.func->type = unalias_type (sym->type);
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
				int far, storage_class_t storage)
{
	defspace_t *space;

	if (sym->sy_type != sy_func) {
		error (0, "%s is not a function", sym->name);
		sym = new_symbol_type (sym->name, &type_func);
		sym = function_symbol (sym, 1);
	}
	if (sym->s.func && sym->s.func->def && sym->s.func->def->initialized) {
		error (0, "%s redefined", sym->name);
		sym = new_symbol_type (sym->name, sym->type);
		sym = function_symbol (sym, 1);
	}
	space = sym->table->space;
	if (far)
		space = pr.far_data;
	make_function (sym, nicename, space, storage);
	if (!sym->s.func->def->external) {
		sym->s.func->def->initialized = 1;
		sym->s.func->def->constant = 1;
		sym->s.func->def->nosave = 1;
		add_function (sym->s.func);
		reloc_def_func (sym->s.func, sym->s.func->def);

		sym->s.func->def->loc = pr.loc;
	}
	sym->s.func->code = pr.code->size;

	sym->s.func->s_file = pr.loc.file;
	if (options.code.debug) {
		pr_lineno_t *lineno = new_lineno ();
		sym->s.func->line_info = lineno - pr.linenos;
	}

	build_scope (sym, parent);
	return sym->s.func;
}

static void
build_function (symbol_t *fsym)
{
	const type_t *func_type = fsym->s.func->type;
	if (func_type->t.func.num_params > PR_MAX_PARAMS) {
		error (0, "too many params");
	}
}

static void
merge_spaces (defspace_t *dst, defspace_t *src, int alignment)
{
	int         offset;

	for (def_t *def = src->defs; def; def = def->next) {
		if (def->type->alignment > alignment) {
			alignment = def->type->alignment;
		}
	}
	offset = defspace_alloc_aligned_highwater (dst, src->size, alignment);
	for (def_t *def = src->defs; def; def = def->next) {
		def->offset += offset;
		def->space = dst;
	}

	if (src->defs) {
		*dst->def_tail = src->defs;
		dst->def_tail = src->def_tail;
		src->def_tail = &src->defs;
		*src->def_tail = 0;
	}

	defspace_delete (src);
}

function_t *
build_code_function (symbol_t *fsym, const expr_t *state_expr,
					 expr_t *statements)
{
	if (fsym->sy_type != sy_func)	// probably in error recovery
		return 0;
	build_function (fsym);
	if (state_expr) {
		prepend_expr (statements, state_expr);
	}
	function_t *func = fsym->s.func;
	if (options.code.progsversion == PROG_VERSION) {
		/* Create a function entry block to set up the stack frame and add the
		 * actual function code to that block. This ensure that the adjstk and
		 * with statements always come first, regardless of what ideas the
		 * optimizer gets.
		 */
		expr_t     *e;
		expr_t     *entry = new_block_expr (0);
		entry->loc = func->def->loc;

		e = new_adjstk_expr (0, 0);
		e->loc = entry->loc;
		append_expr (entry, e);

		e = new_with_expr (2, LOCALS_REG, new_short_expr (0));
		e->loc = entry->loc;
		append_expr (entry, e);

		append_expr (entry, statements);
		statements = entry;

		/* Mark all local defs as using the base register used for stack
		 * references.
		 */
		func->temp_reg = LOCALS_REG;
		for (def_t *def = func->locals->space->defs; def; def = def->next) {
			if (def->local || def->param) {
				def->reg = LOCALS_REG;
			}
		}
		for (def_t *def = func->parameters->space->defs; def; def = def->next) {
			if (def->local || def->param) {
				def->reg = LOCALS_REG;
			}
		}
	}
	emit_function (func, statements);
	defspace_sort_defs (func->parameters->space);
	defspace_sort_defs (func->locals->space);
	if (options.code.progsversion < PROG_VERSION) {
		// stitch parameter and locals data together with parameters coming
		// first
		defspace_t *space = defspace_new (ds_virtual);

		func->params_start = 0;

		merge_spaces (space, func->parameters->space, 1);
		func->parameters->space = space;

		merge_spaces (space, func->locals->space, 1);
		func->locals->space = space;
	} else {
		defspace_t *space = defspace_new (ds_virtual);

		if (func->arguments) {
			func->arguments->size = func->arguments->max_size;
			merge_spaces (space, func->arguments, STACK_ALIGN);
			func->arguments = 0;
		}

		merge_spaces (space, func->locals->space, STACK_ALIGN);
		func->locals->space = space;

		// allocate 0 words to force alignment and get the address
		func->params_start = defspace_alloc_aligned_highwater (space, 0,
															   STACK_ALIGN);

		dstatement_t *st = &pr.code->code[func->code];
		if (pr.code->size > func->code && st->op == OP_ADJSTK) {
			if (func->params_start) {
				st->b = -func->params_start;
			} else {
				// skip over adjstk so a zero adjustment doesn't get executed
				func->code += 1;
			}
		}
		merge_spaces (space, func->parameters->space, STACK_ALIGN);
		func->parameters->space = space;

		// force the alignment again so the full stack slot is counted when
		// the final parameter is smaller than STACK_ALIGN words
		defspace_alloc_aligned_highwater (space, 0, STACK_ALIGN);
	}
	return fsym->s.func;
}

function_t *
build_builtin_function (symbol_t *sym, const expr_t *bi_val, int far,
						storage_class_t storage)
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
	if (!is_int_val (bi_val) && !is_float_val (bi_val)) {
		error (bi_val, "invalid constant for = #");
		return 0;
	}
	space = sym->table->space;
	if (far)
		space = pr.far_data;
	make_function (sym, 0, space, storage);
	if (sym->s.func->def->external)
		return 0;

	sym->s.func->def->initialized = 1;
	sym->s.func->def->constant = 1;
	sym->s.func->def->nosave = 1;
	add_function (sym->s.func);

	if (is_int_val (bi_val))
		bi = expr_int (bi_val);
	else
		bi = expr_float (bi_val);
	if (bi < 0) {
		error (bi_val, "builtin functions must be positive or 0");
		return 0;
	}
	sym->s.func->builtin = bi;
	reloc_def_func (sym->s.func, sym->s.func->def);
	build_function (sym);

	// for debug info
	build_scope (sym, current_symtab);
	sym->s.func->parameters->space->size = 0;
	sym->s.func->locals->space = sym->s.func->parameters->space;
	return sym->s.func;
}

void
emit_function (function_t *f, expr_t *e)
{
	if (pr.error_count)
		return;
	f->code = pr.code->size;
	lineno_base = f->def->loc.line;
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
	auto func = &f->sym->type->t.func;

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
	if (overloaded_functions) {
		Hash_FlushTable (overloaded_functions);
		Hash_FlushTable (function_map);
	} else {
		overloaded_functions = Hash_NewTable (1021, ol_func_get_key, 0, 0, 0);
		function_map = Hash_NewTable (1021, func_map_get_key, 0, 0, 0);
	}
}
