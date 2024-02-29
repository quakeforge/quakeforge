/*
	qf_alias.h

	GLSL specific alias model stuff

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/1

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
#ifndef __QF_GLSL_qf_alias_h
#define __QF_GLSL_qf_alias_h

#include "QF/GLSL/types.h"

typedef struct glsl_alias_mesh_s {
	vec3_t      scale;
	vec3_t      scale_origin;
	int         skinwidth;
	int         skinheight;
	GLuint      vertices;
	GLuint      indices;
	uint32_t    numverts;
	uint32_t    numtris;
} glsl_alias_mesh_t;

typedef struct aliasvrt_s {
	GLshort     st[2];
	GLshort     normal[3];
	GLushort    vertex[3];
} aliasvrt_t;

void glsl_R_InitAlias (void);
struct entity_s;
void glsl_R_DrawAlias (struct entity_s ent);
void glsl_R_AliasBegin (void);
void glsl_R_AliasEnd (void);

#endif//__QF_GLSL_qf_alias_h
