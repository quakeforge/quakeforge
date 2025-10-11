/*
	glsl_model_mesh.c

	mesh model processing for glsl

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_mesh.h"

#include "mod_internal.h"
#include "r_shared.h"
#include "vid_gl.h"

static void
glsl_mesh_clear (model_t *mod, void *data)
{
	auto model = mod->model;
	auto rmesh = (glsl_mesh_t *) ((byte *) model + model->render_data);
	GLuint      bufs[2];

	mod->needload = true;

	bufs[0] = rmesh->vertices;
	bufs[1] = rmesh->indices;
	qfeglDeleteBuffers (2, bufs);
}

static qf_mesh_t *
meshes_ptr (const qf_model_t *model)
{
	return (qf_mesh_t *) ((byte *) model + model->meshes.offset);
}

static qfm_attrdesc_t *
attribs_ptr (const qf_mesh_t *mesh)
{
	return (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
}

static uint32_t *
indices_ptr (const qf_mesh_t *mesh)
{
	return (uint32_t *) ((byte *) mesh + mesh->indices);
}

static void
glsl_mesh_load_arrays (mod_mesh_ctx_t *mesh_ctx, glsl_mesh_t *rmesh,
						 qfm_attrdesc_t *attribs, uint32_t strides[3],
						 uint32_t num_vertices, qfm_type_t index_type)
{
	auto meshes = meshes_ptr (mesh_ctx->in);
	size_t vsizes[4] = { VectorExpand (strides) };
	VectorScale (vsizes, num_vertices, vsizes);
	size_t psize = vsizes[0] + vsizes[2];
	for (uint32_t i = 0; i < mesh_ctx->in->meshes.count; i++) {
		psize += meshes[i].triangle_count * 3 * mesh_type_size (index_type);
	}
	byte *vdata[3] = {};
	byte packet_data[psize];
	vdata[0] = packet_data;
	vdata[2] = vdata[0] + vsizes[0];
	byte *idata = vdata[2] + vsizes[2];

	for (uint32_t i = 0; i < mesh_ctx->in->meshes.count; i++) {
		auto mesh_data = (byte *) &meshes[i] + meshes[i].vertices.offset;
		auto va = attribs_ptr (&meshes[i]);
		for (uint32_t j = 0; j < meshes[i].vertices.count; j++) {
			for (uint32_t k = 0; k < meshes[i].attributes.count; k++) {
				uint32_t size = mesh_attr_size (va[k]);
				memcpy (vdata[attribs[k].set] + attribs[k].offset,
						mesh_data + va[k].offset + j * va[k].stride, size);
			}
			VectorAdd (vdata, strides, vdata);
		}
		auto indices = indices_ptr (&meshes[i]);
		uint32_t num_indices = meshes[i].triangle_count * 3;
		auto index_bytes = pack_indices (idata + vsizes[3], indices,
										 num_indices, index_type);
		vsizes[3] += index_bytes;
	}
	VectorSubtract (vdata, vsizes, vdata);

	GLuint      bufs[2];
	qfeglGenBuffers (2, bufs); 
	rmesh->vertices = bufs[0];
	rmesh->indices = bufs[1];
	qfeglBindBuffer (GL_ARRAY_BUFFER, rmesh->vertices);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, rmesh->indices);
	qfeglBufferData (GL_ARRAY_BUFFER, vsizes[0], vdata[0], GL_STATIC_DRAW);
	qfeglBufferData (GL_ELEMENT_ARRAY_BUFFER, vsizes[3], idata,
					 GL_STATIC_DRAW);
	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void
glsl_mesh_init_bones (mod_mesh_ctx_t *mesh_ctx)
{
}

void
glsl_Mod_MeshFinish (mod_mesh_ctx_t *mesh_ctx)
{
	mesh_ctx->mod->clear = glsl_mesh_clear;

	auto in = mesh_ctx->in;
	auto in_meshes = (qf_mesh_t *) ((byte *) in + in->meshes.offset);

	auto in_attributes = &in_meshes[0].attributes;
	uint32_t num_indices = 0;
	uint32_t num_vertices = 0;
	for (uint32_t i = 0; i < in->meshes.count; i++) {
		if (in_meshes[i].attributes.count != in_attributes->count
			|| (attribs_ptr (&in_meshes[i]) != attribs_ptr (&in_meshes[0]))) {
			Sys_Error ("%s: multiple attribute descriptor sets",
					   mesh_ctx->mod->name);
		}
		num_indices += in_meshes[i].triangle_count * 3;
		num_vertices += in_meshes[i].vertices.count;
	}

	size_t size = sizeof (glsl_mesh_t)
				+ sizeof (qfm_attrdesc_t[in_attributes->count]);

	const char *name = mesh_ctx->mod->name;
	glsl_mesh_t *rmesh = Hunk_AllocName (0, size, name);
	auto attribs = (qfm_attrdesc_t *) &rmesh[1];

	uint32_t offsets[3] = {};
	for (uint32_t i = 0; i < in_attributes->count; i++) {
		auto a = attribs_ptr (&in_meshes[0]);
		attribs[i] = a[i];
		attribs[i].stride = mesh_attr_size (a[i]);
		attribs[i].set = 0;
		attribs[i].offset = offsets[attribs[i].set];
		offsets[attribs[i].set] += attribs[i].stride;
	}
	for (uint32_t i = 0; i < in_attributes->count; i++) {
		attribs[i].stride = offsets[attribs[i].set];
	}

	auto qf_model = mesh_ctx->qf_model;
	qf_model->render_data = (byte *) rmesh - (byte *) qf_model;
	*rmesh = (glsl_mesh_t) { };

	auto index_type = mesh_index_type (num_vertices);
	auto qf_meshes = mesh_ctx->qf_meshes;
	uint32_t vert_offset = 0;
	uint32_t ind_offset = 0;
	for (uint32_t i = 0; i < qf_model->meshes.count; i++) {
		qf_meshes[i].index_type = index_type,
		qf_meshes[i].indices = ind_offset,
		qf_meshes[i].attributes = (qfm_loc_t) {
			.offset = (byte *) attribs - (byte *) &qf_meshes[i],
			.count = in_attributes->count,
		};
		qf_meshes[i].vertex_stride = attribs[0].stride;
		qf_meshes[i].vertices.offset = vert_offset;
		ind_offset += qf_meshes[i].triangle_count * 3;
		vert_offset += qf_meshes[i].vertices.count;
	}

	glsl_mesh_load_arrays (mesh_ctx, rmesh, attribs, offsets,
						   num_vertices, index_type);
	glsl_mesh_init_bones (mesh_ctx);
}
