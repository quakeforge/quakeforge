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

*/
static const char rcsid[] = 
	"$Id$";

/*
  Models are the only shared resource between a client and server running
  on the same machine.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vfs.h"

#include "compat.h"

void        Mod_LoadAliasModel (model_t *mod, void *buf, cache_allocator_t allocator);
void        Mod_LoadSpriteModel (model_t *mod, void *buf);
void        Mod_LoadBrushModel (model_t *mod, void *buf);
model_t	*Mod_RealLoadModel (model_t *mod, qboolean crash, cache_allocator_t allocator);
void		Mod_CallbackLoad (void *object, cache_allocator_t allocator);

model_t    *loadmodel;
char        loadname[32];				// for hunk tags

#define	MAX_MOD_KNOWN	512
model_t     mod_known[MAX_MOD_KNOWN];
int         mod_numknown;

cvar_t     *gl_subdivide_size;
cvar_t     *gl_mesh_cache;


texture_t  *r_notexture_mip;


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
	gl_mesh_cache = Cvar_Get ("gl_mesh_cache", "256", CVAR_ARCHIVE, NULL,
							  "minimum triangle count in a model for its mesh"
							  " to be cached. 0 to disable caching");
}

void
Mod_ClearAll (void)
{
	int         i;
	model_t    *mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (mod->type != mod_alias)
			mod->needload = true;
}

model_t    *
Mod_FindName (const char *name)
{
	int         i;
	model_t    *mod;

	if (!name[0])
		Sys_Error ("Mod_FindName: empty name");

	// search the currently loaded models
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (!strcmp (mod->name, name))
			break;

	if (i == mod_numknown) {
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
		strcpy (mod->name, name);
		mod->needload = true;
		mod_numknown++;
		Cache_Add (&mod->cache, mod, Mod_CallbackLoad);
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
	if (!mod->needload) {
		if (mod->type == mod_alias) {
			if (Cache_Check (&mod->cache))
				return mod;
		} else
			return mod;					// not cached at all
	}
	return Mod_RealLoadModel (mod, crash, Cache_Alloc);
}

model_t *
Mod_RealLoadModel (model_t *mod, qboolean crash, cache_allocator_t allocator)
{
	unsigned int *buf;
	byte		stackbuf[1024];			// avoid dirtying the cache heap
	// load the file
	buf =
		(unsigned int *) COM_LoadStackFile (mod->name, stackbuf,
											sizeof (stackbuf));
	if (!buf) {
		if (crash)
			Sys_Error ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

	// allocate a new model
	COM_FileBase (mod->name, loadname);

	loadmodel = mod;

	// fill it in
	if (strequal (mod->name, "progs/grenade.mdl")) {
		mod->shadow_alpha = 0;
	} else {
		mod->shadow_alpha = 255;
	}
	if (strnequal (mod->name, "progs/flame", 11)
		|| strnequal (mod->name, "progs/bolt", 10)) {
		mod->fullbright = 1;
		mod->shadow_alpha = 0;
	} else {
		mod->fullbright = 0;
	}
	if (strequal (mod->name, "progs/player.mdl")) {
		mod->min_light = 8;
	} else {
		mod->min_light = 0;
	}

	// call the apropriate loader
	mod->needload = false;
	mod->hasfullbrights = false;

	switch (LittleLong (*(unsigned int *) buf)) {
		case IDPOLYHEADER:
			Mod_LoadAliasModel (mod, buf, allocator);
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
	Mod_CallbackLoad

	Callback used to load model automatically
*/
void
Mod_CallbackLoad (void *object, cache_allocator_t allocator)
{
	if (((model_t *)object)->type != mod_alias)
		Sys_Error ("Mod_CallbackLoad for non-alias model?  FIXME!\n");
	// FIXME: do we want crash set to true?
	Mod_RealLoadModel (object, true, allocator);
}

/*
	Mod_ForName

	Loads in a model for the given name
*/
model_t    *
Mod_ForName (const char *name, qboolean crash)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	Sys_DPrintf ("Mod_ForName: %s, %p\n", name, mod);
	return Mod_LoadModel (mod, crash);
}

void
Mod_TouchModel (const char *name)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	if (!mod->needload) {
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

void
Mod_Print (void)
{
	int         i;
	model_t    *mod;

	Sys_Printf ("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		Sys_Printf ("%8p : %s\n", mod->cache.data, mod->name);
	}
}
