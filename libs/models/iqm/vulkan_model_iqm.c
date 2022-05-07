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
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/qf_iqm.h"
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
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	qfv_iqm_t  *mesh = iqm->extra_data;

	mod->needload = true;

	for (int i = 0; i < iqm->num_meshes; i++) {
		Vulkan_IQMRemoveSkin (ctx, &mesh->skins[i]);
	}
	Vulkan_IQMRemoveBones (ctx, iqm);//FIXME doesn't belong here (per-instance)

	QFV_DestroyResource (device, mesh->bones);
	QFV_DestroyResource (device, mesh->mesh);
	free (mesh);
	Mod_FreeIQM (iqm);
}

static void
vulkan_iqm_init_image (iqm_t *iqm, int meshnum, qfv_resobj_t *image)
{
	const char *material = iqm->text + iqm->meshes[meshnum].material;
	dstring_t  *str = dstring_new ();
	dstring_copystr (str, material);
	QFS_StripExtension (str->str, str->str);

	tex_t      *tex;
	if ((tex = LoadImage (va (0, "textures/%s", str->str), 0))) {
		*image = (qfv_resobj_t) {
			.name = material,
			.type = qfv_res_image,
			.image = {
				.type = VK_IMAGE_TYPE_2D,
				.format = QFV_ImageFormat (tex->format),
				.extent = {
					.width = tex->width,
					.height = tex->height,
					.depth = 1,
				},
				.num_mipmaps = QFV_MipLevels (tex->width, tex->height),
				.num_layers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
						| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
						| VK_IMAGE_USAGE_SAMPLED_BIT,
			},
		};
	} else {
		*image = (qfv_resobj_t) {
			.name = material,
			.type = qfv_res_image,
			.image = {
				.type = VK_IMAGE_TYPE_2D,
				.format = QFV_ImageFormat (tex_rgba),
				.extent = {
					.width = 2,
					.height = 2,
					.depth = 1,
				},
				.num_mipmaps = QFV_MipLevels (2, 2),
				.num_layers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
						| VK_IMAGE_USAGE_SAMPLED_BIT,
			},
		};
	}
	dstring_delete (str);
}

static void
iqm_transfer_texture (tex_t *tex, VkImage image, qfv_stagebuf_t *stage,
					  qfv_device_t *device)
{
	qfv_devfuncs_t *dfunc = device->funcs;

	if (tex->format != tex_rgb && tex->format != tex_rgba) {
		Sys_Error ("can't transfer iqm image");
	}
	// FIXME correct only for rgb and rgba
	size_t      layer_size = tex->width * tex->height * tex->format;

	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	byte       *dst = QFV_PacketExtend (packet, layer_size);

	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.image = image;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);
	memcpy (dst, tex->data, layer_size);
	VkBufferImageCopy copy = {
		packet->offset, 0, 0,
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{0, 0, 0}, {tex->width, tex->height, 1},
	};
	dfunc->vkCmdCopyBufferToImage (packet->cmd, packet->stage->buffer,
								   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   1, &copy);

	int         mipLevels = QFV_MipLevels (tex->width, tex->height);
	if (mipLevels == 1) {
		ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
		ib.barrier.image = image;
		ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
									 0, 0, 0, 0, 0,
									 1, &ib.barrier);
	} else {
		QFV_GenerateMipMaps (device, packet->cmd, image, mipLevels,
							 tex->width, tex->height, 1);
	}
	QFV_PacketSubmit (packet);
}

static void
vulkan_iqm_load_textures (model_t *mod, iqm_t *iqm, qfv_iqm_t *mesh,
						  vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	dstring_t  *str = dstring_new ();
	tex_t      *tex;
	size_t      buff_size = 0;
	qfv_resobj_t *objects = mesh->mesh->objects;

	for (int i = 0; i < iqm->num_meshes; i++) {
		int         image_ind = 3 + 2 * i;
		VkExtent3D  extent = objects[image_ind].image.extent;
		// probably 3 or 4 bytes per pixel FIXME
		buff_size = max (buff_size, extent.width * extent.height * 4);
	}

	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (device,
													 va (ctx->va_ctx, "iqm:%s",
														 mod->name),
													 4 * buff_size,
													 ctx->cmdpool);

	for (int i = 0; i < iqm->num_meshes; i++) {
		int         image_ind = 3 + 2 * i;
		__auto_type image = &objects[image_ind].image;
		__auto_type view = &objects[image_ind + 1].image_view;
		qfv_iqm_skin_t *skin = &mesh->skins[i];
		*skin = (qfv_iqm_skin_t) {
			.view = view->view,
			.colora = { 255, 255, 255, 255 },
			.colorb = { 255, 255, 255, 255 },
		};

		dstring_copystr (str, iqm->text + iqm->meshes[i].material);
		QFS_StripExtension (str->str, str->str);
		if (!(tex = LoadImage (va (0, "textures/%s", str->str), 1))) {
			tex_t       null_tex = {
				.width = 2,
				.height = 2,
				.format = tex_rgba,
				.data = null_texture,
			};
			tex = &null_tex;
		}
		iqm_transfer_texture (tex, image->image, stage, device);
		Vulkan_IQMAddSkin (ctx, skin);
	}
	dstring_delete (str);
	QFV_DestroyStagingBuffer (stage);
}

static void
vulkan_iqm_load_arrays (model_t *mod, iqm_t *iqm, qfv_iqm_t *mesh,
						vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	iqmctx_t   *ictx = ctx->iqm_context;

	size_t      geom_size = iqm->num_verts * sizeof (iqmgvert_t);
	size_t      rend_size = iqm->num_verts * sizeof (iqmrvert_t);
	size_t      elem_size = iqm->num_elements * sizeof (uint16_t);
	size_t      buff_size = geom_size + rend_size + elem_size + 1024;
	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (device,
													 va (ctx->va_ctx, "iqm:%s",
														 mod->name),
													 buff_size, ctx->cmdpool);
	qfv_packet_t *gpacket = QFV_PacketAcquire (stage);
	iqmgvert_t *gverts = QFV_PacketExtend (gpacket, geom_size);
	qfv_packet_t *rpacket = QFV_PacketAcquire (stage);
	iqmrvert_t *rverts = QFV_PacketExtend (rpacket, rend_size);
	qfv_packet_t *epacket = QFV_PacketAcquire (stage);
	uint16_t   *elements = QFV_PacketExtend (epacket, elem_size);
	//FIXME this whole thing is silly, but some person went and interleaved
	//all the vertex data prematurely
	for (int i = 0; i < iqm->num_verts; i++) {
		byte       *data = iqm->vertices + i * iqm->stride;
		iqmgvert_t *gv = gverts + i;
		iqmrvert_t *rv = rverts + i;
		for (int j = 0; j < iqm->num_arrays; j++) {
			__auto_type va = &iqm->vertexarrays[j];
			// FIXME assumes standard iqm sizes
			size_t      size = 0;
			switch (va->type) {
				case IQM_POSITION:
					size = sizeof (gv->vertex);
					memcpy (gv->vertex, data, size);
					break;
				case IQM_TEXCOORD:
					size = sizeof (rv->uv);
					memcpy (rv->uv, data, size);
					break;
				case IQM_NORMAL:
					size = sizeof (rv->normal);
					memcpy (rv->normal, data, size);
					break;
				case IQM_TANGENT:
					size = sizeof (rv->tangent);
					memcpy (rv->tangent, data, size);
					break;
				case IQM_BLENDINDEXES:
					size = sizeof (gv->bones);
					memcpy (gv->bones, data, size);
					break;
				case IQM_BLENDWEIGHTS:
					size = sizeof (gv->weights);
					memcpy (gv->weights, data, size);
					break;
				case IQM_COLOR:
					size = sizeof (rv->color);
					memcpy (rv->color, data, size);
					break;
				case IQM_CUSTOM:
					// FIXME model loader doesn't handle these, so nothing to do
					break;
			}
			data += size;
		}
	}
	memcpy (elements, iqm->elements, elem_size);

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
}

void
Vulkan_Mod_IQMFinish (model_t *mod, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	iqmctx_t   *ictx = ctx->iqm_context;
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	mod->clear = vulkan_iqm_clear;
	mod->data = ctx;

	// FIXME assumes only one texture per mesh (currently the case, but
	// when materials are added...)
	// 2 is for image + image view
	int         num_objects = 4 + 2 * iqm->num_meshes;
	qfv_iqm_t  *mesh = calloc (1, sizeof (qfv_iqm_t)
							   + ictx->frames.size * sizeof (VkDescriptorSet)
							   + 2 * sizeof (qfv_resource_t)
							   + num_objects * sizeof (qfv_resobj_t)
							   + iqm->num_meshes * sizeof (qfv_iqm_skin_t));
	mesh->bones_descriptors = (VkDescriptorSet *) &mesh[1];
	mesh->bones = (qfv_resource_t *)&mesh->bones_descriptors[ictx->frames.size];
	mesh->mesh = &mesh->bones[1];

	mesh->bones[0] = (qfv_resource_t) {
		.name = mod->name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		.num_objects = 1,
		.objects = (qfv_resobj_t *) &mesh->bones[2],
	};
	mesh->bones->objects[0] = (qfv_resobj_t) {
		.name = "bones",
		.type = qfv_res_buffer,
		.buffer = {
			.size = ictx->frames.size * iqm->num_joints * 3 * sizeof (vec4f_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		},
	};

	mesh->mesh[0] = (qfv_resource_t) {
		.name = "mesh",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = num_objects - 1,
		.objects = mesh->bones->objects + 1,
	};
	mesh->mesh->objects[0] = (qfv_resobj_t) {
		.name = "geom",
		.type = qfv_res_buffer,
		.buffer = {
			.size = iqm->num_verts * sizeof (iqmgvert_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	mesh->mesh->objects[1] = (qfv_resobj_t) {
		.name = "rend",
		.type = qfv_res_buffer,
		.buffer = {
			.size = iqm->num_verts * sizeof (iqmrvert_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	mesh->mesh->objects[2] = (qfv_resobj_t) {
		.name = "index",
		.type = qfv_res_buffer,
		.buffer = {
			.size = iqm->num_elements * sizeof (uint16_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		},
	};

	for (int i = 0; i < iqm->num_meshes; i++) {
		int         image_ind = 3 + 2 * i;
		__auto_type image = &mesh->mesh->objects[image_ind];
		vulkan_iqm_init_image (iqm, i, image);

		mesh->mesh->objects[image_ind + 1] = (qfv_resobj_t) {
			.name = "view",
			.type = qfv_res_image_view,
			.image_view = {
				.image = image_ind,
				.type = VK_IMAGE_VIEW_TYPE_2D,
				.format = mesh->mesh->objects[image_ind].image.format,
				.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
			},
		};
	}

	mesh->skins = (qfv_iqm_skin_t *) &mesh->bones->objects[num_objects];

	QFV_CreateResource (device, mesh->mesh);
	QFV_CreateResource (device, mesh->bones);
	mesh->geom_buffer = mesh->mesh->objects[0].buffer.buffer;
	mesh->rend_buffer = mesh->mesh->objects[1].buffer.buffer;
	mesh->index_buffer = mesh->mesh->objects[2].buffer.buffer;
	mesh->bones_buffer = mesh->bones->objects[0].buffer.buffer;

	iqm->extra_data = mesh;

	vulkan_iqm_load_textures (mod, iqm, mesh, ctx);
	vulkan_iqm_load_arrays (mod, iqm, mesh, ctx);
}
