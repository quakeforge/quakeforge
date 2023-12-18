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
#include "QF/Vulkan/resource.h"
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
	aliashdr_t *hdr;
	qfv_alias_mesh_t *mesh;

	QFV_DeviceWaitIdle (device);

	m->needload = true;	//FIXME is this right?
	if (!(hdr = m->aliashdr)) {
		hdr = Cache_Get (&m->cache);
	}
	mesh = (qfv_alias_mesh_t *) ((byte *) hdr + hdr->commands);
	QFV_DestroyResource (device, mesh->resources);

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
build_verts (aliasvrt_t *verts, int numposes, int numverts, const mdl_t *mdl,
			 const int *indexmap, const mod_alias_ctx_t *alias_ctx)
{
	int         i, pose;
	// populate the vertex position and normal data, duplicating for
	// back-facing on-seam verts (indicated by non-negative indexmap entry)
	for (i = 0, pose = 0; i < numposes; i++, pose += numverts) {
		for (int j = 0; j < mdl->numverts; j++) {
			auto pv = &alias_ctx->poseverts.a[i][j];
			vec3_t pos;
			if (mdl->ident == HEADER_MDL16) {
				VectorMultAdd (pv[mdl->numverts].v, 256, pv->v, pos);
			} else {
				VectorCopy (pv->v, pos);
			}
			VectorCompMultAdd (mdl->scale_origin, mdl->scale, pos,
							   verts[pose + j].vertex);
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
}

static void
build_uvs (aliasuv_t *uv, const mdl_t *mdl, const int *indexmap,
		   const mod_alias_ctx_t *alias_ctx)
{
	// populate the uvs, duplicating and shifting any that are on the seam
	// and associated with back-facing triangles (marked by non-negative
	// indexmap entry).
	// the s coordinate is shifted right by half the skin width.
	for (int i = 0; i < mdl->numverts; i++) {
		int         vind = indexmap[i];
		uv[i].u = (float) alias_ctx->stverts.a[i].s / mdl->skinwidth;
		uv[i].v = (float) alias_ctx->stverts.a[i].t / mdl->skinheight;
		if (vind != -1) {
			uv[vind] = uv[i];
			uv[vind].u += 0.5;
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
Vulkan_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx, vulkan_ctx_t *ctx)
{
	auto device = ctx->device;

	alias_ctx->mod->clear = vulkan_alias_clear;
	alias_ctx->mod->data = ctx;

	auto hdr = alias_ctx->header;

	int numverts = hdr->mdl.numverts;
	int numtris = hdr->mdl.numtris;

	int indexmap[numverts];
	// initialize indexmap to -1 (unduplicated). any other value indicates
	// both that the vertex has been duplicated and the index of the
	// duplicate vertex.
	memset (indexmap, -1, sizeof (indexmap));

	numverts = separate_verts (indexmap, numverts, numtris, alias_ctx);
	hdr->poseverts = numverts;
	// we now know exactly how many vertices we need, so build the vertex
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
	size_t      vert_count = numverts * hdr->numposes;
	size_t      vert_size = vert_count * sizeof (aliasvrt_t);
	size_t      uv_size = numverts * sizeof (aliasuv_t);
	size_t      ind_size = 3 * numtris * sizeof (uint32_t);
	auto mesh = (qfv_alias_mesh_t *) ((byte *) hdr + hdr->commands);
	mesh->resources = malloc (sizeof (qfv_resource_t)
							  + sizeof (qfv_resobj_t)
							  + sizeof (qfv_resobj_t)
							  + sizeof (qfv_resobj_t));
	mesh->resources[0] = (qfv_resource_t) {
		.name = va (ctx->va_ctx, "alias:%s", alias_ctx->mod->name),
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 3,
		.objects = (qfv_resobj_t *) &mesh->resources[1],
	};
	auto vert_obj = mesh->resources->objects;
	auto uv_obj = &vert_obj[1];
	auto index_obj = &uv_obj[1];

	*vert_obj = (qfv_resobj_t) {
		.name = "vertex",
		.type = qfv_res_buffer,
		.buffer = {
			.size = vert_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
				   | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	*uv_obj = (qfv_resobj_t) {
		.name = "uv",
		.type = qfv_res_buffer,
		.buffer = {
			.size = uv_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
				   | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	*index_obj = (qfv_resobj_t) {
		.name = "index",
		.type = qfv_res_buffer,
		.buffer = {
			.size = ind_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
				   | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		},
	};
	QFV_CreateResource (device, mesh->resources);
	mesh->vertex_buffer = vert_obj->buffer.buffer;
	mesh->uv_buffer = uv_obj->buffer.buffer;
	mesh->index_buffer = index_obj->buffer.buffer;

	size_t packet_size = vert_size + uv_size + ind_size;
	auto packet = QFV_PacketAcquire (ctx->staging);
	byte *packet_start = QFV_PacketExtend (packet, packet_size);
	byte *packet_data = packet_start;

	qfv_scatter_t vert_scatter = {
		.srcOffset = packet_data - packet_start,
		.dstOffset = 0,
		.length = vert_size,
	};
	auto verts = (aliasvrt_t *) packet_data;
	packet_data += vert_scatter.length;
	build_verts (verts, hdr->numposes, numverts, &hdr->mdl,
				 indexmap, alias_ctx);

	qfv_scatter_t uv_scatter = {
		.srcOffset = packet_data - packet_start,
		.dstOffset = 0,
		.length = uv_size,
	};
	auto uv = (aliasuv_t *) packet_data;
	packet_data += uv_scatter.length;
	build_uvs (uv, &hdr->mdl, indexmap, alias_ctx);

	qfv_scatter_t ind_scatter = {
		.srcOffset = packet_data - packet_start,
		.dstOffset = 0,
		.length = ind_size,
	};
	auto indices = (uint32_t *) packet_data;
	packet_data += ind_scatter.length;
	build_inds (indices, hdr->mdl.numtris, indexmap, alias_ctx);

	QFV_PacketScatterBuffer (packet, mesh->vertex_buffer, 1, &vert_scatter,
				&bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead]);
	QFV_PacketScatterBuffer (packet, mesh->uv_buffer, 1, &uv_scatter,
				&bufferBarriers[qfv_BB_TransferWrite_to_VertexAttrRead]);
	QFV_PacketScatterBuffer (packet, mesh->index_buffer, 1, &ind_scatter,
				&bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketSubmit (packet);
}

void
Vulkan_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx, void *_m,
									   int _s, int extra, vulkan_ctx_t *ctx)
{
	aliashdr_t *header = alias_ctx->header;

	if (header->mdl.ident == HEADER_MDL16)
		VectorScale (header->mdl.scale, 1/256.0, header->mdl.scale);

	qfv_alias_mesh_t *mesh = Hunk_Alloc (0, sizeof (qfv_alias_mesh_t));
	header->commands = (byte *) mesh - (byte *) header;
}
