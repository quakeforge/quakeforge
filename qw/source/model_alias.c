/*
	gl_model.c

	model loading and caching

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

	$Id$
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

#include "cl_main.h"
#include "client.h"
#include "crc.h"
#include "info.h"
#include "model.h"
#include "msg.h"
#include "qendian.h"
#include "quakefs.h"
#include "r_local.h"
#include "server.h"

extern char loadname[];
extern model_t *loadmodel;

void       *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype,

							  int *pskinindex);
void        GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *model, int size);

/*
	ALIAS MODELS
*/

aliashdr_t *pheader;

stvert_t    stverts[MAXALIASVERTS];
mtriangle_t triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t *poseverts[MAXALIASFRAMES];
int         posenum = 0;

void       *Mod_LoadAliasFrame (void *pin, maliasframedesc_t *frame);
void       *Mod_LoadAliasGroup (void *pin, maliasframedesc_t *frame);


//=========================================================================

/*
	Mod_LoadAliasModel
*/
void
Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int         i, j;
	mdl_t      *pinmodel, *pmodel;
	stvert_t   *pinstverts;
	dtriangle_t *pintriangles;
	int         version, numframes;
	int         size;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;
	int         start, end, total;

	if (!strcmp (loadmodel->name, "progs/player.mdl") ||
		!strcmp (loadmodel->name, "progs/eyes.mdl")) {
		unsigned short crc;
		byte       *p;
		int         len;
		char        st[40];

		CRC_Init (&crc);
		for (len = com_filesize, p = buffer; len; len--, p++)
			CRC_ProcessByte (&crc, *p);

		snprintf (st, sizeof (st), "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo,
							 !strcmp (loadmodel->name,
									  "progs/player.mdl") ? pmodel_name :
							 emodel_name, st, MAX_INFO_STRING);

		if (cls.state >= ca_connected) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			snprintf (st, sizeof (st), "setinfo %s %d",
					  !strcmp (loadmodel->name,
							   "progs/player.mdl") ? pmodel_name : emodel_name,
					  (int) crc);
			SZ_Print (&cls.netchan.message, st);
		}
	}

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *) buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		SV_Error ("%s has wrong version number (%i should be %i)",
				  mod->name, version, ALIAS_VERSION);

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = (int) &((aliashdr_t *) 0)->frames[LittleLong (pinmodel->numframes)];
	pheader = Hunk_AllocName (size, loadname);
	memset (pheader, 0, size);
	pmodel = &pheader->mdl;
	pheader->model = (byte *) pmodel - (byte *) pheader;

	mod->flags = LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pmodel->boundingradius = LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->numskins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
		SV_Error ("model %s has a skin taller than %d", mod->name,
				  MAX_LBM_HEIGHT);

	pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
		SV_Error ("model %s has no vertices", mod->name);

	if (pmodel->numverts > MAXALIASVERTS)
		SV_Error ("model %s has too many vertices", mod->name);

	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
		SV_Error ("model %s has no triangles", mod->name);

	pmodel->numframes = LittleLong (pinmodel->numframes);
	numframes = pmodel->numframes;
	if (numframes < 1)
		SV_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pmodel->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i = 0; i < 3; i++) {
		pmodel->scale[i] = LittleFloat (pinmodel->scale[i]);
		pmodel->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pmodel->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}


//
// load the skins
//
	pskintype = (daliasskintype_t *) &pinmodel[1];
	pskintype =
		Mod_LoadAllSkins (pheader->mdl.numskins, pskintype, &pheader->skindesc);

//
// load base s and t vertices
//
	pinstverts = (stvert_t *) pskintype;

	for (i = 0; i < pheader->mdl.numverts; i++) {
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

//
// load triangle lists
//
	pintriangles = (dtriangle_t *) &pinstverts[pheader->mdl.numverts];

	for (i = 0; i < pheader->mdl.numtris; i++) {
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++) {
			triangles[i].vertindex[j] =
				LittleLong (pintriangles[i].vertindex[j]);
		}
	}

//
// load the frames
//
	posenum = 0;
	pframetype = (daliasframetype_t *) &pintriangles[pheader->mdl.numtris];

	for (i = 0; i < numframes; i++) {
		aliasframetype_t frametype;

		frametype = LittleLong (pframetype->type);
		pheader->frames[i].type = frametype;

		if (frametype == ALIAS_SINGLE) {
			pframetype = (daliasframetype_t *)
				Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		} else {
			pframetype = (daliasframetype_t *)
				Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
		}
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

// FIXME: do this right
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;

	// 
	// build the draw lists
	// 
	GL_MakeAliasModelDisplayLists (mod, pheader, buffer, com_filesize);

//
// move the complete, relocatable alias model to the cache
//  
	end = Hunk_LowMark ();
	total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}
