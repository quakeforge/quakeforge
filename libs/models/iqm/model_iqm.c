/*
	model_iqm.c

	iqm model processing

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/04/27

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

#include "QF/crc.h"
#include "QF/hash.h"
#include "QF/iqm.h"
#include "QF/mathlib.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "compat.h"
#include "mod_internal.h"

static qfm_attr_t attrib_map[] = {
	[IQM_POSITION]      = qfm_position,
	[IQM_TEXCOORD]      = qfm_texcoord,
	[IQM_NORMAL]        = qfm_normal,
	[IQM_TANGENT]       = qfm_tangent,
	[IQM_BLENDINDEXES]  = qfm_joints,
	[IQM_BLENDWEIGHTS]  = qfm_weights,
	[IQM_COLOR]         = qfm_color,
	[IQM_CUSTOM]        = 0,
};

static bool attrib_norm[] = {
	[IQM_POSITION]      = true,
	[IQM_TEXCOORD]      = true,
	[IQM_NORMAL]        = true,
	[IQM_TANGENT]       = true,
	[IQM_BLENDINDEXES]  = false,
	[IQM_BLENDWEIGHTS]  = true,
	[IQM_COLOR]         = true,
	[IQM_CUSTOM]        = false,
};

static qfm_type_t type_map[] = {
	[IQM_BYTE]   = qfm_s8,
	[IQM_UBYTE]  = qfm_u8,
	[IQM_SHORT]  = qfm_s16,
	[IQM_USHORT] = qfm_u16,
	[IQM_INT]    = qfm_s32,
	[IQM_UINT]   = qfm_u32,
	[IQM_HALF]   = qfm_f16,
	[IQM_FLOAT]  = qfm_f32,
	[IQM_DOUBLE] = qfm_f64,
};

static qfm_type_t norm_map[] = {
	[IQM_BYTE]   = qfm_s8n,
	[IQM_UBYTE]  = qfm_u8n,
	[IQM_SHORT]  = qfm_s16n,
	[IQM_USHORT] = qfm_u16n,
	[IQM_INT]    = qfm_s32,
	[IQM_UINT]   = qfm_u32,
	[IQM_HALF]   = qfm_f16,
	[IQM_FLOAT]  = qfm_f32,
	[IQM_DOUBLE] = qfm_f64,
};

static uint32_t type_size[] = {
	[IQM_BYTE]   = 1,
	[IQM_UBYTE]  = 1,
	[IQM_SHORT]  = 2,
	[IQM_USHORT] = 2,
	[IQM_INT]    = 4,
	[IQM_UINT]   = 4,
	[IQM_HALF]   = 3,
	[IQM_FLOAT]  = 4,
	[IQM_DOUBLE] = 8,
};

uint32_t
iqm_attr_size (const iqmvertexarray *a)
{
	return a->size * type_size[a->format];
}

qfm_attrdesc_t
iqm_mesh_attribute (iqmvertexarray a, uint32_t offset)
{
	return (qfm_attrdesc_t) {
		.offset     = offset,
		.stride     = a.size * type_size[a.format],
		.attr       = attrib_map[a.type],
		.type       = attrib_norm[a.type] ? norm_map[a.format]
										  : type_map[a.format],
		.components = a.size,
	};
}

static void
swap_shorts (void *data, size_t count)
{
	// count is in bytes
	count /= sizeof (uint16_t);
	for (uint16_t *vals = data; count-- > 0; vals++) {
		*vals = LittleShort (*vals);
	}
}

static void
swap_longs (void *data, size_t count)
{
	// count is in bytes
	count /= sizeof (uint32_t);
	for (uint16_t *vals = data; count-- > 0; vals++) {
		*vals = LittleShort (*vals);
	}
}

static uint32_t *
build_bone_map (iqmjoint *joints, uint32_t num_joints)
{
	hierarchy_t tmp = {};
	Hierarchy_Reserve (&tmp, num_joints + 1);
	// IQM models seem to be in depth-first order, but
	// breadth-first works better for updates, so swizzle
	// the hierarchy
	Hierarchy_SetTreeMode (&tmp, true);
	{
		// insert a dummy root that all loose roots
		// can temporarily attach to
		uint32_t ind = Hierarchy_Insert (&tmp, -1);
		tmp.ent[ind] = -1;
		tmp.own[ind] = false;
	}
	for (uint32_t i = 0; i < num_joints; i++) {
		auto j = &joints[i];
		// IQM models can have multiple root nodes (parent == -1)
		uint32_t ind = Hierarchy_Insert (&tmp, j->parent + 1);
		if (ind != i + 1) {
			Sys_Error ("%d != %d + 1", ind, i);
		}
		tmp.ent[ind] = i;
		tmp.own[ind] = false;
	}
	Hierarchy_SetTreeMode (&tmp, false);
	// Reuse nextIndex's array for the mapping from IQM joint
	// index to breadth-first joint index
	tmp.nextIndex[0] = -1;
	for (uint32_t i = 0; i < num_joints; i++) {
		tmp.nextIndex[tmp.ent[i + 1] + 1] = i;
	}
	auto bone_map = tmp.nextIndex;
	// keep our block safe
	tmp.nextIndex = nullptr;
	Hierarchy_Destroy (&tmp);

	return bone_map;
}

static void
convert_joints (uint32_t num_joints, qfm_joint_t *joints, qfm_motor_t *inverse,
				const uint32_t *bone_map, const iqmjoint *iqm_joints,
				uint32_t text_base)
{
	for (uint32_t i = 0; i < num_joints; i++) {
		auto j = &iqm_joints[i];
		joints[bone_map[i + 1]] = (qfm_joint_t) {
			.translate = { VectorExpand (j->translate) },
			.name = j->name + text_base,
			.rotate = { QuatExpand (j->rotate) },
			.scale = { VectorExpand (j->scale) },
			.parent = bone_map[j->parent + 1],
		};
	}
	for (uint32_t i = 0; i < num_joints; i++) {
		inverse[i] = qfm_motor_invert (qfm_make_motor (joints[i]));
		if (joints[i].parent >= 0) {
			inverse[i] = qfm_motor_mul (inverse[i], inverse[joints[i].parent]);
			inverse[joints[i].parent].flags |= qfm_nonleaf;
		}
	}
}

static void
update_vertex_joints (mod_iqm_ctx_t *iqm_ctx, uint32_t *bone_map)
{
	qfm_attrdesc_t blend_indices = {};

	for (uint32_t i = 0; i < iqm_ctx->hdr->num_vertexarrays; i++) {
		auto va = &iqm_ctx->vtxarr[i];
		if (va->type == IQM_BLENDINDEXES) {
			blend_indices = iqm_mesh_attribute (*va, va->offset);
		}
	}
	if (!blend_indices.offset) {
		return;
	}
	auto data = (byte *) iqm_ctx->hdr + blend_indices.offset;
	for (uint32_t i = 0; i < iqm_ctx->hdr->num_vertexes; i++) {
		//FIXME assumes bytes or ushorts
		if (blend_indices.type == qfm_u16) {
			auto indices = (uint16_t *) (data + i * blend_indices.stride);
			for (int j = 0; j < blend_indices.components; j++) {
				if (indices[j] >= iqm_ctx->hdr->num_joints) {
					indices[j] = 0;
				}
				indices[j] = bone_map[indices[j] + 1];
			}
		} else {
			auto indices = data + i * blend_indices.stride;
			for (int j = 0; j < blend_indices.components; j++) {
				if (indices[j] >= iqm_ctx->hdr->num_joints) {
					indices[j] = 0;
				}
				indices[j] = bone_map[indices[j] + 1];
			}
		}
	}
}

void
Mod_LoadIQM (model_t *mod, void *buffer)
{
	byte       *buf = buffer;
	iqmheader  *hdr = buffer;

	if (memcmp (hdr->magic, IQM_MAGIC, sizeof (IQM_MAGIC))) {
		Sys_Error ("%s: not an IQM", mod->path);
	}
	uint16_t    crc;
	CRC_Init (&crc);
	CRC_ProcessBlock (buffer, &crc, qfs_filesize);

	swap_longs (&hdr->version,
				sizeof (iqmheader) - offsetof (iqmheader, version));

	if (hdr->version != IQM_VERSION) {
		Sys_Error ("%s: unable to handle iqm version %d", mod->path,
				   hdr->version);
	}
	if (hdr->filesize != (uint32_t) qfs_filesize) {
		Sys_Error ("%s: invalid filesize", mod->path);
	}

	mod_iqm_ctx_t iqm = {
		.mod       = mod,
		.hdr       = hdr,
		.text      = (const char *)     (buf + hdr->ofs_text),
		.meshes    = (iqmmesh *)        (buf + hdr->ofs_meshes),
		.vtxarr    = (iqmvertexarray *) (buf + hdr->ofs_vertexarrays),
		.triangles = (iqmtriangle *)    (buf + hdr->ofs_triangles),
		.adjacency = (iqmtriangle *)    (buf + hdr->ofs_adjacency),
		.joints    = (iqmjoint *)       (buf + hdr->ofs_joints),
		.poses     = (iqmpose *)        (buf + hdr->ofs_poses),
		.anims     = (iqmanim *)        (buf + hdr->ofs_anims),
		.frames    = (uint16_t *)       (buf + hdr->ofs_frames),
		.bounds    = (iqmbounds *)      (buf + hdr->ofs_bounds),
		.comment   = (const char *)     (buf + hdr->ofs_comment),
	};

	size_t frame_data_count = hdr->num_frames * hdr->num_framechannels;
	swap_longs (iqm.meshes, hdr->num_meshes * sizeof (iqmmesh));
	swap_longs (iqm.vtxarr, hdr->num_vertexarrays * sizeof (iqmvertexarray));
	swap_longs (iqm.triangles, hdr->num_triangles * sizeof (iqmtriangle));
	swap_longs (iqm.adjacency, hdr->num_triangles * sizeof (iqmtriangle));
	swap_longs (iqm.joints, hdr->num_joints * sizeof (iqmjoint));
	swap_longs (iqm.poses, hdr->num_poses * sizeof (iqmpose));
	swap_longs (iqm.anims, hdr->num_anims * sizeof (iqmanim));
	swap_shorts (iqm.frames, frame_data_count * sizeof (uint16_t));
	swap_longs (iqm.bounds, hdr->num_frames * sizeof (iqmbounds));

	auto bone_map = build_bone_map (iqm.joints, hdr->num_joints);

	uint32_t text_base = RUP (hdr->num_comment + 1, sizeof (uint32_t));
	size_t size = sizeof (qf_model_t)
				+ sizeof (qf_mesh_t[hdr->num_meshes])
				+ sizeof (qfm_joint_t[hdr->num_joints])
				+ sizeof (qfm_motor_t[hdr->num_joints])
				+ sizeof (qfm_joint_t[hdr->num_poses])
				+ sizeof (clipdesc_t[hdr->num_anims])
				+ sizeof (keyframe_t[hdr->num_frames])
				+ sizeof (qfm_channel_t[hdr->num_framechannels])
				+ sizeof (uint16_t[frame_data_count])
				+ sizeof (qfm_frame_t[hdr->num_frames])
				+ text_base + hdr->num_text;
	qf_model_t *model = Hunk_AllocName (nullptr, size, mod->name);
	auto meshes    = (qf_mesh_t *)      &model[1];
	auto joints    = (qfm_joint_t *)    &meshes[hdr->num_meshes];
	auto inverse   = (qfm_motor_t *)    &joints[hdr->num_joints];
	auto pose      = (qfm_joint_t *)    &inverse[hdr->num_joints];
	auto clips     = (clipdesc_t *)     &pose[hdr->num_poses];
	auto keyframes = (keyframe_t *)     &clips[hdr->num_anims];
	auto channels  = (qfm_channel_t *)  &keyframes[hdr->num_frames];
	auto framedata = (uint16_t *)       &channels[hdr->num_framechannels];
	auto bounds    = (qfm_frame_t *)    &framedata[frame_data_count];
	auto text      = (char *)           &bounds[hdr->num_frames];

	iqm.qf_model = model;
	iqm.qf_meshes = meshes;

	memcpy (text, iqm.comment, hdr->num_comment);
	memcpy (text + text_base, iqm.text, hdr->num_text);
	memcpy (framedata, iqm.frames, sizeof (uint16_t[frame_data_count]));

	*model = (qf_model_t) {
		.meshes = {
			.offset = (byte *) meshes - (byte *) model,
			.count = hdr->num_meshes,
		},
		.joints = {
			.offset = (byte *) joints - (byte *) model,
			.count = hdr->num_joints,
		},
		.inverse = {
			.offset = (byte *) inverse - (byte *) model,
			.count = hdr->num_joints,
		},
		.pose = {
			.offset = (byte *) pose - (byte *) model,
			.count = hdr->num_poses,
		},
		.channels = {
			.offset = (byte *) channels - (byte *) model,
			.count = hdr->num_framechannels,
		},
		.text = {
			.offset = (byte *) text - (byte *) model,
			.count = text_base + hdr->num_text,
		},
		.anim = {
			.numclips = hdr->num_anims,
			.clips = (byte *) clips - (byte *) model,
			.keyframes = (byte *) keyframes - (byte *) model,
			.data = (byte *) bounds - (byte *) model,
		},
		.crc = crc,
	};
	for (uint32_t i = 0; i < hdr->num_meshes; i++) {
		auto m = &iqm.meshes[i];
		meshes[i] = (qf_mesh_t) {
			.name = m->name + text_base,
			.triangle_count = m->num_triangles,
			.vertices = (qfm_loc_t) {
				.offset = iqm.meshes[i].first_vertex,
				.count = iqm.meshes[i].num_vertexes,
			},
			.material = m->material + text_base,
			.scale = { 1, 1, 1 },
			.scale_origin = { 0, 0, 0 },
			.bounds_min = { INFINITY, INFINITY, INFINITY },
			.bounds_max = {-INFINITY,-INFINITY,-INFINITY },
		};
		auto verts = (float *) (buf + iqm.vtxarr[0].offset);
		verts += iqm.meshes[i].first_vertex * 3;
		for (uint32_t j = 0; j < iqm.meshes[i].num_vertexes; j++) {
			VectorCompMin (meshes[i].bounds_min, verts, meshes[i].bounds_min);
			VectorCompMax (meshes[i].bounds_max, verts, meshes[i].bounds_max);
			verts += 3;
		}
	}

	convert_joints (hdr->num_joints, joints, inverse, bone_map,
					iqm.joints, text_base);
	update_vertex_joints (&iqm, bone_map);

	for (uint32_t i = 0, j = 0; i < hdr->num_poses; i++) {
		uint32_t id = bone_map[i + 1];
		auto jnt = &joints[id];
		auto p = &iqm.poses[i];
		pose[id] = *jnt;
		for (int k = 0; k < 3; k++) {
			pose[id].translate[k] = p->channeloffset[k + 0];
			if (p->mask & (1 << (k + 0))) {
				auto translate = (float *) &pose[id].translate;
				channels[j++] = (qfm_channel_t) {
					.data = (byte *) (translate + k) - (byte *) pose,
					.base = p->channeloffset[k + 0],
					.scale = p->channelscale[k + 0],
				};
			}
		}
		for (int k = 0; k < 4; k++) {
			pose[id].rotate[k] = p->channeloffset[k + 3];
			if (p->mask & (1 << (k + 3))) {
				auto rotate = (float *) &pose[id].rotate;
				channels[j++] = (qfm_channel_t) {
					.data = (byte *) (rotate + k) - (byte *) pose,
					.base = p->channeloffset[k + 3],
					.scale = p->channelscale[k + 3],
				};
			}
		}
		for (int k = 0; k < 3; k++) {
			pose[id].scale[k] = p->channeloffset[k + 7];
			if (p->mask & (1 << (k + 7))) {
				auto scale = (float *) &pose[id].scale;
				channels[j++] = (qfm_channel_t) {
					.data = (byte *) (scale + k) - (byte *) pose,
					.base = p->channeloffset[k + 7],
					.scale = p->channelscale[k + 7],
				};
			}
		}
	}

	for (uint32_t i = 0; i < hdr->num_anims; i++) {
		auto a = &iqm.anims[i];
		clips[i] = (clipdesc_t) {
			.firstframe = a->first_frame,
			.numframes = a->num_frames,
			.flags = a->flags,
			.name = a->name + text_base,
		};
		for (uint32_t j = 0; j < a->num_frames; j++) {
			uint32_t abs_frame_num = a->first_frame + j;
			auto data = &framedata[abs_frame_num * hdr->num_framechannels];
			keyframes[abs_frame_num] = (keyframe_t) {
				.endtime = (j + 1) / a->framerate,
				.data = (byte *) data - (byte *) model,
			};
		}
	}
	for (uint32_t i = 0; i < hdr->num_frames; i++) {
		auto b = &iqm.bounds[i];
		bounds[i] = (qfm_frame_t) {
			.bounds_min = { VectorExpand (b->bbmin) },
			.bounds_max = { VectorExpand (b->bbmax) },
		};
	}
	double area = 0;
	auto verts = (float *) (buf + iqm.vtxarr[0].offset);
	for (uint32_t i = 0; i < hdr->num_triangles; i++) {
		auto a = verts + 3 * iqm.triangles[i].vertex[0];
		auto b = verts + 3 * iqm.triangles[i].vertex[1];
		auto c = verts + 3 * iqm.triangles[i].vertex[2];
		vec3_t ab, ac, n;
		VectorSubtract (b, a, ab);
		VectorSubtract (c, a, ac);
		CrossProduct (ab, ac, n);
		area += sqrt (DotProduct (n, n));
	}
	iqm.average_area = area / hdr->num_triangles;

	mod->type = mod_mesh;

	m_funcs->Mod_IQMFinish (&iqm);
	free (bone_map);

	memset (&mod->cache, 0, sizeof (mod->cache));
	mod->model = model;
}

static void
swap_bones (byte *bi, byte *bw, int b1, int b2)
{
	byte        t;

	t = bi[b1];
	bi[b1] = bi[b2];
	bi[b2] = t;

	t = bw[b1];
	bw[b1] = bw[b2];
	bw[b2] = t;
}

static uintptr_t
blend_get_hash (const void *e, void *unused)
{
	return CRC_Block ((byte *) e, sizeof (qfm_blend_t));
}

static int
blend_compare (const void *e1, const void *e2, void *unused)
{
	return !memcmp (e1, e2, sizeof (qfm_blend_t));
}

#define MAX_BLENDS 1024

qfm_blend_t *
Mod_IQMBuildBlendPalette (mod_iqm_ctx_t *iqm, uint32_t *size)
{
	iqmvertexarray *bindices = 0;
	iqmvertexarray *bweights = 0;
	qfm_blend_t *blend_list;
	int         num_blends;
	hashtab_t  *blend_hash;

	for (uint32_t i = 0; i < iqm->hdr->num_vertexarrays; i++) {
		if (iqm->vtxarr[i].type == IQM_BLENDINDEXES)
			bindices = &iqm->vtxarr[i];
		if (iqm->vtxarr[i].type == IQM_BLENDWEIGHTS)
			bweights = &iqm->vtxarr[i];
	}
	if (!bindices || !bweights) {
		// Not necessarily an error: might be a static model with no bones
		// Either way, no need to make a blend palette
		Sys_MaskPrintf (SYS_model, "bone index or weight array missing\n");
		*size = 0;
		return 0;
	}

	blend_hash = Hash_NewTable (1023, 0, 0, 0, 0);
	Hash_SetHashCompare (blend_hash, blend_get_hash, blend_compare);

	blend_list = calloc (MAX_BLENDS, sizeof (qfm_blend_t));
	for (uint32_t i = 0; i < iqm->hdr->num_joints; i++) {
		blend_list[i].indices[0] = i;
		blend_list[i].weights[0] = 255;
		Hash_AddElement (blend_hash, &blend_list[i]);
	}
	num_blends = iqm->hdr->num_joints;

	blend_list[iqm->hdr->num_joints] = (qfm_blend_t) { };
	if (!Hash_FindElement (blend_hash, &blend_list[num_blends])) {
		Hash_AddElement (blend_hash, &blend_list[num_blends]);
		num_blends++;
	}

	for (uint32_t i = 0; i < iqm->hdr->num_vertexes; i++) {
		byte       *base = (byte *) iqm->hdr;
		byte       *bi = base + bindices->offset + i * 4;
		byte       *bw = base + bweights->offset + i * 4;
		qfm_blend_t blend;
		qfm_blend_t *bl;

		// First, canonicalize vextex bone data:
		//   bone indices are in increasing order
		//   bone weight of zero is never followed by a non-zero weight
		//   bone weight of zero has bone index of zero

		// if the weight is zero, ensure the index is also zero
		// also, ensure non-zero weights never follow zero weights
		for (uint32_t j = 0; j < 4; j++) {
			if (!bw[j]) {
				bi[j] = 0;
			} else {
				if (j && !bw[j-1]) {
					swap_bones (bi, bw, j - 1, j);
					j = 0;					// force a rescan
				}
			}
		}
		// sort the bones such that the indices are increasing (unless the
		// weight is zero)
		for (uint32_t j = 0; j < 3; j++) {
			if (!bw[j+1])					// zero weight == end of list
				break;
			if (bi[j] > bi[j+1]) {
				swap_bones (bi, bw, j, j + 1);
				j = -1;						// force rescan
			}
		}

		// Now that the bone data is canonical, it can be hashed.
		// However, no need to check other combinations if the vertex has
		// only one influencing bone: the bone index will only change format.
		if (!bw[1]) {
			*(uint32_t *) bi = bi[0];
			continue;
		}
		QuatCopy (bi, blend.indices);
		QuatCopy (bw, blend.weights);
		if ((bl = Hash_FindElement (blend_hash, &blend))) {
			*(uint32_t *) bi = (bl - blend_list);
			continue;
		}
		if (num_blends >= MAX_BLENDS)
			Sys_Error ("Too many blends. Tell taniwha to stop being lazy.");
		blend_list[num_blends] = blend;
		Hash_AddElement (blend_hash, &blend_list[num_blends]);
		*(uint32_t *) bi = num_blends;
		num_blends++;
	}

	Hash_DelTable (blend_hash);
	*size = num_blends;
	return realloc (blend_list, num_blends * sizeof (qfm_blend_t));
}
