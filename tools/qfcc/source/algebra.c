/*
	algebra.c

	QC geometric algebra support code

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

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

#include <ctype.h>
#include <string.h>

#include "QF/darray.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/va.h"

#include "QF/math/bitop.h"

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#define MAX_SIGNATURE 16

static struct DARRAY_TYPE (algebra_t *) algebras = DARRAY_STATIC_INIT (16);

static void
binomial (int *coef, int n)
{
	int         c = 1;
	for (int i = 0; i < n + 1; i++) {
		coef[i] = c;
		c = (c * (n - i)) / (i + 1);
	}
}

static int
count_bits (uint32_t v)
{
	int         c = 0;
	for (; v; c++) {
		v &= v - 1;
	}
	return c;
}

static int
count_flips (uint32_t a, uint32_t b)
{
	int         c = 0;
	a >>= 1;
	while (a) {
		c += count_bits (a & b);
		a >>= 1;
	}
	return c;
}

static int
count_minus (uint32_t minus)
{
	return count_bits (minus) & 1 ? -1 : 1;
}

static const char *mvec_1d_names[] = {
	"vec",
	"scalar",
	nullptr
};

static const char *mvec_2d_names[] = {
	"vec",
	"scalar",
	"bvec",
	nullptr
};

static const char *mvec_3d_names[] = {
	"vec",
	"tvec",
	"bvec",
	"scalar",
	nullptr
};

static const char *mvec_4d_names[] = {
	"vec",
	"bvect",	// tangential (directional) (in PGA)
	"scalar",
	"bvecp",	// positional (in PGA)
	"qvec",
	"tvec",
	nullptr
};

static const char **mvec_names[] = {
	[1] = mvec_1d_names,
	[2] = mvec_2d_names,
	[3] = mvec_3d_names,
	[4] = mvec_4d_names,
};

static void
basis_blade_init (basis_blade_t *blade, pr_uint_t mask)
{
	*blade = (basis_blade_t) {
		.mask = mask,
		.scale = 1,
	};
}

static void
basis_group_init (basis_group_t *group, int count, basis_blade_t *blades,
				  algebra_t *a, int group_id)
{
	pr_uint_t group_mask = 1 << group_id;
	*group = (basis_group_t) {
		.count = count,
		.group_mask = group_mask,
		.range = { ~0u, 0 },
		.blades = malloc (sizeof (basis_blade_t[count])),
		.set = set_new (),
	};
	memcpy (group->blades, blades, sizeof (basis_blade_t[count]));
	for (int i = 0; i < count; i++) {
		pr_uint_t m = blades[i].mask;
		group->range[0] = min (m, group->range[0]);
		group->range[1] = max (m, group->range[1]);
		set_add (group->set, m);
	}
	int         num = group->range[1] - group->range[0] + 1;
	group->map = malloc (sizeof (int[num]));
	for (int i = 0; i < count; i++) {
		group->map[blades[i].mask - group->range[0]] = i;
	}
}

static void
basis_layout_init (basis_layout_t *layout, int count, basis_group_t *groups)
{
	*layout = (basis_layout_t) {
		.count = count,
		.range = { ~0u, 0 },
		.groups = groups,
		.set = set_new (),
	};
	int group_base[count + 1];
	group_base[0] = 0;
	int num_blades = 0;
	for (int i = 0; i < count; i++) {
		set_union (layout->set, groups[i].set);
		group_base[i + 1] = group_base[i] + groups[i].count;
		num_blades += groups[i].count;
		layout->range[0] = min (groups[i].range[0], layout->range[0]);
		layout->range[1] = max (groups[i].range[1], layout->range[1]);
	}
	layout->blade_count = num_blades;
	layout->group_map = malloc (sizeof (pr_ivec3_t[num_blades]));

	int         num = layout->range[1] - layout->range[0] + 1;
	layout->mask_map = calloc (1, sizeof (int[num]));
	int group_inds[count + 1] = {};
	for (int i = 0; i < count; i++) {
		auto g = &groups[i];
		group_inds[i] = 0;
		for (int j = 0; j < g->count; j++) {
			auto b = g->blades[j];
			layout->mask_map[b.mask - layout->range[0]] = group_inds[count];
			layout->group_map[group_inds[count]][0] = i;
			layout->group_map[group_inds[count]][1] = group_inds[i]++;
			layout->group_map[group_inds[count]][2] = group_base[i];
			group_inds[count]++;
		}
	}
}

static void
metric_init (metric_t *metric, int p, int m, int z)
{
	metric->plus = ((1 << p) - 1) << z;
	metric->minus = ((1 << m) - 1) << (z + p);
	metric->zero = (1 << z) - 1;
}

int
metric_apply (const metric_t *metric, pr_uint_t a, pr_uint_t b)
{
	// find all the squared elements
	pr_uint_t c = a & b;
	// any elements that square to 0 result in 0
	if (c & metric->zero) {
		return 0;
	}
	return count_minus (c & metric->minus);
}

static const type_t **
alloc_mvec_types (int num_groups)
{
	return calloc (1 << num_groups, sizeof (type_t *));
}

static void
algebra_init (algebra_t *a)
{
	int p = a->plus;
	int m = a->minus;
	int z = a->zero;
	int d = p + m + z;
	if (d > MAX_SIGNATURE) {
		internal_error (nullptr, "algebra too large");
	}
	metric_init (&a->metric, p, m, z);
	a->dimension = d;
	a->num_components = 1 << d;

	basis_blade_t blades[a->num_components];
	int indices[d + 1];
	int counts[d + 1];
	binomial (counts, d);

	indices[0] = 0;
	for (int i = 0; i < d; i++) {
		indices[i + 1] = indices[i] + counts[i];
	}

	for (int i = 0; i < a->num_components; i++) {
		int         grade = count_bits (i);
		int         ind = indices[grade]++;
		pr_uint_t   mask = i;
		basis_blade_init (&blades[ind], mask);
	}

	if (p == 3 && m == 0 && z == 1) {
		// 3d PGA (w squares to 0, x y z square to +1):
		// : x   y   z   w
		// : yz  zx  xy  1
		// : wx  wy  wz  wxyz
		// : wzy wxz wyx xyz
		basis_blade_t pga_blades[16] = {
			blades[2],  blades[3],  blades[4],  blades[1],
			blades[10], blades[9],  blades[7],  blades[0],
			blades[5],  blades[6],  blades[8],  blades[15],
			blades[13], blades[12], blades[11], blades[14],
		};
		a->groups = malloc (sizeof (basis_group_t[6]));
		a->mvec_types = alloc_mvec_types (6);
		basis_group_init (&a->groups[0], 4, pga_blades +  0, a, 0);
		basis_group_init (&a->groups[1], 3, pga_blades +  4, a, 1);
		basis_group_init (&a->groups[2], 1, pga_blades +  7, a, 2);
		basis_group_init (&a->groups[3], 3, pga_blades +  8, a, 3);
		basis_group_init (&a->groups[4], 1, pga_blades + 11, a, 4);
		basis_group_init (&a->groups[5], 4, pga_blades + 12, a, 5);
		basis_layout_init (&a->layout, 6, a->groups);
	} else if (p == 3 && m == 0 && z == 0) {
		// 3d VGA (x y z square to +1):
		// : x   y   z   xyz
		// : yz  zx  xy  1
		basis_blade_t pga_blades[8] = {
			blades[1], blades[2], blades[3], blades[7],
			blades[6], blades[5], blades[4], blades[0],
		};
		a->groups = malloc (sizeof (basis_group_t[4]));
		a->mvec_types = alloc_mvec_types (4);
		basis_group_init (&a->groups[0], 3, pga_blades +  0, a, 0);
		basis_group_init (&a->groups[1], 1, pga_blades +  3, a, 1);
		basis_group_init (&a->groups[2], 3, pga_blades +  4, a, 2);
		basis_group_init (&a->groups[3], 1, pga_blades +  7, a, 3);
		basis_layout_init (&a->layout, 4, a->groups);
	} else if (p == 2 && m == 0 && z == 1) {
		// 2d PGA (w squares to 0, x y square to +1):
		// : x   y   w   wxy
		// : yw  wx  xy  1
		basis_blade_t pga_blades[8] = {
			blades[2], blades[3], blades[1], blades[7],
			blades[5], blades[4], blades[6], blades[0],
		};
		a->groups = malloc (sizeof (basis_group_t[4]));
		a->mvec_types = alloc_mvec_types (4);
		basis_group_init (&a->groups[0], 3, pga_blades + 0, a, 0);
		basis_group_init (&a->groups[1], 1, pga_blades + 3, a, 1);
		basis_group_init (&a->groups[2], 3, pga_blades + 4, a, 2);
		basis_group_init (&a->groups[3], 1, pga_blades + 7, a, 3);
		basis_layout_init (&a->layout, 4, a->groups);
	} else {
		// just use the grades as the default layout
		a->groups = malloc (sizeof (basis_group_t[d + 1]));
		a->mvec_types = alloc_mvec_types (d + 1);
		for (int i = 0; i < d + 1; i++) {
			int         c = counts[i];
			int         ind = indices[i];
			basis_group_init (&a->groups[i], c, &blades[ind - c], a, i);
		}
		basis_layout_init (&a->layout, d + 1, a->groups);
	}

	for (int i = 0; i < a->layout.count; i++) {
		auto g = &a->layout.groups[i];
		if (g->count == 1 && g->blades[0].mask == 0) {
			a->mvec_types[g->group_mask] = a->type;
		} else {
			algebra_mvec_type (a, g->group_mask);
		}
	}
	auto type = algebra_mvec_type (a, (1 << a->layout.count) - 1);
	a->mvec_sym = type->multivec->mvec_sym;
}

bool
is_algebra (const type_t *type)
{
	type = unalias_type (type);
	return type->meta == ty_algebra;
}

static const expr_t *
algebra_property (const type_t *type, const attribute_t *attr)
{
	type = algebra_subtype (type, attr);
	return new_type_expr (type);
}

const type_t *
algebra_type (const type_t *type, const expr_t *params)
{
	if (!is_float (type) && !is_double (type)) {
		error (nullptr, "algebra type must be float or double");
		return type_default;
	}
	int param_count = params ? list_count (&params->list) : 0;
	if (param_count > 3) {
		error (params, "too many arguments in signature");
		return type_default;
	}
	const expr_t *param_exprs[3] = {};
	if (params) {
		list_scatter (&params->list, param_exprs);
	}
	auto plus = param_exprs[0];
	auto minus = param_exprs[1];
	auto zero = param_exprs[2];

	const expr_t *err = 0;
	if ((plus && !is_integral_val (err = plus))
		|| (minus && !is_integral_val (err = minus))
		|| (zero && !is_integral_val (err = zero))) {
		error (err, "signature must be integral constant");
		return type_default;
	}

	algebra_t search_algebra = {
		.type = type,
		// default to 3,0,1 (plane-based PGA)
		.plus = plus ? expr_integral (plus) : 3,
		.minus = minus ? expr_integral (minus) : 0,
		.zero = zero ? expr_integral (zero) : plus ? 0 : 1,
	};
	int dim = search_algebra.plus + search_algebra.minus + search_algebra.zero;
	if (search_algebra.plus < 0
		|| search_algebra.minus < 0
		|| search_algebra.zero < 0
		|| dim < 1) {
		error (err, "signature must be positive");
		return type_default;
	}
	if (dim > MAX_SIGNATURE) {
		error (err, "signature too large (that's %zd components!)",
			   ((size_t) 1) << dim);
		return type_default;
	}
	algebra_t *algebra = 0;
	for (size_t i = 0; i < algebras.size; i++) {
		auto a = algebras.a[i];
		if (a->type == search_algebra.type
			&& a->plus == search_algebra.plus
			&& a->minus == search_algebra.minus
			&& a->zero == search_algebra.zero) {
			algebra = a;
			break;
		}
	}
	if (!algebra) {
		algebra = malloc (sizeof (algebra_t));
		*algebra = search_algebra;
		DARRAY_APPEND (&algebras, algebra);
		algebra_init (algebra);
	}
	auto t = new_type ();
	t->meta = ty_algebra;
	t->type = ev_invalid;
	t->alignment = (dim > 1 ? 4 : 2) * type->alignment;
	t->algebra = algebra;
	t->property = algebra_property;
	algebra->algebra_type = t;
	return find_type (t);
}

const type_t *
algebra_subtype (const type_t *type, const attribute_t *attr)
{
	if (!is_algebra (type)) {
		internal_error (nullptr, "unexpected type");
	}
	auto algebra = algebra_get (type);
	if (strcmp (attr->name, "group_mask") == 0) {
		if (!attr->params || attr->params->list.head->next) {
			error (nullptr, "incorrect number of parameters to 'group_mask'");
			return type;
		}
		auto param = attr->params->list.head->expr;
		if (!is_integral_val (param)) {
			error (nullptr,
				   "'group_mask' parameter must be an integer constant");
			return type;
		}
		pr_uint_t mask = expr_integral (param);
		if (!mask || mask > ((1u << algebra->layout.count) - 1)) {
			error (nullptr, "invalid group_mask");
			return type;
		}
		return algebra_mvec_type (algebra, mask);
	}
	auto symtab = algebra->mvec_sym->type->symtab;
	auto field = symtab_lookup (symtab, attr->name);
	if (field) {
		return field->type;
	}
	error (nullptr, "invalid subtype %s", attr->name);
	return type;
}

static int
algebra_alignment (const type_t *type, int width)
{
	if (width > 4) {
		// don't need more than 4 units
		width = 4;
	}
	// alignment reflects the size for doubles vs floats
	return type->alignment * BITOP_RUP (width);
}

static symbol_t *
mvec_struct (algebra_t *algebra, pr_uint_t group_mask, type_t *type)
{
	int count = count_bits (group_mask);

	const char **mv_names = nullptr;
	if (algebra->dimension < 5) {
		mv_names = mvec_names[algebra->dimension];
	}
	struct_def_t fields[count + 1] = {};
	for (int i = 0, c = 0; i < algebra->layout.count; i++) {
		pr_uint_t   mask = 1 << i;
		if (group_mask & mask) {
			const char *name;
			if (mv_names) {
				name = mv_names[i];
			} else {
				name = va ("group_%d", i);
			}
			fields[c] = (struct_def_t) {
				.name = save_string (name),
				.type = type ? type : algebra_mvec_type (algebra, mask),
			};
			c++;
		};
	};
	return make_structure (nullptr, 's', fields, nullptr);
}

const type_t *
algebra_mvec_type (algebra_t *algebra, pr_uint_t group_mask)
{
	if (!group_mask || group_mask > ((1u << algebra->layout.count) - 1)) {
		internal_error (nullptr, "invalid group_mask");
	}
	if (!algebra->mvec_types[group_mask]) {
		int components = 0;
		for (int i = 0; i < algebra->layout.count; i++) {
			if (group_mask & (1 << i)) {
				components += algebra->layout.groups[i].count;
			}
		}
		symbol_t   *mvec_sym = nullptr;
		if (group_mask & (group_mask - 1)) {
			mvec_sym = mvec_struct (algebra, group_mask, nullptr);
		}
		multivector_t *mvec = malloc (sizeof (multivector_t));
		*mvec = (multivector_t) {
			.num_components = components,
			.group_mask = group_mask,
			.algebra = algebra,
			.mvec_sym = mvec_sym,
		};
		auto type = new_type ();
		algebra->mvec_types[group_mask] = type;
		*type = (type_t) {
			.type = algebra->type->type,
			.name = save_string (va ("algebra(%s(%d,%d,%d):%04x)",
									 algebra->type->name,
									 algebra->plus, algebra->minus,
									 algebra->zero, group_mask)),
			.alignment = algebra_alignment (algebra->type, components),
			.width = components,
			.meta = ty_algebra,
			.algebra = (algebra_t *) mvec,
			.freeable = true,
			.allocated = true,
			.property = algebra_property,
		};
		chain_type (type);
		if (!(group_mask & (group_mask - 1))) {
			mvec->mvec_sym = mvec_struct (algebra, group_mask, type);
		}
	}
	return algebra->mvec_types[group_mask];
}

static int pga_swaps_2d[8] = {
	[0x5] = 1,	// e20
};
static int pga_swaps_3d[16] = {
	[0x7] = 1,	// e021
	[0xa] = 1,	// e31
	[0xd] = 1,	// e032
};
static int vga_swaps_3d[8] = {
	[0x5] = 1,	// e31
};

int
algebra_count_flips (const algebra_t *alg, pr_uint_t a, pr_uint_t b)
{
	bool pga_2d = (alg->plus == 2 && alg->minus == 0 && alg->zero == 1);
	bool pga_3d = (alg->plus == 3 && alg->minus == 0 && alg->zero == 1);
	bool vga_3d = (alg->plus == 3 && alg->minus == 0 && alg->zero == 0);
	int swaps = count_flips (a, b);
	if (pga_2d) {
		swaps += pga_swaps_2d[a];
		swaps += pga_swaps_2d[b];
	}
	if (pga_3d) {
		swaps += pga_swaps_3d[a];
		swaps += pga_swaps_3d[b];
	}
	if (vga_3d) {
		swaps += vga_swaps_3d[a];
		swaps += vga_swaps_3d[b];
	}
	return swaps;
}

ex_value_t *
algebra_blade_value (algebra_t *alg, const char *name)
{
	uint32_t dimension = alg->plus + alg->minus + alg->zero;
	bool pga_2d = (alg->plus == 2 && alg->minus == 0 && alg->zero == 1);
	bool pga_3d = (alg->plus == 3 && alg->minus == 0 && alg->zero == 1);
	bool vga_3d = (alg->plus == 3 && alg->minus == 0 && alg->zero == 0);

	//FIXME supports only 0-9 (ie, up to 10d)
	if (name[0] == 'e' && isdigit(name[1])) {
		int ind = 1;
		while (name[ind] && isdigit ((byte) name[ind])) {
			ind++;
		}
		if (name[ind]) {
			// not a valid basis blade name
			return nullptr;
		}
		char indices[ind--];
		strcpy (indices, name + 1);
		int swaps = 0;
		uint32_t blade = 0;
		for (int i = 0; i < ind; i++) {
			uint32_t c = indices[i] - '0';
			c -= alg->zero != 1;
			if (c >= dimension) {
				error (nullptr, "basis %c not in algebra(%d,%d,%d)",
					   indices[i], alg->plus, alg->minus, alg->zero);
				continue;
			}
			uint32_t mask = 1u << c;
			if (blade & mask) {
				warning (nullptr, "duplicate index in basis blade");
			}
			swaps += count_flips (blade, mask);
			blade |= mask;
		}
		if (pga_2d) {
			swaps += pga_swaps_2d[blade];
		}
		if (pga_3d) {
			swaps += pga_swaps_3d[blade];
		}
		if (vga_3d) {
			swaps += vga_swaps_3d[blade];
		}
		int sign = 1 - 2 * (swaps & 1);
		auto g = alg->layout.group_map[alg->layout.mask_map[blade]];
		auto group = &alg->layout.groups[g[0]];
		auto group_type = alg->mvec_types[group->group_mask];
		ex_value_t *blade_val = nullptr;
		if (is_float (alg->type)) {
			float components[group->count] = {};
			components[g[1]] = sign;
			blade_val = new_type_value (group_type, (pr_type_t *)components);
		} else {
			double components[group->count] = {};
			components[g[1]] = sign;
			blade_val = new_type_value (group_type, (pr_type_t *)components);
		}
		return blade_val;
	}
	return nullptr;
}

static symbol_t *
algebra_symbol (const char *name, symtab_t *symtab)
{
	algebra_t *alg = symtab->procsymbol_data;
	symbol_t *sym = nullptr;

	auto blade_val = algebra_blade_value (alg, name);
	if (blade_val) {
		sym = new_symbol_type (name, blade_val->type);
		sym->sy_type = sy_const;
		sym->value = blade_val;
		symtab_addsymbol (symtab, sym);
	}
	return sym;
}

symtab_t *
algebra_scope (const type_t *type, symtab_t *curscope)
{
	auto scope = new_symtab (curscope, stab_local);
	scope->space = curscope->space;

	if (!is_algebra (type)) {
		error (nullptr, "algebra type required for algebra scope");
		return scope;
	}
	scope->procsymbol = algebra_symbol;
	scope->procsymbol_data = unalias_type (type)->algebra;
	return scope;
}

algebra_t *
algebra_get (const type_t *type)
{
	if (!is_algebra (type)) {
		return nullptr;
	}
	type = unalias_type (type);
	if (type->type == ev_invalid) {
		return type->algebra;
	} else {
		return type->multivec->algebra;
	}
}

etype_t
algebra_low_level_type (const type_t *type)
{
	type = unalias_type (type);

	if (type->type == ev_invalid) {
		internal_error (nullptr, "unexpected algebra type");
	}
	auto multivec = type->multivec;
	if (multivec->num_components > 4
		|| count_bits (multivec->group_mask) > 1) {
		return ev_void;
	}
	return type->type;
}

algebra_t *
algebra_context (const type_t *type)
{
	algebra_t *alg = nullptr;
	if (type && is_algebra (type)) {
		alg = algebra_get (type);
	}
	if (!alg) {
		symtab_t   *scope = current_symtab;
		while (scope && scope->procsymbol != algebra_symbol) {
			scope = scope->parent;
		}
		if (scope) {
			alg = scope->procsymbol_data;
		}
	}
	return alg;
}

void
algebra_print_type_str (dstring_t *str, const type_t *type)
{
	if (type->type == ev_invalid) {
		auto a = type->algebra;
		dasprintf (str, " algebra(%s(%d,%d,%d))", a->type->name,
				   a->plus, a->minus, a->zero);
	} else if (type->type == ev_float || type->type == ev_double) {
		auto m = type->multivec;
		auto a = m->algebra;
		dasprintf (str, " algebra(%s(%d,%d,%d):%04x)", a->type->name,
				   a->plus, a->minus, a->zero, m->group_mask);
	} else {
		internal_error (nullptr, "invalid algebra type");
	}
}

void
algebra_encode_type (dstring_t *encoding, const type_t *type)
{
	if (type->type == ev_invalid) {
		auto a = type->algebra;
		dasprintf (encoding, "{∧");
		encode_type (encoding, a->type);
		dasprintf (encoding, "(%d,%d,%d)}", a->plus, a->minus, a->zero);
	} else if (type->type == ev_float || type->type == ev_double) {
		auto m = type->multivec;
		auto a = m->algebra;
		dasprintf (encoding, "{∧");
		encode_type (encoding, a->type);
		dasprintf (encoding, "(%d,%d,%d):%04x}", a->plus, a->minus, a->zero,
				   m->group_mask);
	} else {
		internal_error (nullptr, "invalid algebra type");
	}
}

int
algebra_type_size (const type_t *type)
{
	if (type->type == ev_invalid) {
		auto a = type->algebra;
		return a->num_components * type_size (a->type);
	} else if (type->type == ev_float || type->type == ev_double) {
		auto m = type->multivec;
		int  size = 0;
		if (m->group_mask & (m->group_mask - 1)) {
			if (!m->mvec_sym) {
				internal_error (nullptr, "multi group multivec missing struct");
			}
			size = type_size (m->mvec_sym->type);
		} else {
			size = m->num_components * type_size (m->algebra->type);
		}
		return size;
	} else {
		internal_error (nullptr, "invalid algebra type");
	}
}

int
algebra_type_width (const type_t *type)
{
	if (type->type == ev_invalid) {
		return 0;
	} else if (type->type == ev_float || type->type == ev_double) {
		auto m = type->multivec;
		return m->num_components;
	} else {
		internal_error (nullptr, "invalid algebra type");
	}
}

bool
algebra_type_assignable (const type_t *dst, const type_t *src)
{
	if (src->meta == ty_algebra && src->type == ev_invalid) {
		// full algebra types cannot be assigned to anything but the same
		// full algebra type (type represents a full multivector, so the two
		// types are fundametally different), and cannot be assigned to
		// elements of even the same algebra (to get here, the two types
		// had to be different)
		return false;
	}
	if (!is_algebra (dst)) {
		// no implicit casts from multi-vectors to scalars
		return false;
	}
	if (dst->type == ev_invalid) {
		if (is_scalar (src)) {
			// scalars can always be assigned to a full algebra type (sets
			// the scalar element and zeros the other elements)
			return true;
		}
		if (src->meta != ty_algebra) {
			return false;
		}
		if (src->multivec->algebra != dst->algebra) {
			return false;
		}
		// the multivec is a member of the destination algebra
		return true;
	}
	if (!is_algebra (src)) {
		if (is_scalar (src)) {
			auto algebra = dst->multivec->algebra;
			auto layout = &algebra->layout;
			int group = layout->group_map[layout->mask_map[0]][0];
			if (dst->multivec->group_mask & (1u << group)) {
				// the source scalar is a member of the destination
				// multi-vector
				return true;
			}
		}
		return false;
	}
	if (dst->multivec->algebra != src->multivec->algebra) {
		return false;
	}
	if (src->multivec->group_mask & ~dst->multivec->group_mask) {
		return false;
	}
	// the source multi-vector is a subset of the destinatin multi-vector
	return true;
}

const type_t *
algebra_base_type (const type_t *type)
{
	if (type->type == ev_invalid) {
		return type->algebra->type;
	}
	return ev_types[type->type];
}

const type_t *
algebra_struct_type (const type_t *type)
{
	symbol_t   *sym = nullptr;

	if (type->type == ev_invalid) {
		sym = type->algebra->mvec_sym;
	} else {
		sym = type->multivec->mvec_sym;
	}
	return sym ? sym->type : nullptr;
}

int
algebra_blade_grade (basis_blade_t blade)
{
	return count_bits (blade.mask);
}

bool
is_mono_grade (const type_t *type)
{
	if (!is_algebra (type)) {
		return true;
	}
	if (type->type == ev_invalid) {
		return false;
	}
	auto alg = algebra_get (type);
	auto layout = &alg->layout;
	auto multivec = type->multivec;
	int  grade = -1;
	for (int i = 0; i < layout->count; i++) {
		pr_uint_t   mask = 1u << i;
		if (mask & multivec->group_mask) {
			auto group = &layout->groups[i];
			for (int j = 0; j < group->count; j++) {
				int blade_grade = algebra_blade_grade (group->blades[j]);
				if (grade == -1) {
					grade = blade_grade;
				}
				if (blade_grade != grade) {
					return false;
				}
			}
		}
	}
	return true;
}

int
algebra_get_grade (const type_t *type)
{
	if (!is_algebra (type)) {
		return 0;
	}
	if (type->type == ev_invalid) {
		return -1;
	}
	auto alg = algebra_get (type);
	auto layout = &alg->layout;
	auto multivec = type->multivec;
	for (int i = 0; i < layout->count; i++) {
		pr_uint_t   mask = 1u << i;
		if (mask & multivec->group_mask) {
			auto group = &layout->groups[i];
			return algebra_blade_grade (group->blades[0]);
		}
	}
	return 0;
}
