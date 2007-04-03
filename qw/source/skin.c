/*
	skin.c

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

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/image.h"
#include "QF/pcx.h"
#include "QF/quakefs.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "client.h"
#include "compat.h"
#include "skin_stencil.h"

#define MAX_TEMP_SKINS 64	//XXX dynamic?

extern cvar_t *noskins;	//FIXME shouldn't be here

cvar_t     *baseskin;

char        allskins[128];

skin_t      skin_cache[MAX_CACHED_SKINS];
static hashtab_t  *skin_hash;
static int         numskins;
static skin_t      temp_skins[MAX_TEMP_SKINS];
static int         num_temp_skins;
static int         fullfb;

int         skin_textures;
int         skin_fb_textures;

static const char *
skin_get_key (void *_skin, void *unused)
{
	skin_t *skin = (skin_t*)_skin;
	return skin->name;
}


/*
	Skin_Find

	Determines the best skin for the given scoreboard
	slot, and sets scoreboard->skin
*/
void
Skin_Find (player_info_t *sc)
{
	skin_t     *skin;
	char        name[128];
	const char *s = NULL;

	if (allskins[0]) {
		strncpy (name, allskins, sizeof (name));
	} else {
		if ((s = sc->skinname->value) && s[0])
			strncpy (name, s, sizeof (name));
		else
			strncpy (name, baseskin->string, sizeof (name));
	}

	if (strstr (name, "..") || *name == '.')
		strcpy (name, "base");

	if (!allskins[0] && (!s || !strcaseequal (name, s)))
		Info_SetValueForKey (sc->userinfo, "skin", name, 1);

	QFS_StripExtension (name, name);

	skin = Hash_Find (skin_hash, name);
	if (skin) {
		sc->skin = skin;
		Skin_Cache (sc->skin);
		return;
	}

	if (numskins == MAX_CACHED_SKINS) {
		// ran out of spots, so flush everything
		Skin_Flush ();
		return;
	}

	skin = &skin_cache[numskins];
	sc->skin = skin;
	numskins++;

	memset (skin, 0, sizeof (*skin));
	strncpy (skin->name, name, sizeof (skin->name) - 1);

	Hash_Add (skin_hash, skin);
}

/*
	Skin_Cache

	Returns a pointer to the skin bitmap, or NULL to use the default
*/
tex_t *
Skin_Cache (skin_t *skin)
{
	char        name[1024];
	tex_t      *out;
	QFile      *file;
	tex_t      *tex;
	int         pixels;
	byte       *ipix, *opix;
	int         i, numfb;

	if (cls.downloadtype == dl_skin)		// use base until downloaded
		return NULL;
	// NOSKINS > 1 will show skins, but not download new ones.
	if (noskins->int_val == 1) //XXX FIXME
		return NULL;

	if (skin->failedload)
		return NULL;

	out = Cache_Check (&skin->data.cache);
	if (out)
		return out;

	// load the pic from disk
	snprintf (name, sizeof (name), "skins/%s.pcx", skin->name);
	QFS_FOpenFile (name, &file);
	if (!file) {
		Con_Printf ("Couldn't load skin %s\n", name);
		snprintf (name, sizeof (name), "skins/%s.pcx", baseskin->string);
		QFS_FOpenFile (name, &file);
		if (!file) {
			skin->failedload = true;
			return NULL;
		}
	}

	pixels = 320 * 200;
	out = Cache_Alloc (&skin->data.cache, field_offset (tex_t, data[pixels]),
					   skin->name);
	if (!out)
		Sys_Error ("Skin_Cache: couldn't allocate");

	tex = LoadPCX (file, 0, vid.palette);
	Qclose (file);

	if (!tex || tex->width > 320 || tex->height > 200) {
		skin->failedload = true;
		Con_Printf ("Bad skin %s\n", name);
		return NULL;
	}

	opix = out->data;
	out->width = 320;
	out->height = 200;
	out->palette = tex->palette; //FIXME assumes 0 or vid.palette
	memset (opix, 0, pixels);
	for (i = 0, ipix = tex->data; i < tex->height;
	     i++, opix += 320, ipix += tex->width)
		memcpy (opix, ipix, tex->width);

	Skin_Process (skin, out);

	skin->failedload = false;

	for (i = 0,numfb = 0; i < 320*200; i++)
		if (skin_stencil[i] && out->data[i] >= 256 - 32)
			numfb++;
	skin->numfb = numfb;
	return out;
}


void
Skin_Flush (void)
{
	int         i;

	for (i = 0; i < numskins; i++) {
		if (skin_cache[i].data.cache.data)
			Cache_Free (&skin_cache[i].data.cache);
	}
	numskins = 0;
	Hash_FlushTable (skin_hash);
}

skin_t *
Skin_NewTempSkin (void)
{
	if (num_temp_skins == MAX_TEMP_SKINS)
		return 0;	// ran out
	return &temp_skins[num_temp_skins++];
}

void
Skin_ClearTempSkins (void)
{
	num_temp_skins = 0;
}


int
Skin_Init_Textures (int base)
{
	skin_textures = base;
	base += MAX_TEMP_SKINS;
	skin_fb_textures = base;
	base += MAX_TEMP_SKINS;
	return base;
}


void
Skin_Init (void)
{
	int         i;

	skin_hash = Hash_NewTable (1021, skin_get_key, 0, 0);
	Skin_Init_Translation ();
	for (i = 0, fullfb = 0; i < 320*200; i++)
		fullfb += skin_stencil[i];
}


void
Skin_Init_Cvars (void)
{
	baseskin = Cvar_Get ("baseskin", "base", CVAR_NONE, NULL,
						 "default base skin name");
}

int
Skin_FbPercent (const char *skin_name)
{
	int         i, totalfb = 0;
	skin_t     *skin = 0;

	if (skin_name) {
		skin = Hash_Find (skin_hash, skin_name);
		if (skin)
			return skin->numfb * 1000 / fullfb;
		return -1;
	}
	for (i = 0; i < numskins; i++)
		totalfb += skin_cache[i].numfb;
	return totalfb * 1000 / (numskins * fullfb);
}
