/*
	vulkan_model_mesh.c

	mesh model processing for Vulkan

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

#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/qf_mesh.h"
#include "QF/Vulkan/qf_model.h"
#include "QF/Vulkan/qf_texture.h"

#include "mod_internal.h"
#include "r_shared.h"
#include "vid_vulkan.h"

static void
vulkan_mesh_clear (model_t *mod, void *data)
{
	vulkan_ctx_t *ctx = data;
	qfv_device_t *device = ctx->device;
	auto model = mod->model;
	if (!model) {
		return;
	}

	QFV_DeviceWaitIdle (device);

	//FIXME doesn't belong here (per-instance)
	Vulkan_MeshRemoveBones (ctx, model);

	mod->needload = true;	//FIXME is this right?
	auto rmesh = (qfv_mesh_t *) ((byte *) model + model->render_data);
	auto resources = (qfv_resource_t *) ((byte *) rmesh + rmesh->resources);
	for (int i = 0; i < 2; i++) {
		auto res = &resources[i];
		auto offset = (intptr_t) res->objects;
		res->objects = (qfv_resobj_t *) ((byte *) res + offset);
	}
	QFV_DestroyResource (device, &resources[0]);
	QFV_DestroyResource (device, &resources[1]);
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
vulkan_mesh_load_arrays (mod_mesh_ctx_t *mesh_ctx, qfv_mesh_t *rmesh,
						 qfm_attrdesc_t *attribs, uint32_t strides[3],
						 uint32_t num_vertices, qfm_type_t index_type,
						 vulkan_ctx_t *ctx)
{
	auto packet = QFV_PacketAcquire (ctx->staging, "mesh.vdata");

	auto meshes = meshes_ptr (mesh_ctx->in);
	size_t vsizes[4] = { VectorExpand (strides) };
	VectorScale (vsizes, num_vertices, vsizes);
	size_t psize = vsizes[0] + vsizes[2];
	for (uint32_t i = 0; i < mesh_ctx->in->meshes.count; i++) {
		psize += meshes[i].triangle_count * 3 * mesh_type_size (index_type);
	}
	byte *vdata[3] = {};
	vdata[0] = QFV_PacketExtend (packet, psize);
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

	qfv_scatter_t scatter[] = {
		{
			.srcOffset = QFV_PacketOffset (packet, vdata[0]),
			.dstOffset = 0,
			.length = vsizes[0],
		},
		{
			.srcOffset = QFV_PacketOffset (packet, vdata[2]),
			.dstOffset = 0,
			.length = vsizes[2],
		},
		{
			.srcOffset = QFV_PacketOffset (packet, idata),
			.dstOffset = 0,
			.length = vsizes[3],
		},
	};
	auto sb = &bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
	auto db = &bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	QFV_PacketScatterBuffer (packet, rmesh->geom_buffer, 1,
							 &scatter[0], sb, db);
	QFV_PacketScatterBuffer (packet, rmesh->rend_buffer, 1,
							 &scatter[1], sb, db);
	QFV_PacketScatterBuffer (packet, rmesh->index_buffer, 1,
							 &scatter[2], sb, db);
	QFV_PacketSubmit (packet);
}

static void
vulkan_mesh_init_bones (mod_mesh_ctx_t *mesh_ctx, qfv_resource_t *bones,
					   vulkan_ctx_t *ctx)
{
}

static void
setup_bone_resources (qfv_resource_t *bones, qfv_resobj_t *bone_obj,
					  uint32_t num_frames, vulkan_ctx_t *ctx,
					  mod_mesh_ctx_t *mesh_ctx)
{
	const char *name = mesh_ctx->mod->name;
	auto model = mesh_ctx->qf_model;

	bones[0] = (qfv_resource_t) {
		.name = name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		.num_objects = 1,
		.objects = bone_obj,
	};

	size_t joint_size = 3 * sizeof (vec4f_t);
	bone_obj[0] = (qfv_resobj_t) {
		.name = "bones",
		.type = qfv_res_buffer,
		.buffer = {
			.size = num_frames * model->joints.count * joint_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		},
	};
}

static void
setup_mesh_resources (qfv_resource_t *mesh_res, qfv_resobj_t *mesh_objs,
					  uint32_t num_objects, uint32_t strides[3],
					  uint32_t num_verts, uint32_t num_indices,
					  uint32_t num_frames,
					  vulkan_ctx_t *ctx, mod_mesh_ctx_t *mesh_ctx)
{
	const char *name = mesh_ctx->mod->name;
	uint32_t index_size = mesh_type_size (mesh_index_type (num_verts));

	mesh_res[0] = (qfv_resource_t) {
		.name = vac (ctx->va_ctx, "%s:mesh", name),
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = num_objects,
		.objects = mesh_objs,
	};
	mesh_objs[0] = (qfv_resobj_t) {
		.name = "geom",
		.type = qfv_res_buffer,
		.buffer = {
			.size = num_verts * strides[0],
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	mesh_objs[1] = (qfv_resobj_t) {
		.name = "rend",
		.type = qfv_res_buffer,
		.buffer = {
			.size = num_verts * strides[2],
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	mesh_objs[2] = (qfv_resobj_t) {
		.name = "index",
		.type = qfv_res_buffer,
		.buffer = {
			.size = index_size * num_indices,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		},
	};
}

void
Vulkan_Mod_MeshFinish (mod_mesh_ctx_t *mesh_ctx, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	auto rctx = ctx->render_context;

	mesh_ctx->mod->clear = vulkan_mesh_clear;
	mesh_ctx->mod->data = ctx;

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

	int         num_objects = 4;
	size_t size = sizeof (qfv_mesh_t)
				+ sizeof (qfm_attrdesc_t[in_attributes->count])
				+ sizeof (VkDescriptorSet[rctx->frames.size])
				+ sizeof (qfv_resource_t[2])
				+ sizeof (qfv_resobj_t[num_objects]);

	const char *name = mesh_ctx->mod->name;
	qfv_mesh_t *rmesh = Hunk_AllocName (0, size, name);
	auto attribs = (qfm_attrdesc_t *) &rmesh[1];
	auto bone_descs = (VkDescriptorSet *) &attribs[in_attributes->count];
	auto resources = (qfv_resource_t *) &bone_descs[rctx->frames.size];
	auto bones = &resources[0];
	auto mesh = &resources[1];
	auto resobj = (qfv_resobj_t *) &resources[2];

	uint32_t offsets[3] = {};
	for (uint32_t i = 0; i < in_attributes->count; i++) {
		auto a = attribs_ptr (&in_meshes[0]);
		attribs[i] = a[i];
		attribs[i].stride = mesh_attr_size (a[i]);
		if (attribs[i].attr == qfm_position
			|| attribs[i].attr == qfm_joints
			|| attribs[i].attr == qfm_weights) {
			// geometry set
			attribs[i].set = 0;
		} else {
			// render set
			attribs[i].set = 2;
		}
		attribs[i].offset = offsets[attribs[i].set];
		offsets[attribs[i].set] += attribs[i].stride;
	}
	for (uint32_t i = 0; i < in_attributes->count; i++) {
		attribs[i].stride = offsets[attribs[i].set];
	}

	setup_bone_resources (bones, &resobj[0], rctx->frames.size,
						  ctx, mesh_ctx);
	setup_mesh_resources (mesh, &resobj[1], num_objects - 1, offsets,
						  num_vertices, num_indices,
						  rctx->frames.size, ctx, mesh_ctx);
	QFV_CreateResource (device, mesh);
	if (bones[0].objects[0].buffer.size) {
		QFV_CreateResource (device, bones);
	}

	auto qf_model = mesh_ctx->qf_model;
	qf_model->render_data = (byte *) rmesh - (byte *) qf_model;
	*rmesh = (qfv_mesh_t) {
		.geom_buffer = mesh->objects[0].buffer.buffer,
		.rend_buffer = mesh->objects[1].buffer.buffer,
		.index_buffer = mesh->objects[2].buffer.buffer,
		.bones_buffer = bones->objects[0].buffer.buffer,
		.bones_memory = bones->memory,
		.resources = (byte *) resources - (byte *) rmesh,
		.bone_descriptors = (byte *) bone_descs - (byte *) rmesh,
	};

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
		qf_meshes[i].vertices.offset = vert_offset;
		ind_offset += qf_meshes[i].triangle_count * 3;
		vert_offset += qf_meshes[i].vertices.count;
	}

	vulkan_mesh_load_arrays (mesh_ctx, rmesh, attribs, offsets,
							 num_vertices, index_type, ctx);
	vulkan_mesh_init_bones (mesh_ctx, bones, ctx);

	for (int i = 0; i < 2; i++) {
		auto res = &resources[i];
		res->objects = (qfv_resobj_t *)((byte *) res->objects - (byte *) res);
	}

	//FIXME doesn't belong here (per-instance)
	Vulkan_MeshAddBones (ctx, qf_model);
}
