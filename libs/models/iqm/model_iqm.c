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
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "compat.h"
#include "d_iface.h"
#include "mod_internal.h"
#include "r_local.h"

static iqmvertexarray *
get_vertex_arrays (const iqmheader *hdr, byte *buffer)
{
	iqmvertexarray *va;
	uint32_t    i;

	if (hdr->ofs_vertexarrays + hdr->num_vertexarrays * sizeof (iqmvertexarray)
		> hdr->filesize)
		return 0;
	va = (iqmvertexarray *) (buffer + hdr->ofs_vertexarrays);
	for (i = 0; i < hdr->num_vertexarrays; i++) {
		va[i].type = LittleLong (va[i].type);
		va[i].flags = LittleLong (va[i].flags);
		va[i].format = LittleLong (va[i].format);
		va[i].size = LittleLong (va[i].size);
		va[i].offset = LittleLong (va[i].offset);
	}
	return va;
}

static iqmtriangle *
get_triangles (const iqmheader *hdr, byte *buffer)
{
	iqmtriangle *tri;
	uint32_t    i, j;

	if (hdr->ofs_triangles + hdr->num_triangles * sizeof (iqmtriangle)
		> hdr->filesize)
		return 0;
	tri = (iqmtriangle *) (buffer + hdr->ofs_triangles);
	for (i = 0; i < hdr->num_triangles; i++) {
		for (j = 0; j < 3; j++) {
			tri[i].vertex[j] = LittleLong (tri[i].vertex[j]);
			if (tri[i].vertex[j] >= hdr->num_vertexes) {
				Sys_Printf ("invalid tri vertex\n");
				return 0;
			}
		}
	}
	return tri;
}

static iqmmesh *
get_meshes (const iqmheader *hdr, byte *buffer)
{
	iqmmesh    *mesh;
	uint32_t    i;

	if (hdr->ofs_meshes + hdr->num_meshes * sizeof (iqmmesh) > hdr->filesize)
		return 0;
	mesh = (iqmmesh *) (buffer + hdr->ofs_meshes);
	for (i = 0; i < hdr->num_meshes; i++) {
		mesh[i].name = LittleLong (mesh[i].name);
		mesh[i].material = LittleLong (mesh[i].material);
		mesh[i].first_vertex = LittleLong (mesh[i].first_vertex);
		mesh[i].num_vertexes = LittleLong (mesh[i].num_vertexes);
		mesh[i].first_triangle = LittleLong (mesh[i].first_triangle);
		mesh[i].num_triangles = LittleLong (mesh[i].num_triangles);
	}
	return mesh;
}

static iqmjoint *
get_joints (const iqmheader *hdr, byte *buffer)
{
	iqmjoint   *joint;
	uint32_t    i, j;

	if (hdr->ofs_joints + hdr->num_joints * sizeof (iqmjoint) > hdr->filesize)
		return 0;
	joint = (iqmjoint *) (buffer + hdr->ofs_joints);
	for (i = 0; i < hdr->num_joints; i++) {
		joint[i].name = LittleLong (joint[i].name);
		joint[i].parent = LittleLong (joint[i].parent);
		if (joint[i].parent >= 0
			&& (uint32_t) joint[i].parent >= hdr->num_joints) {
			Sys_Printf ("invalid parent\n");
			return 0;
		}
		for (j = 0; j < 3; j++)
			joint[i].translate[j] = LittleFloat (joint[i].translate[j]);
		for (j = 0; j < 4; j++)
			joint[i].rotate[j] = LittleFloat (joint[i].rotate[j]);
		for (j = 0; j < 3; j++)
			joint[i].scale[j] = LittleFloat (joint[i].scale[j]);
	}
	return joint;
}

static bool
load_iqm_vertex_arrays (model_t *mod, const iqmheader *hdr, byte *buffer)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	iqmvertexarray *vas;
	float      *position = 0;
	float      *normal = 0;
	float      *tangent = 0;
	float      *texcoord = 0;
	byte       *blendindex = 0;
	byte       *blendweight = 0;
	byte       *color = 0;
	byte       *vert;
	iqmvertexarray *va;
	size_t      bytes = 0;
	uint32_t    i, j;

	if (!(vas = get_vertex_arrays (hdr, buffer)))
		return false;

	for (i = 0; i < hdr->num_vertexarrays; i++) {
		va = vas + i;
		Sys_MaskPrintf (SYS_model, "%u %u %u %u %u %u\n", i, va->type, va->flags, va->format, va->size, va->offset);
		switch (va->type) {
			case IQM_POSITION:
				if (position)
					return false;
				if (va->format != IQM_FLOAT || va->size != 3)
					return false;
				iqm->num_arrays++;
				bytes += va->size * sizeof (float);
				position = (float *) (buffer + va->offset);
				for (j = 0; j < va->size * hdr->num_vertexes; j++)
					position[j] = LittleFloat (position[j]);
				break;
			case IQM_NORMAL:
				if (normal)
					return false;
				if (va->format != IQM_FLOAT || va->size != 3)
					return false;
				iqm->num_arrays++;
				bytes += va->size * sizeof (float);
				normal = (float *) (buffer + va->offset);
				for (j = 0; j < va->size * hdr->num_vertexes; j++)
					normal[j] = LittleFloat (normal[j]);
				break;
			case IQM_TANGENT:
				if (tangent)
					return false;
				if (va->format != IQM_FLOAT || va->size != 4)
					return false;
				iqm->num_arrays++;
				bytes += va->size * sizeof (float);
				tangent = (float *) (buffer + va->offset);
				for (j = 0; j < va->size * hdr->num_vertexes; j++)
					tangent[j] = LittleFloat (tangent[j]);
				break;
			case IQM_TEXCOORD:
				if (texcoord)
					return false;
				if (va->format != IQM_FLOAT || va->size != 2)
					return false;
				iqm->num_arrays++;
				bytes += va->size * sizeof (float);
				texcoord = (float *) (buffer + va->offset);
				for (j = 0; j < va->size * hdr->num_vertexes; j++)
					texcoord[j] = LittleFloat (texcoord[j]);
				break;
			case IQM_BLENDINDEXES:
				if (blendindex)
					return false;
				if (va->format != IQM_UBYTE || va->size != 4)
					return false;
				iqm->num_arrays++;
				bytes += va->size;
				blendindex = (byte *) (buffer + va->offset);
				break;
			case IQM_BLENDWEIGHTS:
				if (blendweight)
					return false;
				if (va->format != IQM_UBYTE || va->size != 4)
					return false;
				iqm->num_arrays++;
				bytes += va->size;
				blendweight = (byte *) (buffer + va->offset);
				break;
			case IQM_COLOR:
				if (color)
					return false;
				if (va->format != IQM_UBYTE || va->size != 4)
					return false;
				iqm->num_arrays++;
				bytes += va->size;
				color = (byte *) (buffer + va->offset);
				break;
		}
	}
	iqm->vertexarrays = calloc (iqm->num_arrays + 1, sizeof (iqmvertexarray));
	va = iqm->vertexarrays;
	if (position) {
		va->type = IQM_POSITION;
		va->format = IQM_FLOAT;
		va->size = 3;
		va[1].offset = va->offset + va->size * sizeof (float);
		va++;
	}
	if (texcoord) {
		va->type = IQM_TEXCOORD;
		va->format = IQM_FLOAT;
		va->size = 2;
		va[1].offset = va->offset + va->size * sizeof (float);
		va++;
	}
	if (normal) {
		va->type = IQM_NORMAL;
		va->format = IQM_FLOAT;
		va->size = 3;
		va[1].offset = va->offset + va->size * sizeof (float);
		va++;
	}
	if (tangent) {
		va->type = IQM_TANGENT;
		va->format = IQM_FLOAT;
		va->size = 4;
		va[1].offset = va->offset + va->size * sizeof (float);
		va++;
	}
	if (blendindex) {
		va->type = IQM_BLENDINDEXES;
		va->format = IQM_UBYTE;
		va->size = 4;
		va[1].offset = va->offset + va->size;
		va++;
	}
	if (blendweight) {
		va->type = IQM_BLENDWEIGHTS;
		va->format = IQM_UBYTE;
		va->size = 4;
		va[1].offset = va->offset + va->size;
		va++;
	}
	if (color) {
		va->type = IQM_COLOR;
		va->format = IQM_UBYTE;
		va->size = 4;
		va[1].offset = va->offset + va->size;
		va++;
	}
	iqm->vertexarrays = realloc (iqm->vertexarrays,
								 iqm->num_arrays * sizeof (iqmvertexarray));
	iqm->num_verts = hdr->num_vertexes;
	iqm->vertices = malloc (hdr->num_vertexes * bytes);
	iqm->stride = bytes;
	for (i = 0; i < hdr->num_vertexes; i++) {
		va = iqm->vertexarrays;
		vert = iqm->vertices + i * bytes;
		if (position) {
			memcpy (vert + va->offset, &position[i * 3], 3 * sizeof (float));
			va++;
		}
		if (texcoord) {
			memcpy (vert + va->offset, &texcoord[i * 2], 2 * sizeof (float));
			va++;
		}
		if (normal) {
			memcpy (vert + va->offset, &normal[i * 3], 3 * sizeof (float));
			va++;
		}
		if (tangent) {
			memcpy (vert + va->offset, &tangent[i * 4], 4 * sizeof (float));
			va++;
		}
		if (blendindex) {
			memcpy (vert + va->offset, &blendindex[i * 4], 4);
			va++;
		}
		if (blendweight) {
			memcpy (vert + va->offset, &blendweight[i * 4], 4);
			va++;
		}
		if (color) {
			memcpy (vert + va->offset, &color[i * 4], 4);
			va++;
		}
	}
	return true;
}

static bool
load_iqm_meshes (model_t *mod, const iqmheader *hdr, byte *buffer)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	iqmtriangle *tris;
	iqmmesh    *meshes;
	iqmjoint   *joints;
	uint32_t    i;

	if (!load_iqm_vertex_arrays (mod, hdr, buffer))
		return false;
	if (!(tris = get_triangles (hdr, buffer)))
		return false;
	iqm->num_elements = hdr->num_triangles * 3;
	iqm->elements = malloc (hdr->num_triangles * 3 * sizeof (uint16_t));
	for (i = 0; i < hdr->num_triangles; i++)
		VectorCopy (tris[i].vertex, iqm->elements + i * 3);
	if (!(meshes = get_meshes (hdr, buffer)))
		return false;
	iqm->num_meshes = hdr->num_meshes;
	iqm->meshes = malloc (hdr->num_meshes * sizeof (iqmmesh));
	memcpy (iqm->meshes, meshes, hdr->num_meshes * sizeof (iqmmesh));
	if (!(joints = get_joints (hdr, buffer)))
		return false;
	iqm->num_joints = hdr->num_joints;
	iqm->joints = malloc (iqm->num_joints * sizeof (iqmjoint));
	iqm->baseframe = malloc (iqm->num_joints * sizeof (mat4_t));
	iqm->inverse_baseframe = malloc (iqm->num_joints * sizeof (mat4_t));
	memcpy (iqm->joints, joints, iqm->num_joints * sizeof (iqmjoint));
	for (i = 0; i < hdr->num_joints; i++) {
		iqmjoint   *j = &iqm->joints[i];
		mat4_t     *bf = &iqm->baseframe[i];
		mat4_t     *ibf = &iqm->inverse_baseframe[i];
		quat_t      t;
		float       ilen;
		ilen = 1.0 / sqrt(QDotProduct (j->rotate, j->rotate));
		QuatScale (j->rotate, ilen, t);
		Mat4Init (t, j->scale, j->translate, *bf);
		Mat4Inverse (*bf, *ibf);
		if (j->parent >= 0) {
			Mat4Mult (iqm->baseframe[j->parent], *bf, *bf);
			Mat4Mult (*ibf, iqm->inverse_baseframe[j->parent], *ibf);
		}
	}
	return true;
}

static bool
load_iqm_anims (model_t *mod, const iqmheader *hdr, byte *buffer)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	iqmanim    *anims;
	iqmpose    *poses;
	uint16_t   *framedata;
	uint32_t    i, j;

	if (hdr->num_poses != hdr->num_joints)
		return false;

	iqm->num_anims = hdr->num_anims;
	iqm->anims = malloc (hdr->num_anims * sizeof (iqmanim));
	anims = (iqmanim *) (buffer + hdr->ofs_anims);
	for (i = 0; i < hdr->num_anims; i++) {
		iqm->anims[i].name = LittleLong (anims[i].name);
		iqm->anims[i].first_frame = LittleLong (anims[i].first_frame);
		iqm->anims[i].num_frames = LittleLong (anims[i].num_frames);
		iqm->anims[i].framerate = LittleFloat (anims[i].framerate);
		iqm->anims[i].flags = LittleLong (anims[i].flags);
	}

	poses = (iqmpose *) (buffer + hdr->ofs_poses);
	for (i = 0; i < hdr->num_poses; i++) {
		poses[i].parent = LittleLong (poses[i].parent);
		poses[i].mask = LittleLong (poses[i].mask);
		for (j = 0; j < 10; j++) {
			poses[i].channeloffset[j] = LittleFloat(poses[i].channeloffset[j]);
			poses[i].channelscale[j] = LittleFloat (poses[i].channelscale[j]);
		}
	}

	framedata = (uint16_t *) (buffer + hdr->ofs_frames);
	for (i = 0; i < hdr->num_frames * hdr->num_framechannels; i++)
		framedata[i] = LittleShort (framedata[i]);

	iqm->num_frames = hdr->num_frames;
	iqm->frames = malloc (hdr->num_frames * sizeof (iqmframe_t *));
	iqm->frames[0] = malloc (hdr->num_frames * hdr->num_poses
							 * sizeof (iqmframe_t));

	for (i = 0; i < hdr->num_frames; i++) {
		iqm->frames[i] = iqm->frames[0] + i * hdr->num_poses;
		for (j = 0; j < hdr->num_poses; j++) {
			iqmframe_t *frame = &iqm->frames[i][j];
			iqmpose    *p = &poses[j];
			quat_t      rotation;
			vec3_t      scale, translation;
			mat4_t      mat;
			float       ilen;

			translation[0] = p->channeloffset[0];
			if (p->mask & 0x001)
				translation[0] += *framedata++ * p->channelscale[0];
			translation[1] = p->channeloffset[1];
			if (p->mask & 0x002)
				translation[1] += *framedata++ * p->channelscale[1];
			translation[2] = p->channeloffset[2];
			if (p->mask & 0x004)
				translation[2] += *framedata++ * p->channelscale[2];

			rotation[0] = p->channeloffset[3];
			if (p->mask & 0x008)
				rotation[0] += *framedata++ * p->channelscale[3];
			rotation[1] = p->channeloffset[4];
			if (p->mask & 0x010)
				rotation[1] += *framedata++ * p->channelscale[4];
			rotation[2] = p->channeloffset[5];
			if (p->mask & 0x020)
				rotation[2] += *framedata++ * p->channelscale[5];
			rotation[3] = p->channeloffset[6];
			if (p->mask & 0x040)
				rotation[3] += *framedata++ * p->channelscale[6];

			scale[0] = p->channeloffset[7];
			if (p->mask & 0x080)
				scale[0] += *framedata++ * p->channelscale[7];
			scale[1] = p->channeloffset[8];
			if (p->mask & 0x100)
				scale[1] += *framedata++ * p->channelscale[8];
			scale[2] = p->channeloffset[9];
			if (p->mask & 0x200)
				scale[2] += *framedata++ * p->channelscale[9];

			ilen = 1.0 / sqrt(QDotProduct (rotation, rotation));
			QuatScale (rotation, ilen, rotation);
			Mat4Init (rotation, scale, translation, mat);
			if (p->parent >= 0)
				Mat4Mult (iqm->baseframe[p->parent], mat, mat);
#if 0
			Mat4Mult (mat, iqm->inverse_baseframe[j], mat);
			// convert the matrix to dual quaternion + shear + scale
			Mat4Decompose (mat, frame->rt.q0.q, frame->shear, frame->scale,
						   frame->rt.qe.sv.v);
			frame->rt.qe.sv.s = 0;
			// apply the inverse of scale and shear to translation so
			// everything works out properly in the shader.
			// Normally v' = T*Sc*Sh*R*v, but with the dual quaternion, we get
			// v' = Sc*Sh*T'*R*v
			VectorCompDiv (frame->rt.qe.sv.v, frame->scale, frame->rt.qe.sv.v);
			VectorUnshear (frame->shear, frame->rt.qe.sv.v, frame->rt.qe.sv.v);
			// Dual quaternions need 1/2 translation.
			VectorScale (frame->rt.qe.sv.v, 0.5, frame->rt.qe.sv.v);
			// and tranlation * rotation
			QuatMult (frame->rt.qe.q, frame->rt.q0.q, frame->rt.qe.q);
#else
			Mat4Mult (mat, iqm->inverse_baseframe[j], (float *)frame);
#endif
		}
	}
	return true;
}

void
Mod_LoadIQM (model_t *mod, void *buffer)
{
	iqmheader  *hdr = (iqmheader *) buffer;
	iqm_t      *iqm;
	uint32_t   *swap;

	if (!strequal (hdr->magic, IQM_MAGIC))
		Sys_Error ("%s: not an IQM", mod->path);
	// Byte swap the header. Everything is the same type, so no problem :)
	for (swap = &hdr->version; swap <= &hdr->ofs_extensions; swap++)
		*swap = LittleLong (*swap);
	//if (hdr->version < 1 || hdr->version > IQM_VERSION)
	if (hdr->version != IQM_VERSION)
		Sys_Error ("%s: unable to handle iqm version %d", mod->path,
				   hdr->version);
	if (hdr->filesize != (uint32_t) qfs_filesize)
		Sys_Error ("%s: invalid filesize", mod->path);
	iqm = calloc (1, sizeof (iqm_t));
	iqm->text = malloc (hdr->num_text);
	memcpy (iqm->text, (byte *) buffer + hdr->ofs_text, hdr->num_text);
	mod->aliashdr = (aliashdr_t *) iqm;
	mod->type = mod_iqm;
	if (hdr->num_meshes && !load_iqm_meshes (mod, hdr, (byte *) buffer))
		Sys_Error ("%s: error loading meshes", mod->path);
	if (hdr->num_anims && !load_iqm_anims (mod, hdr, (byte *) buffer))
		Sys_Error ("%s: error loading anims", mod->path);
	m_funcs->Mod_IQMFinish (mod);
}

void
Mod_FreeIQM (iqm_t *iqm)
{
	free (iqm->text);
	if (iqm->vertices)
		free (iqm->vertices);
	free (iqm->vertexarrays);
	if (iqm->elements)
		free (iqm->elements);
	free (iqm->meshes);
	free (iqm->joints);
	free (iqm->baseframe);
	free (iqm->inverse_baseframe);
	free (iqm->anims);
	if (iqm->frames) {
		free (iqm->frames[0]);
		free (iqm->frames);
	}
	free (iqm);
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
	iqmblend_t *b = (iqmblend_t *) e;
	return CRC_Block ((byte *) b, sizeof (iqmblend_t));
}

static int
blend_compare (const void *e1, const void *e2, void *unused)
{
	iqmblend_t *b1 = (iqmblend_t *) e1;
	iqmblend_t *b2 = (iqmblend_t *) e2;
	return !memcmp (b1, b2, sizeof (iqmblend_t));
}

#define MAX_BLENDS 1024

iqmblend_t *
Mod_IQMBuildBlendPalette (iqm_t *iqm, int *size)
{
	int         i, j;
	iqmvertexarray *bindices = 0;
	iqmvertexarray *bweights = 0;
	iqmblend_t *blend_list;
	int         num_blends;
	hashtab_t  *blend_hash;

	for (i = 0; i < iqm->num_arrays; i++) {
		if (iqm->vertexarrays[i].type == IQM_BLENDINDEXES)
			bindices = &iqm->vertexarrays[i];
		if (iqm->vertexarrays[i].type == IQM_BLENDWEIGHTS)
			bweights = &iqm->vertexarrays[i];
	}
	if (!bindices || !bweights) {
		// Not necessarily an error: might be a static model with no bones
		// Either way, no need to make a blend palette
		Sys_MaskPrintf (SYS_model, "bone index or weight array missing\n");
		*size = 0;
		return 0;
	}

	blend_list = calloc (MAX_BLENDS, sizeof (iqmblend_t));
	for (i = 0; i < iqm->num_joints; i++) {
		blend_list[i].indices[0] = i;
		blend_list[i].weights[0] = 255;
	}
	num_blends = iqm->num_joints;

	blend_hash = Hash_NewTable (1023, 0, 0, 0, 0);
	Hash_SetHashCompare (blend_hash, blend_get_hash, blend_compare);

	for (i = 0; i < iqm->num_verts; i++) {
		byte       *vert = iqm->vertices + i * iqm->stride;
		byte       *bi = vert + bindices->offset;
		byte       *bw = vert + bweights->offset;
		iqmblend_t  blend;
		iqmblend_t *bl;

		// First, canonicalize vextex bone data:
		//   bone indices are in increasing order
		//   bone weight of zero is never followed by a non-zero weight
		//   bone weight of zero has bone index of zero

		// if the weight is zero, ensure the index is also zero
		// also, ensure non-zero weights never follow zero weights
		for (j = 0; j < 4; j++) {
			if (!bw[j]) {
				bi[j] = 0;
			} else {
				if (j && !bw[j-1]) {
					swap_bones (bi, bw, j - 1, j);
					j = 0;					// force a rescan
				}
			}
		}
		// sort the bones such that the indeces are increasing (unless the
		// weight is zero)
		for (j = 0; j < 3; j++) {
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
	return realloc (blend_list, num_blends * sizeof (iqmblend_t));
}
