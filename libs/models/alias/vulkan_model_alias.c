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

#include "QF/va.h"

#include "QF/modelgen.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/staging.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

static vec3_t vertex_normals[NUMVERTEXNORMALS] = {
#include "anorms.h"
};

static void
vulkan_alias_clear (model_t *m, void *data)
{
}

void *
Vulkan_Mod_LoadSkin (byte *skin, int skinsize, int snum, int gnum,
				     qboolean group, maliasskindesc_t *skindesc,
					 vulkan_ctx_t *ctx)
{
	byte       *tskin;
	int         w, h;

	w = pheader->mdl.skinwidth;
	h = pheader->mdl.skinheight;
	tskin = malloc (skinsize);
	memcpy (tskin, skin, skinsize);
	Mod_FloodFillSkin (tskin, w, h);
	tex_t skin_tex = {w, h, tex_palette, 1, vid.palette, tskin};
	skindesc->tex = Vulkan_LoadTex (ctx, &skin_tex, 1);
	free (tskin);
	return skin + skinsize;
}

void
Vulkan_Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr, vulkan_ctx_t *ctx)
{
	if (hdr->mdl.ident == HEADER_MDL16)
		VectorScale (hdr->mdl.scale, 1/256.0, hdr->mdl.scale);
	m->clear = vulkan_alias_clear;
}

void
Vulkan_Mod_LoadExternalSkins (model_t *mod, vulkan_ctx_t *ctx)
{
}

void
Vulkan_Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *_m,
									   int _s, int extra, vulkan_ctx_t *ctx)
{
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

	numverts = hdr->mdl.numverts;
	numtris = hdr->mdl.numtris;

	// initialize indexmap to -1 (unduplicated). any other value indicates
	// both that the vertex has been duplicated and the index of the
	// duplicate vertex.
	indexmap = malloc (numverts * sizeof (int));
	memset (indexmap, -1, numverts * sizeof (int));

	// check for onseam verts, and duplicate any that are associated with
	// back-facing triangles
	for (i = 0; i < numtris; i++) {
		for (j = 0; j < 3; j++) {
			int         vind = triangles.a[i].vertindex[j];
			if (stverts.a[vind].onseam && !triangles.a[i].facesfront) {
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
	size_t      atom = device->physDev->properties.limits.nonCoherentAtomSize;
	size_t      atom_mask = atom - 1;
	size_t      vert_count = numverts * hdr->numposes;
	size_t      vert_size = vert_count * sizeof (aliasvrt_t);
	size_t      uv_size = numverts * sizeof (aliasuv_t);
	size_t      ind_size = numverts * sizeof (uint32_t);
	vert_size = (vert_size + atom_mask) & ~atom_mask;
	uv_size = (uv_size + atom_mask) & ~atom_mask;
	ind_size = (ind_size + atom_mask) & ~atom_mask;
	size_t      buff_size = vert_size + ind_size + uv_size;

	VkBuffer    vbuff = QFV_CreateBuffer (device, buff_size,
										  VK_BUFFER_USAGE_TRANSFER_DST_BIT
										  | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	VkBuffer    uvbuff = QFV_CreateBuffer (device, buff_size,
										   VK_BUFFER_USAGE_TRANSFER_DST_BIT
										   | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	VkBuffer    ibuff = QFV_CreateBuffer (device, buff_size,
										  VK_BUFFER_USAGE_TRANSFER_DST_BIT
										  | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	VkDeviceMemory mem;
	mem = QFV_AllocBufferMemory (device, vbuff,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								 buff_size, 0);
	QFV_BindBufferMemory (device, vbuff, mem, 0);
	QFV_BindBufferMemory (device, uvbuff, mem, vert_size);
	QFV_BindBufferMemory (device, ibuff, mem, vert_size + uv_size);

	qfv_stagebuf_t *stage = QFV_CreateStagingBuffer (device, buff_size,
													  ctx->cmdpool);
	qfv_packet_t *packet = QFV_PacketAcquire (stage);
	verts = QFV_PacketExtend (packet, vert_size);
	uv = QFV_PacketExtend (packet, uv_size);
	indices = QFV_PacketExtend (packet, ind_size);

	// populate the uvs, duplicating and shifting any that are on the seam
	// and associated with back-facing triangles (marked by non-negative
	// indexmap entry).
	// the s coordinate is shifted right by half the skin width.
	for (i = 0; i < hdr->mdl.numverts; i++) {
		int         vind = indexmap[i];
		uv[i].u = (float) stverts.a[i].s / hdr->mdl.skinwidth;
		uv[i].v = (float) stverts.a[i].t / hdr->mdl.skinheight;
		if (vind != -1) {
			uv[vind] = uv[i];
			uv[vind].u += 0.5;
		}
	}

	// poputlate the vertex position and normal data, duplicating for
	// back-facing on-seam verts (indicated by non-negative indexmap entry)
	for (i = 0, pose = 0; i < hdr->numposes; i++, pose += numverts) {
		for (j = 0; j < hdr->mdl.numverts; j++) {
			pv = &poseverts.a[i][j];
			if (extra) {
				VectorMultAdd (pv[hdr->mdl.numverts].v, 256, pv->v, pos);
			} else {
				VectorCopy (pv->v, pos);
			}
			VectorCompMultAdd (hdr->mdl.scale_origin, hdr->mdl.scale,
							   pos, verts[pose + j].vertex);
			VectorCopy (vertex_normals[pv->lightnormalindex],
						verts[pose + j].normal);
			// duplicate on-seam vert associated with back-facing triangle
			if (indexmap[j] != -1) {
				verts[pose + indexmap[j]] = verts[pose + j];
			}
		}
	}

	// now build the indices for DrawElements
	for (i = 0; i < numverts; i++) {
		indexmap[i] = indexmap[i] != -1 ? indexmap[i] : i;
	}
	for (i = 0; i < numtris; i++) {
		for (j = 0; j < 3; j++) {
			indices[3 * i + j] = indexmap[triangles.a[i].vertindex[j]];
		}
	}
	// finished with indexmap
	free (indexmap);

	hdr->poseverts = numverts;

	VkBufferMemoryBarrier wr_barriers[] = {
		{	VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			vbuff, 0, vert_size},
		{	VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			uvbuff, 0, uv_size},
		{	VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			ibuff, 0, ind_size},
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
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
	VkBufferMemoryBarrier rd_barriers[] = {
		{	VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			vbuff, 0, vert_size },
		{	VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			uvbuff, 0, uv_size },
		{	VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDEX_READ_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			ibuff, 0, ind_size },
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
								 0, 0, 0, 1, rd_barriers, 0, 0);
	QFV_PacketSubmit (packet);
	QFV_DestroyStagingBuffer (stage);

	qfv_alias_mesh_t *mesh = Hunk_Alloc (sizeof (qfv_alias_mesh_t));
	mesh->vertex_buffer = vbuff;
	mesh->uv_buffer = uvbuff;
	mesh->index_buffer = ibuff;
	mesh->memory = mem;
	hdr->commands = (byte *) mesh - (byte *) hdr;
}
