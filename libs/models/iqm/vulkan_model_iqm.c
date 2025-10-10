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
	if (!(tex = LoadImage (va ("textures/%s", str->str), 0))) {
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

	qfv_packet_t *packet = QFV_PacketAcquire (stage, "iqm.tex");
	byte       *dst = QFV_PacketExtend (packet, layer_size * 3);

	memcpy (dst, tex->data, layer_size);
	// don't have glow/color maps yet
	memset (dst + layer_size, 0, 2 * layer_size);

	int mipLevels = QFV_MipLevels (tex->width, tex->height);
	auto sb = &imageBarriers[qfv_LT_Undefined_to_TransferDst];
	auto ib = mipLevels == 1
			? &imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly]
			: nullptr;
	qfv_extent_t extent = {
		.width = tex->width,
		.height = tex->height,
		.depth = 1,
		.layers = 1,
	};
	QFV_PacketCopyImage (packet, image, extent, 0, sb, ib);

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
	auto text = (const char *) model + model->text.offset;
	dstring_t  *str = dstring_new ();
	tex_t      *tex;
	size_t      buff_size = 0;

	for (uint32_t i = 0; i < model->meshes.count; i++) {
		uint32_t    image_ind = 2 * i;
		VkExtent3D  extent = objects[image_ind].image.extent;
		// probably 3 or 4 bytes per pixel FIXME
		buff_size = max (buff_size, extent.width * extent.height * 4);
	}

	for (uint32_t i = 0; i < model->meshes.count; i++) {
		uint32_t    image_ind = 2 * i;
		auto image = &objects[image_ind].image;
		auto view = &objects[image_ind + 1].image_view;
		skins[i] = (qfv_skin_t) {
			.view = view->view,
			.colors = { TOP_RANGE + 7, BOTTOM_RANGE + 7, 0, 0 },
		};

		dstring_copystr (str, text + meshes[i].material);
		QFS_StripExtension (str->str, str->str);
		if (!(tex = LoadImage (va ("textures/%s", str->str), 1))) {
			static tex_t       null_tex = {
				.width = 2,
				.height = 2,
				.format = tex_rgba,
				.data = null_texture,
			};
			tex = &null_tex;
		}
		iqm_transfer_texture (tex, image->image, ctx->staging, device);
		Vulkan_MeshAddSkin (ctx, &skins[i]);
	}
	dstring_delete (str);
}

static void
vulkan_iqm_load_arrays (mod_iqm_ctx_t *iqm_ctx, qfv_mesh_t *rmesh,
						qfm_attrdesc_t *attribs, uint32_t strides[3],
						uint32_t *indices, uint32_t index_bytes,
						vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	auto iqm = iqm_ctx->hdr;
	size_t vsizes[3] = { VectorExpand (strides) };
	VectorScale (vsizes, iqm->num_vertexes, vsizes);
	qfv_packet_t *vpackets[3] = {};
	byte *vdata[3] = {};
	for (int i = 0; i < 3; i++) {
		if (i == 1) {
			continue;
		}
		vpackets[i] = QFV_PacketAcquire (ctx->staging,
										 vac (ctx->va_ctx, "iqm.vdata%d", i));
		vdata[i] = QFV_PacketExtend (vpackets[i], vsizes[i]);
	}
	qfv_packet_t *ipacket = QFV_PacketAcquire (ctx->staging, "iqm.idata");
	byte *idata = QFV_PacketExtend (ipacket, index_bytes);

	auto iqm_data = (byte *) iqm;
	auto va = (iqmvertexarray *) (iqm_data + iqm->ofs_vertexarrays);
	for (uint32_t i = 0; i < iqm->num_vertexes; i++) {
		for (uint32_t j = 0; j < iqm->num_vertexarrays; j++) {
			uint32_t size = iqm_attr_size (&va[j]);
			memcpy (vdata[attribs[j].set] + attribs[j].offset,
					iqm_data + va[j].offset + i * size, size);
		}
		VectorAdd (vdata, strides, vdata);
	}
	memcpy (idata, indices, index_bytes);

	qfv_bufferbarrier_t bb[] = {
		bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
		bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
		bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
	};
	bb[0].barrier.buffer = rmesh->geom_buffer;
	bb[0].barrier.size = vsizes[0];
	bb[1].barrier.buffer = rmesh->rend_buffer;
	bb[1].barrier.size = vsizes[2];
	bb[2].barrier.buffer = rmesh->index_buffer;
	bb[2].barrier.size = index_bytes;
	VkBufferCopy copy_region[] = {
		{ vpackets[0]->offset, 0, vsizes[0] },
		{ vpackets[2]->offset, 0, vsizes[2] },
		{ ipacket->offset, 0, index_bytes },
	};

	dfunc->vkCmdPipelineBarrier (vpackets[0]->cmd,
								 bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[0].barrier, 0, 0);
	dfunc->vkCmdPipelineBarrier (vpackets[2]->cmd,
								 bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[1].barrier, 0, 0);
	dfunc->vkCmdPipelineBarrier (ipacket->cmd,
								 bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[2].barrier, 0, 0);
	dfunc->vkCmdCopyBuffer (vpackets[0]->cmd, ctx->staging->buffer,
							rmesh->geom_buffer, 1, &copy_region[0]);
	dfunc->vkCmdCopyBuffer (vpackets[0]->cmd, ctx->staging->buffer,
							rmesh->rend_buffer, 1, &copy_region[1]);
	dfunc->vkCmdCopyBuffer (ipacket->cmd, ctx->staging->buffer,
							rmesh->index_buffer, 1, &copy_region[2]);
	bb[0] = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	bb[1] = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	bb[2] = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	bb[0].barrier.buffer = rmesh->geom_buffer;
	bb[0].barrier.size = vsizes[0];
	bb[1].barrier.buffer = rmesh->rend_buffer;
	bb[1].barrier.size = vsizes[2];
	bb[2].barrier.buffer = rmesh->index_buffer;
	bb[2].barrier.size = index_bytes;
	dfunc->vkCmdPipelineBarrier (vpackets[0]->cmd,
								 bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[0].barrier, 0, 0);
	dfunc->vkCmdPipelineBarrier (vpackets[2]->cmd,
								 bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[1].barrier, 0, 0);
	dfunc->vkCmdPipelineBarrier (ipacket->cmd,
								 bb[0].srcStages, bb[0].dstStages,
								 0, 0, 0, 1, &bb[2].barrier, 0, 0);
	QFV_PacketSubmit (vpackets[0]);
	QFV_PacketSubmit (vpackets[2]);
	QFV_PacketSubmit (ipacket);
}

static void
vulkan_iqm_init_bones (mod_iqm_ctx_t *iqm_ctx, qfv_resource_t *bones,
					   vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	auto rctx = ctx->render_context;

	auto model = iqm_ctx->qf_model;
	auto iqm = iqm_ctx->hdr;

	vec4f_t    *bone_data;
	dfunc->vkMapMemory (device->dev, bones->memory, 0, VK_WHOLE_SIZE,
						0, (void **)&bone_data);
	for (size_t i = 0; i < rctx->frames.size * iqm->num_joints; i++) {
		vec4f_t    *bone = bone_data + i * 3;
		bone[0] = (vec4f_t) {1, 0, 0, 0};
		bone[1] = (vec4f_t) {0, 1, 0, 0};
		bone[2] = (vec4f_t) {0, 0, 1, 0};
	}
	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		bones->memory, 0, VK_WHOLE_SIZE,
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);

	dfunc->vkUnmapMemory (device->dev, bones->memory);

	Vulkan_MeshAddBones (ctx, model);//FIXME doesn't belong here (per-instance)
}

static void
setup_bone_resources (qfv_resource_t *bones, qfv_resobj_t *bone_obj,
					  uint32_t num_frames, vulkan_ctx_t *ctx,
					  mod_iqm_ctx_t *iqm_ctx)
{
	const char *name = iqm_ctx->mod->name;
	auto model = iqm_ctx->qf_model;

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
					  vulkan_ctx_t *ctx, mod_iqm_ctx_t *iqm_ctx)
{
	const char *name = iqm_ctx->mod->name;
	auto model = iqm_ctx->qf_model;
	auto meshes = iqm_ctx->qf_meshes;
	auto text = (const char *) model + model->text.offset;
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

	auto image_objs = &mesh_objs[3];
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		int   image_ind = 2 * i;
		auto  image = &image_objs[image_ind];
		vulkan_iqm_init_image (text, &meshes[i], image);

		image_objs[image_ind + 1] = (qfv_resobj_t) {
			.name = "view",
			.type = qfv_res_image_view,
			.image_view = {
				.image = image_ind + image_objs - mesh_objs,
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
	auto iqm = iqm_ctx->hdr;

	// FIXME assumes only one texture per mesh (currently the case, but
	// when materials are added...)
	// 2 is for image + image view
	int         num_objects = 4 + 2 * model->meshes.count;
	size_t size = sizeof (qfv_mesh_t)
				+ sizeof (qfm_attrdesc_t[iqm->num_vertexarrays])
				+ sizeof (qfv_skin_t[model->meshes.count])
				+ sizeof (clipdesc_t[model->meshes.count])
				+ sizeof (keyframe_t[model->meshes.count])
				+ sizeof (VkDescriptorSet[rctx->frames.size])
				+ sizeof (qfv_resource_t[2])
				+ sizeof (qfv_resobj_t[num_objects]);

	const char *name = iqm_ctx->mod->name;
	qfv_mesh_t *rmesh = Hunk_AllocName (0, size, name);
	auto attribs = (qfm_attrdesc_t *) &rmesh[1];
	auto skins = (qfv_skin_t *) &attribs[iqm->num_vertexarrays];
	auto skinclips = (clipdesc_t *) &skins[model->meshes.count];
	auto skinframes = (keyframe_t *) &skinclips[model->meshes.count];
	auto bone_descs = (VkDescriptorSet *) &skinframes[model->meshes.count];
	auto resources = (qfv_resource_t *) &bone_descs[rctx->frames.size];
	auto bones = &resources[0];
	auto mesh = &resources[1];
	auto resobj = (qfv_resobj_t *) &resources[2];

	uint32_t offsets[3] = {};
	for (uint32_t i = 0; i < iqm->num_vertexarrays; i++) {
		auto a = iqm_ctx->vtxarr[i];
		attribs[i] = iqm_mesh_attribute (a, 0);
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
	for (uint32_t i = 0; i < iqm->num_vertexarrays; i++) {
		attribs[i].stride = offsets[attribs[i].set];
	}

	setup_bone_resources (bones, &resobj[0], rctx->frames.size,
						  ctx, iqm_ctx);
	setup_mesh_resources (mesh, &resobj[1], num_objects - 1, offsets,
						  iqm->num_vertexes, iqm->num_triangles * 3,
						  rctx->frames.size, ctx, iqm_ctx);
	QFV_CreateResource (device, mesh);
	QFV_CreateResource (device, bones);

	model->render_data = (byte *) rmesh - (byte *) model;
	*rmesh = (qfv_mesh_t) {
		.geom_buffer = mesh->objects[0].buffer.buffer,
		.rend_buffer = mesh->objects[1].buffer.buffer,
		.index_buffer = mesh->objects[2].buffer.buffer,
		.bones_buffer = bones->objects[0].buffer.buffer,
		.bones_memory = bones->memory,
		.resources = (byte *) resources - (byte *) rmesh,
		.bone_descriptors = (byte *) bone_descs - (byte *) rmesh,
	};

	uint32_t index_type = mesh_index_type (iqm->num_vertexes);
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		meshes[i].triangle_count = iqm_ctx->meshes[i].num_triangles;
		meshes[i].index_type = index_type;
		meshes[i].indices = iqm_ctx->meshes[i].first_triangle * 3;
		meshes[i].attributes = (qfm_loc_t) {
			.offset = (byte *) attribs - (byte *) &meshes[i],
			.count = iqm->num_vertexarrays,
		};
		meshes[i].skin = (anim_t) {
			.numclips = 1,
			.clips = (byte *) &skinclips[i] - (byte *) &meshes[i],
			.keyframes = (byte *) &skinframes[i] - (byte *) &meshes[i],
		};
		skinclips[i] = (clipdesc_t) {
			.firstframe = 0,
			.numframes = 1,
		};
		skinframes[i] = (keyframe_t) {
			.data = (byte *) &skins[i] - (byte *) &meshes[i],
		};
	}

	uint32_t index_count = iqm->num_triangles * 3;
	auto indices = (uint32_t *) ((byte *) iqm + iqm->ofs_triangles);
	auto index_bytes = pack_indices (indices, indices, index_count, index_type);
	vulkan_iqm_load_arrays (iqm_ctx, rmesh, attribs, offsets,
							indices, index_bytes, ctx);
	vulkan_iqm_init_bones (iqm_ctx, bones, ctx);
	vulkan_iqm_load_textures (iqm_ctx, rmesh, skins, &mesh->objects[3], ctx);

	for (int i = 0; i < 2; i++) {
		auto res = &resources[i];
		res->objects = (qfv_resobj_t *)((byte *) res->objects - (byte *) res);
	}
}
