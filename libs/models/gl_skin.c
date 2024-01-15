/*
	glsl_skin.c

	GLSL Skin support

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/23

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/fbsearch.h"
#include "QF/image.h"
#include "QF/model.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_textures.h"

#include "mod_internal.h"
#include "r_internal.h"

typedef struct {
	const tex_t *tex;
	glskin_t    skin;
} skinpair_t;

#define MAX_GLSKINS 64

static skinpair_t skin_table[256][MAX_GLSKINS];
static int skin_counts[256];

static void
build_skin_8 (const tex_t *tex, int texnum, byte *translate,
			  unsigned scaled_width, unsigned scaled_height, bool alpha)
{
	//  Improvements should be mirrored in GL_ResampleTexture in gl_textures.c
	byte        *inrow;
	byte         pixels[512 * 256], *out;
	unsigned int i, j;
	unsigned int frac, fracstep;

	out = pixels;
	memset (pixels, 0, sizeof (pixels));
	fracstep = tex->width * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow = tex->data + tex->width * (i * tex->height / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j++) {
			out[j] = translate[inrow[frac >> 16]];
			frac += fracstep;
		}
	}

	GL_Upload8_EXT ((byte *) pixels, scaled_width, scaled_height, false,
					alpha);
}

static void
build_skin_32 (const tex_t *tex, int texnum, byte *translate,
			   unsigned scaled_width, unsigned scaled_height, bool alpha)
{
	//  Improvements should be mirrored in GL_ResampleTexture in gl_textures.c
	byte       *inrow;
	unsigned    i, j;
	int         samples = alpha ? gl_alpha_format : gl_solid_format;
	unsigned    frac, fracstep;
	byte        pixels[512 * 256 * 4], *out;
	const byte *pal;
	byte        c;

	out = pixels;
	memset (pixels, 0, sizeof (pixels));
	fracstep = tex->width * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++) {
		inrow = tex->data + tex->width * (i * tex->height / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j++) {
			c = translate[inrow[frac >> 16]];
			pal = vid.palette + c * 3;
			*out++ = *pal++;
			*out++ = *pal++;
			*out++ = *pal++;
			*out++ = (alpha && c == 0) ? 0 : 255;
			frac += fracstep;
		}
	}

	qfglBindTexture (GL_TEXTURE_2D, texnum);
	qfglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (gl_Anisotropy)
        qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						   gl_aniso);
}

static int
skinpair_cmp (const void *_tex, const void *_skinpair)
{
	const tex_t *tex = _tex;
	const skinpair_t *skinpair = _skinpair;
	intptr_t diff = (intptr_t) tex - (intptr_t) skinpair->tex;
	return diff < 0 ? -1 : diff > 0 ? 1 : 0;
}

glskin_t
gl_Skin_Get (const tex_t *tex, const colormap_t *colormap,
			 const byte *texel_base)
{
	byte top = colormap ? colormap->top & 0x0f : TOP_COLOR;
	byte bot = colormap ? colormap->bottom & 0x0f : BOTTOM_COLOR;
	int  ind = top | (bot << 4);
	skinpair_t *sp = fbsearch (tex, skin_table[ind], skin_counts[ind],
							   sizeof (skinpair_t), skinpair_cmp);
	if (sp && sp->tex == tex) {
		return sp->skin;
	}
	if (skin_counts[ind] == MAX_GLSKINS) {
		return (glskin_t) {};
	}
	sp = sp ? sp + 1 : skin_table[ind];
	int insert = sp - skin_table[ind];
	memmove (&skin_table[ind][insert + 1], &skin_table[ind][insert],
			 sizeof (skinpair_t[skin_counts[ind] - insert]));
	skin_counts[ind]++;

	sp->tex = tex;

	auto build_skin = vid.is8bit ? build_skin_8 : build_skin_32;

	unsigned    swidth = min (gl_max_size, 512);
	unsigned    sheight = min (gl_max_size, 256);
	// allow users to crunch sizes down even more if they want
	swidth >>= gl_playermip;
	sheight >>= gl_playermip;
	swidth = max (swidth, 1);
	sheight = max (sheight, 1);

	tex_t wtex = *tex;
	if (wtex.relative) {
		wtex.relative = 0;
		// discarding const :(
		wtex.data = (byte *) (texel_base + (uintptr_t) wtex.data);
	}

	byte palette[256];
	Skin_SetPalette (palette, top, bot);
	qfglGenTextures (1, &sp->skin.id);
	build_skin_32 (&wtex, sp->skin.id, palette, swidth, sheight, false);

	int        size = wtex.width * wtex.height;
	byte       fbskin[size];
	if (Mod_CalcFullbright (fbskin, wtex.data, size)) {
		tex_t       fb_tex = wtex;
		fb_tex.data = fbskin;
		qfglGenTextures (1, &sp->skin.fb);
		build_skin (&fb_tex, sp->skin.fb, palette, swidth, sheight, true);
	}
	return sp->skin;
}

void
gl_Skin_SetupSkin (skin_t *skin, int cmap)
{
	skin->tex = Skin_DupTex (skin->tex);
}

void
gl_Skin_Destroy (skin_t *skin)
{
	for (int i = 0; i < 256; i++) {
		for (int j = skin_counts[i]; j-- > 0; ) {
			auto sp = &skin_table[i][j];
			if (sp->tex == skin->tex) {
				qfglDeleteTextures (1, &sp->skin.id);
				if (sp->skin.fb) {
					qfglDeleteTextures (1, &sp->skin.fb);
				}
				skin_counts[i]--;
				memmove (&skin_table[i][j], &skin_table[i][j + 1],
						 sizeof (skinpair_t[skin_counts[i] - j]));
			}
		}
	}
}
