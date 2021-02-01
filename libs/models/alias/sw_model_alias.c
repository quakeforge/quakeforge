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
sw_Mod_LoadSkin (model_t *mod, byte *skin, int skinsize, int snum, int gnum,
				 qboolean group, maliasskindesc_t *skindesc)
{
	byte		*pskin;

	pskin = Hunk_AllocName (skinsize, mod->name);
	skindesc->skin = (byte *) pskin - (byte *) pheader;

	memcpy (pskin, skin, skinsize);

	return skin + skinsize;
}

static void
process_frame (model_t *mod, maliasframedesc_t *frame, int posenum, int extra)
{
	int         size = pheader->mdl.numverts * sizeof (trivertx_t);
	trivertx_t *frame_verts;

	if (extra)
		size *= 2;

	frame_verts = Hunk_AllocName (size, mod->name);
	frame->frame = (byte *) frame_verts - (byte *) pheader;

	// The low-order 8 bits (actually, fractional) are completely separate
	// from the high-order bits (see R_AliasTransformFinalVert16 in
	// sw_ralias.c), but in adjacant arrays. This means we can get away with
	// just one memcpy as there are no endian issues.
	memcpy (frame_verts, poseverts.a[posenum], size);
}

void
sw_Mod_MakeAliasModelDisplayLists (model_t *mod, aliashdr_t *hdr, void *_m,
								   int _s, int extra)
{
	int			 i, j;
	int          posenum = 0;
	int			 numv = hdr->mdl.numverts, numt = hdr->mdl.numtris;
	stvert_t	*pstverts;
	mtriangle_t *ptri;

	pstverts = (stvert_t *) Hunk_AllocName (numv * sizeof (stvert_t),
											mod->name);
	ptri = (mtriangle_t *) Hunk_AllocName (numt * sizeof (mtriangle_t),
										   mod->name);

	hdr->stverts = (byte *) pstverts - (byte *) hdr;
	hdr->triangles = (byte *) ptri - (byte *) hdr;

	for (i = 0; i < numv; i++) {
		pstverts[i].onseam = stverts.a[i].onseam;
		pstverts[i].s = stverts.a[i].s << 16;
		pstverts[i].t = stverts.a[i].t << 16;
	}

	for (i = 0; i < numt; i++) {
		ptri[i].facesfront = triangles.a[i].facesfront;
		VectorCopy (triangles.a[i].vertindex, ptri[i].vertindex);
	}

	for (i = 0; i < pheader->mdl.numframes; i++) {
		maliasframedesc_t *frame = pheader->frames + i;
		if (frame->type) {
			maliasgroup_t *group;
			group = (maliasgroup_t *) ((byte *) pheader + frame->frame);
			for (j = 0; j < group->numframes; j++)
				process_frame (mod, (maliasframedesc_t *) &group->frames[j],
							   posenum++, extra);
		} else {
			process_frame (mod, frame, posenum++, extra);
		}
	}
}

void
sw_Mod_FinalizeAliasModel (model_t *mod, aliashdr_t *hdr)
{
}

void
sw_Mod_LoadExternalSkins (model_t *mod)
{
}
