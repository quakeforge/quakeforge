/*
	convert.c

	Image/color conversion routins (RGB to paletted)

	Copyright (C) 2013 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2013/5/10

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

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/image.h"
#include "QF/mathlib.h"

struct colcache_s {
	struct colcache_s *next;
	hashtab_t  *tab;
};

typedef struct colcache_color_s {
	struct colcache_color_s *next;
	byte        rgb[3];
	byte        col;
} colcache_color_t;

static colcache_t *colcache_freelist;
static colcache_color_t *colcache_color_freelist;

static colcache_color_t *
colcache_new_color (const byte *rgb, byte ind)
{
	colcache_color_t *col;
	ALLOC (256, colcache_color_t, colcache_color, col);
	VectorCopy (rgb, col->rgb);
	col->col = ind;
	return col;
}

static void
colcache_free_color (void *_col, void *unused)
{
	colcache_color_t *col = (colcache_color_t *) _col;
	FREE (colcache_color, col);
}

static uintptr_t
colcache_get_hash (const void *_col, void *unused)
{
	colcache_color_t *col = (colcache_color_t *) _col;
	uintptr_t   r, g, b;
	r = col->rgb[0];
	g = col->rgb[1];
	b = col->rgb[2];
	return (r << 8) ^ (g << 4) ^ b;
}

static int
colcache_compare (const void *_cola, const void *_colb, void *unused)
{
	colcache_color_t *cola = (colcache_color_t *) _cola;
	colcache_color_t *colb = (colcache_color_t *) _colb;

	return VectorCompare (cola->rgb, colb->rgb);
}

colcache_t *
ColorCache_New (void)
{
	colcache_t *cache;

	ALLOC (16, colcache_t, colcache, cache);
	cache->tab = Hash_NewTable (1023, 0, colcache_free_color, 0, 0);
	Hash_SetHashCompare (cache->tab, colcache_get_hash, colcache_compare);
	return cache;
}

void
ColorCache_Delete (colcache_t *cache)
{
	Hash_DelTable (cache->tab);
	FREE (colcache, cache);
}

byte
ConvertColor (const byte *rgb, const byte *pal, colcache_t *cache)
{
	//FIXME slow!
	int         dist[3];
	int         d, bestd = 256 * 256 * 3, bestc = -1;
	int         i;
	colcache_color_t *col;

	if (cache) {
		colcache_color_t search;
		VectorCopy (rgb, search.rgb);
		col = Hash_FindElement (cache->tab, &search);
		if (col)
			return col->col;
	}

	for (i = 0; i < 256; i++) {
		VectorSubtract (pal + i * 3, rgb, dist);
		d = DotProduct (dist, dist);
		if (d < bestd) {
			bestd = d;
			bestc = i;
		}
	}
	if (cache) {
		col = colcache_new_color (rgb, bestc);
		Hash_AddElement (cache->tab, col);
	}
	return bestc;
}

byte
ConvertFloatColor (const float *frgb, const byte *pal, colcache_t *cache)
{
	//FIXME slow!
	int         dist[3];
	int         d, bestd = 256 * 256 * 3, bestc = -1;
	int         i;
	byte        rgb[3];
	colcache_color_t *col;

	VectorScale (frgb, 255, rgb);
	if (cache) {
		colcache_color_t search;
		VectorCopy (rgb, search.rgb);
		col = Hash_FindElement (cache->tab, &search);
		if (col)
			return col->col;
	}

	for (i = 0; i < 256; i++) {
		VectorSubtract (pal + i * 3, rgb, dist);
		d = DotProduct (dist, dist);
		if (d < bestd) {
			bestd = d;
			bestc = i;
		}
	}
	if (cache) {
		col = colcache_new_color (rgb, bestc);
		Hash_AddElement (cache->tab, col);
	}
	return bestc;
}

tex_t *
ConvertImage (const tex_t *tex, const byte *pal)
{
	tex_t      *new;
	int         pixels;
	int         bpp = 3;
	int         i;
	colcache_t *cache;

	pixels = tex->width * tex->height;
	new = malloc (sizeof (tex_t) + pixels);
	new->data = (byte *) (new + 1);
	new->width = tex->width;
	new->height = tex->height;
	new->format = tex_palette;
	new->palette = pal;
	switch (tex->format) {
		case tex_palette:
		case tex_l:			// will not work as expected FIXME
		case tex_a:			// will not work as expected FIXME
			memcpy (new->data, tex->data, pixels);
			break;
		case tex_la:		// will not work as expected FIXME
			for (i = 0; i < pixels; i++)
				new->data[i] = tex->data[i * 2];
			break;
		case tex_rgba:
			bpp = 4;
		case tex_rgb:
			cache = ColorCache_New ();
			for (i = 0; i < pixels; i++)
				new->data[i] = ConvertColor (tex->data + i * bpp, pal, cache);
			ColorCache_Delete (cache);
			break;
		case tex_frgba:
			cache = ColorCache_New ();
			for (i = 0; i < pixels; i++) {
				float      *pix = (float *) (tex->data + i * bpp);
				new->data[i] = ConvertFloatColor (pix, pal, cache);
			}
			ColorCache_Delete (cache);
			break;
	}
	return new;
}
