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
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

ALLOC_STATE (param_t, params);
ALLOC_STATE (function_t, functions);
ALLOC_STATE (metafunc_t, metafuncs);
ALLOC_STATE (genfunc_t, genfuncs);
static hashtab_t *generic_functions;
static hashtab_t *metafuncs;
static hashtab_t *function_map;

// standardized base register to use for all locals (arguments, local defs,
// params)
#define LOCALS_REG 1
// keep the stack aligned to 8 words (32 bytes) so lvec etc can be used without
// having to do shenanigans with mixed-alignment stack frames
#define STACK_ALIGN 8

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

static metafunc_t *
get_function (const char *name, const type_t *type, specifier_t spec)
{
	metafunc_t *func = Hash_Find (function_map, name);
	if (func && func->meta_type == mf_generic) {
		error (0, "can't mix generic and simple or overload");
		return nullptr;
	}

	bool overload = spec.is_overload;
	const char *full_name;

	full_name = save_string (va (0, "%s|%s", name, encode_params (type)));

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
	//FIXME want to be able to provide specific overloads for generic functions
	//but need to figure out details, so disallow for now.
	if (genfunc) {
		if (func && func->meta_type != mf_generic) {
			error (0, "can't mix generic and simple or overload");
			return nullptr;
		}
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
		if (!spec.sym->type || !spec.sym->type->encoding) {
			spec = default_type (spec, spec.sym);
			spec.sym->type = append_type (spec.sym->type, spec.type);
			set_func_type_attrs (spec.sym->type, spec);
			spec.sym->type = find_type (spec.sym->type);
		}
		func = get_function (name, unalias_type (sym->type), spec);
	}

	if (func && func->meta_type == mf_overload)
		name = func->full_name;
	symbol_t   *s = symtab_lookup (current_symtab, name);
	if (!s || s->table != current_symtab) {
		s = new_symbol (name);
		s->sy_type = sy_func;
		s->type = unalias_type (sym->type);
		s->params = sym->params;
		s->metafunc = func;
		symtab_addsymbol (current_symtab, s);
	}
	return s;
}

// NOTE sorts the list in /reverse/ order
static int
func_compare (const void *a, const void *b)
{
	metafunc_t *fa = *(metafunc_t **) a;
	metafunc_t *fb = *(metafunc_t **) b;
	const type_t *ta = fa->type;
	const type_t *tb = fb->type;
	int         na = ta->func.num_params;
	int         nb = tb->func.num_params;
	int         ret, i;

	if (na < 0)
		na = ~na;
	if (nb < 0)
		nb = ~nb;
	if (na != nb)
		return nb - na;
	if ((ret = (tb->func.num_params - ta->func.num_params)))
		return ret;
	for (i = 0; i < na && i < nb; i++) {
		auto diff = tb->func.param_types[i] - ta->func.param_types[i];
		if (diff) {
			return diff < 0 ? -1 : 1;
		}
	}
	return 0;
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

static const type_t * __attribute__((pure))
select_type (gentype_t *gentype, const type_t *param_type)
{
	for (auto t = gentype->valid_types; t && *t; t++) {
		if (*t == param_type || type_promotes (*t, param_type)) {
			return *t;
		}
	}
	return nullptr;
}

static bool
check_type (const type_t *type, const type_t *param_type, unsigned *cost)
{
	if (type != param_type) {
		if (type_promotes (type, param_type)) {
			*cost += 1;
		} else {
			return false;
		}
	}
	return true;
}

static const expr_t *
find_generic_function (const expr_t *fexpr, genfunc_t **genfuncs,
					   const type_t *call_type)
{
	int num_funcs = 0;
	for (auto gf = genfuncs; *gf; gf++, num_funcs++) continue;
	unsigned costs[num_funcs] = {};

	int num_params = call_type->func.num_params;
	auto call_params = call_type->func.param_types;
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
				ok &= check_type (types[ind], call_params[i], costs + j);
			} else {
				ok &= check_type (p->fixed_type, call_params[i], costs + j);
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
			return error (fexpr, "unable to disambiguate %s", fsym->name);
		}
		if (costs[i] < best_cost) {
			best_ind = i;
			best_cost = costs[i];
		}
	}
	if (best_ind < 0) {
		return error (fexpr, "unable to find generic function matching %s",
					  fsym->name);
	}

	auto g = genfuncs[best_ind];
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
	return new_symbol_expr (sym);
}

const expr_t *
find_function (const expr_t *fexpr, const expr_t *params)
{
	int         func_count, parm_count, reported = 0;
	metafunc_t  dummy, *best = 0;
	void       *dummy_p = &dummy;

	if (fexpr->type != ex_symbol) {
		return fexpr;
	}

	int         num_params = params ? list_count (&params->list) : 0;
	const type_t *arg_types[num_params + 1];
	param_qual_t arg_quals[num_params + 1];
	const expr_t *args[num_params + 1];
	if (params) {
		list_scatter_rev (&params->list, args);
	}
	for (int i = 0; i < num_params; i++) {
		auto e = args[i];
		if (e->type == ex_error) {
			return e;
		}
		arg_types[i] = get_type (e);
		arg_quals[i] = pq_in;
	}

	type_t call_type = {
		.type = ev_func,
		.func = {
			.num_params = num_params,
			.param_types = arg_types,
			.param_quals = arg_quals,
		},
	};

	const char *fname = fexpr->symbol->name;
	auto genfuncs = (genfunc_t **) Hash_FindList (generic_functions, fname);
	if (genfuncs) {
		return find_generic_function (fexpr, genfuncs, &call_type);
	}

	auto funcs = (metafunc_t **) Hash_FindList (function_map, fname);
	if (!funcs)
		return fexpr;
	for (func_count = 0; funcs[func_count]; func_count++) continue;
	if (func_count < 2) {
		if (func_count && funcs[0]->meta_type != mf_overload) {
			free (funcs);
			return fexpr;
		}
	}
	call_type.func.ret_type = funcs[0]->type->func.ret_type;
	dummy.type = find_type (&call_type);

	qsort (funcs, func_count, sizeof (void *), func_compare);
	dummy.full_name = save_string (va (0, "%s|%s", fexpr->symbol->name,
									   encode_params (&call_type)));
	dummy_p = bsearch (&dummy_p, funcs, func_count, sizeof (void *),
					   func_compare);
	if (dummy_p) {
		auto f = (metafunc_t *) *(void **) dummy_p;
		if (f->meta_type == mf_overload) {
			fexpr = set_func_symbol (fexpr, f);
		}
		free (funcs);
		return fexpr;
	}
	for (int i = 0; i < func_count; i++) {
		auto f = (metafunc_t *) funcs[i];
		parm_count = f->type->func.num_params;
		if ((parm_count >= 0 && parm_count != call_type.func.num_params)
			|| (parm_count < 0 && ~parm_count > call_type.func.num_params)) {
			funcs[i] = 0;
			continue;
		}
		if (parm_count < 0)
			parm_count = ~parm_count;
		int j;
		for (j = 0; j < parm_count; j++) {
			if (!type_assignable (f->type->func.param_types[j],
								  call_type.func.param_types[j])) {
				funcs[i] = 0;
				break;
			}
		}
		if (j < parm_count)
			continue;
	}
	for (int i = 0; i < func_count; i++) {
		auto f = (metafunc_t *) funcs[i];
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
		if (best->meta_type == mf_overload) {
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
build_v6p_scope (symbol_t *fsym)
{
	int         i;
	param_t    *p;
	symbol_t   *args = 0;
	symbol_t   *param;
	function_t *func = fsym->metafunc->func;
	symtab_t   *parameters = func->parameters;
	symtab_t   *locals = func->locals;

	if (func->type->func.num_params < 0) {
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
		if (p->qual == pq_out) {
			param->def->param = false;
			param->def->out_param = true;
		} else if (p->qual == pq_inout) {
			param->def->out_param = true;
		} else if (p->qual == pq_const) {
			param->def->readonly = true;
		}
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
	param->def = def;
	param->sy_type = sy_def;
	symtab_addsymbol (parameters, param);
	if (is_vector(param->type) && options.code.vector_components)
		init_vector_components (param, 0, parameters);
}

static void
build_rua_scope (symbol_t *fsym)
{
	function_t *func = fsym->metafunc->func;

	for (param_t *p = fsym->params; p; p = p->next) {
		symbol_t   *param;
		if (!p->selector && !p->type && !p->name) {
			// ellipsis marker
			param = new_symbol_type (".args", &type_va_list);
		} else {
			if (!p->type) {
				continue;					// non-param selector
			}
			if (is_void (p->type)) {
				if (p->name) {
					error (0, "invalid parameter type for %s", p->name);
				} else if (p != fsym->params || p->next) {
					error (0, "void must be the only parameter");
					continue;
				} else {
					continue;
				}
			}
			if (!p->name) {
				error (0, "parameter name omitted");
				p->name = save_string ("");
			}
			param = new_symbol_type (p->name, p->type);
		}
		create_param (func->parameters, param);
		if (p->qual == pq_out) {
			param->def->param = false;
			param->def->out_param = true;
		} else if (p->qual == pq_inout) {
			param->def->out_param = true;
		} else if (p->qual == pq_const) {
			param->def->readonly = true;
		}
		param->def->reg = func->temp_reg;
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

	if (options.code.progsversion == PROG_VERSION) {
		build_rua_scope (fsym);
	} else {
		build_v6p_scope (fsym);
	}
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
	if (func_type->func.num_params > PR_MAX_PARAMS) {
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
	function_t *func = fsym->metafunc->func;
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
