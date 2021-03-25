/*
	gl_lightmap.h

	GL lightmap stuff from the renderer.

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

#ifndef __QF_GL_lightmap_h
#define __QF_GL_lightmap_h

// LordHavoc: since lightmaps are now allocated only as needed, allow a ridiculous number :)
#define MAX_LIGHTMAPS	1024
#define BLOCK_WIDTH		64
#define BLOCK_HEIGHT	64

typedef struct glRect_s {
	unsigned short l, t, w, h;
} glRect_t;

extern int lm_src_blend, lm_dest_blend;
extern model_t *gl_currentmodel;

extern int			gl_lightmap_textures;
extern qboolean		gl_lightmap_modified[MAX_LIGHTMAPS];
extern instsurf_t  *gl_lightmap_polys[MAX_LIGHTMAPS];
extern glRect_t		gl_lightmap_rectchange[MAX_LIGHTMAPS];

void GL_BuildSurfaceDisplayList (msurface_t *fa);
void gl_lightmap_init (void);
void GL_BuildLightmaps (struct model_s **models, int num_models);
void R_BlendLightmaps (void);
void R_CalcLightmaps (void);
extern void (*R_BuildLightMap) (mod_brush_t *brush, msurface_t *surf);

#endif // __QF_GL_lightmap_h
