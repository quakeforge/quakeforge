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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/crc.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "compat.h"
#include "d_iface.h"
#include "r_local.h"

aliashdr_t *pheader;

stvert_t    *stverts;
mtriangle_t *triangles;
int stverts_size = 0;
int triangles_size = 0;

// a pose is a single set of vertexes.  a frame may be an animating
// sequence of poses
trivertx_t *poseverts[MAXALIASFRAMES];
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
			skin = Mod_LoadSkin (skin, skinsize, snum, 0, false,
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
				skin = Mod_LoadSkin (skin, skinsize, snum, gnum, true,
									 &paliasskingroup->skindescs[gnum]);
			}
		}
		pskintype = (daliasskintype_t *) skin;
	}

	return pskintype;
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

	pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
		Sys_Error ("model %s has no vertices", mod->name);

	if (pmodel->numverts > stverts_size) {
		stverts = realloc (stverts, pmodel->numverts * sizeof (stvert_t));
		if (!stverts)
			Sys_Error ("model_alias: out of memory");
		stverts_size = pmodel->numverts;
	}

	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
		Sys_Error ("model %s has no triangles", mod->name);

	if (pmodel->numtris > triangles_size) {
		triangles = realloc (triangles,
							 pmodel->numtris * sizeof (mtriangle_t));
		if (!triangles)
			Sys_Error ("model_alias: out of memory");
		triangles_size = pmodel->numtris;
	}

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
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

	// load triangle lists
	pintriangles = (dtriangle_t *) &pinstverts[pheader->mdl.numverts];

	for (i = 0; i < pheader->mdl.numtris; i++) {
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++) {
			triangles[i].vertindex[j] =
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
	Mod_MakeAliasModelDisplayLists (mod, pheader, buffer, qfs_filesize, extra);

	Mod_FinalizeAliasModel (mod, pheader);

	Mod_LoadExternalSkins (mod);

	// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	total = end - start;

	mem = allocator (&mod->cache, total, loadname);
	if (mem)
		memcpy (mem, pheader, total);

	Hunk_FreeToLowMark (start);
}
