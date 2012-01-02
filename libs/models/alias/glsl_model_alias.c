/*
	glsl_model_alais.c

	Alias model processing for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/model.h"
#include "QF/va.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_alias.h"
#include "QF/GLSL/qf_textures.h"

void *
Mod_LoadSkin (byte *skin, int skinsize, int snum, int gnum, qboolean group,
			  maliasskindesc_t *skindesc)
{
	byte       *tskin;
	const char *name;
	int         w, h;

	w = pheader->mdl.skinwidth;
	h = pheader->mdl.skinheight;
	tskin = malloc (skinsize);
	memcpy (tskin, skin, skinsize);
	Mod_FloodFillSkin (tskin, w, h);
	if (group)
		name = va ("%s_%i_%i", loadmodel->name, snum, gnum);
	else
		name = va ("%s_%i", loadmodel->name, snum);
	skindesc->texnum = GL_LoadQuakeTexture (name, w, h, tskin);
	free (tskin);
	return skin + skinsize;
}

void
Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr)
{
}

void
Mod_LoadExternalSkins (model_t *mod)
{
}

void
Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *_m, int _s,
								int extra)
{
	mtriangle_t *tris;
	stvert_t   *st;
	aliasvrt_t *verts;
	trivertx_t *pv;
	int        *indexmap;
	GLushort   *indices;
	GLuint      bnum[2];
	int         vertexsize, indexsize;
	int         numverts;
	int         numtris;
	int         i, j;
	int         pose;
	float       w, h;

	numverts = hdr->mdl.numverts;
	numtris = hdr->mdl.numtris;
	w = hdr->mdl.skinwidth;
	h = hdr->mdl.skinheight;

	// copy triangles before editing them
	tris = malloc (numtris * sizeof (mtriangle_t));
	memcpy (tris, triangles, numtris * sizeof (mtriangle_t));

	// initialize indexmap to -1 (unduplicated). any other value indicates
	// both that the vertex has been duplicated and the index of the
	// duplicate vertex.
	indexmap = malloc (numverts * sizeof (int));
	memset (indexmap, -1, numverts * sizeof (int));

	// copy stverts. need space for duplicates
	st = malloc (2 * numverts * sizeof (stvert_t));
	memcpy (st, stverts, numverts * sizeof (stvert_t));

	// check for onseam verts, and duplicate any that are associated with
	// back-facing triangles. the s coordinate is shifted right by half
	// the skin width.
	for (i = 0; i < numtris; i++) {
		for (j = 0; j < 3; j++) {
			int         vind = tris[i].vertindex[j];
			if (st[vind].onseam && !tris[i].facesfront) {
				if (indexmap[vind] == -1) {
					st[numverts] = st[vind];
					st[numverts].s += hdr->mdl.skinwidth / 2;
					indexmap[vind] = numverts++;
				}
				tris[i].vertindex[j] = indexmap[vind];
			}
		}
	}

	// we now know exactly how many vertices we need, so built the vertex
	// array
	vertexsize = hdr->numposes * numverts * sizeof (aliasvrt_t);
	verts = malloc (vertexsize);
	for (i = 0, pose = 0; i < hdr->numposes; i++, pose += numverts) {
		for (j = 0; j < hdr->mdl.numverts; j++) {
			pv = &poseverts[i][j];
			VectorCopy (pv->v, verts[pose + j].vertex);
			VectorSet (st[j].s / w, st[j].t / h, pv->lightnormalindex,
					   verts[pose + j].stn);
			// duplicate any verts that are marked for duplication by the
			// stvert setup, using the modified st coordinates
			if (indexmap[j] != -1) {
				// the vertex position and normal are duplicated, only s and t
				// are not (and really, only s, but this feels cleaner)
				verts[pose + indexmap[j]] = verts[pose + j];
				verts[pose + indexmap[j]].stn[0] = st[indexmap[j]].s / w;
				verts[pose + indexmap[j]].stn[1] = st[indexmap[j]].t / h;
			}
		}
	}

	// finished with st and indexmap
	free (st);
	free (indexmap);

	// now build the indices for DrawElements
	indexsize = 3 * numtris * sizeof (GLushort);
	indices = malloc (indexsize);
	for (i = 0; i < numtris; i++)
		VectorCopy (tris[i].vertindex, indices + 3 * i);

	// finished with tris
	free (tris);

	// load the vertex data and indices into GL
	qfglGenBuffers (2, bnum);
	hdr->posedata = bnum[0];
	hdr->commands = bnum[1];
	qfglBindBuffer (GL_ARRAY_BUFFER, hdr->posedata);
	qfglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, hdr->commands);
	qfglBufferData (GL_ARRAY_BUFFER, vertexsize, verts, GL_STATIC_DRAW);
	qfglBufferData (GL_ELEMENT_ARRAY_BUFFER, indexsize, indices,
					GL_STATIC_DRAW);

	// all done
	qfglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
	free (verts);
	free (indices);
}
