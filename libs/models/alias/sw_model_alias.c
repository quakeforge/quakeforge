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

void
Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *_m, int _s,
								int extra)
{
	int			 i, j;
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
		for (j = 0; j < 3; j++) {
			ptri[i].vertindex[j] = triangles[i].vertindex[j];
		}
	}
}

void *
Mod_LoadAliasFrame (void *pin, int *posenum, maliasframedesc_t *frame,
					int extra)
{
	daliasframe_t  *pdaliasframe;
	int				i, j;
	trivertx_t	   *pframe, *pinframe;

	pdaliasframe = (daliasframe_t *) pin;

	strcpy (frame->name, pdaliasframe->name);

	for (i = 0; i < 3; i++) {
		// byte values, don't worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];

		aliasbboxmins[i] = min (frame->bboxmin.v[i], aliasbboxmins[i]);
		aliasbboxmaxs[i] = max (frame->bboxmax.v[i], aliasbboxmaxs[i]);
	}

	pinframe = (trivertx_t *) (pdaliasframe + 1);

	if (extra)
		pframe = Hunk_AllocName (pheader->mdl.numverts * sizeof (*pframe) * 2,
							 loadname);
	else
		pframe = Hunk_AllocName (pheader->mdl.numverts * sizeof (*pframe),
							 loadname);

	frame->frame = (byte *) pframe - (byte *) pheader;

	for (j = 0; j < pheader->mdl.numverts; j++) {
		int		k;

		// these are all byte values, so no need to deal with endianness
		pframe[j].lightnormalindex = pinframe[j].lightnormalindex;

		for (k = 0; k < 3; k++) {
			pframe[j].v[k] = pinframe[j].v[k];
		}
	}

	if (extra)
	{
		for (j = pheader->mdl.numverts; j < pheader->mdl.numverts * 2; j++)
		{
			int		k;
			for (k = 0; k < 3; k++)
				pframe[j].v[k] = pinframe[j].v[k];
		}
	}

	if (extra)
		pinframe += pheader->mdl.numverts * 2;
	else
		pinframe += pheader->mdl.numverts;


	return (void *) pinframe;
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

	paliasgroup = Hunk_AllocName (field_offset (maliasgroup_t,
												frames[numframes]), loadname);
	paliasgroup->numframes = numframes;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];

		aliasbboxmins[i] = min (frame->bboxmin.v[i], aliasbboxmins[i]);
		aliasbboxmaxs[i] = max (frame->bboxmax.v[i], aliasbboxmaxs[i]);
	}

	frame->frame = (byte *) paliasgroup - (byte *) pheader;

	pin_intervals = (daliasinterval_t *) (pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	paliasgroup->intervals = (byte *) poutintervals - (byte *) pheader;

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
		ptemp = Mod_LoadAliasFrame (ptemp, &i, &temp_frame, extra);
		memcpy (&paliasgroup->frames[i], &temp_frame,
				sizeof(paliasgroup->frames[i]));
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
