/*
	glsl-layout.c

	GLSL layout attribute handling

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

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

#include <strings.h>

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/heapsort.h"
#include "QF/va.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

typedef enum {
	decl_var    = 1 << 0,
	decl_qual   = 1 << 1,
	decl_block  = 1 << 2,
	decl_member = 1 << 3,
} glsl_obj_t;

typedef enum {
	var_any,
	var_opaque,
	var_atomic,
	var_subpass,
	var_scalar,
	var_image,
	var_gl_FragCoord,
	var_gl_FragDepth,
} glsl_var_t;

typedef struct layout_qual_s {
	const char *name;
	bool      (*apply) ();
	bool      (*apply_expr) ();
	unsigned    obj_mask;
	glsl_var_t  var_type;
	unsigned    if_mask;
	const char **stage_filter;
} layout_qual_t;

#define A(a) a, nullptr
#define E(e) nullptr, e
#define D(d) (decl_##d)
#define D_all D(qual)|D(var)|D(block)|D(member)
#define V(v) (var_##v)
#define I(i) (1 << (glsl_##i))
#define C (const char *[])

static bool sorted_layout_qualifiers;
static layout_qual_t layout_qualifiers[] = {
	{	.name = "shared",
		.apply = A(nullptr),
		.obj_mask = D(qual)|D(block),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "packed",
		.apply = A(nullptr),
		.obj_mask = D(qual)|D(block),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "std140",
		.apply = A(nullptr),
		.obj_mask = D(qual)|D(block),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "std430",
		.apply = A(nullptr),
		.obj_mask = D(qual)|D(block),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "row_major",
		.apply = A(nullptr),
		.obj_mask = D(qual)|D(block)|D(member),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "column_major",
		.apply = A(nullptr),
		.obj_mask = D(qual)|D(block)|D(member),
		.if_mask = I(uniform)|I(buffer),
	},

	{	.name = "binding",
		.apply = E(nullptr),
		.obj_mask = D(var)|D(block),
		.var_type = V(opaque),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "offset",
		.apply = E(nullptr),
		.obj_mask = D(var)|D(member),
		.var_type = V(opaque),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "align",
		.apply = E(nullptr),
		.obj_mask = D(var)|D(block),
		.var_type = V(opaque),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "set",
		.apply = E(nullptr),
		.obj_mask = D(var)|D(block),
		.var_type = V(opaque),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "push_constant",
		.apply = A(nullptr),
		.obj_mask = D(block),
		.var_type = V(any),
		.if_mask = I(uniform),
	},
	{	.name = "input_attachment_index",
		.apply = E(nullptr),
		.obj_mask = D(var),
		.var_type = V(subpass),
		.if_mask = I(uniform),
	},
	{	.name = "location",
		.apply = E(nullptr),
		.obj_mask = D(var),
		.var_type = V(any),
		.if_mask = I(uniform)|I(buffer),
	},

	{	.name = "location",
		.apply = E(nullptr),
		.obj_mask = D(var)|D(block)|D(member),
		.var_type = V(any),
		.if_mask = I(in)|I(out),
		.stage_filter = C { "!compute", nullptr },
	},
	{	.name = "component",
		.apply = E(nullptr),
		.obj_mask = D(var)|D(member),
		.var_type = V(any),
		.if_mask = I(in)|I(out),
		.stage_filter = C { "!compute", nullptr },
	},

	{	.name = "index",
		.apply = E(nullptr),
		.obj_mask = D(var),
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
	},

	{	.name = "triangles",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},
	{	.name = "quads",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},
	{	.name = "isolines",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},
	{	.name = "equal_spacing",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},
	{	.name = "fractional_even_spacing",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},
	{	.name = "fractional_odd_spacing",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},
	{	.name = "cw",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},
	{	.name = "ccw",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},
	{	.name = "point_mode",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
	},

	{	.name = "points",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in)|I(out),
		.stage_filter = C { "geometry", nullptr },
	},
//? [ points ]					// q _ _ _ geom in
	{	.name = "lines",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
	},
	{	.name = "lines_adjacency",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
	},
	{	.name = "triangles",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
	},
	{	.name = "triangles_adjacency",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
	},
	{	.name = "invocations",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
	},

	{	.name = "origin_upper_left",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "fragment", nullptr },
	},
	{	.name = "pixel_center_integer",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "fragment", nullptr },
	},
	{	.name = "early_fragment_tests",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "fragment", nullptr },
	},

	{	.name = "local_size_x",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "comp", nullptr },
	},
	{	.name = "local_size_y",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "comp", nullptr },
	},
	{	.name = "local_size_z",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "comp", nullptr },
	},
	{	.name = "local_size_x_id",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "comp", nullptr },
	},
	{	.name = "local_size_y_id",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "comp", nullptr },
	},
	{	.name = "local_size_z_id",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "comp", nullptr },
	},

	{	.name = "xfb_buffer",
		.apply = E(nullptr),
		.obj_mask = D_all,
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C {
			"vertex",
			"tessellation control",
			"tessellation evaluation",
			"geometry",
			nullptr
		},
	},
	{	.name = "xfb_stride",
		.apply = E(nullptr),
		.obj_mask = D_all,
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C {
			"vertex",
			"tessellation control",
			"tessellation evaluation",
			"geometry",
			nullptr
		},
	},
	{	.name = "xfb_offset",
		.apply = E(nullptr),
		.obj_mask = D_all,
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C {
			"vertex",
			"tessellation control",
			"tessellation evaluation",
			"geometry",
			nullptr
		},
	},

	{	.name = "vertices",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "tessellation control", nullptr },
	},

//? [ points ]					// q _ _ _ geom out
	{	.name = "line_strip",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
	},
	{	.name = "triangle_strip",
		.apply = A(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
	},
	{	.name = "max_vertices",
		.apply = E(nullptr),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
	},
	{	.name = "stream",
		.apply = E(nullptr),
		.obj_mask = D_all,
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
	},

	{	.name = "depth_any",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
	},
	{	.name = "depth_greater",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
	},
	{	.name = "depth_less",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
	},
	{	.name = "depth_unchanged",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
	},

	{	.name = "constant_id",
		.apply = E(nullptr),
		.obj_mask = D(var),
		.var_type = V(scalar),
	},

	{	.name = "rgba32f",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16f",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg32f",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16f",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r11f_g11f_b10f",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r32f",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16f",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgb10_a2",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba8",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg8",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r8",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16_snorm",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba8_snorm",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16_snorm",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg8_snorm",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16_snorm",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r8_snorm",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba32i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba8i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg32i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg8i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r32i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r8i",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba32ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgb10_a2ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba8ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg32ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg8ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r32ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r8ui",
		.apply = A(nullptr),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
};
#undef A
#undef E
#undef D
#undef D_all
#undef V
#undef I
#undef C
#define num_quals (sizeof (layout_qualifiers) / sizeof (layout_qualifiers[0]))
#define layout_qual_sz num_quals, sizeof (layout_qual_t)

static int
layout_qual_cmp (const void *_a, const void *_b)
{
	auto a = (const layout_qual_t *) _a;
	auto b = (const layout_qual_t *) _b;
	return strcasecmp (a->name, b->name);
}

static bool
layout_check_qualifier (const layout_qual_t *qual, specifier_t spec)
{
	unsigned obj_mask = 0;
	glsl_var_t var_type = var_any;
	unsigned if_mask = 0;

	auto interface = glsl_iftype_from_sc (spec.storage);
	if (interface < glsl_num_interfaces) {
		if_mask = 1 << interface;
	}
	if (spec.sym) {
		auto sym = spec.sym;
		if (is_handle (sym->type)) {
			var_type = var_opaque;
		} else if (is_scalar (sym->type)) {
			var_type = var_scalar;
		} else if (strcmp (sym->name, "gl_FragCoord") == 0) {
			var_type = var_gl_FragCoord;
		} else if (strcmp (sym->name, "gl_FragDepth") == 0) {
			var_type = var_gl_FragDepth;
		}
		obj_mask = decl_var;
		if (sym->table->parent && sym->table->parent->type == stab_struct) {
			obj_mask = decl_member;
		}
	} else {
		obj_mask = decl_qual;
	}
	if (!(qual->obj_mask & obj_mask)) {
		return false;
	}
	if (qual->var_type != var_type) {
		return false;
	}
	if (!(qual->if_mask & if_mask)) {
		return false;
	}
	if (qual->stage_filter) {
		bool ok = false;
		for (auto sf = qual->stage_filter; *sf; sf++) {
			if (**sf == '!') {
				if (strcmp (*sf + 1, glsl_sublang.name) == 0) {
					return false;
				}
				ok = true;
			} else {
				if (strcmp (*sf, glsl_sublang.name) == 0) {
					return true;
				}
			}
		}
		return ok;
	}
	return true;
}

static void
layout_apply_qualifier (const expr_t *qualifier, specifier_t spec)
{
	layout_qual_t key = {
		.name = qualifier->type == ex_expr
				? expr_string (qualifier->expr.e1)
				: expr_string (qualifier)
	};
	auto val = qualifier->type == ex_expr
				? qualifier->expr.e2
				: nullptr;

	notice (qualifier, "%s %p", key.name, spec.sym);
	const layout_qual_t *qual;
	qual = bsearch (&key, layout_qualifiers, layout_qual_sz, layout_qual_cmp);
	if (!qual) {
		error (0, "invalid layout qualifier: %s", key.name);
		return;
	}
	// there may be multiple entries with the same qualifier name, so find
	// the first one
	while (qual > layout_qualifiers && layout_qual_cmp (&key, qual - 1) == 0) {
		qual--;
	}
	// check all matching entries
	while (qual - layout_qualifiers < (ptrdiff_t) num_quals
		   && layout_qual_cmp (&key, qual) == 0) {
		if (layout_check_qualifier (qual, spec)) {
			if (qual->apply_expr) {
				if (!val) {
					error (qualifier, "%s requires a value", key.name);
				} else {
					qual->apply_expr ();
				}
				return;
			} else if (qual->apply) {
				if (!val) {
					error (qualifier, "%s does not take a value", key.name);
				} else {
					qual->apply ();
				}
				return;
			} else {
				error (0, "unsupported layout qualifier: %s", key.name);
				return;
			}
		}

		qual++;
	}
	error (0, "invalid layout qualifier: %s", key.name);
	return;
}

void
glsl_layout (const ex_list_t *qualifiers, specifier_t spec)
{
	if (!sorted_layout_qualifiers) {
		heapsort (layout_qualifiers, layout_qual_sz, layout_qual_cmp);
	}
	for (auto q = qualifiers->head; q; q = q->next) {
		layout_apply_qualifier (q->expr, spec);
	}
}
