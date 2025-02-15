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

#include <ctype.h>
#include <strings.h>

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/heapsort.h"
#include "QF/va.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/image.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/spirv_grammar.h"
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

typedef struct layout_acc_s {
	const char *accessor;
	int         val;
} layout_acc_t;

typedef struct layout_qual_s layout_qual_t;
typedef struct layout_qual_s {
	const char *name;
	void      (*apply) (const layout_qual_t *qual, specifier_t spec,
						const expr_t *qual_name);
	void      (*apply_expr) (const layout_qual_t *qual, specifier_t spec,
							 const expr_t *qual_name, const expr_t *val);
	unsigned    obj_mask;
	glsl_var_t  var_type;
	unsigned    if_mask;
	const char **stage_filter;
	const layout_acc_t *accessors;
} layout_qual_t;

static void
glsl_layout_invalid_A (const layout_qual_t *qual, specifier_t spec,
					   const expr_t *qual_name)
{
	error (qual_name, "not allowed for vulkan");
}

static void
glsl_layout_invalid_E (const layout_qual_t *qual, specifier_t spec,
					   const expr_t *qual_name, const expr_t *val)
{
	error (qual_name, "not allowed for vulkan");
}

static void
set_attribute (attribute_t **attributes, const char *name, const expr_t *val)
{
	for (auto a = *attributes; a; a = a->next) {
		if (strcmp (a->name, name) == 0) {
			a->params = val;
			return;
		}
	}
	auto attr = new_attribute (name, val);
	attr->next = *attributes;
	*attributes = attr;
}

static void
glsl_layout_packing (const layout_qual_t *qual, specifier_t spec,
					 const expr_t *qual_name)
{
}

static void
glsl_layout_builtin (const layout_qual_t *qual, specifier_t spec,
					 const expr_t *qual_name, const expr_t *val)
{
	const char *name = "BuiltIn";
	set_attribute (&spec.sym->attributes, name, val);
}

static void
glsl_layout_location (const layout_qual_t *qual, specifier_t spec,
					  const expr_t *qual_name, const expr_t *val)
{
	const char *name = "Location";
	set_attribute (&spec.sym->attributes, name, val);
}

static void
glsl_layout_constant_id (const layout_qual_t *qual, specifier_t spec,
						 const expr_t *qual_name, const expr_t *val)
{
	if (spec.sym->sy_type == sy_const) {
		auto expr = new_value_expr (spec.sym->value, false);
		spec.sym->sy_type = sy_expr;
		spec.sym->expr = expr;
	}
	if (spec.sym->sy_type == sy_expr && spec.sym->expr->type == ex_value) {
		auto value = new_value ();
		*value = *spec.sym->expr->value;
		value->is_constexpr = true;
		spec.sym->expr = new_value_expr (value, false);
	} else {
		error (spec.sym->expr,
			   "specialization constant must be a scalar literal");
		return;
	}
	spec.sym->is_constexpr = true;
	const char *name = "SpecId";
	set_attribute (&spec.sym->attributes, name, val);
}

static void
glsl_layout_binding (const layout_qual_t *qual, specifier_t spec,
					 const expr_t *qual_name, const expr_t *val)
{
	const char *name = "Binding";
	set_attribute (&spec.sym->attributes, name, val);
}

static void
glsl_layout_offset (const layout_qual_t *qual, specifier_t spec,
					const expr_t *qual_name, const expr_t *val)
{
	spec.sym->offset = expr_integral (val);
}

static void
glsl_layout_set (const layout_qual_t *qual, specifier_t spec,
				 const expr_t *qual_name, const expr_t *val)
{
	const char *name = "DescriptorSet";
	set_attribute (&spec.sym->attributes, name, val);
}

static const type_t *
set_image_format (const type_t *type, const char *format)
{
	bool reference = false;
	unsigned tag = 0;
	if (is_reference (type)) {
		reference = true;
		tag = type->fldptr.tag;
		type = dereference_type (type);
	}
	bool array = false;
	int count = 0;
	if (is_array (type)) {
		array = true;
		count = type_count (type);
		type = dereference_type (type);
	}
	if (is_array (type)) {
		type = set_image_format (type, format);
	} else {
		if (!is_image (type) && !is_sampled_image (type)) {
			internal_error (0, "not an image type");
		}
		auto htype = type->handle.type;
		auto image = imageset.a[type->handle.extra];
		if (image.format == SpvImageFormatUnknown) {
			image.format = spirv_enum_val ("ImageFormat", format);
		} else {
			error (0, "format already set");
		}
		type = create_image_type (&image, htype);
	}
	if (array) {
		type = array_type (type, count);
	}
	if (reference) {
		type = tagged_reference_type (tag, type);
	}
	return type;
}

static void
glsl_layout_format (const layout_qual_t *qual, specifier_t spec,
					const expr_t *qual_name)
{
	auto type = spec.sym->type;
	type = set_image_format (type, qual->name);
	spec.sym->type = type;
}

static void
glsl_layout_push_constant (const layout_qual_t *qual, specifier_t spec,
		                   const expr_t *qual_name)
{
	if (spec.block) {
		spec.block->interface = glsl_push_constant;
	}
}

static void
glsl_layout_input_attachment_index (const layout_qual_t *qual, specifier_t spec,
									const expr_t *qual_name, const expr_t *val)
{
}

#define EP(n)  {.name = #n, .offset = offsetof (entrypoint_t, n)}
#define EPF(n) {.name = #n, .offset = offsetof (entrypoint_t, n), .flag = true}
#define EPV(n) {.name = #n, .offset = offsetof (entrypoint_t, n), .val = true}
static struct {
	const char *name;
	int         offset;
	bool        flag;
	bool        val;
} entrypoint_fields[] = {
	EP (invocations),
	EP (local_size[0]),
	EP (local_size[1]),
	EP (local_size[2]),
	EP (primitive_in),
	EP (primitive_out),
	EP (max_vertices),
	EP (spacing),
	EP (order),
	EP (frag_depth),
	EPF (point_mode),
	EPF (early_fragment_tests),
	EPV (gl_in_length),
	{}
};
#undef EP

static void
glsl_layout_exec_mode_param (const layout_qual_t *qual, specifier_t spec,
							 const expr_t *qual_name, const expr_t *val)
{
	auto entry_point = pr.module->entry_points;
	if (!entry_point) {
		internal_error (0, "no entry point");
	}
	for (auto acc = qual->accessors; acc->accessor; acc++) {
		bool found = false;
		for (auto ep = entrypoint_fields; ep->name; ep++) {
			if (strcmp (ep->name, acc->accessor) == 0) {
				auto val_ptr = ((byte *) entry_point + ep->offset);
				if (ep->flag) {
					auto flag = (bool *) val_ptr;
					*flag = true;
				} else if (ep->val) {
					auto expr = (const expr_t **) val_ptr;
					*expr = new_uint_expr (acc->val);
				} else {
					auto expr = (const expr_t **) val_ptr;
					*expr = val;
				}
				found = true;
				break;
			}
		}
		if (!found) {
			internal_error (0, "invalid accessor: %s", acc->accessor);
		}
	}
}

static void
glsl_layout_exec_mode (const layout_qual_t *qual, specifier_t spec,
					   const expr_t *qual_name)
{
	auto val = new_uint_expr (qual->accessors[0].val);
	glsl_layout_exec_mode_param (qual, spec, qual_name, val);
}

static void
set_array_size (const char *name, int count, const expr_t *qual_name)
{
	auto sym = symtab_lookup (pr.symtab, name);
	if (!sym || !sym->type) {
		internal_error (qual_name, "%s not found", name);
	}
	auto type = sym->type;
	bool reference = false;
	unsigned tag = 0;
	if (is_reference (type)) {
		reference = true;
		tag = type->fldptr.tag;
		type = dereference_type (type);
	}
	if (!is_array (type) || !type->array.type) {
		internal_error (qual_name, "%s not an array", name);
	}
	if (type->array.count) {
		error (qual_name, "%s size already specified", name);
		return;
	}
	type = array_type (type->array.type, count);
	if (reference) {
		type = tagged_reference_type (tag, type);
	}
	sym->type = type;
}

static void
glsl_layout_tess_vertices (const layout_qual_t *qual, specifier_t spec,
						   const expr_t *qual_name, const expr_t *val)
{
	glsl_layout_exec_mode_param (qual, spec, qual_name, val);

	auto entry_point = pr.module->entry_points;
	set_array_size ("gl_out", expr_integral (entry_point->max_vertices),
					qual_name);
}

static void
glsl_layout_geom_topo (const layout_qual_t *qual, specifier_t spec,
					   const expr_t *qual_name)
{
	glsl_layout_exec_mode (qual, spec, qual_name);

	auto entry_point = pr.module->entry_points;
	set_array_size ("gl_in", expr_integral (entry_point->gl_in_length),
					qual_name);
}

static void
glsl_layout_ignore (const layout_qual_t *qual, specifier_t spec,
					const expr_t *qual_name)
{
	const char *name = expr_string (qual_name);
	debug (qual_name, "ignoring %s", name);
}

#define A(a) a, nullptr
#define E(e) nullptr, e
#define D(d) (decl_##d)
#define D_all D(qual)|D(var)|D(block)|D(member)
#define V(v) (var_##v)
#define I(i) (1 << (glsl_##i))
#define C (const char *[])
#define ACC (const layout_acc_t [])

static bool sorted_layout_qualifiers;
static layout_qual_t layout_qualifiers[] = {
	{	.name = "builtin",
		.apply = E(glsl_layout_builtin),
		.obj_mask = D(var)|D(member),
		.var_type = V(any),
		.if_mask = I(in)|I(out),
	},

	{	.name = "shared",
		.apply = A(glsl_layout_invalid_A),
		.obj_mask = D(qual)|D(block),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "packed",
		.apply = A(glsl_layout_invalid_A),
		.obj_mask = D(qual)|D(block),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "std140",
		.apply = A(glsl_layout_packing),
		.obj_mask = D(qual)|D(block),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "std430",
		.apply = A(glsl_layout_packing),
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
		.apply = E(glsl_layout_binding),
		.obj_mask = D(var)|D(block),
		.var_type = V(opaque),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "offset",
		.apply = E(glsl_layout_offset),
		.obj_mask = D(member),
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
		.apply = E(glsl_layout_set),
		.obj_mask = D(var)|D(block),
		.var_type = V(opaque),
		.if_mask = I(uniform)|I(buffer),
	},
	{	.name = "push_constant",
		.apply = A(glsl_layout_push_constant),
		.obj_mask = D(block),
		.var_type = V(any),
		.if_mask = I(uniform)|I(push_constant),//FIXME push_constant redunant
	},
	{	.name = "input_attachment_index",
		.apply = E(glsl_layout_input_attachment_index),
		.obj_mask = D(var),
		.var_type = V(subpass),
		.if_mask = I(uniform),
	},
	{	.name = "location",
		.apply = E(glsl_layout_invalid_E),
		.obj_mask = D(var),
		.var_type = V(any),
		.if_mask = I(uniform)|I(buffer),
	},

	{	.name = "location",
		.apply = E(glsl_layout_location),
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
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_out", .val = SpvExecutionModeTriangles },
			{}
		},
	},
	{	.name = "quads",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_out", .val = SpvExecutionModeQuads },
			{}
		},
	},
	{	.name = "isolines",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_out", .val = SpvExecutionModeIsolines },
			{}
		},
	},
	{	.name = "equal_spacing",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "spacing", .val = SpvExecutionModeSpacingEqual },
			{}
		},
	},
	{	.name = "fractional_even_spacing",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "spacing",
				.val = SpvExecutionModeSpacingFractionalEven },
			{}
		},
	},
	{	.name = "fractional_odd_spacing",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "spacing",
				.val = SpvExecutionModeSpacingFractionalOdd },
			{}
		},
	},
	{	.name = "cw",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "order", .val = SpvExecutionModeVertexOrderCw },
			{}
		},
	},
	{	.name = "ccw",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "order", .val = SpvExecutionModeVertexOrderCcw },
			{}
		},
	},
	{	.name = "point_mode",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "tessellation evaluation", nullptr },
		.accessors = ACC {
			{ .accessor = "point_mode" },
			{}
		},
	},

	{	.name = "points",
		.apply = A(glsl_layout_geom_topo),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_in", .val = SpvExecutionModeInputPoints },
			{ .accessor = "gl_in_length", .val = 1 },
			{}
		},
	},
	{	.name = "lines",
		.apply = A(glsl_layout_geom_topo),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_in", .val = SpvExecutionModeInputLines },
			{ .accessor = "gl_in_length", .val = 2 },
			{}
		},
	},
	{	.name = "lines_adjacency",
		.apply = A(glsl_layout_geom_topo),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_in",
				.val = SpvExecutionModeInputLinesAdjacency },
			{ .accessor = "gl_in_length", .val = 4 },
			{}
		},
	},
	{	.name = "triangles",
		.apply = A(glsl_layout_geom_topo),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_in", .val = SpvExecutionModeTriangles },
			{ .accessor = "gl_in_length", .val = 3 },
			{}
		},
	},
	{	.name = "triangles_adjacency",
		.apply = A(glsl_layout_geom_topo),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_in",
				.val = SpvExecutionModeInputTrianglesAdjacency },
			{ .accessor = "gl_in_length", .val = 6 },
			{}
		},
	},
	{	.name = "invocations",
		.apply = E(glsl_layout_exec_mode_param),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "invocations" },
			{}
		},
	},

	{	.name = "origin_upper_left",
		.apply = A(glsl_layout_ignore),
		.obj_mask = D(qual),
		.var_type = V(gl_FragCoord),
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
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "fragment", nullptr },
		.accessors = ACC {
			{ .accessor = "early_fragment_tests" },
			{}
		},
	},

	{	.name = "local_size_x",
		.apply = E(glsl_layout_exec_mode_param),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "compute", nullptr },
		.accessors = ACC {
			{ .accessor = "local_size[0]" },
			{}
		},
	},
	{	.name = "local_size_y",
		.apply = E(glsl_layout_exec_mode_param),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "compute", nullptr },
		.accessors = ACC {
			{ .accessor = "local_size[1]" },
			{}
		},
	},
	{	.name = "local_size_z",
		.apply = E(glsl_layout_exec_mode_param),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "compute", nullptr },
		.accessors = ACC {
			{ .accessor = "local_size[2]" },
			{}
		},
	},
	{	.name = "local_size_x_id",
		.apply = E(glsl_layout_exec_mode_param),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "compute", nullptr },
		.accessors = ACC {
			{ .accessor = "local_size[0]" },
			{}
		},
	},
	{	.name = "local_size_y_id",
		.apply = E(glsl_layout_exec_mode_param),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "compute", nullptr },
		.accessors = ACC {
			{ .accessor = "local_size[1]" },
			{}
		},
	},
	{	.name = "local_size_z_id",
		.apply = E(glsl_layout_exec_mode_param),
		.obj_mask = D(qual),
		.if_mask = I(in),
		.stage_filter = C { "compute", nullptr },
		.accessors = ACC {
			{ .accessor = "local_size[2]" },
			{}
		},
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
		.apply = E(glsl_layout_tess_vertices),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "tessellation control", nullptr },
		.accessors = ACC {
			{ .accessor = "max_vertices" },
			{}
		},
	},

	{	.name = "points",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_out",
				.val = SpvExecutionModeOutputPoints },
			{}
		},
	},
	{	.name = "line_strip",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_out",
				.val = SpvExecutionModeOutputLineStrip },
			{}
		},
	},
	{	.name = "triangle_strip",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "primitive_out",
				.val = SpvExecutionModeOutputTriangleStrip },
			{}
		},
	},
	{	.name = "max_vertices",
		.apply = E(glsl_layout_exec_mode_param),
		.obj_mask = D(qual),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
		.accessors = ACC {
			{ .accessor = "max_vertices" },
			{}
		},
	},
	{	.name = "stream",
		.apply = E(nullptr),
		.obj_mask = D_all,
		.var_type = V(any),
		.if_mask = I(out),
		.stage_filter = C { "geometry", nullptr },
	},

	{	.name = "depth_any",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(var),
		.var_type = V(gl_FragDepth),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
		.accessors = ACC {
			{ .accessor = "frag_depth", .val = SpvExecutionModeDepthReplacing },
			{}
		},
	},
	{	.name = "depth_greater",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(var),
		.var_type = V(gl_FragDepth),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
		.accessors = ACC {
			{ .accessor = "frag_depth", .val = SpvExecutionModeDepthGreater },
			{}
		},
	},
	{	.name = "depth_less",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(var),
		.var_type = V(gl_FragDepth),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
		.accessors = ACC {
			{ .accessor = "frag_depth", .val = SpvExecutionModeDepthLess },
			{}
		},
	},
	{	.name = "depth_unchanged",
		.apply = A(glsl_layout_exec_mode),
		.obj_mask = D(var),
		.var_type = V(gl_FragDepth),
		.if_mask = I(out),
		.stage_filter = C { "fragment", nullptr },
		.accessors = ACC {
			{ .accessor = "frag_depth", .val = SpvExecutionModeDepthUnchanged },
			{}
		},
	},

	{	.name = "constant_id",
		.apply = E(glsl_layout_constant_id),
		.obj_mask = D(var),
		.var_type = V(scalar),
	},

	{	.name = "rgba32f",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16f",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg32f",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16f",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r11f_g11f_b10f",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r32f",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16f",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgb10_a2",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba8",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg8",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r8",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16_snorm",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba8_snorm",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16_snorm",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg8_snorm",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16_snorm",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r8_snorm",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba32i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba8i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg32i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg8i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r32i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r8i",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba32ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba16ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgb10_a2ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rgba8ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg32ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg16ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "rg8ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r32ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r16ui",
		.apply = A(glsl_layout_format),
		.obj_mask = D(var),
		.var_type = V(image),
		.if_mask = I(uniform),
	},
	{	.name = "r8ui",
		.apply = A(glsl_layout_format),
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

static unsigned
get_interface_mask (int storage)
{
	unsigned if_mask = 0;

	auto interface = glsl_iftype_from_sc (storage);
	if (interface < glsl_num_interfaces) {
		if_mask = 1 << interface;
	}
	return if_mask;
}

static bool __attribute__((pure))
layout_check_qualifier (const layout_qual_t *qual, specifier_t spec)
{
	unsigned obj_mask = 0;
	glsl_var_t var_type = var_any;
	unsigned if_mask = get_interface_mask (spec.storage);
	if (spec.sym) {
		auto sym = spec.sym;
		if (sym->type) {
			auto type = sym->type;
			if (is_reference (type)) {
				type = dereference_type (type);
			}
			if (is_array (type)) {
				type = dereference_type (type);
			}
			//FIXME is_handle works only for glsl as there are no user handles
			if (is_handle (type)) {
				var_type = var_opaque;
				// images are opaque types, but certain qualifiers support
				// only images and not other opaque types, but qualifiers that
				// support opaque types in general also support images
				image_t *image = nullptr;
				if (is_image (type)) {
					image = &imageset.a[type->handle.extra];
				}
				if (qual->var_type != var_opaque && image) {
					if (image->dim == img_subpassdata) {
						var_type = var_subpass;
					} else {
						var_type = var_image;
					}
				}
			} else if (spec.is_const && is_scalar (type)) {
				var_type = var_scalar;
			} else if (strcmp (sym->name, "gl_FragCoord") == 0) {
				var_type = var_gl_FragCoord;
			} else if (strcmp (sym->name, "gl_FragDepth") == 0) {
				var_type = var_gl_FragDepth;
			}
			obj_mask = decl_var;
			if (sym->table->type == stab_block) {
				obj_mask = decl_member;
				if_mask = get_interface_mask (sym->table->storage);
			} else if (is_struct (type)
					   && type_symtab (type)->type == stab_block) {
				obj_mask = decl_block;
			}
		} else {
			obj_mask = decl_block;
		}
	} else {
		obj_mask = decl_qual;
	}
	if (!(qual->obj_mask & obj_mask)) {
		return false;
	}
	if (obj_mask == decl_var
		&& qual->var_type != var_any
		&& qual->var_type != var_type) {
		return false;
	}
	if (qual->if_mask != if_mask && !(qual->if_mask & if_mask)) {
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
					qual->apply_expr (qual, spec, qualifier->expr.e1, val);
				}
				return;
			} else if (qual->apply) {
				if (val) {
					error (qualifier, "%s does not take a value", key.name);
				} else {
					qual->apply (qual, spec, qualifier->expr.e1);
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
