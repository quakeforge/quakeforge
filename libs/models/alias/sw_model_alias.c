/*
	sw_model_alias.c

	alias model loading and caching for the software renderer

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

#include "QF/qendian.h"
#include "QF/sys.h"

#include "compat.h"
#include "d_iface.h"
#include "r_local.h"
#include "mod_internal.h"

static void
sw_alias_clear (model_t *m, void *data)
{
	m->needload = true;

	Cache_Free (&m->cache);
}

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses

void
sw_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx)
{
	mesh_t     *mesh = alias_ctx->mesh;
	int         width = alias_ctx->skinwidth;
	int         height = alias_ctx->skinheight;
	int         skinsize = width * height;
	int         picsize = field_offset (qpic_t, data[width * height]);
	int         num_skins = alias_ctx->skins.size;
	byte       *texel_block = Hunk_AllocName (0, picsize * num_skins,
											  alias_ctx->mod->name);

	for (size_t i = 0; i < alias_ctx->skins.size; i++) {
		auto skin = alias_ctx->skins.a + i;
		auto pic = (qpic_t *) (texel_block + i * picsize);

		pic->width = width;
		pic->height = height;
		skin->skindesc->data = (byte *) pic - (byte *) mesh;
		memcpy (pic->data, skin->texels, skinsize);
	}
}

void
sw_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx)
{
	alias_ctx->mod->clear = sw_alias_clear;
}

static void
process_frame (mod_alias_ctx_t *alias_ctx, maliasframe_t *frame,
			   trivertx_t *frame_verts, int posenum, int extra)
{
	mesh_t     *mesh = alias_ctx->mesh;
	int         size = alias_ctx->mdl->numverts * sizeof (trivertx_t);

	frame->bboxmin = alias_ctx->dframes[posenum]->bboxmin;
	frame->bboxmax = alias_ctx->dframes[posenum]->bboxmax;

	if (extra)
		size *= 2;

	frame->data = (byte *) frame_verts - (byte *) mesh;

	// The low-order 8 bits (actually, fractional) are completely separate
	// from the high-order bits (see R_AliasTransformFinalVert16 in
	// sw_ralias.c), but in adjacant arrays. This means we can get away with
	// just one memcpy as there are no endian issues.
	memcpy (frame_verts, alias_ctx->poseverts.a[posenum], size);
}

void
sw_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx, void *_m,
								   int _s, int extra)
{
	auto mdl = alias_ctx->mdl;
	mesh_t     *mesh = alias_ctx->mesh;
	int         numv = alias_ctx->stverts.size;
	int         numt = alias_ctx->triangles.size;
	int         nump = alias_ctx->poseverts.size;
	int         size = sizeof (sw_alias_mesh_t)
					 + sizeof (stvert_t[numv])
					 + sizeof (mtriangle_t[numt])
					 + sizeof (maliasframe_t[nump])
					 + sizeof (trivertx_t[nump * numv]);

	const char *name = alias_ctx->mod->name;
	sw_alias_mesh_t *rmesh = Hunk_AllocName (nullptr, size, name);
	auto stverts = (stvert_t *) &rmesh[1];
	auto tris = (mtriangle_t *) &stverts[numv];
	auto aframes = (maliasframe_t *) &tris[numt];
	auto frame_verts = (trivertx_t *) &aframes[nump];

	*rmesh = (sw_alias_mesh_t) {
		.scale = { VectorExpand (mdl->scale) },
		.scale_origin = { VectorExpand (mdl->scale_origin) },
		.stverts = (byte *) stverts - (byte *) rmesh,
		.triangles = (byte *) tris - (byte *) rmesh,
		.numverts = mdl->numverts,
		.numtris = mdl->numtris,
		.size = mdl->size * ALIAS_BASE_SIZE_RATIO,
	};
	mesh->render_data = (byte *) rmesh - (byte *) mesh;
	mesh->morph.data = (byte *) aframes - (byte *) mesh;

	for (int i = 0; i < numv; i++) {
		stverts[i].onseam = alias_ctx->stverts.a[i].onseam;
		stverts[i].s = alias_ctx->stverts.a[i].s << 16;
		stverts[i].t = alias_ctx->stverts.a[i].t << 16;
	}

	for (int i = 0; i < numt; i++) {
		tris[i].facesfront = alias_ctx->triangles.a[i].facesfront;
		VectorCopy (alias_ctx->triangles.a[i].vertindex, tris[i].vertindex);
	}

	auto desc = (framedesc_t *) ((byte *) mesh + mesh->morph.descriptors);
	auto frames = (frame_t *) ((byte *) mesh + mesh->morph.frames);
	int  posenum = 0;
	for (int i = 0; i < mesh->morph.numdesc; i++) {
		for (int j = 0; j < desc[i].numframes; j++, posenum++) {
			frames[posenum].data = (byte *) &aframes[posenum] - (byte *) mesh;
			process_frame (alias_ctx, &aframes[posenum],
						   &frame_verts[posenum * numv], posenum, extra);
		}
	}
}
