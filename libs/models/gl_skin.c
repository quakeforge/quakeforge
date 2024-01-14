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

// FIXME colormap (top/bottom colors)
//static byte skin_cmap[MAX_TRANSLATIONS][256];

static void
build_skin_8 (tex_t *tex, int texnum, byte *translate,
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
build_skin_32 (tex_t *tex, int texnum, byte *translate,
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

void
gl_Skin_SetupSkin (skin_t *skin, int cmap)
{
	//skin->tex = Skin_DupTex (skin->tex);
	tex_t      *tex = skin->tex;
	skin->tex = nullptr;	// tex memory is only temporarily allocated

	auto build_skin = vid.is8bit ? build_skin_8 : build_skin_32;

	unsigned    swidth = min (gl_max_size, 512);
	unsigned    sheight = min (gl_max_size, 256);
	// allow users to crunch sizes down even more if they want
	swidth >>= gl_playermip;
	sheight >>= gl_playermip;
	swidth = max (swidth, 1);
	sheight = max (sheight, 1);

	int        size = tex->width * tex->height;
	byte       fbskin[size];
	qfglGenTextures (1, &skin->id);
	// FIXME colormap (top/bottom colors)
	build_skin_32 (tex, skin->id, vid.colormap8, swidth, sheight, false);
	if (Mod_CalcFullbright (fbskin, tex->data, size)) {
		tex_t       fb_tex = *tex;
		fb_tex.data = fbskin;
		qfglGenTextures (1, &skin->fb);
		build_skin (&fb_tex, skin->fb, vid.colormap8, swidth, sheight, true);
	}
}

void
gl_Skin_Destroy (skin_t *skin)
{
	qfglDeleteTextures (1, &skin->id);
	if (skin->fb) {
		qfglDeleteTextures (1, &skin->fb);
	}
}
