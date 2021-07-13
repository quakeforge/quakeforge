/*
	qf_bsp.h

	GLSL specific brush model stuff

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/7

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
#ifndef __QF_GLSL_qf_bsp_h
#define __QF_GLSL_qf_bsp_h

#include "QF/GLSL/types.h"

typedef struct bspvert_s {
	quat_t      vertex;
	quat_t      tlst;
} bspvert_t;

typedef struct elements_s {
	struct elements_s *_next;
	struct elements_s *next;
	byte       *base;
	struct dstring_s *list;
} elements_t;

typedef struct elechain_s {
	struct elechain_s *_next;
	struct elechain_s *next;
	int         model_index;
	elements_t *elements;
	vec_t      *transform;
	float      *color;
} elechain_t;

void glsl_R_ClearElements (void);
void glsl_R_DrawWorld (void);
void glsl_R_DrawSky (void);
void glsl_R_RegisterTextures (model_t **models, int num_models);
void glsl_R_BuildDisplayLists (model_t **models, int num_models);
void glsl_R_InitBsp (void);

#endif//__QF_GLSL_qf_bsp_h
