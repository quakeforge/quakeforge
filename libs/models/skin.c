/*
	skin.c

	Skin support

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

#include "QF/hash.h"
#include "QF/image.h"
#include "QF/model.h"
#include "QF/pcx.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/vid_render.h"

#include "mod_internal.h"

typedef struct skinbank_s {
	char       *name;
	tex_t      *texels;
	int         users;
} skinbank_t;

vid_model_funcs_t *m_funcs;

// each translation has one extra line for palette (vs colormap) based
// translation (for 32bit rendering)
static byte translations[MAX_TRANSLATIONS][(VID_GRADES + 1) * 256];
static hashtab_t *skin_cache;

static skin_t *
new_skin (void)
{
	return calloc (1, sizeof (skin_t));
}

VISIBLE void
Skin_SetTranslation (int cmap, int top, int bottom)
{
	int         i, j;
	byte       *source;
	byte       *dest;

	if (!cmap)		// 0 is meant for no custom mapping. this just makes
		return;		// other code simpler
	top = bound (0, top, 13) * 16;
	bottom = bound (0, bottom, 13) * 16;

	if (cmap < 0 || cmap > MAX_TRANSLATIONS) {
		Sys_MaskPrintf (SYS_SKIN, "invalid skin slot: %d\n", cmap);
		cmap = 1;
	}

	dest = translations[cmap - 1];
	source = r_data->vid->colormap8;
	memcpy (dest, source, VID_GRADES * 256);

	for (i = 0; i < VID_GRADES; i++, dest += 256, source += 256) {
		if (top < 128)	// the artists made some backwards ranges.
			memcpy (dest + TOP_RANGE, source + top, 16);
		else
			for (j = 0; j < 16; j++)
				dest[TOP_RANGE + j] = source[top + 15 - j];

		if (bottom < 128)
			memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
		else
			for (j = 0; j < 16; j++)
				dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
	}
	// set up the palette translation
	// dest currently points to the palette line
	for (i = 0; i < 256; i++)
		dest[i] = i;
	for (i = 0; i < 16; i++) {
		if (top < 128)
			dest[TOP_RANGE + i] = top + i;
		else
			dest[TOP_RANGE + i] = top + 15 - i;
		if (bottom < 128)
			dest[BOTTOM_RANGE + i] = bottom + i;
		else
			dest[BOTTOM_RANGE + i] = bottom + 15 - i;
	}
	m_funcs->Skin_ProcessTranslation (cmap, translations[cmap - 1]);
}

skin_t *
Skin_SetColormap (skin_t *skin, int cmap)
{
	if (!skin)
		skin = new_skin ();
	skin->colormap = 0;
	if (cmap < 0 || cmap > MAX_TRANSLATIONS) {
		Sys_MaskPrintf (SYS_SKIN, "invalid skin slot: %d\n", cmap);
		cmap = 0;
	}
	if (cmap)
		skin->colormap = translations[cmap - 1];
	m_funcs->Skin_SetupSkin (skin, cmap);
	return skin;
}

skin_t *
Skin_SetSkin (skin_t *skin, int cmap, const char *skinname)
{
	char       *name = 0;
	skinbank_t *sb = 0;
	tex_t      *tex = 0;

	if (skinname) {
		name = QFS_CompressPath (skinname);
		QFS_StripExtension (name, name);
		if (strchr (name, '.') || strchr (name, '/')) {
			Sys_Printf ("Bad skin name: '%s'\n", skinname);
			free (name);
			name = 0;
		}
	}

	do {
		QFile      *file;
		byte       *ipix, *opix;
		int         i;
		tex_t      *out;

		if (!name)
			break;
		sb = Hash_Find (skin_cache, name);
		if (sb) {
			sb->users++;
			tex = sb->texels;
			break;
		}

		if (Hash_NumElements (skin_cache) >= MAX_CACHED_SKINS) {
			Sys_Printf ("Too many skins\n");
			free (name);
			name = 0;
			break;
		}

		file = QFS_FOpenFile (va ("skins/%s.pcx", name));
		if (!file) {
			Sys_Printf ("Couldn't load skin %s\n", name);
			free (name);
			name = 0;
			break;
		}
		tex = LoadPCX (file, 0, r_data->vid->palette, 1);
		Qclose (file);
		if (!tex || tex->width > 320 || tex->height > 200) {
			Sys_Printf ("Bad skin %s\n", name);
			free (name);
			name = 0;
			tex = 0;
			break;
		}
		out = malloc (sizeof (tex_t) + PLAYER_WIDTH*PLAYER_HEIGHT);
		out->data = (byte *) (out + 1);
		out->width = PLAYER_WIDTH;
		out->height = PLAYER_HEIGHT;
		out->format = tex_palette;
		out->palette = r_data->vid->palette;
		memset (out->data, 0, PLAYER_WIDTH * PLAYER_HEIGHT);
		opix = out->data;
		ipix = tex->data;
		for (i = 0; i < out->height; i++) {
			memcpy (opix, ipix, min (tex->width, out->width));
			ipix += tex->width;
			opix += out->width;
		}
		tex = out;

		sb = malloc (sizeof (skinbank_t));
		sb->texels = tex;
		sb->name = name;
		sb->users = 1;
		Hash_Add (skin_cache, sb);
	} while (0);

	if (!skin)
		skin = new_skin ();
	skin->texels = tex;
	skin->name = name;
	m_funcs->Skin_SetupSkin (skin, cmap);
	return skin;
}

static const char *
skin_getkey (const void *sb, void *unused)
{
	return ((skinbank_t *) sb)->name;
}

static void
skin_free (void *_sb, void *unused)
{
	skinbank_t *sb = (skinbank_t *) _sb;

	free (sb->name);
	free (sb->texels);
	free (sb);
}

void
Skin_Init (void)
{
	skin_cache = Hash_NewTable (127, skin_getkey, skin_free, 0, 0);
	m_funcs->Skin_InitTranslations ();
}

VISIBLE int
Skin_CalcTopColors (const byte *in, byte *out, int pixels)
{
	byte        tc = 0;

	while (pixels-- > 0) {
		byte        pix = *in++;
		byte        a = (pix < TOP_RANGE) - 1;
		byte        b = (pix > TOP_RANGE + 15) - 1;
		byte        mask = ~(a & b);
		tc |= mask;
		// mask is 0 for top color otherwise 0xff
		*out++ = (pix - TOP_RANGE) | mask;
	}
	return tc;
}

VISIBLE int
Skin_CalcBottomColors (const byte *in, byte *out, int pixels)
{
	byte        bc = 0;

	while (pixels-- > 0) {
		byte        pix = *in++;
		byte        a = (pix < BOTTOM_RANGE) - 1;
		byte        b = (pix > BOTTOM_RANGE + 15) - 1;
		byte        mask = ~(a & b);
		bc |= mask;
		// mask is 0 for bottom color otherwise 0xff
		*out++ = (pix - BOTTOM_RANGE) | mask;
	}
	return bc;
}

VISIBLE void
Skin_ClearTopColors (const byte *in, byte *out, int pixels)
{
	while (pixels-- > 0) {
		byte        pix = *in++;
		byte        a = (pix < TOP_RANGE) - 1;
		byte        b = (pix > TOP_RANGE + 15) - 1;
		byte        mask = (a & b);
		// mask is 0xff for top color otherwise 0
		*out++ = (pix - TOP_RANGE) | mask;
	}
}

VISIBLE void
Skin_ClearBottomColors (const byte *in, byte *out, int pixels)
{
	while (pixels-- > 0) {
		byte        pix = *in++;
		byte        a = (pix < BOTTOM_RANGE) - 1;
		byte        b = (pix > BOTTOM_RANGE + 15) - 1;
		byte        mask = (a & b);
		// mask is 0xff for bottom color otherwise 0
		*out++ = (pix - BOTTOM_RANGE) | mask;
	}
}
