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
	malias_t   *alias = alias_ctx->alias;
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
		skin->skindesc->data = (byte *) pic - (byte *) alias;
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
			   int posenum, int extra)
{
	malias_t   *alias = alias_ctx->alias;
	int         size = alias_ctx->mdl->numverts * sizeof (trivertx_t);
	trivertx_t *frame_verts;

	frame->bboxmin = alias_ctx->dframes[posenum]->bboxmin;
	frame->bboxmax = alias_ctx->dframes[posenum]->bboxmax;

	if (extra)
		size *= 2;

	frame_verts = Hunk_AllocName (0, size, alias_ctx->mod->name);
	frame->data = (byte *) frame_verts - (byte *) alias;

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
	malias_t   *alias = alias_ctx->alias;
	int         numv = alias_ctx->stverts.size;
	int         numt = alias_ctx->triangles.size;
	int         nump = alias_ctx->poseverts.size;

	stvert_t *stverts = Hunk_AllocName (nullptr, sizeof (stvert_t[numv]),
										alias_ctx->mod->name);
	mtriangle_t *tris = Hunk_AllocName (nullptr, sizeof (mtriangle_t[numt]),
										alias_ctx->mod->name);
	maliasframe_t *aframes = Hunk_AllocName (nullptr,
											 sizeof (maliasframe_t[nump]),
											 alias_ctx->mod->name);

	alias->stverts = (byte *) stverts - (byte *) alias;
	alias->triangles = (byte *) tris - (byte *) alias;
	alias->morph.data = (byte *) aframes - (byte *) alias;

	for (int i = 0; i < numv; i++) {
		stverts[i].onseam = alias_ctx->stverts.a[i].onseam;
		stverts[i].s = alias_ctx->stverts.a[i].s << 16;
		stverts[i].t = alias_ctx->stverts.a[i].t << 16;
	}

	for (int i = 0; i < numt; i++) {
		tris[i].facesfront = alias_ctx->triangles.a[i].facesfront;
		VectorCopy (alias_ctx->triangles.a[i].vertindex, tris[i].vertindex);
	}

	int posenum = 0;
	auto desc = (mframedesc_t *) ((byte *) alias + alias->morph.descriptors);
	auto frames = (mframe_t *) ((byte *) alias + alias->morph.frames);
	for (int i = 0; i < alias->morph.numdesc; i++) {
		for (int j = 0; j < desc[i].numframes; j++, posenum++) {
			frames[posenum].data = (byte *) &aframes[posenum] - (byte *) alias;
			process_frame (alias_ctx, &aframes[posenum], posenum, extra);
		}
	}
}
