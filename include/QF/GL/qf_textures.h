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

#ifndef __gl_textures_h
#define __gl_textures_h

#include "QF/qtypes.h"
#include "QF/GL/types.h"

#define MAX_GLTEXTURES	2048

extern int gl_alpha_format;
extern int gl_solid_format;
extern int gl_lightmap_format;
extern int gl_filter_min;
extern int gl_filter_max;
extern qboolean gl_Anisotropy;
extern float gl_aniso;
extern GLuint gl_part_tex;

void GL_Upload8 (const byte *data, int width, int height,  qboolean mipmap, qboolean alpha);
void GL_Upload8_EXT (const byte *data, int width, int height,  qboolean mipmap, qboolean alpha);
int GL_LoadTexture (const char *identifier, int width, int height, const byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel);
int GL_FindTexture (const char *identifier);

void GL_TextureMode_f (void);

void GDT_Init (void);

#endif // __gl_textures_h
