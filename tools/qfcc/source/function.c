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
#include "tools/qfcc/include/method.h"
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
	if (p1->compute && p2->compute) {
		auto c1 = p1->compute;
		auto c2 = p2->compute;
		if (c1 == c2) {
			// same program
			return true;
		}
		if (!c1 || !c2) {
			// only one parameter is computed so they can't be the same
			return false;
		}
		if (c1->code_size != c2->code_size || c1->data_size != c2->data_size
			|| c1->string_size != c2->string_size) {
			// computation programs are different sizes, so not the same
			return false;
		}
		size_t csize = c1->code_size * sizeof (dstatement_t);
		size_t dsize = c1->data_size * sizeof (pr_type_t);
		size_t ssize = c1->string_size;
		if (memcmp (c1->code, c2->code, csize) != 0) {
			return false;
		}
		if (memcmp (c1->data, c2->data, dsize) != 0) {
			return false;
		}
		if (memcmp (c1->strings, c2->strings, ssize) != 0) {
			return false;
		}
		return true;
	}
	// fixed_type for both p1 and p2 is null
	auto t1 = g1->types[p1->gentype];
	auto t2 = g2->types[p2->gentype];
	auto vt1 = t1.valid_types;
	auto vt2 = t2.valid_types;
	for (; *vt1 && *vt2 && *vt1 == *vt2; vt1++, vt2++) continue;
	return *vt1 == *vt2;
}

static genfunc_t *
_add_generic_function (genfunc_t *genfunc)
{
	auto name = genfunc->name;
	bool is_new = true;
	genfunc_t *old = nullptr;
	auto old_list = (genfunc_t **) Hash_FindList (generic_functions, name);
	for (auto o = old_list; is_new && o && *o; o++) {
		old = *o;
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
				return nullptr;
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
		genfunc = old;
	}
	return genfunc;
}

genfunc_t *
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

	return _add_generic_function (genfunc);
}

static const type_t **
valid_type_list (const expr_t *expr, rua_ctx_t *ctx)
{
	if (expr->type != ex_list) {
		return expand_type (expr, ctx);
	}
	int count = list_count (&expr->list);
	const expr_t *type_refs[count];
	list_scatter (&expr->list, type_refs);
	const type_t **types = malloc (sizeof (type_t *[count + 1]));
	types[count] = nullptr;
	bool err = false;
	for (int i = 0; i < count; i++) {
		if (!(types[i] = resolve_type (type_refs[i], ctx))) {
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

static symbol_t *
get_gensym (const expr_t *type_expr, bool *is_reference, unsigned *tag,
			rua_ctx_t *ctx)
{
	*is_reference = false;
	*tag = 0;
	if (!type_expr) {
		return nullptr;
	}
	if (type_expr->type == ex_type && type_expr->typ.op == QC_REFERENCE) {
		auto params = type_expr->typ.params;
		if (params && params->list.head) {
			*is_reference = true;
			if (params->list.head->next) {
				auto tag_expr = params->list.head->next->expr;
				tag_expr = expr_process (tag_expr, ctx);
				*tag = expr_integral (tag_expr);
			}
			type_expr = params->list.head->expr;
		}
	}
	if (type_expr->type != ex_symbol
		|| type_expr->symbol->sy_type != sy_type_param) {
		return nullptr;
	}
	return type_expr->symbol;
}

static gentype_t
make_gentype (const expr_t *expr, rua_ctx_t *ctx)
{
	bool is_reference = false;
	unsigned tag = 0;
	// strip off any reference type
	auto sym = get_gensym (expr, &is_reference, &tag, ctx);
	gentype_t gentype = {
		.name = save_string (sym->name),
		.valid_types = valid_type_list (sym->expr, ctx),
	};
	if (!gentype.valid_types) {
		error (expr, "empty generic type");
	}
	return gentype;
}

static int
find_gentype (const expr_t *expr, genfunc_t *genfunc, bool *is_reference,
			  unsigned *tag, rua_ctx_t *ctx)
{
	auto sym = get_gensym (expr, is_reference, tag, ctx);
	if (!sym) {
		return -1;
	}
	for (int i = 0; i < genfunc->num_types; i++) {
		auto t = &genfunc->types[i];
		if (strcmp (sym->name, t->name) == 0) {
			return i;
		}
	}
	return -1;
}

static genparam_t
make_genparam (param_t *param, genfunc_t *genfunc, rua_ctx_t *ctx)
{
	bool is_reference = false;
	unsigned tag = 0;
	int gentype = find_gentype (param->type_expr, genfunc, &is_reference, &tag,
								ctx);
	typeeval_t *compute = nullptr;
	if (gentype < 0 && param->type_expr) {
		compute = build_type_function (param->type_expr,
									   genfunc->num_types, genfunc->types, ctx);
	}
	genparam_t genparam = {
		.name = save_string (param->name),
		.fixed_type = param->type,
		.compute = compute,
		.gentype = gentype,
		.qual = param->qual,
		.is_reference = is_reference,
		.tag = tag,
	};
	return genparam;
}

static genfunc_t *
parse_generic_function (const char *name, specifier_t spec, rua_ctx_t *ctx)
{
	if (!spec.is_generic) {
		return nullptr;
	}
	spec = default_type (spec, spec.sym);
	// fake parameter for the return type
	param_t ret_param = {
		.next = spec.params,
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
			// or rely on earlier parameters, but need to check references
			bool is_reference;
			unsigned tag;
			auto gsym = get_gensym (q->type_expr, &is_reference, &tag, ctx);
			if (!gsym) {
				continue;
			}
			if (strcmp (gsym->name, s->name) == 0) {
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
			bool is_reference;
			unsigned tag;
			auto gsym = get_gensym (q->type_expr, &is_reference, &tag, ctx);
			if (!gsym) {
				continue;
			}
			if (strcmp (gsym->name, s->name) == 0) {
				int ind = num_gentype++;
				genfunc->types[ind] = make_gentype (q->type_expr, ctx);
				break;
			}
		}
	}

	auto type = new_type ();
	*type = type_func;
	spec.sym->type = type;

	num_params = 0;
	// skip return type so it can be done last to support complex expressions
	for (auto p = ret_param.next; p; p = p->next) {
		genfunc->params[num_params++] = make_genparam (p, genfunc, ctx);
	}
	*genfunc->ret_type = make_genparam (&ret_param, genfunc, ctx);
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
	const type_t **types;
} calltype_t;

static bool
check_type (const type_t *type, callparm_t param, unsigned *cost, bool promote)
{
	if (!type) {
		return false;
	}
	if (type_same (type, param.type)) {
		return true;
	}
	if (is_reference (type)) {
		type = dereference_type (type);
		if (is_reference (param.type)) {
			return type_same (type, dereference_type (param.type));
		} else {
			// pass by references is a free conversion, but no promotion
			return type_same (type, param.type);
		}
	}
	if (is_reference (param.type)) {
		// dereferencing a reference is free so long as there's no
		// promotion, otherwise there's the promotion cost
		param.type = dereference_type (param.type);
	}
	if (type_same (type, param.type)) {
		return true;
	}
	int ret = obj_types_assignable (type, param.type);
	if (ret >= 0) {
		return ret;
	}
	if (!promote) {
		// want exact match
		return false;
	}
	if (!type_promotes (type, param.type)) {
		bool demotes = param.implicit && type_demotes (type, param.type);
		if (demotes) {
			*cost += 1;
		}
		return demotes;
	}
	*cost += 2;
	return true;
}

static const type_t * __attribute__((pure))
select_type (gentype_t *gentype, callparm_t param)
{
	for (auto t = gentype->valid_types; t && *t; t++) {
		unsigned cost = 0;
		if (check_type (*t, param, &cost, true)) {
			return *t;
		}
	}
	return nullptr;
}

static genfunc_t *
find_generic_function (genfunc_t **genfuncs, const expr_t *fexpr,
					   calltype_t *calltype, bool promote, rua_ctx_t *ctx)
{
	int num_funcs = 0;
	for (auto gf = genfuncs; *gf; gf++, num_funcs++) continue;
	unsigned costs[num_funcs] = {};

	int num_params = calltype->num_params;
	auto call_params = calltype->params;
	for (int j = 0; j < num_funcs; j++) {
		auto g = genfuncs[j];
		if (g->num_params != num_params) {
			costs[j] = ~0u;
			continue;
		}
		const type_t *types[g->num_types] = {};
		bool ok = true;
		for (int i = 0; ok && i < num_params; i++) {
			auto p = &g->params[i];
			if (p->compute) {
				auto ptype = evaluate_type (p->compute, g->num_types,
										    types, fexpr, ctx);
				ok &= check_type (ptype, call_params[i], costs + j, promote);
			} else if (!p->fixed_type) {
				costs[j] += 1;
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
	for (int i = 0; i < num_funcs; i++) {
		if (i != best_ind && costs[i] == best_cost) {
			error (fexpr, "unable to disambiguate %s", fsym->name);
			return nullptr;
		}
	}
	return genfuncs[best_ind];
}

static const type_t *
compute_param_type (const genparam_t *param, int param_ind,
					const genfunc_t *genfunc, calltype_t *calltype,
					const expr_t *fexpr, rua_ctx_t *ctx)
{
	auto call_types = calltype->types;
	auto call_params = calltype->params;
	if (param->fixed_type) {
		return param->fixed_type;
	}
	if (param->compute) {
		return evaluate_type (param->compute, genfunc->num_types,
							  calltype->types, fexpr, ctx);
	}
	int ind = param->gentype;
	auto type = call_types[ind];
	if (!type && param_ind >= 0) {
		type = select_type (&genfunc->types[ind], call_params[param_ind]);
		call_types[ind] = type;
	}
	if (param->is_reference) {
		type = tagged_reference_type (param->tag, type);
	}
	return type;
}

static symbol_t *
create_generic_sym (genfunc_t *g, const expr_t *fexpr, calltype_t *calltype,
					rua_ctx_t *ctx)
{
	int num_params = calltype->num_params;
	const type_t *param_types[num_params] = {};
	param_qual_t param_quals[num_params] = {};
	const type_t *return_type;
	for (int i = 0; i < num_params; i++) {
		auto p = &g->params[i];
		param_types[i] = compute_param_type (p, i, g, calltype, fexpr, ctx);
		param_quals[i] = p->qual;
	}
	return_type = compute_param_type (g->ret_type, -1, g, calltype, fexpr, ctx);
	if (!return_type) {
		internal_error (0, "return type not determined");
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
		sym->metafunc->expr = g->expr;
		sym->metafunc->can_inline = g->can_inline;
		symtab_addsymbol (fsym->table, sym);
	}
	return sym;
}

static metafunc_t *
get_function (const char *name, specifier_t spec, rua_ctx_t *ctx)
{
	spec = spec_process (spec, ctx);
	spec.sym->type = spec.type;
	set_func_attrs (spec.sym->type, spec.attributes);
	spec.sym->type = find_type (spec.sym->type);

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

	bool overload = spec.is_overload;
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
		auto gen = find_generic_function (genfuncs, &fexpr, &calltype, false,
										  ctx);
		if (gen) {
			const type_t *ref_types[gen->num_types] = {};
			calltype.types = ref_types;
			auto sym = create_generic_sym (gen, &fexpr, &calltype, ctx);
			if (sym == fexpr.symbol
				|| sym->metafunc == fexpr.symbol->metafunc) {
				internal_error (0, "genfunc oops");
			}
			func = sym->metafunc;
			overload = true;
			calltype.types = nullptr;
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
function_symbol (specifier_t spec, rua_ctx_t *ctx)
{
	symbol_t   *sym = spec.sym;
	sym->params = spec.params;
	const char *name = sym->name;
	metafunc_t *func = Hash_Find (function_map, name);

	auto check = symtab_lookup (current_symtab, name);
	if ((sym->table == current_symtab && sym->sy_type != sy_func)
		|| (check && check->table == current_symtab
			&& check->sy_type != sy_func)) {
		auto err = new_symbol (nullptr);
		err->sy_type = sy_expr;
		err->expr = error (0, "`%s` redeclared as different kind of symbol",
						   name);
		return err;
	}

	auto genfunc = parse_generic_function (name, spec, ctx);
	if (genfunc) {
		_add_generic_function (genfunc);

		func = new_metafunc ();
		*func = (metafunc_t) {
			.name = save_string (name),
			.full_name = name,
			.loc = pr.loc,
			.meta_type = mf_generic,
			.genfunc = genfunc,
		};
		Hash_Add (metafuncs, func);
		Hash_Add (function_map, func);
	} else {
		func = get_function (name, spec, ctx);
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
	if (!sym->table && strcmp (s->name, sym->name) != 0) {
		// record unmangled function symbol to avoid false undefined symbol
		// errors
		sym->sy_type = sy_func;
		sym->metafunc = new_metafunc ();
		*sym->metafunc = (metafunc_t) {
			.name = save_string (name),
			.meta_type = mf_overload,
		};
		symtab_addsymbol (current_symtab, sym);
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

static void
build_core_scope (symbol_t *fsym, symtab_t *parent, bool new_space)
{
	auto func = fsym->metafunc->func;
	func->label_scope = new_symtab (0, stab_label);

	auto parameters = new_symtab (parent, stab_param);
	parameters->space = parent->space;
	func->parameters = parameters;

	auto locals = new_symtab (parameters, stab_local);
	locals->space = parent->space;
	func->locals = locals;

	if (new_space) {
		parameters->space = defspace_new (ds_virtual);
		locals->space = defspace_new (ds_virtual);
	}
}

static void
build_generic_scope (symbol_t *fsym, symtab_t *parent, genfunc_t *genfunc,
					 const type_t **ref_types)
{
	build_core_scope (fsym, parent, false);

	auto func = fsym->metafunc->func;
	for (int i = 0; i < genfunc->num_types; i++) {
		auto type = &genfunc->types[i];
		auto sym = new_symbol (type->name);
		sym->sy_type = sy_type_param;
		if (ref_types) {
			sym->expr = new_type_expr (ref_types[i]);
		}
		symtab_addsymbol (func->parameters, sym);
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

const expr_t *
find_function (const expr_t *fexpr, const expr_t *params, rua_ctx_t *ctx)
{
	if (fexpr->type != ex_symbol) {
		return fexpr;
	}

	int         num_params = params ? list_count (&params->list) : 0;
	const expr_t *args[num_params + 1] = {};
	if (params) {
		list_scatter_rev (&params->list, args);
	}

	callparm_t call_params[num_params + 1] = {};
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

	auto fsym = fexpr->symbol;
	const char *fname = fsym->name;
	auto genfuncs = (genfunc_t **) Hash_FindList (generic_functions, fname);
	if (genfuncs) {
		auto gen = find_generic_function (genfuncs, fexpr, &calltype, true,
										  ctx);
		if (!gen) {
			return new_error_expr ();
		}
		const type_t *ref_types[gen->num_types] = {};
		calltype.types = ref_types;
		auto sym = create_generic_sym (gen, fexpr, &calltype, ctx);
		if (gen->can_inline) {
			// the call will be inlined, so a new scope is needed every
			// time
			sym->metafunc->func = new_function (sym->name, gen->name);
			sym->metafunc->func->type = sym->type;
			sym->metafunc->func->sym = sym;
			build_generic_scope (sym, current_symtab, gen, ref_types);
		}
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

	unsigned costs[num_funcs + 1] = {};
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
			error (fexpr, "unable to disambiguate %s", fsym->name);
			continue;
		}
		if (costs[i] < best_cost) {
			best_ind = i;
			best_cost = costs[i];
		}
	}
	if (best_ind < 0) {
		free (funcs);
		return error (fexpr, "unable to find function matching");
	}
	for (int i = 0; i < num_funcs; i++) {
		if (i != best_ind && costs[i] == best_cost) {
			free (funcs);
			return error (fexpr, "unable to disambiguate %s", fsym->name);
		}
	}

	auto best = funcs[best_ind];
	if (best->meta_type == mf_overload) {
		fexpr = set_func_symbol (fexpr, best);
	}
	if (best->can_inline) {
		// the call will be inlined, so a new scope is needed every
		// time
		auto sym = fexpr->symbol;
		best->func = new_function (best->full_name, best->name);
		best->func->type = sym->type;
		best->func->sym = sym;
		build_core_scope (sym, current_symtab, false);
	}
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
	function_t *func = fsym->metafunc->func;
	param_t    *params = fsym->params;
	param_t    *p;
	int         i;
	auto ret_type = fsym->type->func.ret_type;

	if (!func) {
		internal_error (0, "function %s not defined", fsym->name);
	}
	if (!is_func (func->type)) {
		internal_error (0, "function type %s not a funciton", fsym->name);
	}

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
	build_core_scope (fsym, parent, true);
	current_target.build_scope (fsym);
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
begin_function (specifier_t spec, const char *nicename, symtab_t *parent,
				rua_ctx_t *ctx)
{
	auto sym = spec.sym;
	if (sym->sy_type != sy_func) {
		error (0, "%s is not a function", sym->name);
		sym = new_symbol_type (sym->name, &type_func);
		sym = function_symbol ((specifier_t) {
								.sym = sym,
								.is_overload = true
							   }, ctx);
	}
	function_t *func = sym->metafunc->func;
	if (func && func->def && func->def->initialized) {
		error (0, "%s redefined", sym->name);
		sym = new_symbol_type (sym->name, sym->type);
		sym = function_symbol ((specifier_t) {
								.sym = sym,
								.is_overload = true
							   }, ctx);
	}

	if (spec.is_generic) {
		auto genfunc = sym->metafunc->genfunc;
		func = new_function (sym->name, nicename);
		sym->metafunc->func = func;
		sym->metafunc->genfunc = genfunc;
		build_generic_scope (sym, parent, genfunc, nullptr);
		return func;
	}

	defspace_t *space = spec.is_far ? pr.far_data : sym->table->space;

	func = make_function (sym, nicename, space, spec.storage);
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

	check_function (sym);

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

void
build_code_function (specifier_t spec, const expr_t *state_expr,
					 expr_t *statements, rua_ctx_t *ctx)
{
	auto fsym = spec.sym;
	if (fsym->metafunc->meta_type == mf_generic) {
		auto genfunc = fsym->metafunc->genfunc;
		if (genfunc->expr) {
			error (statements, "%s already defined", fsym->name);
			return;
		}
		genfunc->expr = statements;
		genfunc->can_inline = can_inline (statements, fsym);
		return;
	}
	function_t *func = fsym->metafunc->func;
	current_func = func;
	if (ctx) {
		statements = (expr_t *) expr_process (statements, ctx);
	}
	if (fsym->sy_type != sy_func) {	// probably in error recovery
		return;
	}
	build_function (fsym);
	if (state_expr) {
		prepend_expr (statements, state_expr);
	}
	current_target.build_code (func, statements);
}

void
build_builtin_function (specifier_t spec, const char *ext_name,
						const expr_t *bi_val)
{
	auto sym = spec.sym;
	int         bi;

	if (sym->sy_type != sy_func) {
		error (bi_val, "%s is not a function", sym->name);
		return;
	}
	if (!is_int_val (bi_val)
		&& !(type_default != &type_int && is_float_val (bi_val))) {
		error (bi_val, "invalid constant for = #");
		return;
	}
	if (sym->metafunc->meta_type == mf_generic) {
		return;
	}

	function_t *func = sym->metafunc->func;
	if (func && func->def && func->def->initialized) {
		error (bi_val, "%s redefined", sym->name);
		return;
	}

	defspace_t *space = spec.is_far ? pr.far_data : sym->table->space;
	func = make_function (sym, nullptr, space, spec.storage);
	if (ext_name) {
		func->s_name = ReuseString (ext_name);
	}

	if (func->def->external) {
		return;
	}

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
		return;
	}
	func->builtin = bi;
	reloc_def_func (func, func->def);
	build_function (sym);

	check_function (sym);

	// for debug info
	build_scope (sym, current_symtab);
	func->parameters->space->size = 0;
	func->locals->space = func->parameters->space;
}

void
build_intrinsic_function (specifier_t spec, const expr_t *intrinsic,
						  rua_ctx_t *ctx)
{
	auto sym = function_symbol (spec, ctx);
	if (sym->type->func.num_params < 0) {
		error (intrinsic, "intrinsic functions cannot be variadic");
		return;
	}
	if (sym->metafunc->meta_type == mf_generic) {
		auto genfunc = sym->metafunc->genfunc;
		if (genfunc->expr) {
			error (intrinsic, "%s already defined", sym->name);
			return;
		}
		genfunc->expr = intrinsic;
		// intrinsic functions with extra (ie, explict) arguments are similar
		// to inline functions
		genfunc->can_inline = intrinsic->intrinsic.extra;
	} else {
		if (sym->metafunc->expr || sym->metafunc->func) {
			error (intrinsic, "%s already defined", sym->name);
			return;
		}
		sym->metafunc->expr = intrinsic;
		sym->metafunc->can_inline = intrinsic->intrinsic.extra;
	}
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
emit_ctor (rua_ctx_t *ctx)
{
	if (!pr.ctor_exprs) {
		return;
	}

	auto spec = (specifier_t) {
		.type = &type_func,
		.sym = new_symbol (".ctor"),
		.storage = sc_static,
		.is_far = true,
	};
	spec.sym = function_symbol (spec, ctx);
	current_func = begin_function (spec, nullptr, current_symtab, ctx);
	build_code_function (spec, 0, pr.ctor_exprs, nullptr);
}
