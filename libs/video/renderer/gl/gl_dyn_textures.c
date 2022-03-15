/*
	gl_dyn_textures.c

	Dynamic texture generation.

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/image.h"
#include "QF/qtypes.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "r_internal.h"

GLuint      gl_part_tex;
static GLint part_tex_internal_format = 2;


static void
GDT_InitParticleTexture (void)
{
	byte data[64][64][2];

	memset (data, 0, sizeof (data));

	qfglGenTextures (1, &gl_part_tex);
	qfglBindTexture (GL_TEXTURE_2D, gl_part_tex);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qfglTexImage2D (GL_TEXTURE_2D, 0, part_tex_internal_format, 64, 64, 0, GL_LUMINANCE_ALPHA,
					GL_UNSIGNED_BYTE, data);
}

static void
GDT_InitDotParticleTexture (void)
{
	tex_t      *tex;

	tex = R_DotParticleTexture ();
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, tex->data);
	free (tex);
}

static void
GDT_InitSparkParticleTexture (void)
{
	tex_t      *tex;

	tex = R_SparkParticleTexture ();
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, 32, 0, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, tex->data);
	free (tex);
}

static void
GDT_InitSmokeParticleTexture (void)
{
	tex_t      *tex;

	tex = R_SmokeParticleTexture ();
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 32, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, tex->data);
	free (tex);
}

void
GDT_Init (void)
{
	if (gl_feature_mach64)
		part_tex_internal_format = 4;
	GDT_InitParticleTexture ();
	GDT_InitDotParticleTexture ();
	GDT_InitSparkParticleTexture ();
	GDT_InitSmokeParticleTexture ();
}
