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

	$Id$
*/

#ifndef __QF_GLSL_textures_h
#define __QF_GLSL_textures_h

#include "QF/qtypes.h"

int GL_LoadQuakeTexture (const char *identifier, int width, int height,
						 byte *data);
int GL_LoadRGBTexture (const char *identifier, int width, int height,
					   byte *data);
void GL_ReleaseTexture (int tex);
int GL_LoadTexture (const char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel);

void GDT_Init (void);

#endif//__QF_GLSL_textures_h
