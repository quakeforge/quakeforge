/*
	gl_textures.h

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

#ifndef __QF_GLSL_textures_h
#define __QF_GLSL_textures_h

#include "QF/qtypes.h"

typedef struct scrap_s scrap_t;
typedef struct tex_s tex_t;
typedef struct texture_s texture_t;
typedef struct scrapbox_s scrapbox_t;
typedef struct atlas_update_s atlas_update_t;

int GLSL_LoadQuakeTexture (const char *identifier, int width, int height,
						   const byte *data);
int GLSL_LoadQuakeMipTex (const texture_t *tex);
int GLSL_LoadRGBTexture (const char *identifier, int width, int height,
						 const byte *data);
int GLSL_LoadRGBATexture (const char *identifier, int width, int height,
						  const byte *data);
int GLSL_LoadTex (const char *identifier, int mips, tex_t *tex);
void GLSL_ReleaseTexture (int tex);
void GLSL_TextureInit (void);

scrap_t *GLSL_CreateScrap (int size, int format, int linear);
void GLSL_DestroyScrap (scrap_t *scrap);
void GLSL_ScrapClear (scrap_t *scrap);
int GLSL_ScrapTexture (scrap_t *scrap) __attribute__((pure));
float GLSL_ScrapSize (scrap_t *scrap) __attribute__((pure));
struct subpic_s *GLSL_ScrapSubpic (scrap_t *scrap, int width, int height);
void GLSL_SubpicDelete (struct subpic_s *subpic);
void GLSL_SubpicUpdate (struct subpic_s *subpic, byte *data, int batch);
void GLSL_ScrapFlush (scrap_t *scrap);

uint32_t glsl_make_atlas (const char *name, const tex_t *tex, int num_rects,
						  const scrapbox_t *rects);
atlas_update_t glsl_atlas_prepare_update (uint32_t atlasid, uint32_t num_rects,
										  uint32_t tex_bytes);
void glsl_atlas_finish_update (void);

#endif//__QF_GLSL_textures_h
