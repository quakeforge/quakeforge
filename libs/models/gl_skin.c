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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>

#include "QF/image.h"
#include "QF/model.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"

static int skin_textures;
static int skin_fb_textures;
static byte skin_cmap[MAX_TRANSLATIONS][256];
static tex_t *skin_tex[MAX_TRANSLATIONS];
static tex_t *player_tex;

void
Skin_SetPlayerSkin (int width, int height, const byte *data)
{
	int         size = width * height;

	player_tex = realloc (player_tex, field_offset(tex_t, data[size]));
	player_tex->width = width;
	player_tex->height = height;
	player_tex->format = tex_palette;
	player_tex->palette = vid.palette;
	memcpy (player_tex->data, data, size);
}

void
Skin_ProcessTranslation (int cmap, const byte *translation)
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
}

void
Skin_SetupSkin (skin_t *skin, int cmap)
{
	int         changed;

	// simplify cmap usage (texture offset/array index)
	cmap--;
	changed = (skin_tex[cmap] != skin->texels);
	skin_tex[cmap] = skin->texels;
	if (!changed)
		return;
}

void
Skin_InitTranslations (void)
{
}

int
Skin_Init_Textures (int base)
{
	skin_textures = base;
	base += MAX_TRANSLATIONS;
	skin_fb_textures = base;
	base += MAX_TRANSLATIONS;
	return base;
}
