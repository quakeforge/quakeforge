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

#include "r_local.h"
#include "sys.h"
#include "console.h"
#include "qendian.h"
#include "checksum.h"

model_t    *loadmodel;
char        loadname[32];				// for hunk tags

void        Mod_LoadSpriteModel (model_t *mod, void *buffer);
void        Mod_LoadBrushModel (model_t *mod, void *buffer);
void        Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t    *Mod_LoadModel (model_t *mod, qboolean crash);

byte        mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	512
model_t     mod_known[MAX_MOD_KNOWN];
int         mod_numknown;

unsigned   *model_checksum;
texture_t  *r_notexture_mip;
cvar_t     *gl_subdivide_size;

/*
===============
Mod_Init
===============
*/
void
Mod_Init (void)
{
	gl_subdivide_size =
		Cvar_Get ("gl_subdivide_size", "128", CVAR_ARCHIVE, "None");
	memset (mod_novis, 0xff, sizeof (mod_novis));
}

/*
===================
Mod_ClearAll
===================
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
==================
Mod_FindName

==================
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
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t    *
Mod_LoadModel (model_t *mod, qboolean crash)
{
	void       *d;
	unsigned   *buf;
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
		(unsigned *) COM_LoadStackFile (mod->name, stackbuf, sizeof (stackbuf));
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

	switch (LittleLong (*(unsigned *) buf)) {
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
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t    *
Mod_ForName (char *name, qboolean crash)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
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
==================
Mod_TouchModel

==================
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
================
Mod_Print
================
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
