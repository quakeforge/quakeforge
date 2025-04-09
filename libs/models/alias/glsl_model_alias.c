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
#include "QF/GLSL/qf_mesh.h"
#include "QF/GLSL/qf_textures.h"

#include "mod_internal.h"
#include "qfalloca.h"
#include "r_shared.h"

static vec3_t vertex_normals[NUMVERTEXNORMALS] = {
#include "anorms.h"
};

static void
glsl_alias_clear (model_t *m, void *data)
{
	auto model = m->model;
	auto rmesh = (glsl_mesh_t *) ((byte *) model + model->render_data);
	auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);

	m->needload = true;

	GLuint      bufs[2];
	bufs[0] = rmesh->vertices;
	bufs[1] = rmesh->indices;
	qfeglDeleteBuffers (2, bufs);

	auto skin = &mesh->skin;
	auto skindesc = (keyframedesc_t *) ((byte *) mesh + skin->descriptors);
	auto skinframe = (keyframe_t *) ((byte *) mesh + skin->keyframes);
	int index = 0;

	for (uint32_t i = 0; i < skin->numdesc; i++) {
		for (uint32_t j = 0; j < skindesc[i].numframes; j++) {
			GLSL_ReleaseTexture (skinframe[index++].data);
		}
	}
}

static void
glsl_Mod_LoadSkin (mod_alias_ctx_t *alias_ctx, byte *texels,
				   int snum, int gnum, keyframe_t *skinframe)
{
	int         w = alias_ctx->skinwidth;
	int         h = alias_ctx->skinheight;
	size_t      skinsize = w * h;
	byte       *tskin = alloca (skinsize);
	const char *name;

	memcpy (tskin, texels, skinsize);
	Mod_FloodFillSkin (tskin, w, h);
	if (gnum != -1)
		name = va ("%s_%i_%i", alias_ctx->mod->path, snum, gnum);
	else
		name = va ("%s_%i", alias_ctx->mod->path, snum);
	skinframe->data = GLSL_LoadQuakeTexture (name, w, h, tskin);
}

void
glsl_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx)
{
	for (size_t i = 0; i < alias_ctx->skins.size; i++) {
		__auto_type skin = alias_ctx->skins.a + i;
		glsl_Mod_LoadSkin (alias_ctx, skin->texels,
						   skin->skin_num, skin->group_num, skin->skindesc);
	}
}

void
glsl_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx)
{
	//mesh_t     *mesh = alias_ctx->mesh;

	//if (mesh->mdl.ident == HEADER_MDL16)
	//	VectorScale (mesh->mdl.scale, 1/256.0, mesh->mdl.scale);
	alias_ctx->mod->clear = glsl_alias_clear;
}

void
glsl_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx)
{
}

static int
separate_verts (int *indexmap, int numverts, int numtris,
				const mod_alias_ctx_t *alias_ctx)
{
	// check for onseam verts, and duplicate any that are associated with
	// back-facing triangles
	for (int i = 0; i < numtris; i++) {
		for (int j = 0; j < 3; j++) {
			int         vind = alias_ctx->triangles.a[i].vertindex[j];
			if (alias_ctx->stverts.a[vind].onseam
				&& !alias_ctx->triangles.a[i].facesfront) {
				// duplicate the vertex if it has not alreaddy been
				// duplicated
				if (indexmap[vind] == -1) {
					indexmap[vind] = numverts++;
				}
			}
		}
	}
	return numverts;
}

static void
build_verts (mesh_vrt_t *verts, int numposes, int numverts,
			 const int *indexmap, const mod_alias_ctx_t *alias_ctx)
{
	auto st = alias_ctx->stverts.a;
	auto mesh = alias_ctx->mesh;
	auto mdl = alias_ctx->mdl;
	auto frames = (keyframe_t *) ((byte *) mesh + mesh->morph.keyframes);
	int         i, pose;
	// populate the vertex position and normal data, duplicating for
	// back-facing on-seam verts (indicated by non-negative indexmap entry)
	for (i = 0, pose = 0; i < numposes; i++, pose += numverts) {
		frames[i].data = i * numverts * sizeof (mesh_vrt_t);
		for (int j = 0; j < mdl->numverts; j++) {
			auto pv = &alias_ctx->poseverts.a[i][j];
			if (mdl->ident == HEADER_MDL16) {
				VectorMultAdd (pv[mdl->numverts].v, 256, pv->v,
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
				// are not (and really, only s)
				verts[pose + indexmap[j]] = verts[pose + j];
				verts[pose + indexmap[j]].st[0] += mdl->skinwidth / 2;
			}
		}
	}
}

static void
build_inds (uint32_t *indices, int numtris, const int *indexmap,
			const mod_alias_ctx_t *alias_ctx)
{

	// now build the indices for DrawElements
	for (int i = 0; i < numtris; i++) {
		for (int j = 0; j < 3; j++) {
			int         vind = alias_ctx->triangles.a[i].vertindex[j];
			// can't use indexmap to do the test because it indicates only
			// that the vertex has been duplicated, not whether or not
			// the vertex is the original or the duplicate
			if (alias_ctx->stverts.a[vind].onseam
				&& !alias_ctx->triangles.a[i].facesfront) {
				vind = indexmap[vind];
			}
			indices[3 * i + j] = vind;
		}
	}
}

void
glsl_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx, void *_m,
									 int _s, int extra)
{
	auto model = alias_ctx->model;
	auto mesh = alias_ctx->mesh;
	int         numverts = alias_ctx->stverts.size;
	int         numtris = alias_ctx->triangles.size;
	int         numposes = alias_ctx->poseverts.size;

	// copy triangles before editing them
	dtriangle_t tris[numtris];
	memcpy (tris, alias_ctx->triangles.a, sizeof (tris));

	// initialize indexmap to -1 (unduplicated). any other value indicates
	// both that the vertex has been duplicated and the index of the
	// duplicate vertex.
	int indexmap[numverts];
	memset (indexmap, -1, sizeof (indexmap));

	numverts = separate_verts (indexmap, numverts, numtris, alias_ctx);

	mesh_vrt_t  verts[numverts * numposes];
	build_verts (verts, numposes, numverts, indexmap, alias_ctx);

	// now build the indices for DrawElements
	uint32_t indices[3 * numtris];
	build_inds (indices, numtris, indexmap, alias_ctx);

	if (extra) {
		auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
		VectorScale (mesh->scale, 1.0f/256, mesh->scale);
	}

	GLuint      bnum[2];
	qfeglGenBuffers (2, bnum);

	size_t size = sizeof (glsl_mesh_t)
				+ sizeof (qfm_attrdesc_t[3]);
	const char * name = alias_ctx->mod->name;
	glsl_mesh_t *rmesh = Hunk_AllocName (nullptr, size, name);
	auto attribs = (qfm_attrdesc_t *) &rmesh[1];

	*rmesh = (glsl_mesh_t) {
		.skinwidth = alias_ctx->skinwidth,
		.skinheight = alias_ctx->skinheight,
		.vertices = bnum[0],
		.indices = bnum[1],
	};
	model->render_data = (byte *) rmesh - (byte *) model;

	mesh->triangle_count = numtris;
	mesh->index_type = mesh_index_type (numverts);
	mesh->attributes = (qfm_loc_t) {
		.offset = (byte *) attribs - (byte *) mesh,
		.count = 3,
	};
	mesh->vertices = (qfm_loc_t) {
		.count = numverts,
	};
	// alias model morphing is absolute so just copy the base vertex attributes
	mesh->morph_attributes = mesh->attributes;
	mesh->vertex_stride = sizeof (mesh_vrt_t);
	mesh->morph_stride = mesh->vertex_stride;

	attribs[0] = (qfm_attrdesc_t) {
		.offset = offsetof (mesh_vrt_t, vertex),
		.stride = sizeof (mesh_vrt_t),
		.attr = qfm_position,
		.abs = 1,
		.type = qfm_u16,
		.components = 3,
	};
	attribs[1] = (qfm_attrdesc_t) {
		.offset = offsetof (mesh_vrt_t, normal),
		.stride = sizeof (mesh_vrt_t),
		.attr = qfm_normal,
		.abs = 1,
		.type = qfm_s16n,
		.components = 3,
	};
	attribs[2] = (qfm_attrdesc_t) {
		.offset = offsetof (mesh_vrt_t, st),
		.stride = sizeof (mesh_vrt_t),
		.attr = qfm_texcoord,
		.abs = 1,
		.type = qfm_s16,
		.components = 2,
	};

	uint32_t indices_size = pack_indices (indices, numtris * 3,
										  mesh->index_type);

	qfeglBindBuffer (GL_ARRAY_BUFFER, rmesh->vertices);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, rmesh->indices);
	qfeglBufferData (GL_ARRAY_BUFFER, sizeof (verts), verts, GL_STATIC_DRAW);
	qfeglBufferData (GL_ELEMENT_ARRAY_BUFFER, indices_size, indices,
					 GL_STATIC_DRAW);

	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}
