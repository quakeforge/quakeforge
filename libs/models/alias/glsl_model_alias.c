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
	aliashdr_t *header;
	GLuint      bufs[2];
	maliasskindesc_t *skins;
	maliasskingroup_t *group;

	m->needload = true;

	if (!(header = m->aliashdr))
		header = Cache_Get (&m->cache);

	bufs[0] = header->posedata;
	bufs[1] = header->commands;
	qfeglDeleteBuffers (2, bufs);

	skins = ((maliasskindesc_t *) ((byte *) header + header->skindesc));
	for (i = 0; i < header->mdl.numskins; i++) {
		if (skins[i].type == ALIAS_SKIN_GROUP) {
			group = (maliasskingroup_t *) ((byte *) header + skins[i].skin);
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

static void *
glsl_Mod_LoadSkin (mod_alias_ctx_t *alias_ctx, byte *skin, int skinsize,
				   int snum, int gnum, qboolean group,
				   maliasskindesc_t *skindesc)
{
	aliashdr_t *header = alias_ctx->header;
	byte       *tskin;
	const char *name;
	int         w, h;

	w = header->mdl.skinwidth;
	h = header->mdl.skinheight;
	tskin = malloc (skinsize);
	memcpy (tskin, skin, skinsize);
	Mod_FloodFillSkin (tskin, w, h);
	if (group)
		name = va (0, "%s_%i_%i", alias_ctx->mod->path, snum, gnum);
	else
		name = va (0, "%s_%i", alias_ctx->mod->path, snum);
	skindesc->texnum = GLSL_LoadQuakeTexture (name, w, h, tskin);
	free (tskin);
	return skin + skinsize;
}

void
glsl_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx)
{
	aliashdr_t *header = alias_ctx->header;
	int         skinsize = header->mdl.skinwidth * header->mdl.skinheight;

	for (size_t i = 0; i < alias_ctx->skins.size; i++) {
		__auto_type skin = alias_ctx->skins.a + i;
		glsl_Mod_LoadSkin (alias_ctx, skin->texels, skinsize,
						   skin->skin_num, skin->group_num,
						   skin->group_num != -1, skin->skindesc);
	}
}

void
glsl_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx)
{
	aliashdr_t *header = alias_ctx->header;

	if (header->mdl.ident == HEADER_MDL16)
		VectorScale (header->mdl.scale, 1/256.0, header->mdl.scale);
	alias_ctx->mod->clear = glsl_alias_clear;
}

void
glsl_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx)
{
}

void
glsl_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx, void *_m,
									 int _s, int extra)
{
	aliashdr_t *header = alias_ctx->header;
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

	numverts = header->mdl.numverts;
	numtris = header->mdl.numtris;

	// copy triangles before editing them
	tris = malloc (numtris * sizeof (mtriangle_t));
	memcpy (tris, alias_ctx->triangles.a, numtris * sizeof (mtriangle_t));

	// initialize indexmap to -1 (unduplicated). any other value indicates
	// both that the vertex has been duplicated and the index of the
	// duplicate vertex.
	indexmap = malloc (numverts * sizeof (int));
	memset (indexmap, -1, numverts * sizeof (int));

	// copy stverts. need space for duplicates
	st = malloc (2 * numverts * sizeof (stvert_t));
	memcpy (st, alias_ctx->stverts.a, numverts * sizeof (stvert_t));

	// check for onseam verts, and duplicate any that are associated with
	// back-facing triangles. the s coordinate is shifted right by half
	// the skin width.
	for (i = 0; i < numtris; i++) {
		for (j = 0; j < 3; j++) {
			int         vind = tris[i].vertindex[j];
			if (st[vind].onseam && !tris[i].facesfront) {
				if (indexmap[vind] == -1) {
					st[numverts] = st[vind];
					st[numverts].s += header->mdl.skinwidth / 2;
					indexmap[vind] = numverts++;
				}
				tris[i].vertindex[j] = indexmap[vind];
			}
		}
	}

	// we now know exactly how many vertices we need, so built the vertex
	// array
	vertexsize = header->numposes * numverts * sizeof (aliasvrt_t);
	verts = malloc (vertexsize);
	for (i = 0, pose = 0; i < header->numposes; i++, pose += numverts) {
		for (j = 0; j < header->mdl.numverts; j++) {
			pv = &alias_ctx->poseverts.a[i][j];
			if (extra) {
				VectorMultAdd (pv[header->mdl.numverts].v, 256, pv->v,
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

	header->poseverts = numverts;

	// load the vertex data and indices into GL
	qfeglGenBuffers (2, bnum);
	header->posedata = bnum[0];
	header->commands = bnum[1];
	qfeglBindBuffer (GL_ARRAY_BUFFER, header->posedata);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, header->commands);
	qfeglBufferData (GL_ARRAY_BUFFER, vertexsize, verts, GL_STATIC_DRAW);
	qfeglBufferData (GL_ELEMENT_ARRAY_BUFFER, indexsize, indices,
					GL_STATIC_DRAW);

	// all done
	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
	free (verts);
	free (indices);
}
