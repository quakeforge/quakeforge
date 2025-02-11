/*
	image.c

	Ruamoko image support code

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/image.h"
#include "tools/qfcc/include/spirv_grammar.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

imageset_t imageset = DARRAY_STATIC_INIT (16);

static struct_def_t image_struct[] = {
	{"type",        &type_ptr},
	{"dim",         &type_uint},	//FIXME enum
	{"depth",       &type_uint},	//FIXME enum
	{"arrayed",     &type_bool},
	{"multisample", &type_bool},
	{"sampled",     &type_uint},	//FIXME enum
	{"format",      &type_uint},	//FIXME enum
	{}
};

static struct_def_t sampled_image_struct[] = {
	{"image_type",  &type_ptr},
	{}
};

static int dim_widths[7] = { 1, 2, 3, 3, 2, 1, 0 };
static int size_widths[7] = { 1, 2, 3, 2, 2, 1, 0 };
static int shadow_widths[7] = { 3, 3, 0, 4, 3, 0, 0 };
static const char *shadow_swizzle[7][2] = {
	{ "x", "xy" },
	{ "xy", "xyz" },
	{},
	{ "xyz", "xyzw" },
	{ "xy", "xyz" },
	{},
	{},
};
static const char *shadow_comp_swizzle[7][2] = {
	{ "z", "z" },	// glsl braindeadery for 1d (non-arrayed) images
	{ "z", "w" },
	{},
	{ "w", "" },	// cubemap array shadows get comp from a param
	{ "z", "w" },
	{},
	{},
};

static const expr_t *
image_property (const type_t *type, const attribute_t *property)
{
	auto image = &imageset.a[type->handle.extra];

	if (image->dim > img_subpassdata) {
		internal_error (0, "image has bogus dimension");
	}

	if (strcmp (property->name, "sample_type") == 0) {
		return new_type_expr (image->sample_type);
	} else if (strcmp (property->name, "image_coord") == 0) {
		int width = dim_widths[image->dim];
		if (image->dim == img_subpassdata) {
			width = 2;
		}
		if (!width) {
			return new_type_expr (&type_void);
		}
		if (image->dim < img_3d) {
			width += image->arrayed;
		}
		return new_type_expr (vector_type (&type_int, width));
	} else if (strcmp (property->name, "size_type") == 0) {
		int width = size_widths[image->dim];
		if (!width) {
			return new_type_expr (&type_void);
		}
		if (width < 3 && image->dim <= img_cube) {
			width += image->arrayed;
		}
		return new_type_expr (vector_type (&type_int, width));
	}
	return error (0, "no property %s on %s", property->name, type->name + 4);
}

static const expr_t *
sampled_shadow_swizzle (const attribute_t *property, const char *swizzle[7][2],
						image_t *image)
{
	int count = list_count (&property->params->list);
	if (count != 1) {
		return error (property->params, "wrong number of params");
	}
	const expr_t *params[count];
	list_scatter (&property->params->list, params);
	const char *swiz = swizzle[image->dim][image->arrayed];
	if (!swiz) {
		return error (property->params, "image does not support"
					  " shadow sampling");
	}
	if (!swiz[0]) {
		// cube map array
		return error (property->params, "cube map array shadow compare is not"
					  " in the coordinate vector");
	}
	if (strcmp (swiz, "xyzw") == 0) {
		// no-op swizzle
		return params[0];
	}
	if (!swiz[1]) {
		auto ptype = get_type (params[0]);
		auto member = new_name_expr (swiz);
		auto field = get_struct_field (ptype, params[0], member);
		if (!field) {
			return error (params[0], "invalid shadow coord");
		}
		member = new_symbol_expr (field);
		auto expr = new_field_expr (params[0], member);
		expr->field.type = member->symbol->type;
		return expr;
	}
	return new_swizzle_expr (params[0], swiz);
}

static const expr_t *
sampled_image_property (const type_t *type, const attribute_t *property)
{
	auto image = &imageset.a[type->handle.extra];

	if (image->dim > img_subpassdata) {
		internal_error (0, "image has bogus dimension");
	}

	if (strcmp (property->name, "tex_coord") == 0) {
		int width = dim_widths[image->dim];
		if (!width) {
			return new_type_expr (&type_void);
		}
		if (image->dim < img_3d) {
			width += image->arrayed;
		}
		return new_type_expr (vector_type (&type_float, width));
	} else if (strcmp (property->name, "shadow_coord") == 0) {
		if (property->params) {
			return sampled_shadow_swizzle (property, shadow_swizzle, image);
		} else {
			int width = shadow_widths[image->dim];
			if (!image->depth || !width) {
				return new_type_expr (&type_void);
			}
			if (image->dim == img_2d) {
				width += image->arrayed;
			}
			return new_type_expr (vector_type (&type_float, width));
		}
	} else if (strcmp (property->name, "comp") == 0) {
		if (property->params) {
			return sampled_shadow_swizzle (property, shadow_comp_swizzle,
										   image);
		} else {
			int width = shadow_widths[image->dim];
			if (!image->depth || !width) {
				return new_type_expr (&type_void);
			}
			if (image->dim == img_2d) {
				width += image->arrayed;
			}
			return new_type_expr (vector_type (&type_float, width));
		}
	}
	return image_property (type, property);
}

type_t type_image = {
	.type = ev_invalid,
	.meta = ty_struct,
	.property = image_property,
};
type_t type_sampled_image = {
	.type = ev_invalid,
	.meta = ty_struct,
	.property = sampled_image_property,
};
type_t type_sampler = {
	.type = ev_int,
	.meta = ty_handle,
};

void
image_init_types (void)
{
	make_structure ("@image", 's', image_struct, &type_image);
	make_structure ("@sampled_image", 's', sampled_image_struct,
					&type_sampled_image);
	make_handle ("@sampler", &type_sampler);
	chain_type (&type_image);
	chain_type (&type_sampler);
	chain_type (&type_sampled_image);

	DARRAY_RESIZE (&imageset, 0);
}

static const expr_t *
image_handle_property (const type_t *type, const attribute_t *attr)
{
	return type->handle.type->property (type, attr);
}

symbol_t *
named_image_type (image_t *image, const type_t *htype, const char *name)
{
	unsigned index = 0;
	// slot 0 is never used
	for (unsigned i = 1; i < imageset.size; i++) {
		if (memcmp (&imageset.a[i], image, sizeof (*image)) == 0) {
			index = i;
			break;
		}
	}
	if (!index) {
		if (!imageset.size) {
			DARRAY_APPEND (&imageset, (image_t) { } );
		}
		index = imageset.size;
		DARRAY_APPEND (&imageset, *image);
	}

	auto sym = new_symbol (name);
	sym = find_handle (sym, &type_int);
	//FIXME the type isn't chained yet and so doesn't need to be const, but
	// symbols keep the type in a const pointer.
	auto t = (type_t *) sym->type;
	if (t->handle.extra) {
		internal_error (0, "image type handle already set");
	}
	t->handle.type = htype;
	t->handle.extra = index;
	t->property = image_handle_property;
	sym->type = find_type (sym->type);

	return sym;
}

const type_t *
image_type (const type_t *type, const expr_t *params)
{
	if (!type || !(is_float (type) || is_int (type) || is_uint (type))
		|| !is_scalar (type)) {
		error (params, "invalid type for @image");
		return &type_int;
	}
	if (is_error (params)) {
		return &type_int;
	}
	if (params->type != ex_list) {
		internal_error (params, "not a list");
	}
	image_t     image = {
		.sample_type = type,
		.dim = ~0u,
		.format = ~0u,
	};
	int count = list_count (&params->list);
	const expr_t *args[count + 1] = {};
	list_scatter (&params->list, args);
	bool err = false;
	for (int i = 0; i < count; i++) {
		uint32_t val;
		if (args[i]->type != ex_symbol) {
			error (args[i], "invalid argument for @image");
			err = true;
			continue;
		}
		const char *name = args[i]->symbol->name;
		if (spirv_enum_val_silent ("Dim", name, &val)) {
			if (image.dim != ~0u) {
				error (args[i], "multiple spec for image dimension");
				err = true;
			}
			image.dim = val;
			continue;
		}
		if (spirv_enum_val_silent ("ImageFormat", name, &val)) {
			if (image.format != ~0u) {
				error (args[i], "multiple spec for image format");
				err = true;
			}
			image.format = val;
			continue;
		}
		if (strcasecmp ("Depth", name) == 0) {
			if (image.depth) {
				error (args[i], "multiple spec for image depth");
				err = true;
			}
			image.depth = 1;
			continue;
		}
		if (strcasecmp ("Array", name) == 0
			|| strcasecmp ("Arrayed", name) == 0) {
			if (image.arrayed) {
				error (args[i], "multiple spec for image array");
				err = true;
			}
			image.arrayed = true;
			continue;
		}
		if (strcasecmp ("MS", name) == 0) {
			if (image.multisample) {
				error (args[i], "multiple spec for image multisample");
				err = true;
			}
			image.multisample = true;
			continue;
		}
		if (strcasecmp ("Sampled", name) == 0) {
			if (image.sampled) {
				error (args[i], "multiple spec for image sampling");
				err = true;
			}
			image.sampled = 1;
			continue;
		}
		if (strcasecmp ("Storage", name) == 0) {
			if (image.sampled) {
				error (args[i], "multiple spec for image sampling");
				err = true;
			}
			image.sampled = 2;
			continue;
		}
		error (args[i], "invalid argument for @image: %s", name);
		err = true;
	}
	if (image.dim == img_subpassdata) {
		if (image.sampled) {
			error (0, "multiple spec for image sampling");
			err = true;
		}
		if (image.format && image.format != ~0u) {
			error (0, "multiple spec for image format");
			err = true;
		}
		image.format = SpvImageFormatUnknown;
		image.sampled = 2;
	}
	if (image.dim == ~0u) {
		error (0, "image dimenion not specified");
		err = true;
	}
	if (image.format == ~0u) {
		image.format = SpvImageFormatUnknown;
	}
	if (err) {
		return &type_int;
	}
	auto sym = named_image_type (&image, &type_image, nullptr);
	return sym->type;
}
