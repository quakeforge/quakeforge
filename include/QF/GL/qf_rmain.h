/*
	qf_rmain.h

	GL light stuff from the renderer.

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef __QF_GL_rmain_h
#define __QF_GL_rmain_h

#include "QF/qtypes.h"

struct cvar_s;
struct entity_s;

extern qboolean gl_envmap;
extern int c_brush_polys, c_alias_polys;
extern float r_world_matrix[16];

extern float gl_modelalpha;
//extern vec3_t shadecolor;

extern void gl_multitexture_f (struct cvar_s *var);

void glrmain_init (void);
void R_RotateForEntity (struct entity_s *e);

#endif // __QF_GL_rmain_h
