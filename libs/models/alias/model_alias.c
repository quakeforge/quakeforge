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

static void *
Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx, int numskins,
				  daliasskintype_t *pskintype, int *pskinindex)
{
	aliashdr_t *header = alias_ctx->header;
	byte       *skin;
	float      *poutskinintervals;
	int         groupskins, skinsize, gnum, snum, t;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;
	maliasskindesc_t *pskindesc;
	maliasskingroup_t *paliasskingroup;

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d", numskins);

	skinsize = header->mdl.skinwidth * header->mdl.skinheight;
	pskindesc = Hunk_AllocName (0, numskins * sizeof (maliasskindesc_t),
								alias_ctx->mod->name);

	*pskinindex = (byte *) pskindesc - (byte *) header;

	for (snum = 0; snum < numskins; snum++) {
		pskindesc[snum].type = pskintype->type;
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			skin = (byte *) (pskintype + 1);
			mod_alias_skin_t askin = {
				.skin_num = snum,
				.group_num = -1,
				.texels = skin,
				.skindesc = &pskindesc[snum],
			};
			skin += skinsize;
			DARRAY_APPEND (&alias_ctx->skins, askin);
		} else {
			pskintype++;
			pinskingroup = (daliasskingroup_t *) pskintype;
			groupskins = LittleLong (pinskingroup->numskins);

			t = field_offset (maliasskingroup_t, skindescs[groupskins]);
			paliasskingroup = Hunk_AllocName (0, t, alias_ctx->mod->name);
			paliasskingroup->numskins = groupskins;

			pskindesc[snum].skin = (byte *) paliasskingroup - (byte *) header;

			pinskinintervals = (daliasskininterval_t *) (pinskingroup + 1);
			poutskinintervals = Hunk_AllocName (0, groupskins * sizeof (float),
												alias_ctx->mod->name);
			paliasskingroup->intervals =
				(byte *) poutskinintervals - (byte *) header;
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
				mod_alias_skin_t askin = {
					.skin_num = snum,
					.group_num = gnum,
					.texels = skin,
					.skindesc = &paliasskingroup->skindescs[gnum],
				};
				skin += skinsize;
				DARRAY_APPEND (&alias_ctx->skins, askin);
			}
		}
		pskintype = (daliasskintype_t *) skin;
	}
	mod_funcs->Mod_LoadAllSkins (alias_ctx);

	return pskintype;
}

static void *
Mod_LoadAliasFrame (mod_alias_ctx_t *alias_ctx, void *pin, int *posenum,
					maliasframedesc_t *frame, int extra)
{
	aliashdr_t *header = alias_ctx->header;
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
	VectorCompMin (frame->bboxmin.v, alias_ctx->aliasbboxmins,
				   alias_ctx->aliasbboxmins);
	VectorCompMax (frame->bboxmax.v, alias_ctx->aliasbboxmaxs,
				   alias_ctx->aliasbboxmaxs);

	pinframe = (trivertx_t *) (pdaliasframe + 1);

	DARRAY_APPEND (&alias_ctx->poseverts, pinframe);
	(*posenum)++;

	pinframe += header->mdl.numverts;
	if (extra)
		pinframe += header->mdl.numverts;

	return pinframe;
}

static void *
Mod_LoadAliasGroup (mod_alias_ctx_t *alias_ctx, void *pin, int *posenum,
					maliasframedesc_t *frame, int extra)
{
	aliashdr_t *header = alias_ctx->header;
	model_t    *mod = alias_ctx->mod;
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

	paliasgroup = Hunk_AllocName (0, field_offset (maliasgroup_t,
												   frames[numframes]),
								  mod->name);
	paliasgroup->numframes = numframes;
	frame->frame = (byte *) paliasgroup - (byte *) header;

	// these are byte values, so we don't have to worry about endianness
	VectorCopy (pingroup->bboxmin.v, frame->bboxmin.v);
	VectorCopy (pingroup->bboxmax.v, frame->bboxmax.v);
	VectorCompMin (frame->bboxmin.v, alias_ctx->aliasbboxmins,
				   alias_ctx->aliasbboxmins);
	VectorCompMax (frame->bboxmax.v, alias_ctx->aliasbboxmaxs,
				   alias_ctx->aliasbboxmaxs);

	pin_intervals = (daliasinterval_t *) (pingroup + 1);
	poutintervals = Hunk_AllocName (0, numframes * sizeof (float), mod->name);
	paliasgroup->intervals = (byte *) poutintervals - (byte *) header;
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
		ptemp = Mod_LoadAliasFrame (alias_ctx, ptemp, posenum, &temp_frame,
									extra);
		memcpy (&paliasgroup->frames[i], &temp_frame,
				sizeof (paliasgroup->frames[i]));
	}

	return ptemp;
}

void
Mod_LoadAliasModel (model_t *mod, void *buffer, cache_allocator_t allocator)
{
	size_t      size, start, end, total;
	int         version, numframes, i, j;
	int			extra = 0;		// extra precision bytes
	void       *mem;
	dtriangle_t *pintriangles;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;
	mdl_t      *pinmodel, *pmodel;
	unsigned short crc;
	stvert_t   *pinstverts;
	mod_alias_ctx_t alias_ctx = {};
	aliashdr_t *header;

	alias_ctx.mod = mod;
	//FIXME should be per batch rather than per model
	DARRAY_INIT (&alias_ctx.poseverts, 256);
	DARRAY_INIT (&alias_ctx.stverts, 256);
	DARRAY_INIT (&alias_ctx.triangles, 256);
	DARRAY_INIT (&alias_ctx.skins, 256);

	if (LittleLong (* (unsigned int *) buffer) == HEADER_MDL16)
		extra = 1;		// extra precision bytes

	CRC_Init (&crc);
	CRC_ProcessBlock (buffer, &crc, qfs_filesize);

	start = Hunk_LowMark (0);

	pinmodel = (mdl_t *) buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION_MDL)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				   mod->name, version, ALIAS_VERSION_MDL);

	// allocate space for a working header, plus all the data except the
	// frame data, skin and group info
	size = field_offset (aliashdr_t, frames[LittleLong (pinmodel->numframes)]);
	header = Hunk_AllocName (0, size, mod->name);
	memset (header, 0, size);
	alias_ctx.header = header;
	pmodel = &header->mdl;
	header->model = (byte *) pmodel - (byte *) header;

	header->crc = crc;

	mod->effects = LittleLong (pinmodel->flags);

	// endian-adjust and copy the data, starting with the alias model header
	pmodel->ident = LittleLong (pinmodel->ident);
	pmodel->boundingradius = LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->numskins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	DARRAY_RESIZE (&alias_ctx.poseverts, 0);
	pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
		Sys_Error ("model %s has no vertices", mod->name);

	DARRAY_RESIZE (&alias_ctx.stverts, pmodel->numverts);

	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
		Sys_Error ("model %s has no triangles", mod->name);

	DARRAY_RESIZE (&alias_ctx.triangles, pmodel->numtris);

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
	pskintype = Mod_LoadAllSkins (&alias_ctx, header->mdl.numskins, pskintype,
								  &header->skindesc);

	// load base s and t vertices
	pinstverts = (stvert_t *) pskintype;

	for (i = 0; i < header->mdl.numverts; i++) {
		alias_ctx.stverts.a[i].onseam = LittleLong (pinstverts[i].onseam);
		alias_ctx.stverts.a[i].s = LittleLong (pinstverts[i].s);
		alias_ctx.stverts.a[i].t = LittleLong (pinstverts[i].t);
	}

	// load triangle lists
	pintriangles = (dtriangle_t *) &pinstverts[header->mdl.numverts];

	for (i = 0; i < header->mdl.numtris; i++) {
		alias_ctx.triangles.a[i].facesfront =
			LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++) {
			alias_ctx.triangles.a[i].vertindex[j] =
				LittleLong (pintriangles[i].vertindex[j]);
		}
	}

	// load the frames
	int posenum = 0;
	pframetype = (daliasframetype_t *) &pintriangles[header->mdl.numtris];
	VectorSet (99999, 99999, 99999, alias_ctx.aliasbboxmins);
	VectorSet (-99999, -99999, -99999, alias_ctx.aliasbboxmaxs);

	for (i = 0; i < numframes; i++) {
		aliasframetype_t frametype;

		frametype = LittleLong (pframetype->type);
		header->frames[i].type = frametype;

		if (frametype == ALIAS_SINGLE) {
			pframetype = (daliasframetype_t *)
				Mod_LoadAliasFrame (&alias_ctx, pframetype + 1, &posenum,
									&header->frames[i], extra);
		} else {
			pframetype = (daliasframetype_t *)
				Mod_LoadAliasGroup (&alias_ctx, pframetype + 1, &posenum,
									&header->frames[i], extra);
		}
	}

	header->numposes = posenum;

	mod->type = mod_alias;

	VectorCompMultAdd (header->mdl.scale_origin, header->mdl.scale,
					   alias_ctx.aliasbboxmins, mod->mins);
	VectorCompMultAdd (header->mdl.scale_origin, header->mdl.scale,
					   alias_ctx.aliasbboxmaxs, mod->maxs);

	mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

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
		end = Hunk_LowMark (0);
		total = end - start;

		mem = allocator (&mod->cache, total, mod->name);
		if (mem)
			memcpy (mem, header, total);

		Hunk_FreeToLowMark (0, start);
		mod->aliashdr = 0;
	} else {
		mod->aliashdr = header;
	}
	DARRAY_CLEAR (&alias_ctx.poseverts);
	DARRAY_CLEAR (&alias_ctx.stverts);
	DARRAY_CLEAR (&alias_ctx.triangles);
	DARRAY_CLEAR (&alias_ctx.skins);
}
