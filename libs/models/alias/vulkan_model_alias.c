/*
	vulkan_model_alais.c

	Alias model processing for Vulkan

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/1/24

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

#include "QF/cvar.h"
#include "QF/va.h"

#include "QF/modelgen.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/staging.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

static vec3_t vertex_normals[NUMVERTEXNORMALS] = {
#include "anorms.h"
};

static void
skin_clear (int skin_offset, aliashdr_t *hdr, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_alias_skin_t *skin = (qfv_alias_skin_t *) ((byte *) hdr + skin_offset);

	Vulkan_AliasRemoveSkin (ctx, skin);
	dfunc->vkDestroyImageView (device->dev, skin->view, 0);
	dfunc->vkDestroyImage (device->dev, skin->image, 0);
	dfunc->vkFreeMemory (device->dev, skin->memory, 0);
}

static void
vulkan_alias_clear (model_t *m, void *data)
{
	vulkan_ctx_t *ctx = data;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliashdr_t *hdr;
	qfv_alias_mesh_t *mesh;

	QFV_DeviceWaitIdle (device);

	m->needload = true;	//FIXME is this right?
	if (!(hdr = m->aliashdr)) {
		hdr = Cache_Get (&m->cache);
	}
	mesh = (qfv_alias_mesh_t *) ((byte *) hdr + hdr->commands);
	dfunc->vkDestroyBuffer (device->dev, mesh->vertex_buffer, 0);
	dfunc->vkDestroyBuffer (device->dev, mesh->uv_buffer, 0);
	dfunc->vkDestroyBuffer (device->dev, mesh->index_buffer, 0);
	dfunc->vkFreeMemory (device->dev, mesh->memory, 0);

	__auto_type skins = (maliasskindesc_t *) ((byte *) hdr + hdr->skindesc);
	for (int i = 0; i < hdr->mdl.numskins; i++) {
		if (skins[i].type == ALIAS_SKIN_GROUP) {
			__auto_type group = (maliasskingroup_t *)
								((byte *) hdr + skins[i].skin);
			for (int j = 0; j < group->numskins; j++) {
				skin_clear (group->skindescs[j].skin, hdr, ctx);
			}
		} else {
			skin_clear (skins[i].skin, hdr, ctx);
		}
	}
}

#define SKIN_LAYERS 3

static void *
Vulkan_Mod_LoadSkin (mod_alias_ctx_t *alias_ctx, byte *skinpix, int skinsize,
					 int snum, int gnum, bool group,
					 maliasskindesc_t *skindesc, vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, va (ctx->va_ctx, "alias.load_skin: %s", alias_ctx->mod->name));
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliashdr_t *header = alias_ctx->header;
	qfv_alias_skin_t *skin;
	byte       *tskin;
	int         w, h;

	skin = Hunk_Alloc (0, sizeof (qfv_alias_skin_t));
	QuatSet (TOP_RANGE + 7, BOTTOM_RANGE + 7, 0, 0, skin->colors);
	skindesc->skin = (byte *) skin - (byte *) header;
	//FIXME move all skins into arrays(?)
	w = header->mdl.skinwidth;
	h = header->mdl.skinheight;
	tskin = malloc (2 * skinsize);
	memcpy (tskin, skinpix, skinsize);
	Mod_FloodFillSkin (tskin, w, h);

	int         mipLevels = QFV_MipLevels (w, h);
	VkExtent3D  extent = { w, h, 1 };
	skin->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
								   VK_FORMAT_R8G8B8A8_UNORM, extent,
								   mipLevels, 3, VK_SAMPLE_COUNT_1_BIT,
								   VK_IMAGE_USAGE_SAMPLED_BIT
								   | VK_IMAGE_USAGE_TRANSFER_DST_BIT
								   | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, skin->image,
						 va (ctx->va_ctx, "image:%s:%d:%d",
							 alias_ctx->mod->name, snum, gnum));
	skin->memory = QFV_AllocImageMemory (device, skin->image,
										 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										 0, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY, skin->memory,
						 va (ctx->va_ctx, "memory:%s:%d:%d",
							 alias_ctx->mod->name, snum, gnum));
	QFV_BindImageMemory (device, skin->image, skin->memory, 0);
	skin->view = QFV_CreateImageView (device, skin->image,
									  VK_IMAGE_VIEW_TYPE_2D_ARRAY,
									  VK_FORMAT_R8G8B8A8_UNORM,
									  VK_IMAGE_ASPECT_COLOR_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, skin->view,
						 va (ctx->va_ctx, "iview:%s:%d:%d",
							 alias_ctx->mod->name, snum, gnum));

	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (device, "alias stage",
													 SKIN_LAYERS * skinsize * 4,
													 ctx->cmdpool);
	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	byte       *base_data = QFV_PacketExtend (packet, skinsize * 4);
	byte       *glow_data = QFV_PacketExtend (packet, skinsize * 4);
	byte       *cmap_data = QFV_PacketExtend (packet, skinsize * 4);

	Mod_CalcFullbright (tskin + skinsize, tskin, skinsize);
	Vulkan_ExpandPalette (glow_data, tskin + skinsize, vid.palette, 1,
						  skinsize);
	Mod_ClearFullbright (tskin, tskin, skinsize);

	Skin_CalcTopColors    (cmap_data + 0, tskin, skinsize, 4);
	Skin_CalcTopMask      (cmap_data + 1, tskin, skinsize, 4);
	Skin_CalcBottomColors (cmap_data + 2, tskin, skinsize, 4);
	Skin_CalcBottomMask   (cmap_data + 3, tskin, skinsize, 4);
	Skin_ClearTopColors (tskin, tskin, skinsize);
	Skin_ClearBottomColors (tskin, tskin, skinsize);

	Vulkan_ExpandPalette (base_data, tskin, vid.palette, 1, skinsize);

	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.image = skin->image;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);

	VkBufferImageCopy copy = {
		packet->offset, 0, 0,
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, SKIN_LAYERS},
		{0, 0, 0}, {w, h, 1},
	};
	dfunc->vkCmdCopyBufferToImage (packet->cmd, packet->stage->buffer,
								   skin->image,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   1, &copy);

	if (mipLevels == 1) {
		ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
		ib.barrier.image = skin->image;
		ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
									 0, 0, 0, 0, 0,
									 1, &ib.barrier);
	} else {
		QFV_GenerateMipMaps (device, packet->cmd, skin->image,
							 mipLevels, w, h, SKIN_LAYERS);
	}
	QFV_PacketSubmit (packet);
	QFV_DestroyStagingBuffer (stage);

	free (tskin);

	Vulkan_AliasAddSkin (ctx, skin);

	qfvPopDebug (ctx);
	return skinpix + skinsize;
}

void
Vulkan_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx, vulkan_ctx_t *ctx)
{
	aliashdr_t *header = alias_ctx->header;
	int         skinsize = header->mdl.skinwidth * header->mdl.skinheight;

	for (size_t i = 0; i < alias_ctx->skins.size; i++) {
		__auto_type skin = alias_ctx->skins.a + i;
		Vulkan_Mod_LoadSkin (alias_ctx, skin->texels, skinsize,
							 skin->skin_num, skin->group_num,
							 skin->group_num != -1, skin->skindesc, ctx);
	}
}

void
Vulkan_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx, vulkan_ctx_t *ctx)
{
	alias_ctx->mod->clear = vulkan_alias_clear;
	alias_ctx->mod->data = ctx;
}

void
Vulkan_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx, vulkan_ctx_t *ctx)
{
}

static size_t
get_buffer_size (qfv_device_t *device, VkBuffer buffer)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	size_t      size;
	size_t      align;

	VkMemoryRequirements requirements;
	dfunc->vkGetBufferMemoryRequirements (device->dev, buffer, &requirements);
	size = requirements.size;
	align = requirements.alignment - 1;
	size = (size + align) & ~(align);
	return size;
}

void
Vulkan_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx, void *_m,
									   int _s, int extra, vulkan_ctx_t *ctx)
{
	aliashdr_t *header = alias_ctx->header;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasvrt_t *verts;
	aliasuv_t  *uv;
	trivertx_t *pv;
	int        *indexmap;
	uint32_t   *indices;
	int         numverts;
	int         numtris;
	int         i, j;
	int         pose;
	vec3_t      pos;

	if (header->mdl.ident == HEADER_MDL16)
		VectorScale (header->mdl.scale, 1/256.0, header->mdl.scale);

	numverts = header->mdl.numverts;
	numtris = header->mdl.numtris;

	// initialize indexmap to -1 (unduplicated). any other value indicates
	// both that the vertex has been duplicated and the index of the
	// duplicate vertex.
	indexmap = malloc (numverts * sizeof (int));
	memset (indexmap, -1, numverts * sizeof (int));

	// check for onseam verts, and duplicate any that are associated with
	// back-facing triangles
	for (i = 0; i < numtris; i++) {
		for (j = 0; j < 3; j++) {
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

	// we now know exactly how many vertices we need, so built the vertex
	// and index data arrays
	// The layout is:
	//    vbuf:{vertex, normal} * (numposes * numverts)
	//    uvbuf:{uv} * (numverts)
	//    ibuf:{index} * (numtris * 3)
	// numverts includes the duplicated seam vertices.
	// The vertex buffer will be bound with various offsets based on the
	// current and previous pose, uvbuff "statically" bound as uvs are not
	// animated by pose, and the same for ibuf: indices will never change for
	// the mesh
	size_t      vert_count = numverts * header->numposes;
	size_t      vert_size = vert_count * sizeof (aliasvrt_t);
	size_t      uv_size = numverts * sizeof (aliasuv_t);
	size_t      ind_size = 3 * numtris * sizeof (uint32_t);

	VkBuffer    vbuff = QFV_CreateBuffer (device, vert_size,
										  VK_BUFFER_USAGE_TRANSFER_DST_BIT
										  | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	VkBuffer    uvbuff = QFV_CreateBuffer (device, uv_size,
										   VK_BUFFER_USAGE_TRANSFER_DST_BIT
										   | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	VkBuffer    ibuff = QFV_CreateBuffer (device, ind_size,
										  VK_BUFFER_USAGE_TRANSFER_DST_BIT
										  | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER, vbuff,
						 va (ctx->va_ctx, "buffer:alias:vertex:%s",
							 alias_ctx->mod->name));
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER, uvbuff,
						 va (ctx->va_ctx, "buffer:alias:uv:%s",
							 alias_ctx->mod->name));
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER, ibuff,
						 va (ctx->va_ctx, "buffer:alias:index:%s",
							 alias_ctx->mod->name));
	size_t      voffs = 0;
	size_t      uvoffs = voffs + get_buffer_size (device, vbuff);
	size_t      ioffs = uvoffs + get_buffer_size (device, uvbuff);
	size_t      buff_size = ioffs + get_buffer_size (device, ibuff);
	VkDeviceMemory mem;
	mem = QFV_AllocBufferMemory (device, vbuff,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								 buff_size, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY, mem,
						 va (ctx->va_ctx, "memory:alias:vuvi:%s",
							 alias_ctx->mod->name));
	QFV_BindBufferMemory (device, vbuff, mem, voffs);
	QFV_BindBufferMemory (device, uvbuff, mem, uvoffs);
	QFV_BindBufferMemory (device, ibuff, mem, ioffs);

	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (device,
													 va (ctx->va_ctx,
														 "alias:%s",
														 alias_ctx->mod->name),
													 buff_size, ctx->cmdpool);
	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	verts = QFV_PacketExtend (packet, vert_size);
	uv = QFV_PacketExtend (packet, uv_size);
	indices = QFV_PacketExtend (packet, ind_size);

	// populate the uvs, duplicating and shifting any that are on the seam
	// and associated with back-facing triangles (marked by non-negative
	// indexmap entry).
	// the s coordinate is shifted right by half the skin width.
	for (i = 0; i < header->mdl.numverts; i++) {
		int         vind = indexmap[i];
		uv[i].u = (float) alias_ctx->stverts.a[i].s / header->mdl.skinwidth;
		uv[i].v = (float) alias_ctx->stverts.a[i].t / header->mdl.skinheight;
		if (vind != -1) {
			uv[vind] = uv[i];
			uv[vind].u += 0.5;
		}
	}

	// poputlate the vertex position and normal data, duplicating for
	// back-facing on-seam verts (indicated by non-negative indexmap entry)
	for (i = 0, pose = 0; i < header->numposes; i++, pose += numverts) {
		for (j = 0; j < header->mdl.numverts; j++) {
			pv = &alias_ctx->poseverts.a[i][j];
			if (extra) {
				VectorMultAdd (pv[header->mdl.numverts].v, 256, pv->v, pos);
			} else {
				VectorCopy (pv->v, pos);
			}
			VectorCompMultAdd (header->mdl.scale_origin, header->mdl.scale,
							   pos, verts[pose + j].vertex);
			verts[pose + j].vertex[3] = 1;
			VectorCopy (vertex_normals[pv->lightnormalindex],
						verts[pose + j].normal);
			verts[pose + j].normal[3] = 0;
			// duplicate on-seam vert associated with back-facing triangle
			if (indexmap[j] != -1) {
				verts[pose + indexmap[j]] = verts[pose + j];
			}
		}
	}

	// now build the indices for DrawElements
	for (i = 0; i < numtris; i++) {
		for (j = 0; j < 3; j++) {
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
	// finished with indexmap
	free (indexmap);

	header->poseverts = numverts;

	qfv_bufferbarrier_t bb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
	VkBufferMemoryBarrier wr_barriers[] = {
		bb.barrier, bb.barrier, bb.barrier,
	};
	wr_barriers[0].buffer = vbuff;
	wr_barriers[0].size = vert_size;
	wr_barriers[1].buffer = uvbuff;
	wr_barriers[1].size = uv_size;
	wr_barriers[2].buffer = ibuff;
	wr_barriers[2].size = ind_size;
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 3, wr_barriers, 0, 0);
	VkBufferCopy copy_region[] = {
		{ packet->offset, 0, vert_size },
		{ packet->offset + vert_size, 0, uv_size },
		{ packet->offset + vert_size + uv_size, 0, ind_size },
	};
	dfunc->vkCmdCopyBuffer (packet->cmd, stage->buffer,
							vbuff, 1, &copy_region[0]);
	dfunc->vkCmdCopyBuffer (packet->cmd, stage->buffer,
							uvbuff, 1, &copy_region[1]);
	dfunc->vkCmdCopyBuffer (packet->cmd, stage->buffer,
							ibuff, 1, &copy_region[2]);
	// both qfv_BB_TransferWrite_to_VertexAttrRead and
	// qfv_BB_TransferWrite_to_IndexRead have the same stage flags
	bb = bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead];
	VkBufferMemoryBarrier rd_barriers[] = {
		bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead].barrier,
		bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead].barrier,
		bufferBarriers[qfv_BB_TransferWrite_to_IndexRead].barrier,
	};
	rd_barriers[0].buffer = vbuff;
	rd_barriers[0].size = vert_size;
	rd_barriers[1].buffer = uvbuff;
	rd_barriers[1].size = uv_size;
	rd_barriers[2].buffer = ibuff;
	rd_barriers[2].size = ind_size;
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 3, rd_barriers, 0, 0);
	QFV_PacketSubmit (packet);
	QFV_DestroyStagingBuffer (stage);

	qfv_alias_mesh_t *mesh = Hunk_Alloc (0, sizeof (qfv_alias_mesh_t));
	mesh->vertex_buffer = vbuff;
	mesh->uv_buffer = uvbuff;
	mesh->index_buffer = ibuff;
	mesh->memory = mem;
	header->commands = (byte *) mesh - (byte *) header;
}
