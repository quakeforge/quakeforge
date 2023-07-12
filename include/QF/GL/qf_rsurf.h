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

struct model_s;
struct entity_s;
struct msurface_s;
struct mod_brush_s;

void GL_BuildSurfaceDisplayList (struct mod_brush_s *brush,
								 struct msurface_s *fa);

void gl_R_DrawBrushModel (struct entity_s e);
void gl_R_DrawWorld (void);
void gl_R_DrawWaterSurfaces (void);

void GL_EmitWaterPolys (struct msurface_s *fa);
void gl_R_LoadSkys (const char *sky);

struct texture_s;
void gl_R_AddTexture (struct texture_s *tx);
void gl_R_ClearTextures (void);
void gl_R_InitSurfaceChains (struct mod_brush_s *brush);

struct framebuffer_s;
void gl_WarpScreen (struct framebuffer_s *fb);

#endif // __QF_GL_rsurf_h
