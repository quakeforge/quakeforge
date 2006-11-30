/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
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

#include <errno.h>

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/wad.h"

#include "wad.h"

typedef struct {
	int         width, height;
	int         widthbits, heightbits;
	unsigned char data[4];
} qtex_t;

#define SCRN(x,y)       (*(image->data+(y)*image->width*4+x))

#if 0
/*
	GrabRaw

	filename RAW x y width height
*/
void
GrabRaw (script_t *script)
{
	int         x, y, xl, yl, xh, yh, w, h;
	byte       *screen_p;
	int         linedelta;

	Script_GetToken (script, false);
	xl = atoi (script->token->str);
	Script_GetToken (script, false);
	yl = atoi (script->token->str);
	Script_GetToken (script, false);
	w = atoi (script->token->str);
	Script_GetToken (script, false);
	h = atoi (script->token->str);

	xh = xl + w;
	yh = yl + h;

	screen_p = byteimage + yl * byteimagewidth + xl;
	linedelta = byteimagewidth - w;

	for (y = yl; y < yh; y++) {
		for (x = xl; x < xh; x++) {
			*lump_p++ = *screen_p;
			*screen_p++ = 0;
		}
		screen_p += linedelta;
	}
}
#endif


/*
	GrabPalette

	filename PALETTE [startcolor endcolor]
*/
void
GrabPalette (script_t *script)
{
	int         start, end, length;

	if (Script_TokenAvailable (script, false)) {
		Script_GetToken (script, false);
		start = atoi (script->token->str);
		Script_GetToken (script, false);
		end = atoi (script->token->str);
	} else {
		start = 0;
		end = 255;
	}

	length = 3 * (end - start + 1);
	lumpbuffer = lump_p = malloc (length);
	memcpy (lump_p, default_palette + start * 3, length);
	lump_p += length;
}

#if 0
/*
	GrabPic

	filename qpic x y width height
*/
void
GrabPic (script_t *script)
{
	int         x, y, xl, yl, xh, yh;
	int         width;
	byte        transcolor;
	qpic_t     *header;

	Script_GetToken (script, false);
	xl = atoi (script->token->str);
	Script_GetToken (script, false);
	yl = atoi (script->token->str);
	Script_GetToken (script, false);
	xh = xl - 1 + atoi (script->token->str);
	Script_GetToken (script, false);
	yh = yl - 1 + atoi (script->token->str);

	if (xh < xl || yh < yl || xl < 0 || yl < 0 || xh > 319 || yh > 199)
		Sys_Error ("GrabPic: Bad size: %i, %i, %i, %i", xl, yl, xh, yh);

	transcolor = 255;


	// fill in header
	header = (qpic_t *) lump_p;
	width = xh - xl + 1;
	header->width = LittleLong (width);
	header->height = LittleLong (yh - yl + 1);

	// start grabbing posts
	lump_p = (byte *) header->data;

	for (y = yl; y <= yh; y++)
		for (x = xl; x <= xh; x++)
			*lump_p++ = SCRN (x, y);
}
#endif

/* COLORMAP GRABBING */

static byte
BestColor (int r, int g, int b, int start, int stop)
{
	int         i;
	int         dr, dg, db;
	int         bestdistortion, distortion;
	int         bestcolor;
	byte       *pal;

	// let any color go to 0 as a last resort
	bestdistortion = ((int) r * r + (int) g * g + (int) b * b) * 2;
	bestcolor = 0;

	pal = default_palette + start * 3;
	for (i = start; i <= stop; i++) {
		dr = r - (int) pal[0];
		dg = g - (int) pal[1];
		db = b - (int) pal[2];
		pal += 3;
		distortion = dr * dr + dg * dg + db * db;
		if (distortion < bestdistortion) {
			if (!distortion)
				return i;				// perfect match

			bestdistortion = distortion;
			bestcolor = i;
		}
	}

	return bestcolor;
}

#if 0

/*
	GrabColormap

	filename COLORMAP levels fullbrights

	the first map is an identiy 0-255
	the final map is all black except for the fullbrights
	the remaining maps are evenly spread
	fullbright colors start at the top of the palette.
*/
void
GrabColormap (script_t *script)
{
	int         levels, brights;
	int         l, c;
	float       frac, red, green, blue;

	Script_GetToken (script, false);
	levels = atoi (script->token->str);
	Script_GetToken (script, false);
	brights = atoi (script->token->str);

	// identity lump
	for (l = 0; l < 256; l++)
		*lump_p++ = l;

	// shaded levels
	for (l = 1; l < levels; l++) {
		frac = 1.0 - (float) l / (levels - 1);
		for (c = 0; c < 256 - brights; c++) {
			red = lbmpalette[c * 3];
			green = lbmpalette[c * 3 + 1];
			blue = lbmpalette[c * 3 + 2];

			red = (int) (red * frac + 0.5);
			green = (int) (green * frac + 0.5);
			blue = (int) (blue * frac + 0.5);

			// note: 254 instead of 255 because 255 is the transparent color,
			// and we don't want anything remapping to that
			*lump_p++ = BestColor (red, green, blue, 0, 254);
		}
		for (; c < 256; c++)
			*lump_p++ = c;
	}

	*lump_p++ = brights;
}

/*
	GrabColormap2

	experimental -- not used by quake

	filename COLORMAP2 range levels fullbrights

	fullbright colors start at the top of the palette.
	Range can be greater than 1 to allow overbright color tables.

	the first map is all 0
	the last (levels-1) map is at range
*/
void
GrabColormap2 (script_t *script)
{
	int         levels, brights;
	int         l, c;
	float       frac, red, green, blue;
	float       range;

	Script_GetToken (script, false);
	range = atof (script->token->str);
	Script_GetToken (script, false);
	levels = atoi (script->token->str);
	Script_GetToken (script, false);
	brights = atoi (script->token->str);

	// shaded levels
	for (l = 0; l < levels; l++) {
		frac = range - range * (float) l / (levels - 1);
		for (c = 0; c < 256 - brights; c++) {
			red = lbmpalette[c * 3];
			green = lbmpalette[c * 3 + 1];
			blue = lbmpalette[c * 3 + 2];

			red = (int) (red * frac + 0.5);
			green = (int) (green * frac + 0.5);
			blue = (int) (blue * frac + 0.5);

			// note: 254 instead of 255 because 255 is the transparent color,
			// and we don't want anything remapping to that
			*lump_p++ = BestColor (red, green, blue, 0, 254);
		}

		// fullbrights allways stay the same
		for (; c < 256; c++)
			*lump_p++ = c;
	}

	*lump_p++ = brights;
}
#endif

/* MIPTEX GRABBING */

typedef struct {
	char        name[16];
	unsigned    width, height;
	unsigned    offsets[4];				// four mip maps stored
} miptex_t;
#if 0
byte        pixdata[256];

int         d_red, d_green, d_blue;

static byte
AveragePixels (int count)
{
	int         r, g, b;
	int         i;
	int         vis;
	int         pix;
	int         dr, dg, db;
	int         bestdistortion, distortion;
	int         bestcolor;
	byte       *pal;
	int         fullbright;
	int         e;

	vis = 0;
	r = g = b = 0;
	fullbright = 0;
	for (i = 0; i < count; i++) {
		pix = pixdata[i];
		if (pix == 255)
			fullbright = 2;
		else if (pix >= 240) {
			return pix;
			if (!fullbright) {
				fullbright = true;
				r = 0;
				g = 0;
				b = 0;
			}
		} else {
			if (fullbright)
				continue;
		}

		r += default_palette[pix * 3];
		g += default_palette[pix * 3 + 1];
		b += default_palette[pix * 3 + 2];
		vis++;
	}

	if (fullbright == 2)
		return 255;

	r /= vis;
	g /= vis;
	b /= vis;

	if (!fullbright) {
		r += d_red;
		g += d_green;
		b += d_blue;
	}
	// find the best color
	bestdistortion = r * r + g * g + b * b;
	bestcolor = 0;
	if (fullbright) {
		i = 240;
		e = 255;
	} else {
		i = 0;
		e = 240;
	}

	for (; i < e; i++) {
		pix = i;						// pixdata[i];

		pal = default_palette + pix * 3;

		dr = r - (int) pal[0];
		dg = g - (int) pal[1];
		db = b - (int) pal[2];

		distortion = dr * dr + dg * dg + db * db;
		if (distortion < bestdistortion) {
			if (!distortion) {
				d_red = d_green = d_blue = 0;	// no distortion yet
				return pix;				// perfect match
			}

			bestdistortion = distortion;
			bestcolor = pix;
		}
	}

	if (!fullbright) {					// error diffusion
		pal = default_palette + bestcolor * 3;
		d_red = r - (int) pal[0];
		d_green = g - (int) pal[1];
		d_blue = b - (int) pal[2];
	}

	return bestcolor;
}
#endif


/*
	GrabMip

	filename MIP x y width height
	must be multiples of sixteen
*/
void
GrabMip (script_t *script)
{
	int         x, y, xl, yl, xh, yh, w, h;
	byte       *screen_p, *source;
	int         linedelta;
	miptex_t   *qtex;
	int         miplevel, mipstep;
	int         xx, yy;
	int         count;
	int         r, g, b;

	Script_GetToken (script, false);
	xl = atoi (script->token->str);
	Script_GetToken (script, false);
	yl = atoi (script->token->str);
	Script_GetToken (script, false);
	w = atoi (script->token->str);
	Script_GetToken (script, false);
	h = atoi (script->token->str);

	if ((w & 15) || (h & 15))
		Sys_Error ("line %i: miptex sizes must be multiples of 16",
				   script->line);

	xh = xl + w;
	yh = yl + h;

	qtex = malloc (sizeof (miptex_t) + w * h / 64 * 85);
	qtex->width = LittleLong (w);
	qtex->height = LittleLong (h);
	strcpy (qtex->name, lumpname->str);

	lumpbuffer = (byte *) qtex;
	lump_p = (byte *) &qtex->offsets[4];

	screen_p = image->data + yl * image->width * 4 + xl;
	linedelta = (image->width - w) * 4;

	source = image->data;
	qtex->offsets[0] = LittleLong (lump_p - (byte *) qtex);

	for (y = yl; y < yh; y++) {
		for (x = xl; x < xh; x++) {
			r = *screen_p++;
			g = *screen_p++;
			b = *screen_p++;
			screen_p++;					// skip over alpha
			*lump_p++ = BestColor (r, g, b, 0, 239);
		}
		screen_p += linedelta;
	}

	// subsample for greater mip levels
	//d_red = d_green = d_blue = 0;		// no distortion yet

	for (miplevel = 1; miplevel < 4; miplevel++) {
		qtex->offsets[miplevel] = LittleLong (lump_p - (byte *) qtex);

		mipstep = 1 << miplevel;
		for (y = 0; y < h; y += mipstep) {

			for (x = 0; x < w; x += mipstep) {
				count = 0;
				r = g = b = 0;
				for (yy = 0; yy < mipstep; yy++)
					for (xx = 0; xx < mipstep; xx++) {
						r += source [((y + yy) * w + x + xx) * 4 + 0];
						g += source [((y + yy) * w + x + xx) * 4 + 1];
						b += source [((y + yy) * w + x + xx) * 4 + 2];
						count++;
					}
				r /= count;
				g /= count;
				b /= count;
				*lump_p++ = BestColor (r, g, b, 0, 239);
			}
		}
	}
}
