/*
	model.c

	model loading and caching

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

// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vfs.h"

void        Mod_LoadAliasModel (model_t *mod, void *buf);
void        Mod_LoadSpriteModel (model_t *mod, void *buf);
void        Mod_LoadBrushModel (model_t *mod, void *buf);

model_t    *loadmodel;
char        loadname[32];				// for hunk tags

#define	MAX_MOD_KNOWN	512
model_t     mod_known[MAX_MOD_KNOWN];
int         mod_numknown;

cvar_t     *gl_subdivide_size;

extern byte mod_novis[MAX_MAP_LEAFS / 8];

texture_t  *r_notexture_mip;

/*
	Mod_Init
*/
void
Mod_Init (void)
{
	int x, y, m;
	byte *dest;
	int mip0size = 16*16;
	int mip1size = 8*8;
	int mip2size = 4*4;
	int mip3size = 2*2;

	memset (mod_novis, 0xff, sizeof (mod_novis));
	r_notexture_mip = Hunk_AllocName (sizeof (texture_t)
									  + mip0size + mip1size
									  + mip2size + mip3size, "notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof (texture_t);

	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + mip0size;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + mip1size;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + mip2size;

	for (m = 0; m < 4; m++) {
		dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
		for (y = 0; y < (16 >> m); y++)
			for (x = 0; x < (16 >> m); x++) {
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

void
Mod_Init_Cvars (void)
{
	gl_subdivide_size =
		Cvar_Get ("gl_subdivide_size", "128", CVAR_ARCHIVE, NULL,
				"Sets the division value for the sky brushes.");
}

/*
	Mod_ClearAll
*/
void
Mod_ClearAll (void)
{
	int         i;
	model_t    *mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (mod->type != mod_alias)
			mod->needload = true;
}

/*
	Mod_FindName
*/
model_t    *
Mod_FindName (char *name)
{
	int         i;
	model_t    *mod;

	if (!name[0])
		Sys_Error ("Mod_FindName: NULL name");

//
// search the currently loaded models
//
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (!strcmp (mod->name, name))
			break;

	if (i == mod_numknown) {
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
		strcpy (mod->name, name);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}

/*
	Mod_LoadModel

	Loads a model into the cache
*/
model_t    *
Mod_LoadModel (model_t *mod, qboolean crash)
{
	void       *d;
	unsigned int *buf;
	byte        stackbuf[1024];			// avoid dirtying the cache heap

	if (!mod->needload) {
		if (mod->type == mod_alias) {
			d = Cache_Check (&mod->cache);
			if (d)
				return mod;
		} else
			return mod;					// not cached at all
	}
//
// load the file
//
	buf =
		(unsigned int *) COM_LoadStackFile (mod->name, stackbuf,
											sizeof (stackbuf));
	if (!buf) {
		if (crash)
			Sys_Error ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}
//
// allocate a new model
//
	COM_FileBase (mod->name, loadname);

	loadmodel = mod;

//
// fill it in
//

// call the apropriate loader
	mod->needload = false;
	mod->hasfullbrights = false;

	switch (LittleLong (*(unsigned int *) buf)) {
		case IDPOLYHEADER:
			Mod_LoadAliasModel (mod, buf);
			break;

		case IDSPRITEHEADER:
			Mod_LoadSpriteModel (mod, buf);
			break;

		default:
			Mod_LoadBrushModel (mod, buf);
			break;
	}

	return mod;
}

/*
	Mod_ForName

	Loads in a model for the given name
*/
model_t    *
Mod_ForName (char *name, qboolean crash)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	Con_DPrintf ("Mod_ForName: %s, %p\n", name, mod);
	return Mod_LoadModel (mod, crash);
}

/*
	Mod_Extradata

	Caches the data if needed
*/
void       *
Mod_Extradata (model_t *mod)
{
	void       *r;

	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
}

/*
	Mod_TouchModel
*/
void
Mod_TouchModel (char *name)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	if (!mod->needload) {
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

/*
	Mod_Print
*/
void
Mod_Print (void)
{
	int         i;
	model_t    *mod;

	Con_Printf ("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		Con_Printf ("%8p : %s\n", mod->cache.data, mod->name);
	}
}
