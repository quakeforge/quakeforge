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

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses


void *
sw_Mod_LoadSkin (mod_alias_ctx_t *alias_ctx, byte *skin,
				 int skinsize, int snum, int gnum,
				 qboolean group, maliasskindesc_t *skindesc)
{
	byte		*pskin;

	pskin = Hunk_AllocName (0, skinsize, alias_ctx->mod->name);
	skindesc->skin = (byte *) pskin - (byte *) alias_ctx->header;

	memcpy (pskin, skin, skinsize);

	return skin + skinsize;
}

static void
process_frame (mod_alias_ctx_t *alias_ctx, maliasframedesc_t *frame,
			   int posenum, int extra)
{
	aliashdr_t *header = alias_ctx->header;
	int         size = header->mdl.numverts * sizeof (trivertx_t);
	trivertx_t *frame_verts;

	if (extra)
		size *= 2;

	frame_verts = Hunk_AllocName (0, size, alias_ctx->mod->name);
	frame->frame = (byte *) frame_verts - (byte *) header;

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
	aliashdr_t *header = alias_ctx->header;
	int			 i, j;
	int          posenum = 0;
	int			 numv = header->mdl.numverts, numt = header->mdl.numtris;
	stvert_t	*stverts;
	mtriangle_t *tris;

	stverts = (stvert_t *) Hunk_AllocName (0, numv * sizeof (stvert_t),
										   alias_ctx->mod->name);
	tris = (mtriangle_t *) Hunk_AllocName (0, numt * sizeof (mtriangle_t),
										   alias_ctx->mod->name);

	header->stverts = (byte *) stverts - (byte *) header;
	header->triangles = (byte *) tris - (byte *) header;

	for (i = 0; i < numv; i++) {
		stverts[i].onseam = alias_ctx->stverts.a[i].onseam;
		stverts[i].s = alias_ctx->stverts.a[i].s << 16;
		stverts[i].t = alias_ctx->stverts.a[i].t << 16;
	}

	for (i = 0; i < numt; i++) {
		tris[i].facesfront = alias_ctx->triangles.a[i].facesfront;
		VectorCopy (alias_ctx->triangles.a[i].vertindex, tris[i].vertindex);
	}

	for (i = 0; i < header->mdl.numframes; i++) {
		maliasframedesc_t *frame = header->frames + i;
		if (frame->type) {
			maliasgroup_t *group;
			group = (maliasgroup_t *) ((byte *) header + frame->frame);
			for (j = 0; j < group->numframes; j++) {
				__auto_type frame = (maliasframedesc_t *) &group->frames[j];
				process_frame (alias_ctx, frame, posenum++, extra);
			}
		} else {
			process_frame (alias_ctx, frame, posenum++, extra);
		}
	}
}
