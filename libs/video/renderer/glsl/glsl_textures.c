/*
	glsl_textures.c

	Texture format setup.

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2001 Ragnvald Maartmann-Moe IV

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"

int
GL_LoadQuakeTexture (const char *identifier, int width, int height, byte *data)
{
	GLuint      tnum;

	qfglGenTextures (1, &tnum);
	qfglBindTexture (GL_TEXTURE_2D, tnum);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE,
					width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfglGenerateMipmap (GL_TEXTURE_2D);

	return tnum;
}
