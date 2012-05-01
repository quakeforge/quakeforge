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
#include "QF/iqm.h"
#include "QF/msg.h"
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

static qboolean
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
	byte       *vert;
	iqmvertexarray *va;
	size_t      bytes = 0;
	uint32_t    i, j;

	if (!(vas = get_vertex_arrays (hdr, buffer)))
		return false;

	for (i = 0; i < hdr->num_vertexarrays; i++) {
		va = vas + i;
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
					position[i] = LittleFloat (position[i]);
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
					normal[i] = LittleFloat (normal[i]);
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
					tangent[i] = LittleFloat (tangent[i]);
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
					texcoord[i] = LittleFloat (texcoord[i]);
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
	if (texcoord) {
		va->type = IQM_TEXCOORD;
		va->format = IQM_FLOAT;
		va->size = 2;
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
	iqm->vertexarrays = realloc (iqm->vertexarrays,
								 iqm->num_arrays * sizeof (iqmvertexarray));
	iqm->vertices = malloc (hdr->num_vertexes * bytes);
	for (i = 0; i < hdr->num_vertexes; i++) {
		va = iqm->vertexarrays;
		vert = iqm->vertices + i * bytes;
		if (position) {
			memcpy (vert + va->offset, &position[i * 3], 3 * sizeof (float));
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
		if (texcoord) {
			memcpy (vert + va->offset, &texcoord[i * 2], 2 * sizeof (float));
			va++;
		}
		if (blendindex) {
			memcpy (vert + va->offset, &blendindex[i * 4], 4);
			va++;
		}
		if (blendweight) {
			memcpy (vert + va->offset, &blendindex[i * 4], 4);
			va++;
		}
	}
	return true;
}

static qboolean
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
	iqm->elements = malloc (hdr->num_triangles * 3 * sizeof (uint16_t));
	for (i = 0; i < hdr->num_triangles; i++)
		VectorCopy (tris[i].vertex, iqm->elements + i * 3);
	if (!(meshes = get_meshes (hdr, buffer)))
		return false;
	if (!(joints = get_joints (hdr, buffer)))
		return false;
	return true;
}

static qboolean
load_iqm_anims (model_t *mod, iqmheader *hdr, byte *buffer)
{
	return true;
}

void
Mod_LoadIQM (model_t *mod, void *buffer)
{
	iqmheader  *hdr = (iqmheader *) buffer;
	uint32_t   *swap;

	if (!strequal (hdr->magic, IQM_MAGIC))
		Sys_Error ("%s: not an IQM", loadname);
	// Byte swap the header. Everything is the same type, so no problem :)
	for (swap = &hdr->version; swap <= &hdr->ofs_extensions; swap++)
		*swap = LittleLong (*swap);
	//if (hdr->version < 1 || hdr->version > IQM_VERSION)
	if (hdr->version != IQM_VERSION)
		Sys_Error ("%s: unable to handle iqm version %d", loadname,
				   hdr->version);
	if (hdr->filesize != (uint32_t) qfs_filesize)
		Sys_Error ("%s: invalid filesize", loadname);
	if (hdr->num_meshes && !load_iqm_meshes (mod, hdr, (byte *) buffer))
		Sys_Error ("%s: error loading meshes", loadname);
	if (hdr->num_anims && !load_iqm_anims (mod, hdr, (byte *) buffer))
		Sys_Error ("%s: error loading anims", loadname);
}
