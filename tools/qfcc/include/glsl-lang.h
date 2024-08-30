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

void glsl_init_comp (void);
void glsl_init_vert (void);
void glsl_init_tesc (void);
void glsl_init_tese (void);
void glsl_init_geom (void);
void glsl_init_frag (void);

int glsl_parse_string (const char *str);

typedef struct language_s language_t;
extern language_t lang_glsl_comp;
extern language_t lang_glsl_vert;
extern language_t lang_glsl_tesc;
extern language_t lang_glsl_tese;
extern language_t lang_glsl_geom;
extern language_t lang_glsl_frag;

typedef struct glsl_block_s {
	struct glsl_block_s *next;
	const char *name;
	struct symtab_s *members;
	struct symbol_s *instance_name;
} glsl_block_t;

struct specifier_s;

void glsl_block_clear (void);
void glsl_declare_block (struct specifier_s spec, struct symbol_s *block_sym,
						 struct symbol_s *instance_name);

#endif//__glsl_lang_h
