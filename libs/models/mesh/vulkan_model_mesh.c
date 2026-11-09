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

#include "QF/animation.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"

#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
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
	auto ib = &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead];
	QFV_PacketScatterBuffer (packet, rmesh->geom_buffer, 1,
							 &scatter[0], sb, db);
	QFV_PacketScatterBuffer (packet, rmesh->rend_buffer, 1,
							 &scatter[1], sb, db);
	QFV_PacketScatterBuffer (packet, rmesh->index_buffer, 1,
							 &scatter[2], sb, ib);
	QFV_PacketSubmit (packet);
}

static void
vulkan_mesh_init_bones (mod_mesh_ctx_t *mesh_ctx, qfv_resource_t *bones,
					   vulkan_ctx_t *ctx)
{
	auto model = mesh_ctx->qf_model;
	uint32_t num_bones = model->pose.count;
	if (!num_bones) {
		return;
	}
	auto joints = (qfm_joint_t *)((byte *) model + model->pose.offset);
	auto inv = (qfm_motor_t *)((byte *) model + model->inverse.offset);
	qfm_motor_t pose[num_bones];
	for (uint32_t i = 0; i < num_bones; i++) {
		pose[i] = qfm_make_motor (joints[i]);
		if (joints[i].parent >= 0) {
			pose[i] = qfm_motor_mul (pose[joints[i].parent], pose[i]);
		}
	}
	for (uint32_t i = 0; i < num_bones; i++) {
		pose[i] = qfm_motor_mul (pose[i], inv[i]);
	}
	//FIXME use motors in the shader
	size_t size = sizeof (vec4f_t[num_bones * 3]);
	auto packet = QFV_PacketAcquire (ctx->staging, "defaul pose");
	vec4f_t *bone_data = QFV_PacketExtend (packet, size);
	for (uint32_t i = 0; i < num_bones; i++) {
		auto    b = &bone_data[i * 3];
		mat4f_t f;
		qfm_motor_to_mat (f, pose[i]);
		mat4ftranspose (f, f);
		memcpy (b, f, 3 * sizeof (vec4f_t));
	}
	auto buffer = bones->objects[0].buffer.buffer;
	QFV_PacketCopyBuffer (packet, buffer, 0,
						  &bufferBarriers[qfv_BB_ShaderRO_to_TransferWrite],
						  &bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]);
	QFV_PacketSubmit (packet);
}

static void
setup_bone_descriptors (VkDescriptorSet *bone_descs, int num_sets,
						VkBuffer bones_buffer, uint32_t num_bones,
						vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto mctx = ctx->mesh_context;

	size_t      bones_size = sizeof (vec4f_t[num_bones * 3]);
	if (!bones_size) {
		bones_size = sizeof (vec4f_t[3]);
	}

	if (!bones_buffer) {
		bones_buffer = mctx->null_bone;
	}

	VkDescriptorBufferInfo bufferInfo[num_sets];
	VkWriteDescriptorSet write[num_sets];
	for (int i = 0; i < num_sets; i++) {
		bufferInfo[i] = (VkDescriptorBufferInfo) {
			.buffer = bones_buffer,
			.offset = i * bones_size,
			.range = bones_size,
		};
		write[i] = (VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = bone_descs[i],
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &bufferInfo[i],
		};
	}
	dfunc->vkUpdateDescriptorSets (device->dev, num_sets, write, 0, 0);
}

static void
setup_bone_resources (qfv_resource_t *bones, qfv_resobj_t *bone_obj,
					  uint32_t num_joints, uint32_t num_frames,
					  vulkan_ctx_t *ctx, const char *name)
{
	bones[0] = (qfv_resource_t) {
		.name = name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 1,
		.objects = bone_obj,
	};

	if (!num_joints) {
		num_joints = 1;
	}

	size_t joint_size = 3 * sizeof (vec4f_t);
	bone_obj[0] = (qfv_resobj_t) {
		.name = "bones",
		.type = qfv_res_buffer,
		.buffer = {
			.size = num_frames * num_joints * joint_size,
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
	auto mctx = ctx->mesh_context;
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
	qfv_mesh_t *rmesh = Hunk_AllocName (mesh_ctx->hunk, size, name);
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

	auto qf_model = mesh_ctx->qf_model;
	setup_bone_resources (bones, &resobj[0], qf_model->joints.count,
						  rctx->frames.size, ctx, mesh_ctx->mod->name);
	setup_mesh_resources (mesh, &resobj[1], num_objects - 1, offsets,
						  num_vertices, num_indices,
						  rctx->frames.size, ctx, mesh_ctx);
	QFV_CreateResource (device, mesh);
	VkDescriptorSet def_pose = 0;
	if (bones[0].objects[0].buffer.size) {
		QFV_CreateResource (device, bones);

		auto bones_buffer = bones[0].objects[0].buffer.buffer;
		def_pose = QFV_DSManager_AllocSet (mctx->dsmanager);
		setup_bone_descriptors (&def_pose, 1, bones_buffer,
								qf_model->pose.count, ctx);
	}

	qf_model->render_data = (byte *) rmesh - (byte *) qf_model;
	*rmesh = (qfv_mesh_t) {
		.geom_buffer = mesh->objects[0].buffer.buffer,
		.rend_buffer = mesh->objects[1].buffer.buffer,
		.index_buffer = mesh->objects[2].buffer.buffer,
		.def_pose = def_pose,
		.resources = (byte *) resources - (byte *) rmesh,
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
}

size_t
Vulkan_animstate_size (vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	uint32_t frames = rctx->frames.size;

	size_t size = sizeof (qfv_anim_t)
				+ sizeof (qfv_resource_t)
				+ sizeof (qfv_resobj_t)
				+ sizeof (VkDescriptorSet[frames]);
	return size;
}

void
Vulkan_animstate_create (animstate_t *animstate, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	auto mctx = ctx->mesh_context;
	auto rctx = ctx->render_context;
	uint32_t frames = rctx->frames.size;

	auto anim = (qfv_anim_t *) ((byte *) animstate + animstate->render_data);
	auto resource = (qfv_resource_t *) &anim[1];
	auto bones = (qfv_resobj_t *) &resource[1];
	auto bone_descs = (VkDescriptorSet *) &bones[1];

	setup_bone_resources (resource, bones, animstate->num_joints, frames, ctx,
						  "animstate");
	QFV_CreateResource (device, resource);

	auto bones_buffer = bones->buffer.buffer;
	*anim = (qfv_anim_t) {
		.descriptors = (byte *) bone_descs - (byte *) anim,
		.resource = (byte *) resource - (byte *) anim,
		.bones_buffer = bones_buffer,
		.anim_time = INFINITY,
	};

	int num_sets = frames;
	for (int i = 0; i < num_sets; i++) {
		auto set = QFV_DSManager_AllocSet (mctx->dsmanager);
		bone_descs[i] = set;
	}
	setup_bone_descriptors (bone_descs, num_sets, bones_buffer,
							animstate->num_joints, ctx);
}

void
Vulkan_animstate_destroy (animstate_t *animstate, vulkan_ctx_t *ctx)
{
	auto anim = (qfv_anim_t *) ((byte *) animstate + animstate->render_data);
	size_t size = sizeof (qfv_resource_t)
				+ sizeof (qfv_resobj_t);
	qfv_resource_t *resource = malloc (size);
	auto bones = (qfv_resobj_t *) &resource[1];
	memcpy (resource, &anim[1], size);
	resource->objects = bones;
	QFV_QueueResourceDelete (ctx, resource, true);
}
