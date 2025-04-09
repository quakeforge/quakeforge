/*
	qf_mesh.h

	GL specific mesh rendering

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
#ifndef __QF_GL_qf_mesh_h
#define __QF_GL_qf_mesh_h

#include "QF/GL/types.h"

typedef struct mesh_vrt_s {
	float       st[2];
	float       normal[3];
	float       vertex[3];
} mesh_vrt_t;

struct entity_s;
void gl_R_DrawAliasModel (struct entity_s ent);

#endif//__QF_GL_qf_mesh_h
