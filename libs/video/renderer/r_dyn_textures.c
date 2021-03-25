/*
	r_dyn_textures.c

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

#include "noisetextures.h"
#include "r_internal.h"

tex_t *
R_DotParticleTexture (void)
{
	byte        (*data)[32][32][2];
	int         x, y, dx2, dy, d;
	tex_t      *tex;

	tex = malloc (sizeof (tex_t) + sizeof (*data));
	tex->data = (byte *) (tex + 1);
	tex->width = 32;
	tex->height = 32;
	tex->format = tex_la;
	tex->palette = 0;
	data = (byte (*)[32][32][2]) tex->data;

	for (x = 0; x < 32; x++) {
		dx2 = x - 16;
		dx2 *= dx2;
		for (y = 0; y < 32; y++) {
			dy = y - 16;
			d = 255 - (dx2 + (dy * dy));
			if (d <= 0)
				d = 0;
			(*data)[y][x][0] = 255;
			(*data)[y][x][1] = (byte) d;
		}
	}
	return tex;
}

tex_t *
R_SparkParticleTexture (void)
{
	byte        (*data)[32][32][2];
	int         x, y, dx2, dy, d;
	tex_t      *tex;

	tex = malloc (sizeof (tex_t) + sizeof (*data));
	tex->data = (byte*) (tex + 1);
	tex->width = 32;
	tex->height = 32;
	tex->format = tex_la;
	tex->palette = 0;
	data = (byte (*)[32][32][2]) tex->data;

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
			(*data)[y][x][0] = 255;
			(*data)[y][x][1] = (byte) d;
		}
	}
	return tex;
}

tex_t *
R_SmokeParticleTexture (void)
{
	byte        d;
	byte        (*data)[32][32][2], noise1[32][32], noise2[32][32];
	float       dx, dy2;
	int         x, y, c;
	tex_t      *tex;

	tex = malloc (sizeof (tex_t) + sizeof (*data));
	tex->data = (byte *) (tex + 1);
	tex->width = 32;
	tex->height = 32;
	tex->format = tex_la;
	tex->palette = 0;
	data = (byte (*)[32][32][2]) tex->data;

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
			(*data)[y][x][0] = 255;
			if (d > 0) {
				(*data)[y][x][1] = (d * c) / 255;
			} else {
				(*data)[y][x][1] = 0;
			}
		}
	}
	return tex;
}
