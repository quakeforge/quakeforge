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

typedef struct {
	tex_t      *tex;
	tex_t      *fb_tex;
	qboolean    fb;
} glskin_t;

static int skin_textures;
static int skin_fb_textures;
static byte skin_cmap[MAX_TRANSLATIONS][256];

static glskin_t skins[MAX_TRANSLATIONS];
static glskin_t player_skin;

static void
do_fb_skin (glskin_t *s)
{
	int         size = s->tex->width * s->tex->height;

	s->fb_tex = realloc (s->fb_tex, sizeof (tex_t) + size);
	s->fb_tex->data = (byte *) (s->fb_tex + 1);
	s->fb_tex->width = s->tex->width;
	s->fb_tex->height = s->tex->height;
	s->fb_tex->format = tex_palette;
	s->fb_tex->palette = vid.palette;
	s->fb = Mod_CalcFullbright (s->fb_tex->data, s->tex->data, size);
}

void
gl_Skin_SetPlayerSkin (int width, int height, const byte *data)
{
	int         size = width * height;
	glskin_t   *s;

	s = &player_skin;
	s->tex = realloc (s->tex, sizeof (tex_t) + size);
	s->tex->data = (byte *) (s->tex + 1);
	s->tex->width = width;
	s->tex->height = height;
	s->tex->format = tex_palette;
	s->tex->palette = vid.palette;
	memcpy (s->tex->data, data, size);

	do_fb_skin (s);
}

static void
build_skin_8 (tex_t *tex, int texnum, byte *translate,
			  unsigned scaled_width, unsigned scaled_height, qboolean alpha)
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
			   unsigned scaled_width, unsigned scaled_height, qboolean alpha)
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
			*out++ = (alpha && c == 255) ? 0 : 255;
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

static void
build_skin (skin_t *skin, int cmap)
{
	glskin_t   *s;
	unsigned    scaled_width, scaled_height;
	int         texnum, fb_texnum;

	// FIXME deek: This 512x256 limit sucks!
	scaled_width = min (gl_max_size->int_val, 512);
	scaled_height = min (gl_max_size->int_val, 256);

	// allow users to crunch sizes down even more if they want
	scaled_width >>= gl_playermip->int_val;
	scaled_height >>= gl_playermip->int_val;
	scaled_width = max (scaled_width, 1);
	scaled_height = max (scaled_height, 1);

	s = skins + cmap;
	if (!s->tex)
		s = &player_skin;
	if (!s->tex)	// we haven't loaded the player model yet
		return;

	texnum = skin_textures + cmap;
	fb_texnum = 0;
	if (s->fb)
		fb_texnum = skin_fb_textures + cmap;
	if (skin) {
		skin->texnum = texnum;
		skin->auxtex = fb_texnum;
	}
	if (vid.is8bit) {
		build_skin_8 (s->tex, texnum, skin_cmap[cmap],
					  scaled_width, scaled_height, false);
		if (s->fb && s->fb_tex)
			build_skin_8 (s->fb_tex, fb_texnum, skin_cmap[cmap],
						  scaled_width, scaled_height, true);
	} else {
		build_skin_32 (s->tex, texnum, skin_cmap[cmap],
					   scaled_width, scaled_height, false);
		if (s->fb && s->fb_tex)
			build_skin_32 (s->fb_tex, fb_texnum, skin_cmap[cmap],
						   scaled_width, scaled_height, true);
	}
}

void
gl_Skin_ProcessTranslation (int cmap, const byte *translation)
{
	int         changed;

	// simplify cmap usage (texture offset/array index)
	cmap--;
	// skip over the colormap (GL can't use it) to the translated palette
	translation += VID_GRADES * 256;
	changed = memcmp (skin_cmap[cmap], translation, 256);
	memcpy (skin_cmap[cmap], translation, 256);
	if (!changed)
		return;
	build_skin (0, cmap);
}

void
gl_Skin_SetupSkin (skin_t *skin, int cmap)
{
	int         changed;
	glskin_t   *s;

	skin->texnum = 0;
	skin->auxtex = 0;
	if (!cmap) {
		return;
	}
	// simplify cmap usage (texture offset/array index)
	cmap--;
	s = skins + cmap;
	changed = (s->tex != skin->texels);
	s->tex = skin->texels;
	if (!changed) {
		skin->texnum = skin_textures + cmap;
		if (s->fb)
			skin->auxtex = skin_fb_textures + cmap;
		return;
	}
	if (s->tex)
		do_fb_skin (s);
	build_skin (skin, cmap);
}

void
gl_Skin_InitTranslations (void)
{
}

int
gl_Skin_Init_Textures (int base)
{
	skin_textures = base;
	base += MAX_TRANSLATIONS;
	skin_fb_textures = base;
	base += MAX_TRANSLATIONS;
	return base;
}
