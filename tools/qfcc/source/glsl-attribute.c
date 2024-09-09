/*
	glsl-declaration.c

	GLSL declaration handling

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

#include "QF/alloc.h"
#include "QF/hash.h"
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

#if 0
typedef struct layout_qual_s {
	const char *name;
	bool      (*valid) (symbol_t *sym, glsl_interface_t interface);
	bool      (*apply) ();
	bool      (*apply_expr) ();
} layout_qual_t;

// qual var block member iface
static layout_qual_t layout_qualifiers[] = {
	{"shared",				/* q _ b _ uniform/buffer */, ___, nullptr},
	{"packed",				/* q _ b _ uniform/buffer */, ___, nullptr},
	{"std140",				/* q _ b _ uniform/buffer */, ___, nullptr},
	{"std430",				/* q _ b _ uniform/buffer */, ___, nullptr},
	{"row_major",			/* q _ b m uniform/buffer */, ___, nullptr},
	{"column_major",		/* q _ b m uniform/buffer */, ___, nullptr},

	{"binding",				/* _ o b _ uniform/buffer */, nullptr, ___},
	{"offset",				/* _ a _ m uniform/buffer */, nullptr, ___},
	{"align",				/* _ o b _ uniform/buffer */, nullptr, ___},
	{"set",					/* _ o b _ uniform/buffer */, nullptr, ___},
	{"push_constant",		/* _ _ b _ uniform(vulkan) */, ___, nullptr},
	{"input_attachment_index", /* _ s _ _ uniform(vulkan) */, nullptr, ___},
	{"location",			/* _ v _ _ uniform/buffer/subroutine */, nullptr, ___},

	{"location",			/* _ v b m all in/out except compute */, nullptr, ___},
	{"component",			/* _ v _ m all in/out except compute */, nullptr, ___},

	{"index",				/* _ v _ _ fragment out/subroutine */, nullptr, ___},

	{"triangles",			/* q _ _ _ tese in */, ___, nullptr},
	{"quads",				/* q _ _ _ tese in */, ___, nullptr},
	{"isolines",			/* q _ _ _ tese in */, ___, nullptr},
	{"equal_spacing",		/* q _ _ _ tese in */, ___, nullptr},
	{"fractional_even_spacing", /* q _ _ _ tese in */, ___, nullptr},
	{"fractional_odd_spacing", /* q _ _ _ tese in */, ___, nullptr},
	{"cw",					/* q _ _ _ tese in */, ___, nullptr},
	{"ccw",					/* q _ _ _ tese in */, ___, nullptr},
	{"point_mode",			/* q _ _ _ tese in */, ___, nullptr},

	{"points",				/* q _ _ _ geom in/out */, ___, nullptr},
//? [ points ]				// q _ _ _ geom in
	{"lines",				/* q _ _ _ geom in */, ___, nullptr},
	{"lines_adjacency",		/* q _ _ _ geom in */, ___, nullptr},
	{"triangles",			/* q _ _ _ geom in */, ___, nullptr},
	{"triangles_adjacency",	/* q _ _ _ geom in */, ___, nullptr},
	{"invocations",			/* q _ _ _ geom in */, nullptr, ___},

	{"origin_upper_left",	/* _ f _ _ frag in */, ___, nullptr},
	{"pixel_center_integer", /* _ f _ _ frag in */, ___, nullptr},
	{"early_fragment_tests", /* q _ _ _ frag in */, ___, nullptr},

	{"local_size_x",		/* q _ _ _ comp in */, nullptr, ___},
	{"local_size_y",		/* q _ _ _ comp in */, nullptr, ___},
	{"local_size_z",		/* q _ _ _ comp in */, nullptr, ___},
	{"local_size_x_id",		/* q _ _ _ comp in */, nullptr, ___},
	{"local_size_y_id",		/* q _ _ _ comp in */, nullptr, ___},
	{"local_size_z_id",		/* q _ _ _ comp in */, nullptr, ___},

	{"xfb_buffer",			/* q v b m vert/tess/geom out */, nullptr, ___},
	{"xfb_stride",			/* q v b m vert/tess/geom out */, nullptr, ___},
	{"xfb_offset",			/* _ v b m vert/tess/geom out */, nullptr, ___},

	{"vertices",			/* q _ _ _ tesc out */, nullptr, ___},

//? [ points ]				// q _ _ _ geom out
	{"line_strip",			/* q _ _ _ geom out */, ___, nullptr},
	{"triangle_strip",		/* q _ _ _ geom out */, ___, nullptr},
	{"max_vertices",		/* q _ _ _ geom out */, nullptr, ___},
	{"stream",				/* q v b m geom out */, nullptr, ___},

	{"depth_any",			/* _ v _ _ frag out */, ___, nullptr},
	{"depth_greater",		/* _ v _ _ frag out */, ___, nullptr},
	{"depth_less",			/* _ v _ _ frag out */, ___, nullptr},
	{"depth_unchanged",		/* _ v _ _ frag out */, ___, nullptr},

	{"constant_id",			/* _ s _ _ const */, nullptr, ___},

	{"rgba32f",				/* _ i _ _ uniform */, ___, nullptr},
	{"rgba16f",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg32f",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg16f",				/* _ i _ _ uniform */, ___, nullptr},
	{"r11f_g11f_b10f",		/* _ i _ _ uniform */, ___, nullptr},
	{"r32f",				/* _ i _ _ uniform */, ___, nullptr},
	{"r16f",				/* _ i _ _ uniform */, ___, nullptr},
	{"rgba16",				/* _ i _ _ uniform */, ___, nullptr},
	{"rgb10_a2",			/* _ i _ _ uniform */, ___, nullptr},
	{"rgba8",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg16",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg8",					/* _ i _ _ uniform */, ___, nullptr},
	{"r16",					/* _ i _ _ uniform */, ___, nullptr},
	{"r8",					/* _ i _ _ uniform */, ___, nullptr},
	{"rgba16_snorm",		/* _ i _ _ uniform */, ___, nullptr},
	{"rgba8_snorm",			/* _ i _ _ uniform */, ___, nullptr},
	{"rg16_snorm",			/* _ i _ _ uniform */, ___, nullptr},
	{"rg8_snorm",			/* _ i _ _ uniform */, ___, nullptr},
	{"r16_snorm",			/* _ i _ _ uniform */, ___, nullptr},
	{"r8_snorm",			/* _ i _ _ uniform */, ___, nullptr},
	{"rgba32i",				/* _ i _ _ uniform */, ___, nullptr},
	{"rgba16i",				/* _ i _ _ uniform */, ___, nullptr},
	{"rgba8i",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg32i",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg16i",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg8i",				/* _ i _ _ uniform */, ___, nullptr},
	{"r32i",				/* _ i _ _ uniform */, ___, nullptr},
	{"r16i",				/* _ i _ _ uniform */, ___, nullptr},
	{"r8i",					/* _ i _ _ uniform */, ___, nullptr},
	{"rgba32ui",			/* _ i _ _ uniform */, ___, nullptr},
	{"rgba16ui",			/* _ i _ _ uniform */, ___, nullptr},
	{"rgb10_a2ui",			/* _ i _ _ uniform */, ___, nullptr},
	{"rgba8ui",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg32ui",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg16ui",				/* _ i _ _ uniform */, ___, nullptr},
	{"rg8ui",				/* _ i _ _ uniform */, ___, nullptr},
	{"r32ui",				/* _ i _ _ uniform */, ___, nullptr},
	{"r16ui",				/* _ i _ _ uniform */, ___, nullptr},
	{"r8ui",				/* _ i _ _ uniform */, ___, nullptr},
};
#endif

const char *glsl_interface_names[glsl_num_interfaces] = {
	"in",
	"out",
	"uniform",
	"buffer",
	"shared",
};

symtab_t *
glsl_optimize_attributes (attribute_t *attributes)
{
	auto attrtab = new_symtab (nullptr, stab_param);

	for (auto attr = attributes; attr; attr = attr->next) {
		auto attrsym = symtab_lookup (attrtab, attr->name);
		if (attrsym) {
			if (strcmp (attr->name, "layout") == 0) {
				list_append_list (&attrsym->list, &attr->params->list);
			} else {
				error (attr->params, "duplicate %s", attr->name);
			}
		} else {
			attrsym = new_symbol (attr->name);
			if (strcmp (attr->name, "layout") == 0) {
				attrsym->sy_type = sy_list;
				list_append_list (&attrsym->list, &attr->params->list);
			} else {
				if (attr->params) {
					notice (attr->params, "attribute params: %s", attr->name);
					attrsym->sy_type = sy_expr;
					attrsym->expr = attr->params;
				}
			}
			symtab_addsymbol (attrtab, attrsym);
		}
		notice (0, "%s", attr->name);
		if (!attr->params) continue;
		for (auto p = attr->params->list.head; p; p = p->next) {
			if (p->expr->type == ex_expr) {
				notice (0, "    %s = %s",
						get_value_string (p->expr->expr.e1->value),
						get_value_string (p->expr->expr.e2->value));
			} else {
				notice (0, "    %s", get_value_string (p->expr->value));
			}
		}
	}
	return attrtab;
}
