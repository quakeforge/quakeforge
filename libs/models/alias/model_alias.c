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
#include "d_iface.h"
#include "mod_internal.h"
#include "r_local.h"

aliashdr_t *pheader;

stvertset_t stverts = { 0, 0, 256 };
mtriangleset_t triangles = { 0, 0, 256 };

// a pose is a single set of vertexes.  a frame may be an animating
// sequence of poses
trivertxset_t poseverts = { 0, 0, 256 };;
int         posenum = 0;
int			aliasbboxmins[3], aliasbboxmaxs[3];


static void *
Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype, int *pskinindex)
{
	byte       *skin;
	float      *poutskinintervals;
	int         groupskins, skinsize, gnum, snum, t;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;
	maliasskindesc_t *pskindesc;
	maliasskingroup_t *paliasskingroup;

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d", numskins);

	skinsize = pheader->mdl.skinwidth * pheader->mdl.skinheight;
	pskindesc = Hunk_AllocName (numskins * sizeof (maliasskindesc_t),
								loadname);

	*pskinindex = (byte *) pskindesc - (byte *) pheader;

	for (snum = 0; snum < numskins; snum++) {
		pskindesc[snum].type = pskintype->type;
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			skin = (byte *) (pskintype + 1);
			skin = m_funcs->Mod_LoadSkin (skin, skinsize, snum, 0, false,
										  &pskindesc[snum]);
		} else {
			pskintype++;
			pinskingroup = (daliasskingroup_t *) pskintype;
			groupskins = LittleLong (pinskingroup->numskins);

			t = field_offset (maliasskingroup_t, skindescs[groupskins]);
			paliasskingroup = Hunk_AllocName (t, loadname);
			paliasskingroup->numskins = groupskins;

			pskindesc[snum].skin = (byte *) paliasskingroup - (byte *) pheader;

			pinskinintervals = (daliasskininterval_t *) (pinskingroup + 1);
			poutskinintervals = Hunk_AllocName (groupskins * sizeof (float),
												loadname);
			paliasskingroup->intervals =
				(byte *) poutskinintervals - (byte *) pheader;
			for (gnum = 0; gnum < groupskins; gnum++) {
				*poutskinintervals = LittleFloat (pinskinintervals->interval);
				if (*poutskinintervals <= 0)
					Sys_Error ("Mod_LoadAliasSkinGroup: interval<=0");

				poutskinintervals++;
				pinskinintervals++;
			}

			pskintype = (void *) pinskinintervals;
			skin = (byte *) pskintype;

			for (gnum = 0; gnum < groupskins; gnum++) {
				paliasskingroup->skindescs[gnum].type = ALIAS_SKIN_SINGLE;
				skin = mod_funcs->Mod_LoadSkin (skin, skinsize, snum, gnum,
									true, &paliasskingroup->skindescs[gnum]);
			}
		}
		pskintype = (daliasskintype_t *) skin;
	}

	return pskintype;
}

void *
Mod_LoadAliasFrame (void *pin, int *posenum, maliasframedesc_t *frame,
					int extra)
{
	daliasframe_t  *pdaliasframe;
	trivertx_t	   *pinframe;

	pdaliasframe = (daliasframe_t *) pin;

	memcpy (frame->name, pdaliasframe->name, sizeof (frame->name));
	frame->name[sizeof (frame->name) - 1] = 0;
	frame->firstpose = (*posenum);
	frame->numposes = 1;

	// byte values, don't worry about endianness
	VectorCopy (pdaliasframe->bboxmin.v, frame->bboxmin.v);
	VectorCopy (pdaliasframe->bboxmax.v, frame->bboxmax.v);
	VectorCompMin (frame->bboxmin.v, aliasbboxmins, aliasbboxmins);
	VectorCompMax (frame->bboxmax.v, aliasbboxmaxs, aliasbboxmaxs);

	pinframe = (trivertx_t *) (pdaliasframe + 1);

	DARRAY_APPEND (&poseverts, pinframe);
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
Mod_LoadAliasModel (model_t *mod, void *buffer, cache_allocator_t allocator)
{
	int         size, version, numframes, start, end, total, i, j;
	int			extra = 0;		// extra precision bytes
	void       *mem;
	dtriangle_t *pintriangles;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;
	mdl_t      *pinmodel, *pmodel;
	unsigned short crc;
	stvert_t   *pinstverts;

	if (LittleLong (* (unsigned int *) buffer) == HEADER_MDL16)
		extra = 1;		// extra precision bytes

	CRC_Init (&crc);
	CRC_ProcessBlock (buffer, &crc, qfs_filesize);

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *) buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION_MDL)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				   mod->name, version, ALIAS_VERSION_MDL);

	// allocate space for a working header, plus all the data except the
	// frames, skin and group info
	size = field_offset (aliashdr_t, frames[LittleLong (pinmodel->numframes)]);
	pheader = Hunk_AllocName (size, loadname);
	memset (pheader, 0, size);
	pmodel = &pheader->mdl;
	pheader->model = (byte *) pmodel - (byte *) pheader;

	pheader->crc = crc;

	mod->flags = LittleLong (pinmodel->flags);

	// endian-adjust and copy the data, starting with the alias model header
	pmodel->ident = LittleLong (pinmodel->ident);
	pmodel->boundingradius = LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->numskins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	DARRAY_RESIZE (&poseverts, 0);
	pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
		Sys_Error ("model %s has no vertices", mod->name);

	DARRAY_RESIZE (&stverts, pmodel->numverts);

	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
		Sys_Error ("model %s has no triangles", mod->name);

	DARRAY_RESIZE (&triangles, pmodel->numtris);

	pmodel->numframes = LittleLong (pinmodel->numframes);
	numframes = pmodel->numframes;
	if (numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d", numframes);

	pmodel->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i = 0; i < 3; i++) {
		pmodel->scale[i] = LittleFloat (pinmodel->scale[i]);
		pmodel->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pmodel->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	// load the skins
	pskintype = (daliasskintype_t *) &pinmodel[1];
	pskintype = Mod_LoadAllSkins (pheader->mdl.numskins, pskintype,
								  &pheader->skindesc);

	// load base s and t vertices
	pinstverts = (stvert_t *) pskintype;

	for (i = 0; i < pheader->mdl.numverts; i++) {
		stverts.a[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts.a[i].s = LittleLong (pinstverts[i].s);
		stverts.a[i].t = LittleLong (pinstverts[i].t);
	}

	// load triangle lists
	pintriangles = (dtriangle_t *) &pinstverts[pheader->mdl.numverts];

	for (i = 0; i < pheader->mdl.numtris; i++) {
		triangles.a[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++) {
			triangles.a[i].vertindex[j] =
				LittleLong (pintriangles[i].vertindex[j]);
		}
	}

	// load the frames
	posenum = 0;
	pframetype = (daliasframetype_t *) &pintriangles[pheader->mdl.numtris];
	aliasbboxmins[0] = aliasbboxmins[1] = aliasbboxmins[2] =  99999;
	aliasbboxmaxs[0] = aliasbboxmaxs[1] = aliasbboxmaxs[2] = -99999;

	for (i = 0; i < numframes; i++) {
		aliasframetype_t frametype;

		frametype = LittleLong (pframetype->type);
		pheader->frames[i].type = frametype;

		if (frametype == ALIAS_SINGLE) {
			pframetype = (daliasframetype_t *)
				Mod_LoadAliasFrame (pframetype + 1, &posenum,
									&pheader->frames[i], extra);
		} else {
			pframetype = (daliasframetype_t *)
				Mod_LoadAliasGroup (pframetype + 1, &posenum,
									&pheader->frames[i], extra);
		}
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

	for (i = 0; i < 3; i++) {
		mod->mins[i] = aliasbboxmins[i] * pheader->mdl.scale[i] +
			pheader->mdl.scale_origin[i];
		mod->maxs[i] = aliasbboxmaxs[i] * pheader->mdl.scale[i] +
			pheader->mdl.scale_origin[i];
	}

	mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

	// build the draw lists
	m_funcs->Mod_MakeAliasModelDisplayLists (mod, pheader, buffer,
											 qfs_filesize, extra);

	m_funcs->Mod_FinalizeAliasModel (mod, pheader);

	m_funcs->Mod_LoadExternalSkins (mod);

	// move the complete, relocatable alias model to the cache
	if (m_funcs->alias_cache) {
		end = Hunk_LowMark ();
		total = end - start;

		mem = allocator (&mod->cache, total, loadname);
		if (mem)
			memcpy (mem, pheader, total);

		Hunk_FreeToLowMark (start);
		mod->aliashdr = 0;
	} else {
		mod->aliashdr = pheader;
	}
}
