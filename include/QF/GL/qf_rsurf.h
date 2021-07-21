/*
	gl_rsurf.h

	GL texture stuff from the renderer.

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

#ifndef __QF_GL_rsurf_h
#define __QF_GL_rsurf_h

typedef struct gltex_s {
	struct texture_s  *texture;
	int			gl_texturenum;
	int			gl_fb_texturenum;
	struct instsurf_s *tex_chain;
	struct instsurf_s **tex_chain_tail;
} gltex_t;

extern int gl_mirrortexturenum;	// quake texturenum, not gltexturenum

struct model_s;
struct entity_s;
struct msurface_s;

void gl_lightmap_init (void);
void GL_BuildLightmaps (struct model_s **models, int num_models);

void R_DrawBrushModel (struct entity_s *e);
void R_DrawWorld (void);

void GL_EmitWaterPolys (struct msurface_s *fa);

#endif // __QF_GL_rsurf_h
