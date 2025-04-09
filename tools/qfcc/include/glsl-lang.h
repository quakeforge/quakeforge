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

symtab_t *glsl_optimize_attributes (attribute_t *attributes);
void glsl_apply_attributes (symtab_t *attributes, specifier_t spec);

void glsl_parse_declaration (specifier_t spec, symbol_t *sym,
							 const expr_t *init, symtab_t *symtab,
							 expr_t *block, rua_ctx_t *ctx);
void glsl_field_attributes (attribute_t *attributes, symbol_t *sym,
							rua_ctx_t *ctx);
void glsl_layout (const ex_list_t *qualifiers, specifier_t spec);
void glsl_qualifier (const char *name, specifier_t spec);

bool glsl_on_include (const char *name, rua_ctx_t *ctx);
void glsl_include (int behavior, void *scanner);
void glsl_multiview (int behavior, void *scanner);

#endif//__glsl_lang_h
