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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/qtypes.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_noisetextures.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

/*
int         part_tex_dot;
int         part_tex_smoke;
int         part_tex_spark;
*/

int			part_tex;
GLint		part_tex_internal_format = 2;


static void
GDT_InitParticleTexture (void)
{
	byte data[64][64][2];

	memset (data, 0, sizeof (data));

	part_tex = texture_extension_number++;
	qfglBindTexture (GL_TEXTURE_2D, part_tex);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qfglTexImage2D (GL_TEXTURE_2D, 0, part_tex_internal_format, 64, 64, 0, GL_LUMINANCE_ALPHA,
					GL_UNSIGNED_BYTE, data);
}

static void
GDT_InitDotParticleTexture (void)
{
	byte        data[32][32][2];
	int         x, y, dx2, dy, d;

	for (x = 0; x < 32; x++) {
		dx2 = x - 16;
		dx2 *= dx2;
		for (y = 0; y < 32; y++) {
			dy = y - 16;
			d = 255 - (dx2 + (dy * dy));
			if (d <= 0)
				d = 0;
			data[y][x][0] = 255;
			data[y][x][1] = (byte) d;
		}
	}
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, data);
}

static void
GDT_InitSparkParticleTexture (void)
{
	byte        data[32][32][2];
	int         x, y, dx2, dy, d;

	for (x = 0; x < 32; x++) {
		dx2 = 16 - abs (x - 16);
		dx2 *= dx2;
		for (y = 0; y < 32; y++) {
			dy = 16 - abs (y - 16);
			d = (dx2 + dy * dy) - 200;
			if (d > 255) {
				d = 255;
			} else if (d < 1) {
				d = 0;
			}
			data[y][x][0] = 255;
			data[y][x][1] = (byte) d;
		}
	}
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, 32, 0, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, data);
}

static void
GDT_InitSmokeParticleTexture (void)
{
	byte        d;
	byte        data[32][32][2], noise1[32][32], noise2[32][32];
	float       dx, dy2;
	int         x, y, c;

	memset (noise1, 0, sizeof (noise1));
	noise_plasma (&noise1[0][0], 32);
	noise_diamondsquare (&noise2[0][0], 32, 4);
	for (y = 0; y < 32; y++) {
		dy2 = y - 16;
		dy2 *= dy2;
		for (x = 0; x < 32; x++) {
			dx = x - 16;
			c = 255 - (dx * dx + dy2);
			if (c < 1)
				c = 0;
			d = (noise1[y][x] + noise2[y][x]) / 2;
			data[y][x][0] = 255;
			if (d > 0) {
				data[y][x][1] = (d * c) / 255;
			} else {
				data[y][x][1] = 0;
			}
		}
	}
	qfglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 32, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, data);
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
