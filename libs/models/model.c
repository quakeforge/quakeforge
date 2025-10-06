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

#define IMPLEMENT_QFMODEL_Funcs

#include "QF/animation.h"
#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/hash.h"
#include "QF/iqm.h"
#include "QF/model.h"
#include "QF/progs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "QF/plugin/vid_render.h"

#include "compat.h"
#include "mod_internal.h"

static const uint32_t qfm_type_size[] = {
	[qfm_s8]      = 1,
	[qfm_s16]     = 2,
	[qfm_s32]     = 4,
	[qfm_s64]     = 8,

	[qfm_u8]      = 1,
	[qfm_u16]     = 2,
	[qfm_u32]     = 4,
	[qfm_u64]     = 8,

	[qfm_s8n]     = 1,
	[qfm_u8n]     = 1,
	[qfm_s16n]    = 2,
	[qfm_u16n]    = 2,

	[qfm_f16]     = 3,
	[qfm_f32]     = 4,
	[qfm_f64]     = 8,

	[qfm_special] = 0, // unknown
};

uint32_t
mesh_type_size (qfm_type_t type)
{
	return qfm_type_size[type];
}

uint32_t
mesh_attr_size (qfm_attrdesc_t attr)
{
	return attr.components * qfm_type_size[attr.type];
}

qfm_type_t
mesh_index_type (uint32_t num_verts)
{
	if (num_verts < 0xff) {
		return qfm_u8;
	} else if (num_verts < 0xffff) {
		return qfm_u16;
	} else {
		return qfm_u32;
	}
}

uint32_t
pack_indices (uint32_t *indices, uint32_t num_inds, qfm_type_t index_type)
{
	if (index_type == qfm_u32) {
		return num_inds * sizeof (uint32_t);
	} else if (index_type == qfm_u16) {
		auto inds = (uint16_t *) indices;
		for (uint32_t i = 0; i < num_inds; i++) {
			inds[i] = indices[i];
		}
		return num_inds * sizeof (*inds);
	} else if (index_type == qfm_u8) {
		auto inds = (uint8_t *) indices;
		for (uint32_t i = 0; i < num_inds; i++) {
			inds[i] = indices[i];
		}
		return num_inds * sizeof (*inds);
	}
	Sys_Error ("invalid index type: %d", index_type);
}

vid_model_funcs_t *mod_funcs;
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

typedef struct {
	hashctx_t  *hashctx;
	hashtab_t  *model_tab;
	PR_RESMAP (model_t) model_map;
} model_registry_t;

#define RESMAP_OBJ_TYPE model_t
#define RESMAP_PREFIX qfm_model
#define RESMAP_MAP_PARAM model_registry_t *reg
#define RESMAP_MAP reg->model_map
#define RESMAP_ERROR(msg, ...) Sys_Error(msg __VA_OPT__(,) __VA_ARGS__)
#include "resmap_template.cinc"
#define qfm_model_get(reg,index) _qfm_model_get(reg,index,__FUNCTION__)

static model_registry_t *model_registry;

model_t *
qfm_alloc_model (void)
{
	return qfm_model_new (model_registry);
}

void
qfm_free_model (model_t *mod)
{
	qfm_model_free (model_registry, mod);
}

static void
mod_shutdown (void *data)
{
	qfZoneScoped (true);
	Mod_ClearAll ();

	qfm_model_reset (model_registry);
	Hash_DelTable (model_registry->model_tab);
	free (model_registry);
	model_registry = nullptr;

	qfa_shutdown ();
}

static void
mod_unload_model (model_t *mod)
{
	qfZoneScoped (true);

	qfa_deregister (mod);

	//FIXME this seems to be correct but need to double check the behavior
	//with alias models
	if (!mod->needload && mod->clear) {
		mod->clear (mod, mod->data);
		mod->clear = 0;
	}
	if (mod->type != mod_mesh) {
		mod->needload = true;
	}
	if (mod->type == mod_sprite) {
		mod->cache.data = 0;
	}
}

static const char *
qfm_model_get_key (const void *m, void *data)
{
	const model_t *mod = m;
	return mod->path;
}

static void
qfm_model_free_model (void *m, void *data)
{
	model_t *mod = m;
	mod_unload_model (mod);
	qfm_model_free (model_registry, mod);
}

VISIBLE void
Mod_Init (void)
{
	qfZoneScoped (true);
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

	model_registry = malloc (sizeof (model_registry_t));
	*model_registry = (model_registry_t) {
	};
	model_registry->model_tab = Hash_NewTable (1021, qfm_model_get_key,
											   qfm_model_free_model,
											   model_registry,
											   &model_registry->hashctx);

	qfa_init ();
}

VISIBLE void
Mod_Init_Cvars (void)
{
	qfZoneScoped (true);
	Cvar_Register (&gl_subdivide_size_cvar, 0, 0);
	Cvar_Register (&gl_mesh_cache_cvar, 0, 0);
	Cvar_Register (&gl_alias_render_tri_cvar, 0, 0);
	Cvar_Register (&gl_textures_external_cvar, 0, 0);
}

VISIBLE void
Mod_ClearAll (void)
{
	qfZoneScoped (true);
	if (model_registry) {
		Hash_FlushTable (model_registry->model_tab);
	}
}

model_t *
Mod_FindName (const char *name)
{
	qfZoneScoped (true);

	if (!name[0])
		Sys_Error ("Mod_FindName: empty name");

	model_t *mod = Hash_Find (model_registry->model_tab, name);
	if (!mod) {
		mod = qfm_model_new (model_registry);
		memset (mod, 0, sizeof (model_t));
		strncpy (mod->path, name, sizeof (mod->path) - 1);
		mod->needload = true;
		Hash_Add (model_registry->model_tab, mod);
		Cache_Add (&mod->cache, mod, Mod_CallbackLoad);
	}

	return mod;
}

static model_t *
Mod_RealLoadModel (model_t *mod, bool crash, cache_allocator_t allocator)
{
	qfZoneScoped (true);
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
	qfZoneScoped (true);
	if (!mod->needload) {
		if (mod->type == mod_mesh && !mod->model) {
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
	qfZoneScoped (true);
	if (((model_t *)object)->type != mod_mesh)
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
	qfZoneScoped (true);
	model_t    *mod;

	mod = Mod_FindName (name);

	Sys_MaskPrintf (SYS_dev, "Mod_ForName: %s, %p\n", name, mod);
	Mod_LoadModel (mod, crash);
	qfa_register (mod);
	return mod;
}

VISIBLE void
Mod_TouchModel (const char *name)
{
	qfZoneScoped (true);
	model_t    *mod;

	mod = Mod_FindName (name);

	if (!mod->needload) {
		if (mod->type == mod_mesh)
			Cache_Check (&mod->cache);
	}
}

VISIBLE void
Mod_UnloadModel (model_t *model)
{
	qfZoneScoped (true);
	model_t *mod = Hash_Find (model_registry->model_tab, model->name);
	if (mod == model) {
		mod_unload_model (mod);
		Hash_Del (model_registry->model_tab, model->name);
	}
}

static void
qfm_print_model (void *ele, void *data)
{
	model_t *mod = ele;
	Sys_Printf ("%8p : %s\n", mod->cache.data, mod->path);
}

VISIBLE void
Mod_Print (void)
{
	qfZoneScoped (true);

	Sys_Printf ("Cached models:\n");
	Hash_ForEach (model_registry->model_tab, qfm_print_model, nullptr);
}

float
RadiusFromBounds (const vec3_t mins, const vec3_t maxs)
{
	qfZoneScoped (true);
	int		i;
	vec3_t	corner;

	for (i = 0; i < 3; i++)
		corner[i] = max (fabs (mins[i]), fabs (maxs[i]));
	return VectorLength (corner);
}
