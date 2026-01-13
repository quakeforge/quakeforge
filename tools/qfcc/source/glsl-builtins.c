/*
	glsl-builtins.c

	Builtin symbols for GLSL

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/va.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/cpp.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/iface_block.h"
#include "tools/qfcc/include/image.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/spirv.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

glsl_sublang_t glsl_sublang;

typedef struct {
	const char *file;
	const char *str;
	int (*parse_string) (const char *str, rua_ctx_t *ctx);
} glsl_embed_t;

#define PARSE_EMBED(name, lang)                    \
	static glsl_embed_t glsl_##name = {            \
		.file = "#line 1 \"" name##_path "\" 0\n", \
		.str = glsl_##name##_str,                  \
		.parse_string = lang##_parse_string,       \
	}

#define Vulkan_vertex_vars_path "ruamoko/shader/GLSL/_vertex_vars.glsl"
static const char glsl_Vulkan_vertex_vars_str[] = {
#embed Vulkan_vertex_vars_path
};
PARSE_EMBED(Vulkan_vertex_vars, glsl);

#define tessctrl_vars_path "ruamoko/shader/GLSL/_tessctrl_vars.glsl"
static const char glsl_tessctrl_vars_str[] = {
#embed "ruamoko/shader/GLSL/_tessctrl_vars.glsl"
};
PARSE_EMBED(tessctrl_vars, glsl);

#define tesseval_vars_path "ruamoko/shader/GLSL/_tesseval_vars.glsl"
static const char glsl_tesseval_vars_str[] = {
#embed tesseval_vars_path
};
PARSE_EMBED(tesseval_vars, glsl);

#define geometry_vars_path "ruamoko/shader/GLSL/_geometry_vars.glsl"
static const char glsl_geometry_vars_str[] = {
#embed geometry_vars_path
};
PARSE_EMBED(geometry_vars, glsl);

#define fragment_vars_path "ruamoko/shader/GLSL/_fragment_vars.glsl"
static const char glsl_fragment_vars_str[] = {
#embed fragment_vars_path
};
PARSE_EMBED(fragment_vars, glsl);

#define compute_vars_path "ruamoko/shader/GLSL/_compute_vars.glsl"
static const char glsl_compute_vars_str[] = {
#embed compute_vars_path
};
PARSE_EMBED(compute_vars, glsl);

#define _system_const_path "ruamoko/shader/GLSL/_system_const.glsl"
static const char glsl__system_const_str[] = {
#embed _system_const_path
};
PARSE_EMBED(_system_const, glsl);

#define SPV(op) "@intrinsic(" #op ")"
#define GLSL(op) "@intrinsic(OpExtInst, \"GLSL.std.450\", " #op ")"

#define _defines_path "ruamoko/shader/GLSL/_defines.h"
static const char glsl__defines_str[] = {
#embed _defines_path
};
PARSE_EMBED(_defines, qc);

#define general_functions_path "ruamoko/shader/GLSL/general.h"
static const char glsl_general_functions_str[] = {
#embed general_functions_path
};
PARSE_EMBED(general_functions, qc);

#define texture_functions_path "ruamoko/shader/GLSL/texture.h"
static const char glsl_texture_functions_str[] = {
#embed texture_functions_path
};
PARSE_EMBED(texture_functions, qc);

#define atomic_functions_path "ruamoko/shader/GLSL/atomic.h"
static const char glsl_atomic_functions_str[] = {
#embed atomic_functions_path
};
PARSE_EMBED(atomic_functions, qc);

#define image_functions_path "ruamoko/shader/GLSL/image.h"
static const char glsl_image_functions_str[] = {
#embed image_functions_path
};
PARSE_EMBED(image_functions, qc);

#define geometry_functions_path "ruamoko/shader/GLSL/geometry.h"
static const char glsl_geometry_functions_str[] = {
#embed geometry_functions_path
};
PARSE_EMBED(geometry_functions, qc);

#define fragment_functions_path "ruamoko/shader/GLSL/fragment.h"
static const char glsl_fragment_functions_str[] = {
#embed fragment_functions_path
};
PARSE_EMBED(fragment_functions, qc);

#undef PARSE_EMBED
#define PARSE_EMBED(name, ctx) parse_embed_string (&glsl_##name, ctx)

void
glsl_multiview (int behavior, void *scanner)
{
	if (behavior) {
		spirv_add_capability (pr.module, SpvCapabilityMultiView);
		rua_parse_define ("GL_EXT_multiview 1\n");
	} else {
		rua_undefine ("GL_EXT_multiview", scanner);
	}
}

static int glsl_include_state = 0;

bool
glsl_on_include (const char *name, rua_ctx_t *ctx)
{
	if (!glsl_include_state) {
		error (0, "'#include' : required extension not requested");
		return false;
	}
	if (glsl_include_state > 1) {
		warning (0, "'#include' : required extension not requested");
	}
	return true;
}

void
glsl_include (int behavior, void *scanner)
{
	glsl_include_state = behavior;
}

void
glsl_pre_init (rua_ctx_t *ctx)
{
	const char *vulkan = va ("VULKAN=%d", 100);
	cpp_define (vulkan);
}

static void
parse_embed_string (const glsl_embed_t *embed, rua_ctx_t *rua_ctx)
{
	auto loc = pr.loc;
	embed->parse_string (embed->file, rua_ctx);
	embed->parse_string (embed->str, rua_ctx);
	pr.loc = loc;
}

static void
glsl_init_common (rua_ctx_t *rua_ctx, rua_ctx_t *ctx)
{
	rua_parse_define ("__GLSL__ 1\n");
	rua_ctx->macros = new_symtab (cpp_macros, stab_global);
	glsl_sublang = *(glsl_sublang_t *) ctx->language->sublanguage;

	current_target.create_entry_point ("main", glsl_sublang.model_name,
									   nullptr);

	image_init_types ();

	block_clear ();
	PARSE_EMBED (_system_const, ctx);
	PARSE_EMBED (_defines, rua_ctx);
	PARSE_EMBED (general_functions, rua_ctx);
	PARSE_EMBED (texture_functions, rua_ctx);
	PARSE_EMBED (atomic_functions, rua_ctx);
	PARSE_EMBED (image_functions, rua_ctx);
}

void
glsl_init_comp (rua_ctx_t *ctx)
{
	rua_parse_define ("__GLSL_COMPUTE__ 1\n");

	rua_ctx_t rua_ctx = { .language = &lang_ruamoko, .sub_parse = true };

	glsl_init_common (&rua_ctx, ctx);
	PARSE_EMBED (compute_vars, ctx);
}

void
glsl_init_vert (rua_ctx_t *ctx)
{
	rua_parse_define ("__GLSL_VERTEX__ 1\n");

	rua_ctx_t rua_ctx = { .language = &lang_ruamoko, .sub_parse = true };

	glsl_init_common (&rua_ctx, ctx);
	PARSE_EMBED (Vulkan_vertex_vars, ctx);
}

void
glsl_init_tesc (rua_ctx_t *ctx)
{
	rua_parse_define ("__GLSL_TESSELATION_CONTROL__ 1\n");

	rua_ctx_t rua_ctx = { .language = &lang_ruamoko, .sub_parse = true };

	glsl_init_common (&rua_ctx, ctx);
	PARSE_EMBED (tessctrl_vars, ctx);

	spirv_add_capability (pr.module, SpvCapabilityTessellation);
}

void
glsl_init_tese (rua_ctx_t *ctx)
{
	rua_parse_define ("__GLSL_TESSELATION_EVALUATION__ 1\n");

	rua_ctx_t rua_ctx = { .language = &lang_ruamoko, .sub_parse = true };

	glsl_init_common (&rua_ctx, ctx);
	PARSE_EMBED (tesseval_vars, ctx);

	spirv_add_capability (pr.module, SpvCapabilityTessellation);
}

void
glsl_init_geom (rua_ctx_t *ctx)
{
	rua_parse_define ("__GLSL_GEOMETRY__ 1\n");

	rua_ctx_t rua_ctx = { .language = &lang_ruamoko, .sub_parse = true };

	glsl_init_common (&rua_ctx, ctx);
	PARSE_EMBED (geometry_functions, &rua_ctx);
	PARSE_EMBED (geometry_vars, ctx);

	spirv_add_capability (pr.module, SpvCapabilityGeometry);
}

void
glsl_init_frag (rua_ctx_t *ctx)
{
	rua_parse_define ("__GLSL_FRAGMENT__ 1\n");

	rua_ctx_t rua_ctx = { .language = &lang_ruamoko, .sub_parse = true };

	glsl_init_common (&rua_ctx, ctx);
	PARSE_EMBED (fragment_functions, &rua_ctx);
	PARSE_EMBED (fragment_vars, ctx);
}
