/*
	gl_skin.c

	(description)

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/compat.h"
#include "QF/skin.h"
#include "QF/texture.h"

#include "glquake.h"
#include "render.h"

static byte translate[256];
static unsigned int translate32[256];

void
Skin_Set_Translate (int top, int bottom, byte *dest)
{
	int         i;

	top = bound (0, top, 13) * 16;
	bottom = bound (0, bottom, 13) * 16;

	for (i = 0; i < 16; i++) {
		if (top < 128)				// the artists made some backwards
									// ranges.  sigh.
			translate[TOP_RANGE + i] = top + i;
		else
			translate[TOP_RANGE + i] = top + 15 - i;

		if (bottom < 128)
			translate[BOTTOM_RANGE + i] = bottom + i;
		else
			translate[BOTTOM_RANGE + i] = bottom + 15 - i;
	}
	if (!VID_Is8bit()) {			// 32 bit upload, the norm.
		for (i = 0; i < 256; i++)
			translate32[i] = d_8to24table[translate[i]];
	}
}

static void
build_skin_8 (byte * original, int tinwidth, int tinheight,
			  unsigned int scaled_width, unsigned int scaled_height,
			  int inwidth, qboolean alpha)
{
	unsigned int frac, fracstep;
	byte       *inrow;
	byte        pixels[512 * 256], *out;
	int         i, j;

	out = pixels;
	memset (pixels, 0, sizeof (pixels));
	fracstep = tinwidth * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow = original + inwidth * (i * tinheight / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j += 4) {
			out[j] = translate[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 1] = translate[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 2] = translate[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 3] = translate[inrow[frac >> 16]];
			frac += fracstep;
		}
	}

	GL_Upload8_EXT ((byte *) pixels, scaled_width, scaled_height, false, alpha);
}

static void
build_skin_32 (byte * original, int tinwidth, int tinheight,
			   unsigned int scaled_width, unsigned int scaled_height,
			   int inwidth, qboolean alpha)
{
	unsigned int frac, fracstep;
	byte       *inrow;
	unsigned int pixels[512 * 256], *out;
	int         i, j;
	int         samples = alpha ? gl_alpha_format : gl_solid_format;

	out = pixels;
	memset (pixels, 0, sizeof (pixels));
	fracstep = tinwidth * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow = original + inwidth * (i * tinheight / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j += 4) {
			out[j] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 1] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 2] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 3] = translate32[inrow[frac >> 16]];
			frac += fracstep;
		}
	}

	glTexImage2D (GL_TEXTURE_2D, 0, samples,
				  scaled_width, scaled_height, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, pixels);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void
build_skin (int texnum, byte *ptexels, int width, int height, qboolean alpha)
{
	int         tinwidth, tinheight;
	unsigned int scaled_width, scaled_height;

	// locate the original skin pixels
	tinwidth = 296;						// real model width
	tinheight = 194;					// real model height

	// because this happens during gameplay, do it fast
	// instead of sending it through GL_Upload8()
	glBindTexture (GL_TEXTURE_2D, texnum);

	// FIXME deek: This 512x256 limit sucks!
	scaled_width = min (gl_max_size->int_val, 512);
	scaled_height = min (gl_max_size->int_val, 256);

	// allow users to crunch sizes down even more if they want
	scaled_width >>= gl_playermip->int_val;
	scaled_height >>= gl_playermip->int_val;

	if (VID_Is8bit ()) {				// 8bit texture upload
		build_skin_8 (ptexels, tinwidth, tinheight, scaled_width,
					  scaled_height, width, alpha);
	} else {
		build_skin_32 (ptexels, tinwidth, tinheight, scaled_width,
					   scaled_height, width, alpha);
	}
}

void
Skin_Do_Translation (skin_t *player_skin, int slot)
{
	int         texnum;
	int         inwidth, inheight;
	byte       *original;
	tex_t      *skin;

	if ((skin = (tex_t*)Skin_Cache (player_skin)) != NULL) {
		// skin data width
		inwidth = 320;
		inheight = 200;
		original = skin->data;
	} else {
		original = player_8bit_texels;
		inwidth = 296;
		inheight = 194;
	}
	texnum = playertextures + slot;
	build_skin (texnum, original, inwidth, inheight, false);
}

void
Skin_Init_Translation (void)
{
	int         i;

	for (i = 0; i < 256; i++) {
		translate[i] = i;
		translate32[i] = d_8to24table[i];
	}
}

void
Skin_Process (skin_t *skin, tex_t *tex)
{
	int pixels = tex->width * tex->height;
	byte *ptexels = Hunk_TempAlloc (pixels);

	if (Mod_CalcFullbright (tex->data, ptexels, pixels)) {
		skin->fb_texture = player_fb_textures + (skin - skin_cache);
		build_skin (skin->fb_texture, ptexels, tex->width, tex->height, true);
	}
}
