/*
	qf_lightmap.h

	GLSL lightmap stuff from the renderer.

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

#ifndef __QF_GLSL_lightmap_h
#define __QF_GLSL_lightmap_h

// LordHavoc: since lightmaps are now allocated only as needed, allow a ridiculous number :)
#define MAX_LIGHTMAPS	1024
#define BLOCK_WIDTH		64
#define BLOCK_HEIGHT	64

void glsl_lightmap_init (void);
struct transform_s;
void glsl_R_BuildLightmaps (struct model_s **models, int num_models);
void glsl_R_CalcLightmaps (void);
extern void (*glsl_R_BuildLightMap) (const vec4f_t *transform,
									 mod_brush_t *brush, msurface_t *surf);
int  glsl_R_LightmapTexture (void) __attribute__((pure));
void glsl_R_FlushLightmaps (void);

#endif//__QF_GLSL_lightmap_h
