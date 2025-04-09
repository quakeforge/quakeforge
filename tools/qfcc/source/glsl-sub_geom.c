/*
	glsl-sub_geom.c

	GLSL geometry shader sublanguage

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
#include "tools/qfcc/include/iface_block.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static const char *glsl_geom_interface_default_names[iface_num_interfaces] = {
	[iface_in] = "gl_PerVertex",
	[iface_out] = "gl_PerVertex",
	[iface_uniform] = ".default",
	[iface_buffer] = ".default",
};

glsl_sublang_t glsl_geom_sublanguage = {
	.name = "geometry",
	.interface_default_names = glsl_geom_interface_default_names,
	.model_name = "Geometry",
};

int
glsl_finish_geom (const char *file, rua_ctx_t *ctx)
{
	if (!pr.module || !pr.module->entry_points) {
		return 1;
	}
	auto invocations = pr.module->entry_points->invocations;
	if (!invocations) {
		invocations = new_int_expr (1, false);
		pr.module->entry_points->invocations = invocations;
	}
	return 1;
}
