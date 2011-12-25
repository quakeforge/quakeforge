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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/sys.h"

#include "compat.h"
#include "d_iface.h"

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses


void *
Mod_LoadSkin (byte *skin, int skinsize, int snum, int gnum,
			  qboolean group, maliasskindesc_t *skindesc)
{
	byte		*pskin;

	pskin = Hunk_AllocName (skinsize, loadname);
	skindesc->skin = (byte *) pskin - (byte *) pheader;

	memcpy (pskin, skin, skinsize);

	return skin + skinsize;
}

static void
process_frame (maliasframedesc_t *frame, int posenum, int extra)
{
	int         size = pheader->mdl.numverts * sizeof (trivertx_t);
	trivertx_t *frame_verts;

	if (extra)
		size *= 2;

	frame_verts = Hunk_AllocName (size, loadname);
	frame->frame = (byte *) frame_verts - (byte *) pheader;

	// I'm really not sure what the format of md16 is, but for now, I'll
	// assume the sw renderer is correct (I believe that's what serplord
	// was using when he developed it), which means the low-order 8 bits
	// (actually, fractional) are completely separate from the high-order
	// bits (see R_AliasTransformFinalVert16 in sw_ralias.c), but in
	// adjacant arrays. This means we can get away with just one memcpy
	// as there are non endian issues.
	memcpy (frame_verts, poseverts[posenum], size);
}

void
Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *_m, int _s,
								int extra)
{
	int			 i, j;
	int          posenum = 0;
	int			 numv = hdr->mdl.numverts, numt = hdr->mdl.numtris;
	stvert_t	*pstverts;
	mtriangle_t *ptri;

	pstverts = (stvert_t *) Hunk_AllocName (numv * sizeof (stvert_t),
											loadname);
	ptri = (mtriangle_t *) Hunk_AllocName (numt * sizeof (mtriangle_t),
										   loadname);

	hdr->stverts = (byte *) pstverts - (byte *) hdr;
	hdr->triangles = (byte *) ptri - (byte *) hdr;

	for (i = 0; i < numv; i++) {
		pstverts[i].onseam = stverts[i].onseam;
		pstverts[i].s = stverts[i].s << 16;
		pstverts[i].t = stverts[i].t << 16;
	}

	for (i = 0; i < numt; i++) {
		ptri[i].facesfront = triangles[i].facesfront;
		VectorCopy (triangles[i].vertindex, ptri[i].vertindex);
	}

	for (i = 0; i < pheader->mdl.numframes; i++) {
		maliasframedesc_t *frame = pheader->frames + i;
		if (frame->type) {
			maliasgroup_t *group;
			group = (maliasgroup_t *) ((byte *) pheader + frame->frame);
			for (j = 0; j < group->numframes; j++)
				process_frame ((maliasframedesc_t *) &group->frames[j],
							   posenum++, extra);
		} else {
			process_frame (frame, posenum++, extra);
		}
	}
}

void *
Mod_LoadAliasFrame (void *pin, int *posenum, maliasframedesc_t *frame,
					int extra)
{
	daliasframe_t  *pdaliasframe;
	trivertx_t	   *pinframe;

	pdaliasframe = (daliasframe_t *) pin;

	strncpy (frame->name, pdaliasframe->name, sizeof (frame->name));
	frame->name[sizeof (frame->name) - 1] = 0;
	frame->firstpose = (*posenum);
	frame->numposes = 1;

	// byte values, don't worry about endianness
	VectorCopy (pdaliasframe->bboxmin.v, frame->bboxmin.v);
	VectorCopy (pdaliasframe->bboxmax.v, frame->bboxmax.v);
	VectorCompMin (frame->bboxmin.v, aliasbboxmins, aliasbboxmins);
	VectorCompMax (frame->bboxmax.v, aliasbboxmaxs, aliasbboxmaxs);

	pinframe = (trivertx_t *) (pdaliasframe + 1);

	poseverts[(*posenum)] = pinframe;
	(*posenum)++;

	pinframe += pheader->mdl.numverts;
	if (extra)
		pinframe += pheader->mdl.numverts;

	return pinframe;
}

void *
Mod_LoadAliasGroup (void *pin, int *posenum, maliasframedesc_t *frame,
					int extra)
{
	daliasgroup_t	 *pingroup;
	daliasinterval_t *pin_intervals;
	float			 *poutintervals;
	int				  i, numframes;
	maliasgroup_t	 *paliasgroup;
	void			 *ptemp;

	pingroup = (daliasgroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = (*posenum);
	frame->numposes = numframes;

	paliasgroup = Hunk_AllocName (field_offset (maliasgroup_t,
												frames[numframes]), loadname);
	paliasgroup->numframes = numframes;
	frame->frame = (byte *) paliasgroup - (byte *) pheader;

	// these are byte values, so we don't have to worry about endianness
	VectorCopy (pingroup->bboxmin.v, frame->bboxmin.v);
	VectorCopy (pingroup->bboxmax.v, frame->bboxmax.v);
	VectorCompMin (frame->bboxmin.v, aliasbboxmins, aliasbboxmins);
	VectorCompMax (frame->bboxmax.v, aliasbboxmaxs, aliasbboxmaxs);

	pin_intervals = (daliasinterval_t *) (pingroup + 1);
	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);
	paliasgroup->intervals = (byte *) poutintervals - (byte *) pheader;
	frame->interval = LittleFloat (pin_intervals->interval);
	for (i = 0; i < numframes; i++) {
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadAliasGroup: interval<=0");
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;
	for (i = 0; i < numframes; i++) {
		maliasframedesc_t temp_frame;
		ptemp = Mod_LoadAliasFrame (ptemp, posenum, &temp_frame, extra);
		memcpy (&paliasgroup->frames[i], &temp_frame,
				sizeof (paliasgroup->frames[i]));
	}

	return ptemp;
}

void
Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr)
{
}

void
Mod_LoadExternalSkins (model_t *mod)
{
}
