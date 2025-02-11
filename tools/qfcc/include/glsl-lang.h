/*
	glsl-lang.h

	Shared stuff for glsl

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

#ifndef __glsl_lang_h
#define __glsl_lang_h

#include "QF/darray.h"

typedef struct specifier_s specifier_t;
typedef struct attribute_s attribute_t;
typedef struct expr_s expr_t;
typedef struct ex_list_s ex_list_t;
typedef struct type_s type_t;
typedef struct symbol_s symbol_t;
typedef struct symtab_s symtab_t;
typedef struct language_s language_t;
typedef struct defspace_s defspace_t;
typedef struct rua_ctx_s rua_ctx_t;

void glsl_init_comp (rua_ctx_t *ctx);
int glsl_finish_comp (const char *file, rua_ctx_t *ctx);
void glsl_init_vert (rua_ctx_t *ctx);
void glsl_init_tesc (rua_ctx_t *ctx);
void glsl_init_tese (rua_ctx_t *ctx);
void glsl_init_geom (rua_ctx_t *ctx);
int glsl_finish_geom (const char *file, rua_ctx_t *ctx);
void glsl_init_frag (rua_ctx_t *ctx);

int glsl_parse_string (const char *str, rua_ctx_t *ctx);

extern language_t lang_glsl_comp;
extern language_t lang_glsl_vert;
extern language_t lang_glsl_tesc;
extern language_t lang_glsl_tese;
extern language_t lang_glsl_geom;
extern language_t lang_glsl_frag;

typedef enum glsl_interface_e : unsigned {
	glsl_in,
	glsl_out,
	glsl_uniform,
	glsl_buffer,
	glsl_shared,
	glsl_push_constant,

	glsl_num_interfaces
} glsl_interface_t;
#define glsl_iftype_from_sc(sc) ((glsl_interface_t)((sc) - sc_in))
#define glsl_sc_from_iftype(it) (((storage_class_t)(it)) + sc_in)

extern const char *glsl_interface_names[glsl_num_interfaces];

typedef struct glsl_block_s {
	struct glsl_block_s *next;
	symbol_t   *name;
	glsl_interface_t interface;
	symtab_t   *attributes;
	symtab_t   *members;
	defspace_t *space;
	symbol_t   *instance_name;
} glsl_block_t;

typedef enum : unsigned {
	glid_1d,
	glid_2d,
	glid_3d,
	glid_cube,
	glid_rect,
	glid_buffer,
	glid_subpassdata,
} glsl_imagedim_t;

typedef struct glsl_image_s {
	const type_t *sample_type;
	glsl_imagedim_t dim;
	char        depth;
	bool        arrayed;
	bool        multisample;
	char        sampled;
	unsigned    format;
	uint32_t    id;
} glsl_image_t;

typedef struct glsl_sampled_image_s {
	const type_t *image_type;
} glsl_sampled_image_t;

typedef struct DARRAY_TYPE (glsl_image_t) glsl_imageset_t;
extern glsl_imageset_t glsl_imageset;
extern type_t type_glsl_image;
extern type_t type_glsl_sampler;
extern type_t type_glsl_sampled_image;

typedef struct glsl_sublang_s {
	const char *name;
	const char **interface_default_names;
	const char *model_name;
} glsl_sublang_t;
extern glsl_sublang_t glsl_sublang;
extern glsl_sublang_t glsl_comp_sublanguage;
extern glsl_sublang_t glsl_vert_sublanguage;
extern glsl_sublang_t glsl_tesc_sublanguage;
extern glsl_sublang_t glsl_tese_sublanguage;
extern glsl_sublang_t glsl_geom_sublanguage;
extern glsl_sublang_t glsl_frag_sublanguage;

void glsl_block_clear (void);
glsl_block_t *glsl_create_block (specifier_t spec, symbol_t *block_sym);
void glsl_finish_block (glsl_block_t *block, specifier_t spec);
void glsl_declare_block_instance (glsl_block_t *block, symbol_t *instance_name);
glsl_block_t *glsl_get_block (const char *name, glsl_interface_t interface);
symtab_t *glsl_optimize_attributes (attribute_t *attributes);
void glsl_apply_attributes (symtab_t *attributes, specifier_t spec);

void glsl_parse_declaration (specifier_t spec, symbol_t *sym,
							 const expr_t *init, symtab_t *symtab,
							 expr_t *block, rua_ctx_t *ctx);
void glsl_declare_field (specifier_t spec, symtab_t *symtab, rua_ctx_t *ctx);
void glsl_layout (const ex_list_t *qualifiers, specifier_t spec);

bool glsl_on_include (const char *name, rua_ctx_t *ctx);
void glsl_include (int behavior, void *scanner);
void glsl_multiview (int behavior, void *scanner);

symbol_t *glsl_image_type (glsl_image_t *image, const type_t *htype,
						   const char *name);

#endif//__glsl_lang_h
