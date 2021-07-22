/*
	qf_iqm.h

	GLSL specific IQM stuff

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/5/11

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
#ifndef __QF_GLSL_qf_iqm_h
#define __QF_GLSL_qf_iqm_h

#include "QF/iqm.h"

typedef struct glsliqm_s {
	GLuint     *textures;
	GLuint     *normmaps;
	GLuint      vertex_array;
	GLuint      element_array;
} glsliqm_t;

void glsl_R_InitIQM (void);
struct entity_s;
void glsl_R_DrawIQM (struct entity_s *ent);
void glsl_R_IQMBegin (void);
void glsl_R_IQMEnd (void);

#endif//__QF_GLSL_qf_iqm_h
