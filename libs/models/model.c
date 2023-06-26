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
// Models are the only shared resource between a client and server running
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

#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/iqm.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "QF/plugin/vid_render.h"

#include "compat.h"
#include "mod_internal.h"

vid_model_funcs_t *mod_funcs;

#define	MOD_BLOCK	16					// allocate 16 models at a time
static struct DARRAY_TYPE (model_t *) mod_known = {0, 0, MOD_BLOCK};
static struct DARRAY_TYPE (model_t *) mod_blocks = {0, 0, MOD_BLOCK};
static size_t mod_numknown;

VISIBLE texture_t  *r_notexture_mip;

int gl_mesh_cache;
static cvar_t gl_mesh_cache_cvar = {
	.name = "gl_mesh_cache",
	.description =
		"minimum triangle count in a model for its mesh to be cached. 0 to "
		"disable caching",
	.default_value = "256",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &gl_mesh_cache },
};
float gl_subdivide_size;
static cvar_t gl_subdivide_size_cvar = {
	.name = "gl_subdivide_size",
	.description =
		"Sets the division value for the sky brushes.",
	.default_value = "128",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &gl_subdivide_size },
};
int gl_alias_render_tri;
static cvar_t gl_alias_render_tri_cvar = {
	.name = "gl_alias_render_tri",
	.description =
		"When loading alias models mesh for pure triangle rendering",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &gl_alias_render_tri },
};
int gl_textures_external;
static cvar_t gl_textures_external_cvar = {
	.name = "gl_textures_external",
	.description =
		"Use external textures to replace BSP textures",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &gl_textures_external },
};

static void Mod_CallbackLoad (void *object, cache_allocator_t allocator);

static void
mod_shutdown (void *data)
{
	Mod_ClearAll ();
	for (size_t i = 0; i < mod_blocks.size; i++) {
		free (mod_blocks.a[i]);
	}
	DARRAY_CLEAR (&mod_known);
	DARRAY_CLEAR (&mod_blocks);
}

VISIBLE void
Mod_Init (void)
{
	byte   *dest;
	int		m, x, y;
	int		mip0size = 16*16, mip1size = 8*8, mip2size = 4*4, mip3size = 2*2;

	Sys_RegisterShutdown (mod_shutdown, 0);

	r_notexture_mip = Hunk_AllocName (0,
									  sizeof (texture_t) + mip0size + mip1size
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

VISIBLE void
Mod_Init_Cvars (void)
{
	Cvar_Register (&gl_subdivide_size_cvar, 0, 0);
	Cvar_Register (&gl_mesh_cache_cvar, 0, 0);
	Cvar_Register (&gl_alias_render_tri_cvar, 0, 0);
	Cvar_Register (&gl_textures_external_cvar, 0, 0);
}

static void
mod_unload_model (size_t ind)
{
	model_t    *mod = mod_known.a[ind];

	//FIXME this seems to be correct but need to double check the behavior
	//with alias models
	if (!mod->needload && mod->clear) {
		mod->clear (mod, mod->data);
	}
	if (mod->type != mod_alias) {
		mod->needload = true;
	}
	if (mod->type == mod_sprite) {
		mod->cache.data = 0;
	}
}

VISIBLE void
Mod_ClearAll (void)
{
	size_t      i;

	for (i = 0; i < mod_numknown; i++) {
		mod_unload_model (i);
	}
	mod_numknown = 0;
}

model_t *
Mod_FindName (const char *name)
{
	size_t      i;
	model_t   **mod;

	if (!name[0])
		Sys_Error ("Mod_FindName: empty name");

	// search the currently loaded models
	for (i = 0, mod = mod_known.a; i < mod_numknown; i++, mod++)
		if (!strcmp ((*mod)->path, name))
			break;

	if (i == mod_numknown) {
		if (mod_numknown == mod_known.size) {
			model_t    *block = calloc (MOD_BLOCK, sizeof (model_t));
			for (i = 0; i < MOD_BLOCK; i++) {
				DARRAY_APPEND (&mod_known, &block[i]);
			}
			mod = &mod_known.a[mod_numknown];
			DARRAY_APPEND (&mod_blocks, block);
		}
		memset ((*mod), 0, sizeof (model_t));
		strncpy ((*mod)->path, name, sizeof (*mod)->path - 1);
		(*mod)->needload = true;
		mod_numknown++;
		Cache_Add (&(*mod)->cache, *mod, Mod_CallbackLoad);
	}

	return *mod;
}

static model_t *
Mod_RealLoadModel (model_t *mod, bool crash, cache_allocator_t allocator)
{
	uint32_t   *buf;

	// load the file
	buf = (uint32_t *) QFS_LoadFile (QFS_FOpenFile (mod->path), 0);
	if (!buf) {
		if (crash)
			Sys_Error ("Mod_LoadModel: %s not found", mod->path);
		return NULL;
	}

	char *name = QFS_FileBase (mod->path);
	strncpy (mod->name, name, sizeof (mod->name) - 1);
	mod->name[sizeof (mod->name) - 1] = 0;
	free (name);

	// fill it in
	mod->vpath = qfs_foundfile.vpath;
	mod->fullbright = 0;
	mod->shadow_alpha = 255;
	mod->min_light = 0.0;

	// call the apropriate loader
	mod->needload = false;
	mod->hasfullbrights = false;

	switch (LittleLong (*buf)) {
		case IQM_SMAGIC:
			if (strequal ((char *) buf, IQM_MAGIC)) {
				if (mod_funcs)
					mod_funcs->Mod_LoadIQM (mod, buf);
			}
			break;
		case IDHEADER_MDL:			// Type 6: Quake 1 .mdl
		case HEADER_MDL16:			// QF Type 6 extended for 16bit precision
			if (strequal (mod->path, "progs/grenade.mdl")) {
				mod->fullbright = 0;
				mod->shadow_alpha = 255;
			} else if (strnequal (mod->path, "progs/flame", 11)
					   || strnequal (mod->path, "progs/bolt", 10)) {
				mod->fullbright = 1;
				mod->shadow_alpha = 0;
			}
			if (strnequal (mod->path, "progs/v_", 8)) {
				mod->min_light = 0.12;
			} else if (strequal (mod->path, "progs/player.mdl")) {
				mod->min_light = 0.04;
			}
			if (mod_funcs)
				mod_funcs->Mod_LoadAliasModel (mod, buf, allocator);
			break;
		case IDHEADER_MD2:			// Type 8: Quake 2 .md2
//			Mod_LoadMD2 (mod, buf, allocator);
			break;
		case IDHEADER_SPR:			// Type 1: Quake 1 .spr
			if (mod_funcs)
				mod_funcs->Mod_LoadSpriteModel (mod, buf);
			break;
		case IDHEADER_SP2:			// Type 2: Quake 2 .sp2
//			Mod_LoadSP2 (mod, buf);
			break;
		default:					// Version 29: Quake 1 .bsp
									// Version 38: Quake 2 .bsp
			Mod_LoadBrushModel (mod, buf);
			break;
	}
	free (buf);

	return mod;
}

/*
	Mod_LoadModel

	Loads a model into the cache
*/
static model_t *
Mod_LoadModel (model_t *mod, bool crash)
{
	if (!mod->needload) {
		if (mod->type == mod_alias && !mod->aliashdr) {
			if (Cache_Check (&mod->cache))
				return mod;
		} else
			return mod;					// not cached at all
	}
	return Mod_RealLoadModel (mod, crash, Cache_Alloc);
}

/*
	Mod_CallbackLoad

	Callback used to load model automatically
*/
static void
Mod_CallbackLoad (void *object, cache_allocator_t allocator)
{
	if (((model_t *)object)->type != mod_alias)
		Sys_Error ("Mod_CallbackLoad for non-alias model?  FIXME!");
	// FIXME: do we want crash set to true?
	Mod_RealLoadModel (object, true, allocator);
}

/*
	Mod_ForName

	Loads in a model for the given name
*/
VISIBLE model_t *
Mod_ForName (const char *name, bool crash)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	Sys_MaskPrintf (SYS_dev, "Mod_ForName: %s, %p\n", name, mod);
	return Mod_LoadModel (mod, crash);
}

VISIBLE void
Mod_TouchModel (const char *name)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	if (!mod->needload) {
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

VISIBLE void
Mod_UnloadModel (model_t *model)
{
	for (size_t i = 0; i < mod_numknown; i++) {
		if (mod_known.a[i] == model) {
			mod_unload_model (i);
		}
	}
}

VISIBLE void
Mod_Print (void)
{
	size_t      i;
	model_t	  **mod;

	Sys_Printf ("Cached models:\n");
	for (i = 0, mod = mod_known.a; i < mod_numknown; i++, mod++) {
		Sys_Printf ("%8p : %s\n", (*mod)->cache.data, (*mod)->path);
	}
}

float
RadiusFromBounds (const vec3_t mins, const vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i = 0; i < 3; i++)
		corner[i] = max (fabs (mins[i]), fabs (maxs[i]));
	return VectorLength (corner);
}
