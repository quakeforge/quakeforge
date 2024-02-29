/*
	model_alias.c

	alias model loading and caching

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

#include "QF/crc.h"
#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "compat.h"
#include "mod_internal.h"

static void
load_skin (mod_alias_ctx_t *alias_ctx, frame_t *frame, float endtime,
		  byte *skin)
{
	*frame = (frame_t) { .endtime = endtime };

	mod_alias_skin_t askin = {
		.texels = skin,
		.skindesc = frame,
	};
	DARRAY_APPEND (&alias_ctx->skins, askin);
}

static void
load_skins (mod_alias_ctx_t *alias_ctx, daliasskintype_t *type,
			const mdl_t *mdl)
{
	int  skinsize = mdl->skinwidth * mdl->skinheight;
	auto mesh = alias_ctx->mesh;
	auto desc = (framedesc_t *) ((byte *) mesh + mesh->skin.descriptors);
	auto skinframe = (frame_t *) ((byte *) mesh + mesh->skin.frames);
	int  index = 0;

	for (int i = 0; i < mdl->numskins; i++) {
		auto skin = (byte *) &type[1];
		if (type->type == ALIAS_SKIN_SINGLE) {
			desc[i] = (framedesc_t) {
				.firstframe = index,
				.numframes = 1,
			};
			load_skin (alias_ctx, &skinframe[index++], 0, skin);
			skin += skinsize;
		} else {
			auto group = (daliasgroup_t *) &type[1];
			desc[i] = (framedesc_t) {
				.firstframe = index,
				.numframes = group->numframes,
			};
			auto intervals = (float *) &group[1];
			skin = (byte *) &intervals[group->numframes];
			for (int j = 0; j < group->numframes; j++) {
				load_skin (alias_ctx, &skinframe[index++], intervals[j], skin);
				skin += skinsize;
			}
		}
		type = (daliasskintype_t *) skin;
	}
}

static void *
load_frame (mod_alias_ctx_t *alias_ctx, daliasframe_t *frame, const mdl_t *mdl)
{
	// everything is bytes...
	size_t len = strnlen (frame->name, 16);
	char *name = alias_ctx->names + alias_ctx->names_size;
	strncpy (name, frame->name, 16);
	name[len] = 0;
	alias_ctx->names_size += len + 1;
	VectorCompMin (frame->bboxmin.v, alias_ctx->aliasbboxmins,
				   alias_ctx->aliasbboxmins);
	VectorCompMax (frame->bboxmax.v, alias_ctx->aliasbboxmaxs,
				   alias_ctx->aliasbboxmaxs);
	auto verts = (trivertx_t *) &frame[1];
	return verts + mdl->numverts;
}

static void
load_frames (mod_alias_ctx_t *alias_ctx, daliasframetype_t *type,
			 const mdl_t *mdl)
{
	auto mesh = alias_ctx->mesh;
	auto desc = (framedesc_t *) ((byte *) mesh + mesh->morph.descriptors);
	auto frame = (frame_t *) ((byte *) mesh + mesh->morph.frames);
	int  index = 0;

	for (int i = 0; i < mdl->numframes; i++) {
		trivertx_t *verts;
		if (type->type == ALIAS_SINGLE) {
			desc[i] = (framedesc_t) {
				.firstframe = index,
				.numframes = 1,
			};
			frame[index] = (frame_t) { };

			void *data = &type[1];
			alias_ctx->dframes[index] = data;
			verts = load_frame (alias_ctx, data, mdl);
			index++;
		} else {
			auto group = (daliasgroup_t *) &type[1];
			desc[i] = (framedesc_t) {
				.firstframe = index,
				.numframes = group->numframes,
			};
			auto intervals = (float *) &group[1];
			void *data = &intervals[group->numframes];
			for (int j = 0; j < group->numframes; j++) {
				frame[index] = (frame_t) {
					.endtime = intervals[j],
				};
				alias_ctx->dframes[index] = data;
				verts = load_frame (alias_ctx, data, mdl);
				data = verts;
				index++;
			}
		}
		type = (daliasframetype_t *) verts;
	}
}

static void *
swap_mdl (mdl_t *mdl)
{
	mdl->ident = LittleLong (mdl->ident);
	mdl->version = LittleLong (mdl->version);
	mdl->boundingradius = LittleFloat (mdl->boundingradius);
	mdl->numskins = LittleLong (mdl->numskins);
	mdl->skinwidth = LittleLong (mdl->skinwidth);
	mdl->skinheight = LittleLong (mdl->skinheight);
	mdl->numverts = LittleLong (mdl->numverts);
	mdl->numtris = LittleLong (mdl->numtris);
	mdl->numframes = LittleLong (mdl->numframes);
	mdl->synctype = LittleLong (mdl->synctype);
	mdl->flags = LittleLong (mdl->flags);
	mdl->size = LittleFloat (mdl->size);
	for (int i = 0; i < 3; i++) {
		mdl->scale[i] = LittleFloat (mdl->scale[i]);
		mdl->scale_origin[i] = LittleFloat (mdl->scale_origin[i]);
		mdl->eyeposition[i] = LittleFloat (mdl->eyeposition[i]);
	}
	return &mdl[1];
}

static void *
swap_skins (mod_alias_ctx_t *alias_ctx, daliasskintype_t *type,
			const mdl_t *mdl)
{
	int         skinsize = mdl->skinwidth * mdl->skinheight;
	for (int i = 0; i < mdl->numskins; i++) {
		type->type = LittleLong (type->type);
		auto skin = (byte *) &type[1];
		if (type->type == ALIAS_SKIN_SINGLE) {
			// skin texels are tightly packed with no other data
			skin += skinsize;
			alias_ctx->numskins++;
		} else {
			auto group = (daliasskingroup_t *) &type[1];
			group->numskins = LittleLong (group->numskins);
			auto intervals = (float *) &group[1];
			for (int j = 0; j < group->numskins; j++) {
				intervals[j] = LittleFloat (intervals[j]);
				if (!intervals[j]) {
					intervals[j] = 0.1;
				}
			}
			skin = (byte *) &intervals[group->numskins];
			// skin texels are tightly packed with no other data
			skin += group->numskins * skinsize;
			alias_ctx->numskins += group->numskins;
		}
		type = (daliasskintype_t *) skin;
	}
	return type;
}

static void *
swap_stverts (mod_alias_ctx_t *alias_ctx, stvert_t *stverts, const mdl_t *mdl)
{
	DARRAY_RESIZE (&alias_ctx->stverts, mdl->numverts);
	for (int i = 0; i < mdl->numverts; i++, stverts++) {
		alias_ctx->stverts.a[i].onseam = LittleLong (stverts->onseam);
		alias_ctx->stverts.a[i].s = LittleLong (stverts->s);
		alias_ctx->stverts.a[i].t = LittleLong (stverts->t);
	}
	return stverts;
}

static void *
swap_frame (mod_alias_ctx_t *alias_ctx, daliasframe_t *frame, const mdl_t *mdl)
{
	// everything is bytes...
	auto verts = (trivertx_t *) &frame[1];
	alias_ctx->names_size += strnlen (frame->name, 16) + 1;
	DARRAY_APPEND (&alias_ctx->poseverts, verts);
	return verts + mdl->numverts;
}

static void *
swap_frames (mod_alias_ctx_t *alias_ctx, daliasframetype_t *type,
			 const mdl_t *mdl)
{
	for (int i = 0; i < mdl->numframes; i++) {
		type->type = LittleLong (type->type);
		trivertx_t *verts;
		if (type->type == ALIAS_SINGLE) {
			void *data = &type[1];
			verts = swap_frame (alias_ctx, data, mdl);
		} else {
			auto group = (daliasgroup_t *) &type[1];
			group->numframes = LittleLong (group->numframes);
			auto intervals = (float *) &group[1];
			for (int j = 0; j < group->numframes; j++) {
				intervals[j] = LittleFloat (intervals[j]);
				if (!intervals[j]) {
					intervals[j] = 0.1;
				}
			}
			void *data = &intervals[group->numframes];
			for (int j = 0; j < group->numframes; j++) {
				verts = swap_frame (alias_ctx, data, mdl);
				data = verts;
			}
		}
		type = (daliasframetype_t *) verts;
	}
	return type;
}

static void *
swap_triangles (mod_alias_ctx_t *alias_ctx, dtriangle_t *tris, const mdl_t *mdl)
{
	DARRAY_RESIZE (&alias_ctx->triangles, mdl->numtris);
	for (int i = 0; i < mdl->numtris; i++, tris++) {
		auto out = &alias_ctx->triangles.a[i];
		out->facesfront = LittleLong (tris->facesfront);
		for (int j = 0; j < 3; j++) {
			out->vertindex[j] = LittleLong (tris->vertindex[j]);
		}
	}
	return tris;
}

void
Mod_LoadAliasModel (model_t *mod, void *buffer, cache_allocator_t allocator)
{
	int			extra = 0;		// extra precision bytes

	if (strcmp (mod->name, "flame2")==0) {
		extra = 0;
	}
	mod_alias_ctx_t alias_ctx = {};
	alias_ctx.mod = mod;
	//FIXME should be per batch rather than per model
	DARRAY_INIT (&alias_ctx.poseverts, 256);
	DARRAY_INIT (&alias_ctx.stverts, 256);
	DARRAY_INIT (&alias_ctx.triangles, 256);
	DARRAY_INIT (&alias_ctx.skins, 256);

	if (LittleLong (* (unsigned int *) buffer) == HEADER_MDL16)
		extra = 1;		// extra precision bytes

	uint16_t    crc;
	CRC_Init (&crc);
	CRC_ProcessBlock (buffer, &crc, qfs_filesize);

	size_t      start = Hunk_LowMark (nullptr);

	mdl_t      *mdl = buffer;
	void       *skindata = swap_mdl (mdl);

	if (mdl->version != ALIAS_VERSION_MDL)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				   mod->name, mdl->version, ALIAS_VERSION_MDL);
	if (mdl->numskins < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d", mdl->numskins);
	if (mdl->numverts < 1)
		Sys_Error ("model %s has no vertices", mod->name);
	if (mdl->numtris < 1)
		Sys_Error ("model %s has no triangles", mod->name);
	if (mdl->numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d",
				   mdl->numframes);

	alias_ctx.mdl = mdl;

	void *stvertdata = swap_skins (&alias_ctx, skindata, mdl);
	void *triangledata = swap_stverts (&alias_ctx, stvertdata, mdl);
	void *framedata = swap_triangles (&alias_ctx, triangledata, mdl);
	swap_frames (&alias_ctx, framedata, mdl);

	daliasframe_t *dframes[alias_ctx.poseverts.size];
	alias_ctx.dframes = dframes;

	// allocate space for a working mesh, plus all the data except the
	// frame data, skin and group info
	size_t      size = sizeof (mesh_t)
						+ sizeof (framedesc_t[mdl->numskins])
						+ sizeof (frame_t[alias_ctx.numskins])
						+ sizeof (framedesc_t[mdl->numframes])
						+ sizeof (frame_t[alias_ctx.poseverts.size])
						+ alias_ctx.names_size;
	mesh_t *mesh = Hunk_AllocName (nullptr, size, mod->name);
	alias_ctx.mesh = mesh;
	auto skindescs = (framedesc_t *) &mesh[1];
	auto skinframes = (frame_t *) &skindescs[mdl->numskins];
	auto framedescs = (framedesc_t *) &skinframes[alias_ctx.numskins];
	auto frames = (frame_t *) &framedescs[mdl->numframes];
	alias_ctx.names = (char *) &frames[alias_ctx.poseverts.size];
	*mesh = (mesh_t) {
		.crc = crc,
		.skin = {
			.numdesc = mdl->numskins,
			.descriptors = (byte *) skindescs - (byte *) mesh,
			.frames = (byte *) skinframes - (byte *) mesh,
		},
		.morph = {
			.numdesc = mdl->numframes,
			.descriptors = (byte *) framedescs - (byte *) mesh,
			.frames = (byte *) frames - (byte *) mesh,
		},
	};
	alias_ctx.skinwidth = mdl->skinwidth;
	alias_ctx.skinheight = mdl->skinheight;

	mod->type = mod_mesh;
	mod->effects = mdl->flags;
	mod->synctype = mdl->synctype;
	mod->numframes = mdl->numframes;

	load_skins (&alias_ctx, skindata, mdl);

	VectorSet (99999, 99999, 99999, alias_ctx.aliasbboxmins);
	VectorSet (-99999, -99999, -99999, alias_ctx.aliasbboxmaxs);
	alias_ctx.names_size = 0;
	load_frames (&alias_ctx, framedata, mdl);

	VectorCompMultAdd (mdl->scale_origin, mdl->scale,
					   alias_ctx.aliasbboxmins, mod->mins);
	VectorCompMultAdd (mdl->scale_origin, mdl->scale,
					   alias_ctx.aliasbboxmaxs, mod->maxs);

	mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

	mod_funcs->Mod_LoadAllSkins (&alias_ctx);
	// build the draw lists
	m_funcs->Mod_MakeAliasModelDisplayLists (&alias_ctx, buffer,
											 qfs_filesize, extra);

	if (m_funcs->Mod_FinalizeAliasModel) {
		m_funcs->Mod_FinalizeAliasModel (&alias_ctx);
	}
	if (m_funcs->Mod_LoadExternalSkins) {
		m_funcs->Mod_LoadExternalSkins (&alias_ctx);
	}

	// move the complete, relocatable alias model to the cache
	if (m_funcs->alias_cache) {
		size_t      end = Hunk_LowMark (nullptr);
		size_t      total = end - start;
		void       *mem = allocator (&mod->cache, total, mod->name);
		if (mem)
			memcpy (mem, mesh, total);

		Hunk_FreeToLowMark (nullptr, start);
		mod->mesh = nullptr;
	} else {
		memset (&mod->cache, 0, sizeof (mod->cache));
		mod->mesh = mesh;
	}
	DARRAY_CLEAR (&alias_ctx.poseverts);
	DARRAY_CLEAR (&alias_ctx.stverts);
	DARRAY_CLEAR (&alias_ctx.triangles);
	DARRAY_CLEAR (&alias_ctx.skins);
}
