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

int GLSL_LoadQuakeTexture (const char *identifier, int width, int height,
						   byte *data);
struct texture_s;
int GLSL_LoadQuakeMipTex (const struct texture_s *tex);
int GLSL_LoadRGBTexture (const char *identifier, int width, int height,
						 byte *data);
int GLSL_LoadRGBATexture (const char *identifier, int width, int height,
						  byte *data);
void GLSL_ReleaseTexture (int tex);
void GLSL_TextureInit (void);

scrap_t *GLSL_CreateScrap (int size, int format, int linear);
void GLSL_DestroyScrap (scrap_t *scrap);
void GLSL_ScrapClear (scrap_t *scrap);
int GLSL_ScrapTexture (scrap_t *scrap) __attribute__((pure));
//XXX slow!
struct subpic_s *GLSL_ScrapSubpic (scrap_t *scrap, int width, int height);
//XXX slow!
void GLSL_SubpicDelete (struct subpic_s *subpic);
void GLSL_SubpicUpdate (struct subpic_s *subpic, byte *data, int batch);
void GLSL_ScrapFlush (scrap_t *scrap);

#endif//__QF_GLSL_textures_h
