/*
	vulkan_model_iqm.c

	iqm model processing for Vulkan

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/05/03

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
#include "QF/Vulkan/qf_iqm.h"
#include "QF/Vulkan/qf_mesh.h"
#include "QF/Vulkan/qf_model.h"
#include "QF/Vulkan/qf_texture.h"

#include "mod_internal.h"
#include "r_shared.h"
#include "vid_vulkan.h"

static byte null_texture[] = {
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
};
#if 0
static byte null_normmap[] = {
	127, 127, 255, 255,
	127, 127, 255, 255,
	127, 127, 255, 255,
	127, 127, 255, 255,
};
#endif

static void
vulkan_iqm_clear (model_t *mod, void *data)
{
	vulkan_ctx_t *ctx = data;
	qfv_device_t *device = ctx->device;
	auto model = mod->model;

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

static void
vulkan_iqm_init_image (const char *text, qf_mesh_t *mesh, qfv_resobj_t *image)
{
	const char *material = text + mesh->material;
	dstring_t  *str = dstring_new ();
	dstring_copystr (str, material);
	QFS_StripExtension (str->str, str->str);

	tex_t       dummy_tex = {
		.width = 2,
		.height = 2,
		.format = tex_rgba,
	};
	tex_t      *tex;
	if (!(tex = LoadImage (va (0, "textures/%s", str->str), 0))) {
		tex = &dummy_tex;
	}
	QFV_ResourceInitTexImage (image, material, 1, tex);
	image->image.num_layers = 3;
	dstring_delete (str);
}

static void
iqm_transfer_texture (tex_t *tex, VkImage image, qfv_stagebuf_t *stage,
					  qfv_device_t *device)
{
	if (tex->format != tex_rgb && tex->format != tex_rgba) {
		Sys_Error ("can't transfer iqm image");
	}
	// FIXME correct only for rgb and rgba
	size_t      layer_size = tex->width * tex->height * tex->format;

	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	byte       *dst = QFV_PacketExtend (packet, layer_size * 3);

	memcpy (dst, tex->data, layer_size);
	// don't have glow/color maps yet
	memset (dst + layer_size, 0, 2 * layer_size);

	int mipLevels = QFV_MipLevels (tex->width, tex->height);
	auto sb = &imageBarriers[qfv_LT_Undefined_to_TransferDst];
	auto ib = mipLevels == 1
			? &imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly]
			: nullptr;
	QFV_PacketCopyImage (packet, image, tex->width, tex->height, sb, ib);

	if (mipLevels != 1) {
		QFV_GenerateMipMaps (device, packet->cmd, image, mipLevels,
							 tex->width, tex->height, 3);
	}
	QFV_PacketSubmit (packet);
}

static void
vulkan_iqm_load_textures (mod_iqm_ctx_t *iqm_ctx, qfv_mesh_t *rmesh,
						  qfv_skin_t *skins, qfv_resobj_t *objects,
						  vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	auto model = iqm_ctx->qf_model;
	auto meshes = iqm_ctx->qf_meshes;
	dstring_t  *str = dstring_new ();
	tex_t      *tex;
	size_t      buff_size = 0;

	for (uint32_t i = 0; i < model->meshes.count; i++) {
		uint32_t    image_ind = 2 * i;
		VkExtent3D  extent = objects[image_ind].image.extent;
		// probably 3 or 4 bytes per pixel FIXME
		buff_size = max (buff_size, extent.width * extent.height * 4);
	}

	auto stage = QFV_CreateStagingBuffer (device, "iqm stage", 4 * buff_size,
										  ctx->cmdpool);

	for (uint32_t i = 0; i < model->meshes.count; i++) {
		uint32_t    image_ind = 2 * i;
		auto image = &objects[image_ind].image;
		auto view = &objects[image_ind + 1].image_view;
		skins[i] = (qfv_skin_t) {
			.view = view->view,
			.colors = { TOP_RANGE + 7, BOTTOM_RANGE + 7, 0, 0 },
		};

		dstring_copystr (str, iqm_ctx->text + meshes[i].material);
		QFS_StripExtension (str->str, str->str);
		if (!(tex = LoadImage (va (0, "textures/%s", str->str), 1))) {
			static tex_t       null_tex = {
				.width = 2,
				.height = 2,
				.format = tex_rgba,
				.data = null_texture,
			};
			tex = &null_tex;
		}
		iqm_transfer_texture (tex, image->image, stage, device);
		Vulkan_AliasAddSkin (ctx, &skins[i]);
	}
	dstring_delete (str);
	QFV_DestroyStagingBuffer (stage);
}

static void
vulkan_iqm_load_arrays (mod_iqm_ctx_t *iqm_ctx, qfv_mesh_t *rmesh,
						vulkan_ctx_t *ctx)
{
#if 0
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	iqmctx_t   *ictx = ctx->iqm_context;

	size_t      geom_size = iqm->num_verts * sizeof (iqmgvert_t);
	size_t      rend_size = iqm->num_verts * sizeof (iqmrvert_t);
	size_t      elem_size = iqm->num_elements * sizeof (uint16_t);
	if (iqm->num_verts > 0xfff0) {
		elem_size = iqm->num_elements * sizeof (uint32_t);
	}
	size_t      buff_size = geom_size + rend_size + elem_size + 1024;
	auto stage = QFV_CreateStagingBuffer (device, "iqm stage" buff_size,
										  ctx->cmdpool);
	qfv_packet_t *gpacket = QFV_PacketAcquire (stage);
	iqmgvert_t *gverts = QFV_PacketExtend (gpacket, geom_size);
	qfv_packet_t *rpacket = QFV_PacketAcquire (stage);
	iqmrvert_t *rverts = QFV_PacketExtend (rpacket, rend_size);
	qfv_packet_t *epacket = QFV_PacketAcquire (stage);
	uint16_t   *elements = QFV_PacketExtend (epacket, elem_size);

	//FIXME this whole thing is silly, but some person went and interleaved
	memcpy (elements, iqm->elements16, elem_size);

	qfv_bufferbarrier_t bb[] = {
		bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
		bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
		bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
	};
	bb[0].barrier.buffer = mesh->geom_buffer;
	bb[0].barrier.size = geom_size;
	bb[1].barrier.buffer = mesh->rend_buffer;
	bb[1].barrier.size = rend_size;
	bb[2].barrier.buffer = mesh->index_buffer;
	bb[2].barrier.size = elem_size;
	VkBufferCopy copy_region[] = {
		{ gpacket->offset, 0, geom_size },
		{ rpacket->offset, 0, rend_size },
		{ epacket->offset, 0, elem_size },
	};

	dfunc->vkCmdPipelineBarrier (gpacket->cmd, bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[0].barrier, 0, 0);
	dfunc->vkCmdPipelineBarrier (rpacket->cmd, bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[1].barrier, 0, 0);
	dfunc->vkCmdPipelineBarrier (epacket->cmd, bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[2].barrier, 0, 0);
	dfunc->vkCmdCopyBuffer (gpacket->cmd, stage->buffer,
							mesh->geom_buffer, 1, &copy_region[0]);
	dfunc->vkCmdCopyBuffer (rpacket->cmd, stage->buffer,
							mesh->rend_buffer, 1, &copy_region[1]);
	dfunc->vkCmdCopyBuffer (epacket->cmd, stage->buffer,
							mesh->index_buffer, 1, &copy_region[2]);
	bb[0] = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	bb[1] = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	bb[2] = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	bb[0].barrier.buffer = mesh->geom_buffer;
	bb[0].barrier.size = geom_size;
	bb[1].barrier.buffer = mesh->rend_buffer;
	bb[1].barrier.size = rend_size;
	bb[2].barrier.buffer = mesh->index_buffer;
	bb[2].barrier.size = elem_size;
	dfunc->vkCmdPipelineBarrier (gpacket->cmd, bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[0].barrier, 0, 0);
	dfunc->vkCmdPipelineBarrier (rpacket->cmd, bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[1].barrier, 0, 0);
	dfunc->vkCmdPipelineBarrier (epacket->cmd, bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[2].barrier, 0, 0);
	QFV_PacketSubmit (gpacket);
	QFV_PacketSubmit (rpacket);
	QFV_PacketSubmit (epacket);
	QFV_DestroyStagingBuffer (stage);

	vec4f_t    *bone_data;
	dfunc->vkMapMemory (device->dev, mesh->bones->memory, 0, VK_WHOLE_SIZE,
						0, (void **)&bone_data);
	for (size_t i = 0; i < ictx->frames.size * iqm->num_joints; i++) {
		vec4f_t    *bone = bone_data + i * 3;
		bone[0] = (vec4f_t) {1, 0, 0, 0};
		bone[1] = (vec4f_t) {0, 1, 0, 0};
		bone[2] = (vec4f_t) {0, 0, 1, 0};
	}
	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		mesh->bones->memory, 0, VK_WHOLE_SIZE,
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);

	dfunc->vkUnmapMemory (device->dev, mesh->bones->memory);

	Vulkan_IQMAddBones (ctx, iqm);	//FIXME doesn't belong here (per-instance)
#endif
}

void
Vulkan_Mod_IQMFinish (mod_iqm_ctx_t *iqm_ctx, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	auto rctx = ctx->render_context;

	iqm_ctx->mod->clear = vulkan_iqm_clear;
	iqm_ctx->mod->data = ctx;

	auto model = iqm_ctx->qf_model;
	auto meshes = iqm_ctx->qf_meshes;

	// FIXME assumes only one texture per mesh (currently the case, but
	// when materials are added...)
	// 2 is for image + image view
	int         num_objects = 4 + 2 * model->meshes.count;
	size_t size = sizeof (qfv_mesh_t)
				+ sizeof (qfv_resource_t[2])
				+ sizeof (qfv_resobj_t[num_objects])
				+ sizeof (VkDescriptorSet[rctx->frames.size])
				+ sizeof (qfv_skin_t[model->meshes.count])
				+ sizeof (keyframedesc_t[model->meshes.count])
				+ sizeof (keyframe_t[model->meshes.count]);
	const char *name = iqm_ctx->mod->name;
	qfv_mesh_t *rmesh = Hunk_AllocName (0, size, name);
	auto bones = (qfv_resource_t *) &rmesh[1];
	auto mesh = &bones[1];

	size_t joint_size = 3 * sizeof (vec4f_t);

	bones[0] = (qfv_resource_t) {
		.name = name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		.num_objects = 1,
		.objects = (qfv_resobj_t *) &bones[2],
	};
	bones[0].objects[0] = (qfv_resobj_t) {
		.name = "bones",
		.type = qfv_res_buffer,
		.buffer = {
			.size = rctx->frames.size * model->joints.count * joint_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		},
	};

	uint32_t num_verts = iqm_ctx->hdr->num_vertexes;
	uint32_t geom_size = 20;//XXX
	uint32_t rend_size = 40;//XXX
	mesh[0] = (qfv_resource_t) {
		.name = va (ctx->va_ctx, "%s:mesh", name),
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = num_objects - 1,
		.objects = bones->objects + 1,
	};
	mesh[0].objects[0] = (qfv_resobj_t) {
		.name = "geom",
		.type = qfv_res_buffer,
		.buffer = {
			.size = num_verts * geom_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	mesh[0].objects[1] = (qfv_resobj_t) {
		.name = "rend",
		.type = qfv_res_buffer,
		.buffer = {
			.size = num_verts * rend_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};

	size_t      elem_size = sizeof (uint16_t);
	if (num_verts > 0xfff0) {
		elem_size = sizeof (uint32_t);
	} else if (num_verts < 0xff) {
		elem_size = sizeof (uint8_t);
	}
	elem_size *= iqm_ctx->hdr->num_triangles * 3;

	mesh[0].objects[2] = (qfv_resobj_t) {
		.name = "index",
		.type = qfv_res_buffer,
		.buffer = {
			.size = elem_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		},
	};

	auto bone_descriptors = (VkDescriptorSet *) &bones[0].objects[num_objects];
	auto skins = (qfv_skin_t *) &bone_descriptors[rctx->frames.size];
	auto skindescs = (keyframedesc_t *) &skins[model->meshes.count];
	auto skinframes = (keyframe_t *) &skindescs[model->meshes.count];

	for (uint32_t i = 0; i < model->meshes.count; i++) {
		int         image_ind = 3 + 2 * i;
		__auto_type image = &mesh->objects[image_ind];
		vulkan_iqm_init_image (iqm_ctx->text, &meshes[i], image);

		meshes[i].skin = (anim_t) {
			.numdesc = 1,
			.descriptors = (byte *) &skindescs[i] - (byte *) &meshes[i],
			.keyframes = (byte *) &skinframes[i] - (byte *) &meshes[i],
		};
		skindescs[i] = (keyframedesc_t) {
			.firstframe = 0,
			.numframes = 1,
		};
		skinframes[i] = (keyframe_t) {
			.data = (byte *) &skins[i] - (byte *) &meshes[i],
		};

		mesh[0].objects[image_ind + 1] = (qfv_resobj_t) {
			.name = "view",
			.type = qfv_res_image_view,
			.image_view = {
				.image = image_ind,
				.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
				.format = image->image.format,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = VK_REMAINING_MIP_LEVELS,
					.layerCount = VK_REMAINING_ARRAY_LAYERS,
				},
			},
		};
	}

	QFV_CreateResource (device, mesh);
	QFV_CreateResource (device, bones);

	vulkan_iqm_load_textures (iqm_ctx, rmesh, skins, &mesh->objects[3], ctx);
	vulkan_iqm_load_arrays (iqm_ctx, rmesh, ctx);

	auto resources = bones;
	*rmesh = (qfv_mesh_t) {
		.geom_buffer = mesh->objects[0].buffer.buffer,
		.rend_buffer = mesh->objects[1].buffer.buffer,
		.index_buffer = mesh->objects[2].buffer.buffer,
		.bones_buffer = bones->objects[0].buffer.buffer,
		.resources = (byte *) resources - (byte *) rmesh,
		.bone_descriptors = (byte *) bone_descriptors - (byte *) rmesh,
	};
	for (int i = 0; i < 2; i++) {
		auto res = &resources[i];
		res->objects = (qfv_resobj_t *)((byte *) res->objects - (byte *) res);
	}
	model->render_data = (byte *) rmesh - (byte *) model;
}
