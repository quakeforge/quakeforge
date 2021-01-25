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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/va.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_alias.h"
#include "QF/GLSL/qf_textures.h"

#include "mod_internal.h"
#include "r_shared.h"

static vec3_t vertex_normals[NUMVERTEXNORMALS] = {
#include "anorms.h"
};

static void
glsl_alias_clear (model_t *m, void *data)
{
	int         i, j;
	aliashdr_t *hdr;
	GLuint      bufs[2];
	maliasskindesc_t *skins;
	maliasskingroup_t *group;

	m->needload = true;

	if (!(hdr = m->aliashdr))
		hdr = Cache_Get (&m->cache);

	bufs[0] = hdr->posedata;
	bufs[1] = hdr->commands;
	qfeglDeleteBuffers (2, bufs);

	skins = ((maliasskindesc_t *) ((byte *) hdr + hdr->skindesc));
	for (i = 0; i < hdr->mdl.numskins; i++) {
		if (skins[i].type == ALIAS_SKIN_GROUP) {
			group = (maliasskingroup_t *) ((byte *) hdr + skins[i].skin);
			for (j = 0; j < group->numskins; j++) {
				GLSL_ReleaseTexture (group->skindescs[j].texnum);
			}
		} else {
			GLSL_ReleaseTexture (skins[i].texnum);
		}
	}

	if (!m->aliashdr) {
		Cache_Release (&m->cache);
		Cache_Free (&m->cache);
	}
}

void *
glsl_Mod_LoadSkin (byte *skin, int skinsize, int snum, int gnum,
				   qboolean group, maliasskindesc_t *skindesc)
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
	skindesc->texnum = GLSL_LoadQuakeTexture (name, w, h, tskin);
	free (tskin);
	return skin + skinsize;
}

void
glsl_Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr)
{
	if (hdr->mdl.ident == HEADER_MDL16)
		VectorScale (hdr->mdl.scale, 1/256.0, hdr->mdl.scale);
	m->clear = glsl_alias_clear;
}

void
glsl_Mod_LoadExternalSkins (model_t *mod)
{
}

void
glsl_Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *_m,
									 int _s, int extra)
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

	numverts = hdr->mdl.numverts;
	numtris = hdr->mdl.numtris;

	// copy triangles before editing them
	tris = malloc (numtris * sizeof (mtriangle_t));
	memcpy (tris, triangles.a, numtris * sizeof (mtriangle_t));

	// initialize indexmap to -1 (unduplicated). any other value indicates
	// both that the vertex has been duplicated and the index of the
	// duplicate vertex.
	indexmap = malloc (numverts * sizeof (int));
	memset (indexmap, -1, numverts * sizeof (int));

	// copy stverts. need space for duplicates
	st = malloc (2 * numverts * sizeof (stvert_t));
	memcpy (st, stverts.a, numverts * sizeof (stvert_t));

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
			pv = &poseverts.a[i][j];
			if (extra) {
				VectorMultAdd (pv[hdr->mdl.numverts].v, 256, pv->v,
							   verts[pose + j].vertex);
			} else {
				VectorCopy (pv->v, verts[pose + j].vertex);
			}
			verts[pose + j].st[0] = st[j].s;
			verts[pose + j].st[1] = st[j].t;
			VectorScale (vertex_normals[pv->lightnormalindex], 32767,
						 verts[pose + j].normal);
			// duplicate any verts that are marked for duplication by the
			// stvert setup, using the modified st coordinates
			if (indexmap[j] != -1) {
				// the vertex position and normal are duplicated, only s and t
				// are not (and really, only s, but this feels cleaner)
				verts[pose + indexmap[j]] = verts[pose + j];
				verts[pose + indexmap[j]].st[0] = st[indexmap[j]].s;
				verts[pose + indexmap[j]].st[1] = st[indexmap[j]].t;
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

	hdr->poseverts = numverts;

	// load the vertex data and indices into GL
	qfeglGenBuffers (2, bnum);
	hdr->posedata = bnum[0];
	hdr->commands = bnum[1];
	qfeglBindBuffer (GL_ARRAY_BUFFER, hdr->posedata);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, hdr->commands);
	qfeglBufferData (GL_ARRAY_BUFFER, vertexsize, verts, GL_STATIC_DRAW);
	qfeglBufferData (GL_ELEMENT_ARRAY_BUFFER, indexsize, indices,
					GL_STATIC_DRAW);

	// all done
	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
	free (verts);
	free (indices);
}
