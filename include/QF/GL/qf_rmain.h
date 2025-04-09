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
#include "QF/simd/types.h"

struct cvar_s;
struct entity_s;

extern float gl_modelalpha;
//extern vec3_t shadecolor;

extern void gl_multitexture_f (void *data, const struct cvar_s *var);

void glrmain_init (void);
void gl_R_RotateForEntity (const vec4f_t *mat);

struct model_s;
struct entqueue_s;
struct scene_s;
void gl_R_NewScene (struct scene_s *scene);
void gl_R_RenderView (void);
void gl_R_RenderEntities (struct entqueue_s *queue);
void gl_R_ClearState (void);
void gl_R_ViewChanged (void);
void gl_R_LineGraph (int x, int y, int *h_vals, int count, int height);
void gl_R_InitGraphTextures (void);

#endif // __QF_GL_rmain_h
