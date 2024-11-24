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

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/evaluate_type.h"
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
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

ALLOC_STATE (param_t, params);
ALLOC_STATE (function_t, functions);
ALLOC_STATE (metafunc_t, metafuncs);
ALLOC_STATE (genfunc_t, genfuncs);
static hashtab_t *generic_functions;
static hashtab_t *metafuncs;
static hashtab_t *function_map;

static const char *
gen_func_get_key (const void *_f, void *unused)
{
	auto f = (genfunc_t *) _f;
	return f->name;
}

static const char *
metafunc_get_full_name (const void *_f, void *unused)
{
	metafunc_t *f = (metafunc_t *) _f;
	return f->full_name;
}

static const char *
metafunc_get_name (const void *_f, void *unused)
{
	metafunc_t *f = (metafunc_t *) _f;
	return f->name;
}

static void
check_generic_param (genparam_t *param, genfunc_t *genfunc)
{
	if (param->fixed_type) {
		if (param->gentype != -1) {
			internal_error (0, "invalid type index %d on %s for %s",
							param->gentype, param->name, genfunc->name);
		}
		if (param->compute) {
			internal_error (0, "fixed and computed types on %s for %s",
							param->name, genfunc->name);
		}
	} else if (param->compute) {
		if (param->gentype != -1) {
			internal_error (0, "invalid type index %d on %s for %s",
							param->gentype, param->name, genfunc->name);
		}
	} else if (param->gentype < 0 || param->gentype >= genfunc->num_types) {
		internal_error (0, "invalid type index %d on %s for %s",
						param->gentype, param->name, genfunc->name);
	}
}

static bool __attribute__((pure))
cmp_genparams (genfunc_t *g1, genparam_t *p1, genfunc_t *g2, genparam_t *p2)
{
	if (p1->fixed_type || p2->fixed_type) {
		return p1->fixed_type == p2->fixed_type;
	}
	// fixed_type for both p1 and p2 is null
	auto t1 = g1->types[p1->gentype];
	auto t2 = g2->types[p2->gentype];
	auto vt1 = t1.valid_types;
	auto vt2 = t2.valid_types;
	for (; *vt1 && *vt2 && *vt1 == *vt2; vt1++, vt2++) continue;
	return *vt1 == *vt2;
}

void
add_generic_function (genfunc_t *genfunc)
{
	auto name = genfunc->name;

	for (int i = 0; i < genfunc->num_types; i++) {
		auto gentype = &genfunc->types[i];
		if (!gentype->valid_types) {
			internal_error (0, "no valid_types set in generic type");
		}
		for (auto type = gentype->valid_types; type && *type; type++) {
			if (is_void (*type)) {
				internal_error (0, "void in list of valid types");
			}
		}
	}
	int gen_params = 0;
	for (int i = 0; i < genfunc->num_params; i++) {
		auto param = &genfunc->params[i];
		if (!param->fixed_type) {
			gen_params++;
			check_generic_param (param, genfunc);
		}
	}
	if (!gen_params) {
		internal_error (0, "%s has no generic parameters", name);
	}
	if (!genfunc->ret_type) {
		internal_error (0, "%s has no return type", name);
	}
	check_generic_param (genfunc->ret_type, genfunc);

	bool is_new = true;
	auto old_list = (genfunc_t **) Hash_FindList (generic_functions, name);
	for (auto o = old_list; is_new && o && *o; o++) {
		auto old = *o;
		if (old->num_params == genfunc->num_params) {
			is_new = false;
			for (int i = 0; i < genfunc->num_params; i++) {
				if (!cmp_genparams (genfunc, &genfunc->params[i],
									old, &old->params[i])) {
					is_new = true;
					break;
				}
			}
			if (!is_new && !cmp_genparams (genfunc, genfunc->ret_type,
										   old, old->ret_type)) {
				error (0, "can't overload on return types");
				return;
			}
		}
	}

	if (is_new) {
		Hash_Add (generic_functions, genfunc);
	} else {
		for (int i = 0; i < genfunc->num_types; i++) {
			auto gentype = &genfunc->types[i];
			free (gentype->valid_types);
		}
		free (genfunc->types);
		FREE (genfuncs, genfunc);
	}
}

static const type_t **
valid_type_list (const expr_t *expr)
{
	if (expr->type != ex_list) {
		return expand_type (expr);
	}
	int count = list_count (&expr->list);
	const expr_t *type_refs[count];
	list_scatter (&expr->list, type_refs);
	const type_t **types = malloc (sizeof (type_t *[count + 1]));
	types[count] = nullptr;
	bool err = false;
	for (int i = 0; i < count; i++) {
		if (!(types[i] = resolve_type (type_refs[i]))) {
			error (type_refs[i], "not a constant type ref");
			err = true;
		}
	}
	if (err) {
		free (types);
		return nullptr;
	}
	return types;
}

static gentype_t
make_gentype (const expr_t *expr)
{
	if (expr->type != ex_symbol || expr->symbol->sy_type != sy_type_param) {
		internal_error (expr, "expected generic type name");
	}
	auto sym = expr->symbol;
	gentype_t gentype = {
		.name = save_string (sym->name),
		.valid_types = valid_type_list (sym->expr),
	};
	if (!gentype.valid_types) {
		internal_error (expr, "empty generic type");
	}
	return gentype;
}

static int
find_gentype (const expr_t *expr, genfunc_t *genfunc)
{
	if (!expr || expr->type != ex_symbol) {
		return -1;
	}
	const char *name = expr->symbol->name;
	for (int i = 0; i < genfunc->num_types; i++) {
		auto t = &genfunc->types[i];
		if (strcmp (name, t->name) == 0) {
			return i;
		}
	}
	return -1;
}

static genparam_t
make_genparam (param_t *param, genfunc_t *genfunc)
{
	int gentype = find_gentype (param->type_expr, genfunc);
	typeeval_t *compute = nullptr;
	if (gentype < 0 && param->type_expr) {
		compute = build_type_function (param->type_expr,
									   genfunc->num_types, genfunc->types);
	}
	genparam_t genparam = {
		.name = save_string (param->name),
		.fixed_type = param->type,
		.compute = compute,
		.gentype = gentype,
		.qual = param->qual,
	};
	return genparam;
}

static genfunc_t *
parse_generic_function (const char *name, specifier_t spec)
{
	if (!spec.is_generic) {
		return nullptr;
	}
	// fake parameter for the return type
	param_t ret_param = {
		.next = spec.sym->params,
		.type = spec.type,
		.type_expr = spec.type_expr,
	};
	int num_params = 0;
	int num_gentype = 0;
	for (auto p = &ret_param; p; p = p->next) {
		num_params++;
	}
	auto generic_tab = spec.symtab;
	for (auto s = generic_tab->symbols; s; s = s->next) {
		bool found = false;
		for (auto q = &ret_param; q; q = q->next) {
			// skip complex expressions because they will be either fixed
			// or rely on earlier parameters
			if (!q->type_expr || q->type_expr->type != ex_symbol) {
				continue;
			}
			if (strcmp (q->type_expr->symbol->name, s->name) == 0) {
				num_gentype++;
				found = true;
				break;
			}
		}
		if (!spec.is_generic_block && !found) {
			warning (0, "generic parameter %s not used", s->name);
		}
	}
	if (!num_gentype) {
		if (!spec.is_generic_block) {
			warning (0, "no generic parameters for %s", name);
		}
		return nullptr;
	}

	genfunc_t *genfunc;
	ALLOC (4096, genfunc_t, genfuncs, genfunc);
	*genfunc = (genfunc_t) {
		.name = save_string (name),
		.types = malloc (sizeof (gentype_t[num_gentype])
						 + sizeof (genparam_t[num_params])),
		.num_types = num_gentype,
		.num_params = num_params - 1, // don't count return type
	};
	genfunc->params = (genparam_t *) &genfunc->types[num_gentype];
	genfunc->ret_type = &genfunc->params[num_params - 1];

	num_gentype = 0;
	for (auto s = generic_tab->symbols; s; s = s->next) {
		for (auto q = &ret_param; q; q = q->next) {
			// see complex expressions comment above
			if (!q->type_expr || q->type_expr->type != ex_symbol) {
				continue;
			}
			if (strcmp (q->type_expr->symbol->name, s->name) == 0) {
				genfunc->types[num_gentype++] = make_gentype (q->type_expr);
				break;
			}
		}
	}

	num_params = 0;
	// skip return type so it can be done last to support complex expressions
	for (auto p = ret_param.next; p; p = p->next) {
		genfunc->params[num_params++] = make_genparam (p, genfunc);
	}
	*genfunc->ret_type = make_genparam (&ret_param, genfunc);
	return genfunc;
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
		.qual = pq_in,
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
		.qual = pq_in,
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
	new->func.ret_type = return_type;
	new->func.num_params = 0;

	for (p = parms; p; p = p->next) {
		if (p->type) {
			count++;
		}
	}
	if (count) {
		new->func.param_types = malloc (count * sizeof (type_t *));
		new->func.param_quals = malloc (count * sizeof (param_qual_t));
	}
	for (p = parms; p; p = p->next) {
		if (!p->selector && !p->type && !p->name) {
			if (p->next)
				internal_error (0, 0);
			new->func.num_params = -(new->func.num_params + 1);
		} else if (p->type) {
			if (is_class (p->type)) {
				error (0, "cannot use an object as a parameter (forgot *?)");
				p->type = &type_id;
			}
			auto ptype = unalias_type (p->type);
			new->func.param_types[new->func.num_params] = ptype;
			new->func.param_quals[new->func.num_params] = p->qual;
			new->func.num_params++;
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

static metafunc_t *
new_metafunc (void)
{
	metafunc_t *metafunc;
	ALLOC (1024, metafunc_t, metafuncs, metafunc);
	return metafunc;
}

static void
set_func_attrs (const type_t *func_type, attribute_t *attr_list)
{
	auto func = &((type_t *) func_type)->func;//FIXME
	for (auto attr = attr_list; attr; attr = attr->next) {
		if (!strcmp (attr->name, "no_va_list")) {
			func->no_va_list = true;
		} else if (!strcmp (attr->name, "void_return")) {
			func->void_return = true;
		} else {
			warning (0, "skipping unknown function attribute '%s'", attr->name);
		}
	}
}

typedef struct {
	const type_t *type;
	bool        implicit;
} callparm_t;

typedef struct {
	int         num_params;
	callparm_t *params;
} calltype_t;

static bool
check_type (const type_t *type, callparm_t param, unsigned *cost, bool promote)
{
	if (!type) {
		return false;
	}
	if (type == param.type) {
		return true;
	}
	if (is_reference (type)) {
		// pass by references is a free conversion, but no promotion
		return dereference_type (type) == param.type;
	}
	if (is_reference (param.type)) {
		// dereferencing a reference is free so long as there's no
		// promotion, otherwise there's the promotion cost
		param.type = dereference_type (param.type);
	}
	if (type == param.type) {
		return true;
	}
	if (!promote) {
		// want exact match
		return false;
	}
	if (!type_promotes (type, param.type)) {
		return param.implicit && type_demotes (type, param.type);
	}
	*cost += 1;
	return true;
}

static const type_t * __attribute__((pure))
select_type (gentype_t *gentype, callparm_t param)
{
	for (auto t = gentype->valid_types; t && *t; t++) {
		if (*t == param.type) {
			return *t;
		}
		if (is_reference (*t) && dereference_type (*t) == param.type) {
			// pass value by reference: no promotion
			return *t;
		}
		auto pt = param.type;
		if (is_reference (pt)) {
			// pass reference by value: promotion ok
			pt = dereference_type (pt);
		}
		if (*t == pt) {
			return *t;
		}
		if (type_promotes (*t, pt)) {
			return *t;
		}
	}
	return nullptr;
}

static genfunc_t *
find_generic_function (genfunc_t **genfuncs, const expr_t *fexpr,
					   calltype_t *calltype, bool promote)
{
	int num_funcs = 0;
	for (auto gf = genfuncs; *gf; gf++, num_funcs++) continue;
	unsigned costs[num_funcs] = {};

	int num_params = calltype->num_params;
	auto call_params = calltype->params;
	for (int j = 0; j < num_funcs; j++) {
		auto g = genfuncs[j];
		if (g->num_params != num_params) {
			continue;
		}
		const type_t *types[g->num_types] = {};
		bool ok = true;
		for (int i = 0; ok && i < num_params; i++) {
			auto p = &g->params[i];
			if (!p->fixed_type) {
				int ind = p->gentype;
				if (!types[ind]) {
					types[ind] = select_type (&g->types[ind], call_params[i]);
				}
				ok &= check_type (types[ind], call_params[i], costs + j,
								  promote);
			} else {
				ok &= check_type (p->fixed_type, call_params[i], costs + j,
								  promote);
			}
		}
		if (!ok) {
			costs[j] = ~0u;
		}
	}

	auto fsym = fexpr->symbol;
	unsigned best_cost = ~0u;
	int best_ind = -1;
	for (int i = 0; i < num_funcs; i++) {
		if (best_ind >= 0 && costs[i] == best_cost) {
			error (fexpr, "unable to disambiguate %s", fsym->name);
			return nullptr;
		}
		if (costs[i] < best_cost) {
			best_ind = i;
			best_cost = costs[i];
		}
	}
	if (best_ind < 0) {
		error (fexpr, "unable to find generic function matching %s",
			   fsym->name);
		return nullptr;
	}
	return genfuncs[best_ind];
}

static symbol_t *
create_generic_sym (genfunc_t *g, const expr_t *fexpr, calltype_t *calltype)
{
	int num_params = calltype->num_params;
	auto call_params = calltype->params;
	const type_t *types[g->num_types] = {};
	const type_t *param_types[num_params];
	param_qual_t param_quals[num_params];
	const type_t *return_type;
	for (int i = 0; i < num_params; i++) {
		auto p = &g->params[i];
		if (!p->fixed_type) {
			int ind = p->gentype;
			if (!types[ind]) {
				types[ind] = select_type (&g->types[ind], call_params[i]);
			}
			param_types[i] = types[ind];
		} else {
			param_types[i] = p->fixed_type;
		}
		param_quals[i] = p->qual;
	}
	if (!g->ret_type->fixed_type) {
		int ind = g->ret_type->gentype;
		if (!types[ind]) {
			internal_error (0, "return type not determined");
		}
		return_type = types[ind];
	} else {
		return_type = g->ret_type->fixed_type;
	}
	param_t *params = nullptr;
	for (int i = 0; i < num_params; i++) {
		param_types[i] = unalias_type (param_types[i]);
		params = append_params (params, new_param (nullptr, param_types[i],
												   g->params[i].name));
	}
	return_type = unalias_type (return_type);

	type_t ftype = {
		.type = ev_func,
		.alignment = 1,
		.width = 1,
		.columns = 1,
		.func = {
			.ret_type = return_type,
			.num_params = num_params,
			.param_types = param_types,
			.param_quals = param_quals,
		},
	};
	auto type = find_type (&ftype);
	auto name = g->name;
	auto full_name = save_string (va (0, "%s|%s", name, encode_params (type)));

	auto fsym = fexpr->symbol;
	auto sym = symtab_lookup (fsym->table, full_name);
	if (!sym || sym->table != fsym->table) {
		sym = new_symbol (full_name);
		sym->sy_type = sy_func;
		sym->type = type;
		sym->params = params;
		sym->metafunc = new_metafunc ();
		*sym->metafunc = *fsym->metafunc;
		symtab_addsymbol (fsym->table, sym);
	}
	return sym;
}

static metafunc_t *
get_function (const char *name, specifier_t spec)
{
	if (!spec.sym->type || !spec.sym->type->encoding) {
		spec = default_type (spec, spec.sym);
		spec.sym->type = append_type (spec.sym->type, spec.type);
		set_func_attrs (spec.sym->type, spec.attributes);
		spec.sym->type = find_type (spec.sym->type);
	}
	auto type = unalias_type (spec.sym->type);
	int num_params = type->func.num_params;
	if (num_params < 0) {
		num_params = ~num_params;
	}
	callparm_t call_params[num_params + 1] = {};
	calltype_t calltype = {
		.num_params = type->func.num_params,
		.params = call_params,
	};
	for (int i = 0; i < num_params; i++) {
		call_params[i].type = type->func.param_types[i];
	}

	bool overload = spec.is_overload | current_language.always_override;
	metafunc_t *func = Hash_Find (function_map, name);
	if (func && func->meta_type == mf_generic) {
		auto genfuncs = (genfunc_t **) Hash_FindList (generic_functions, name);
		expr_t fexpr = {
			.loc = pr.loc,
			.type = ex_symbol,
			.symbol = symtab_lookup (current_symtab, name),
		};
		if (!fexpr.symbol || fexpr.symbol->sy_type != sy_func
			|| !fexpr.symbol->metafunc) {
			internal_error (0, "genfunc oops");
		}
		auto gen = find_generic_function (genfuncs, &fexpr, &calltype, false);
		if (gen) {
			auto sym = create_generic_sym (gen, &fexpr, &calltype);
			if (sym == fexpr.symbol
				|| sym->metafunc == fexpr.symbol->metafunc) {
				internal_error (0, "genfunc oops");
			}
			func = sym->metafunc;
			overload = true;
		} else {
			func = nullptr;
		}
	}

	const char *full_name;
	full_name = save_string (va (0, "%s|%s", name, encode_params (type)));

	if (!func || func->meta_type != mf_generic) {
		// check if the exact function signature already exists, in which case
		// simply return it.
		func = Hash_Find (metafuncs, full_name);
		if (func) {
			if (func->type != type) {
				error (0, "can't overload on return types");
				return func;
			}
			return func;
		}

		func = Hash_Find (function_map, name);
		if (func) {
			if (!overload && func->meta_type != mf_overload) {
				warning (0, "creating overloaded function %s without @overload",
						 full_name);
				warning (&(expr_t) { .loc = func->loc },
						 "(previous function is %s)", func->full_name);
			}
			overload = true;
		}

		func = new_metafunc ();
	}
	*func = (metafunc_t) {
		.name = save_string (name),
		.full_name = full_name,
		.type = type,
		.loc = pr.loc,
		.meta_type = overload ? mf_overload : mf_simple,
	};

	Hash_Add (metafuncs, func);
	Hash_Add (function_map, func);
	return func;
}

symbol_t *
function_symbol (specifier_t spec)
{
	symbol_t   *sym = spec.sym;
	const char *name = sym->name;
	metafunc_t *func = Hash_Find (function_map, name);

	auto genfunc = parse_generic_function (name, spec);
	if (genfunc) {
		add_generic_function (genfunc);

		ALLOC (1024, metafunc_t, metafuncs, func);
		*func = (metafunc_t) {
			.name = save_string (name),
			.full_name = name,
			.loc = pr.loc,
			.meta_type = mf_generic,
		};
		Hash_Add (metafuncs, func);
		Hash_Add (function_map, func);
	} else {
		func = get_function (name, spec);
	}

	if (func && func->meta_type == mf_overload)
		name = func->full_name;
	symbol_t   *s = symtab_lookup (current_symtab, name);
	if (!s || s->table != current_symtab) {
		s = new_symbol (name);
		s->sy_type = sy_func;
		s->type = unalias_type (sym->type);
		symtab_addsymbol (current_symtab, s);
	}
	//if it existed, override the declaration's parameters and metafunc
	s->params = sym->params;
	s->metafunc = func;
	return s;
}

static const expr_t *
set_func_symbol (const expr_t *fexpr, metafunc_t *f)
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
	if (fexpr->type != ex_symbol) {
		return fexpr;
	}

	int         num_params = params ? list_count (&params->list) : 0;
	const expr_t *args[num_params + 1];
	if (params) {
		list_scatter_rev (&params->list, args);
	}

	callparm_t call_params[num_params] = {};
	for (int i = 0; i < num_params; i++) {
		auto e = args[i];
		if (e->type == ex_error) {
			return e;
		}
		call_params[i] = (callparm_t) {
			.type = get_type (e),
			.implicit = e->implicit,
		};
	}
	calltype_t calltype = {
		.num_params = num_params,
		.params = call_params,
	};

	const char *fname = fexpr->symbol->name;
	auto genfuncs = (genfunc_t **) Hash_FindList (generic_functions, fname);
	if (genfuncs) {
		auto gen = find_generic_function (genfuncs, fexpr, &calltype, true);
		if (!gen) {
			return new_error_expr ();
		}
		auto sym = create_generic_sym (gen, fexpr, &calltype);
		return new_symbol_expr (sym);
	}

	auto funcs = (metafunc_t **) Hash_FindList (function_map, fname);
	if (!funcs)
		return fexpr;
	int num_funcs;
	for (num_funcs = 0; funcs[num_funcs]; num_funcs++) continue;
	if (num_funcs < 2) {
		if (num_funcs && funcs[0]->meta_type != mf_overload) {
			free (funcs);
			return fexpr;
		}
	}

	unsigned costs[num_funcs] = {};
	for (int i = 0; i < num_funcs; i++) {
		auto f = (metafunc_t *) funcs[i];
		int num_params = f->type->func.num_params;
		if ((num_params >= 0 && num_params != calltype.num_params)
			|| (num_params < 0 && ~num_params > calltype.num_params)) {
			costs[i] = ~0u;
			continue;
		}
		if (num_params < 0) {
			num_params = ~num_params;
		}
		bool ok = true;
		for (int j = 0; ok && j < num_params; j++) {
			auto fptype = f->type->func.param_types[j];
			auto cparam = calltype.params[j];
			ok &= check_type (fptype, cparam, costs + i, true);
		}
		if (!ok) {
			costs[i] = ~0u;
		}
	}
	unsigned best_cost = ~0u;
	int best_ind = -1;
	for (int i = 0; i < num_funcs; i++) {
		if (best_ind >= 0 && costs[i] == best_cost) {
			error (fexpr, "unable to disambiguate %s", fexpr->symbol->name);
			continue;
		}
		if (costs[i] < best_cost) {
			best_ind = i;
			best_cost = costs[i];
		}
	}

	if (best_ind >= 0) {
		auto best = funcs[best_ind];
		if (best->meta_type == mf_overload) {
			fexpr = set_func_symbol (fexpr, best);
		}
		free (funcs);
		return fexpr;
	}
	error (fexpr, "unable to find function matching");
	free (funcs);
	return fexpr;
}

int
value_too_large (const type_t *val_type)
{
	return current_target.value_too_large (val_type);
}

static void
check_function (symbol_t *fsym)
{
	param_t    *params = fsym->params;
	param_t    *p;
	int         i;
	auto ret_type = fsym->type->func.ret_type;

	if (!ret_type || !type_size (ret_type)) {
		error (0, "return type is an incomplete type");
		return;
		//fsym->type->t.func.type = &type_void;//FIXME better type?
	}
	if (value_too_large (ret_type)) {
		error (0, "return value too large to be passed by value (%d)",
			   type_size (&type_param));
		//fsym->type->func.type = &type_void;//FIXME better type?
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
build_scope (symbol_t *fsym, symtab_t *parent)
{
	function_t *func = fsym->metafunc->func;
	symtab_t   *parameters;
	symtab_t   *locals;

	if (!func) {
		internal_error (0, "function %s not defined", fsym->name);
	}
	if (!is_func (func->type)) {
		internal_error (0, "function type %s not a funciton", fsym->name);
	}

	check_function (fsym);

	func->label_scope = new_symtab (0, stab_label);

	parameters = new_symtab (parent, stab_param);
	parameters->space = defspace_new (ds_virtual);
	func->parameters = parameters;

	locals = new_symtab (parameters, stab_local);
	locals->space = defspace_new (ds_virtual);
	func->locals = locals;

	current_target.build_scope (fsym);
}

static function_t *
new_function (const char *name, const char *nice_name)
{
	function_t	*f;

	ALLOC (1024, function_t, functions, f);
	f->o_name = save_string (name);
	f->s_name = ReuseString (name);
	f->s_file = pr.loc.file;
	if (!(f->name = nice_name))
		f->name = name;
	return f;
}

function_t *
make_function (symbol_t *sym, const char *nice_name, defspace_t *space,
			   storage_class_t storage)
{
	reloc_t    *relocs = 0;
	if (sym->sy_type != sy_func)
		internal_error (0, "%s is not a function", sym->name);
	if (storage == sc_extern && sym->metafunc->func)
		return sym->metafunc->func;
	function_t *func = sym->metafunc->func;
	if (!func) {
		func = new_function (sym->name, nice_name);
		func->sym = sym;
		func->type = unalias_type (sym->type);
		sym->metafunc->func = func;
	}
	if (func->def && func->def->external && storage != sc_extern) {
		//FIXME this really is not the right way
		relocs = func->def->relocs;
		free_def (func->def);
		func->def = 0;
	}
	if (!func->def) {
		func->def = new_def (sym->name, sym->type, space, storage);
		reloc_attach_relocs (relocs, &func->def->relocs);
	}
	return func;
}

static void
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
	if (sym->sy_type != sy_func) {
		error (0, "%s is not a function", sym->name);
		sym = new_symbol_type (sym->name, &type_func);
		sym = function_symbol ((specifier_t) {
								.sym = sym,
								.is_overload = true
							   });
	}
	function_t *func = sym->metafunc->func;
	if (func && func->def && func->def->initialized) {
		error (0, "%s redefined", sym->name);
		sym = new_symbol_type (sym->name, sym->type);
		sym = function_symbol ((specifier_t) {
								.sym = sym,
								.is_overload = true
							   });
	}

	defspace_t *space = far ? pr.far_data : sym->table->space;

	func = make_function (sym, nicename, space, storage);
	if (!func->def->external) {
		func->def->initialized = 1;
		func->def->constant = 1;
		func->def->nosave = 1;
		add_function (func);
		reloc_def_func (func, func->def);

		func->def->loc = pr.loc;
	}
	func->code = pr.code->size;

	func->s_file = pr.loc.file;
	if (options.code.debug) {
		pr_lineno_t *lineno = new_lineno ();
		func->line_info = lineno - pr.linenos;
	}

	build_scope (sym, parent);
	return func;
}

static void
build_function (symbol_t *fsym)
{
	const type_t *func_type = fsym->metafunc->func->type;
	if (options.code.max_params >= 0
		&& func_type->func.num_params > options.code.max_params) {
		error (0, "too many params");
	}
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
	function_t *func = fsym->metafunc->func;
	current_target.build_code (func, statements);

	return fsym->metafunc->func;
}

function_t *
build_builtin_function (symbol_t *sym, const expr_t *bi_val, int far,
						storage_class_t storage)
{
	int         bi;

	if (sym->sy_type != sy_func) {
		error (bi_val, "%s is not a function", sym->name);
		return 0;
	}
	if (!is_int_val (bi_val)
		&& !(type_default != &type_int && is_float_val (bi_val))) {
		error (bi_val, "invalid constant for = #");
		return 0;
	}
	if (sym->metafunc->meta_type == mf_generic) {
		return 0;
	}

	function_t *func = sym->metafunc->func;
	if (func && func->def && func->def->initialized) {
		error (bi_val, "%s redefined", sym->name);
		return 0;
	}

	defspace_t *space = far ? pr.far_data : sym->table->space;
	func = make_function (sym, 0, space, storage);

	if (func->def->external)
		return 0;

	func->def->initialized = 1;
	func->def->constant = 1;
	func->def->nosave = 1;
	add_function (func);

	if (is_int_val (bi_val)) {
		bi = expr_int (bi_val);
	} else {
		bi = expr_float (bi_val);
		if (bi != expr_float (bi_val)) {
			error (bi_val, "invalid constant for = #");
		}
	}
	if (bi < 0) {
		error (bi_val, "builtin functions must be positive or 0");
		return 0;
	}
	func->builtin = bi;
	reloc_def_func (func, func->def);
	build_function (sym);

	// for debug info
	build_scope (sym, current_symtab);
	func->parameters->space->size = 0;
	func->locals->space = func->parameters->space;
	return func;
}

void
clear_functions (void)
{
	if (metafuncs) {
		Hash_FlushTable (generic_functions);
		Hash_FlushTable (metafuncs);
		Hash_FlushTable (function_map);
	} else {
		setup_type_progs ();
		generic_functions = Hash_NewTable (1021, gen_func_get_key, 0, 0, 0);
		metafuncs = Hash_NewTable (1021, metafunc_get_full_name, 0, 0, 0);
		function_map = Hash_NewTable (1021, metafunc_get_name, 0, 0, 0);
	}
}

void
add_ctor_expr (const expr_t *expr)
{
	if (!pr.ctor_exprs) {
		pr.ctor_exprs = new_block_expr (nullptr);
	}
	append_expr (pr.ctor_exprs, expr);
}

void
emit_ctor (void)
{
	if (!pr.ctor_exprs) {
		return;
	}

	auto ctor_sym = new_symbol_type (".ctor", &type_func);
	ctor_sym = function_symbol ((specifier_t) { .sym = ctor_sym });
	current_func = begin_function (ctor_sym, 0, current_symtab, 1, sc_static);
	build_code_function (ctor_sym, 0, pr.ctor_exprs);
}
