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

#include "QF/ecs.h"
#include "QF/hash.h"
#include "QF/image.h"
#include "QF/model.h"
#include "QF/pcx.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/vid_render.h"

#include "mod_internal.h"

vid_model_funcs_t *m_funcs;

static void
skin_name_destroy (void *_name, ecs_registry_t *reg)
{
	char      **name = _name;
	free (*name);
}

static void
skin_destroy (void *_skin, ecs_registry_t *reg)
{
	skin_t     *skin = _skin;
	if (m_funcs->skin_destroy) {
		m_funcs->skin_destroy (skin);
	}
}

static const component_t skin_components[skin_comp_count] = {
	[skin_name] = {
		.size = sizeof (char *),
		.name = "skin name",
		.destroy = skin_name_destroy,
	},
	[skin_skin] = {
		.size = sizeof (skin_t),
		.name = "skin",
		.destroy = skin_destroy,
	},
	[skin_colors] = {
	},
};

static hashtab_t *skin_hash;
static ecs_system_t skinsys;

VISIBLE void
Skin_SetColormap (byte *dest, int top, int bottom)
{
	byte       *source;

	top = bound (0, top, 13) * 16;
	bottom = bound (0, bottom, 13) * 16;

	source = r_data->vid->colormap8;
	memcpy (dest, source, VID_GRADES * 256);

	for (int i = 0; i < VID_GRADES; i++, dest += 256, source += 256) {
		if (top < 128) {
			memcpy (dest + TOP_RANGE, source + top, 16);
		} else {
			// the artists made some backwards ranges.
			for (int j = 0; j < 16; j++) {
				dest[TOP_RANGE + j] = source[top + 15 - j];
			}
		}

		if (bottom < 128) {
			memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
		} else {
			for (int j = 0; j < 16; j++) {
				dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
			}
		}
	}
}

VISIBLE void
Skin_SetPalette (byte *dest, int top, int bottom)
{
	top = bound (0, top, 13) * 16;
	bottom = bound (0, bottom, 13) * 16;

	for (int i = 0; i < 256; i++) {
		dest[i] = i;
	}
	for (int i = 0; i < 16; i++) {
		if (top < 128) {
			dest[TOP_RANGE + i] = top + i;
		} else {
			dest[TOP_RANGE + i] = top + 15 - i;
		}
		if (bottom < 128) {
			dest[BOTTOM_RANGE + i] = bottom + i;
		} else {
			dest[BOTTOM_RANGE + i] = bottom + 15 - i;
		}
	}
}

static void
freestr (char **strptr)
{
	free (*strptr);
}

VISIBLE uint32_t
Skin_SetSkin (const char *skinname, int cmap)
{
	if (!skinname || !*skinname) {
		return nullskin;
	}
	__attribute__((cleanup (freestr))) char *name = QFS_CompressPath (skinname);
	QFS_StripExtension (name, name);
	if (strchr (name, '.') || strchr (name, '/')) {
		Sys_Printf ("Bad skin name: '%s'\n", skinname);
		return nullskin;
	}
	void *_id = Hash_Find (skin_hash, name);
	if (_id) {
		return (uint32_t) (uintptr_t) _id;
	}

	// always create a new skin entity so the name can be associated with
	// a possibly bad skin (to prevent unnecessary retries)
	uint32_t    skinent = ECS_NewEntity (skinsys.reg);
	char       *sname = strdup (name);
	Ent_SetComponent (skinent, skinsys.base + skin_name, skinsys.reg, &sname);
	Hash_Add (skin_hash, (void *) (uintptr_t) skinent);

	QFile      *file = QFS_FOpenFile (va (0, "skins/%s.pcx", name));
	if (!file) {
		Sys_Printf ("Couldn't load skin %s\n", name);
		return skinent;
	}
	tex_t      *tex = LoadPCX (file, 0, r_data->vid->palette, 1);
	Qclose (file);
	if (!tex) {
		Sys_Printf ("Bad skin %s\n", name);
		return skinent;
	}

	skin_t      skin = {
		.tex = tex,
	};
	m_funcs->skin_setupskin (&skin, cmap);
	Ent_SetComponent (skinent, skinsys.base + skin_skin, skinsys.reg, &skin);

	return skinent;
}

skin_t *
Skin_Get (uint32_t skin)
{
	if (ECS_EntValid (skin, skinsys.reg)
		&& Ent_HasComponent (skin, skinsys.base + skin_skin, skinsys.reg)) {
		return Ent_GetComponent (skin, skinsys.base + skin_skin, skinsys.reg);
	}
	return nullptr;
}

static const char *
skin_getkey (const void *_id, void *_sys)
{
	uint32_t    id = (uint32_t) (uintptr_t) _id;
	auto        sys = (ecs_system_t *) _sys;
	char      **name = Ent_GetComponent (id, sys->base + skin_name, sys->reg);
	return *name;
}

static void
skin_free (void *_id, void *_sys)
{
	uint32_t    id = (uint32_t) (uintptr_t) _id;
	auto        sys = (ecs_system_t *) _sys;
	ECS_DelEntity (sys->reg, id);
}

static void
skin_shutdown (void *data)
{
	Hash_DelTable (skin_hash);
	ECS_DelRegistry (skinsys.reg);
}

void
Skin_Init (void)
{
	qfZoneScoped (true);
	Sys_RegisterShutdown (skin_shutdown, 0);
	skin_hash = Hash_NewTable (127, skin_getkey, skin_free, &skinsys, 0);
	auto reg = ECS_NewRegistry ("skins");
	skinsys = (ecs_system_t) {
		.reg = reg,
		.base = ECS_RegisterComponents (reg, skin_components, skin_comp_count),
	};
	ECS_CreateComponentPools (reg);
	ECS_NewEntity (reg);	// reserve entity 0
}

VISIBLE int
Skin_CalcTopColors (byte *out, const byte *in, size_t pixels, int stride)
{
	byte        tc = 0;

	while (pixels-- > 0) {
		byte        pix = *in++;
		if (pix >= TOP_RANGE && pix < TOP_RANGE + 16) {
			tc = 1;
			*out = (pix - TOP_RANGE) * 16 + 8;
		} else {
			*out = 0;
		}
		out += stride;
	}
	return tc;
}

VISIBLE int
Skin_CalcTopMask (byte *out, const byte *in, size_t pixels, int stride)
{
	byte        tc = 0;

	while (pixels-- > 0) {
		byte        pix = *in++;
		if (pix >= TOP_RANGE && pix < TOP_RANGE + 16) {
			tc = 1;
			*out = 0xff;
		} else {
			*out = 0;
		}
		out += stride;
	}
	return tc;
}

VISIBLE int
Skin_CalcBottomColors (byte *out, const byte *in, size_t pixels, int stride)
{
	byte        bc = 0;

	while (pixels-- > 0) {
		byte        pix = *in++;
		if (pix >= BOTTOM_RANGE && pix < BOTTOM_RANGE + 16) {
			bc = 1;
			*out = (pix - BOTTOM_RANGE) * 16 + 8;
		} else {
			*out = 0;
		}
		out += stride;
	}
	return bc;
}

VISIBLE int
Skin_CalcBottomMask (byte *out, const byte *in, size_t pixels, int stride)
{
	byte        bc = 0;

	while (pixels-- > 0) {
		byte        pix = *in++;
		if (pix >= BOTTOM_RANGE && pix < BOTTOM_RANGE + 16) {
			bc = 1;
			*out = 0xff;
		} else {
			*out = 0;
		}
		out += stride;
	}
	return bc;
}

VISIBLE int
Skin_ClearTopColors (byte *out, const byte *in, size_t pixels)
{
	while (pixels-- > 0) {
		byte        pix = *in++;
		if (pix >= TOP_RANGE && pix < TOP_RANGE + 16) {
			*out++ = 0;
		} else {
			*out++ = pix;
		}
	}
	return 0;
}

VISIBLE int
Skin_ClearBottomColors (byte *out, const byte *in, size_t pixels)
{
	while (pixels-- > 0) {
		byte        pix = *in++;
		if (pix >= BOTTOM_RANGE && pix < BOTTOM_RANGE + 16) {
			*out++ = 0;
		} else {
			*out++ = pix;
		}
	}
	return 0;
}

VISIBLE tex_t *
Skin_DupTex (const tex_t *tex)
{
	int        size = tex->width * tex->height;
	tex_t     *dup = malloc (sizeof (tex_t) + size);
	*dup = *tex;
	dup->data = (byte *) (dup + 1);
	memcpy (dup->data, tex->data, size);
	return dup;
}
