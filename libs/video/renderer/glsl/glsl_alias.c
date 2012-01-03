/*
	glsl_alias.c

	GLSL Alias model rendering

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/1

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/render.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_alias.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_local.h"

static vec3_t vertex_normals[NUMVERTEXNORMALS] = {
#include "anorms.h"
};

static const char quakemdl_vert[] =
#include "quakemdl.vc"
;

static const char quakemdl_frag[] =
#include "quakemdl.fc"
;

static struct {
	int         program;
	shaderparam_t normals;
	shaderparam_t mvp_matrix;
	shaderparam_t norm_matrix;
	shaderparam_t skin_size;
	shaderparam_t color;
	shaderparam_t stn;
	shaderparam_t vertex;
	shaderparam_t palette;
	shaderparam_t colormap;
	shaderparam_t skin;
	shaderparam_t ambient;
	shaderparam_t shadelight;
	shaderparam_t lightvec;
} quake_mdl = {
	0,
	{"normals", 1},
	{"mvp_mat", 1},
	{"norm_mat", 1},
	{"skin_size", 1},
	{"vcolor", 0},
	{"stn", 0},
	{"vertex", 0},
	{"palette", 1},
	{"colormap", 1},
	{"skin", 1},
	{"ambient", 1},
	{"shadelight", 1},
	{"lightvec", 1},
};

static int vnorms_tex;
static mat4_t alias_vp;

static void
build_normals_texture (void)
{
	vec3_t      temp;
	static const vec3_t one = { 1, 1, 1};
	unsigned short norm[3];
	int         i, j;
	byte       *data;

	data = malloc (NUMVERTEXNORMALS * 3 * 2);
	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		VectorAdd (vertex_normals[i], one, temp);	// temp is 0.0 .. 2.0
		VectorScale (temp, 32767.5, norm);			// norm is 0 .. 65535
		for (j = 0; j < 3; j++) {
			data[i * 6 + 0 + j] = norm[j] >> 8;
			data[i * 6 + 3 + j] = norm[j] & 0xff;
		}
	}
	vnorms_tex = GL_LoadRGBTexture ("vertex_normals", 2, NUMVERTEXNORMALS,
									data);
	free (data);
}

VISIBLE void
R_InitAlias (void)
{
	int         vert;
	int         frag;

	build_normals_texture ();
	vert = GL_CompileShader ("quakemdl.vert", quakemdl_vert, GL_VERTEX_SHADER);
	frag = GL_CompileShader ("quakemdl.frag", quakemdl_frag,
							 GL_FRAGMENT_SHADER);
	quake_mdl.program = GL_LinkProgram ("quakemdl", vert, frag);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.normals);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.mvp_matrix);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.norm_matrix);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.skin_size);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.color);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.stn);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.vertex);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.palette);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.colormap);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.skin);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.ambient);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.shadelight);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.lightvec);
}

static void
calc_lighting (entity_t *ent, float *ambient, float *shadelight,
			   vec3_t lightvec)
{
	unsigned    i;
	float       add;
	vec3_t      dist;

	VectorSet ( -1, 0, 0, lightvec);	//FIXME
	*ambient = max (R_LightPoint (ent->origin), max (ent->model->min_light,
													 ent->min_light) * 128);
	*shadelight = *ambient;

	for (i = 0; i < r_maxdlights; i++) {
		if (r_dlights[i].die >= r_realtime) {
			VectorSubtract (ent->origin, r_dlights[i].origin, dist);
			add = r_dlights[i].radius - VectorLength (dist);
			if (add > 0)
				*ambient += add;
		}
	}
	if (*ambient >= 128)
		*ambient = 128;
	if (*shadelight > 192 - *ambient)
		*shadelight = 192 - *ambient;
}
//#define TETRAHEDRON
void
R_DrawAlias (void)
{
#ifdef TETRAHEDRON
	static aliasvrt_t debug_verts[] = {
		{{0,0,0},{0,0,0}},
		{{255,255,0},{0,300,0}},
		{{0,255,255},{300,300,0}},
		{{255,0,255},{300,0,0}},
	};
	static GLushort debug_indices[] = {
		0, 1, 2,
		0, 3, 1,
		1, 3, 2,
		0, 2, 3,
	};
#endif
	static quat_t color = { 1, 1, 1, 1};
	static vec3_t lightvec;
	float       ambient;
	float       shadelight;
	float       skin_size[2];
	entity_t   *ent = currententity;
	model_t    *model = ent->model;
	aliashdr_t *hdr;
	vec_t       norm_mat[9];
	mat4_t      mvp_mat;
	maliasskindesc_t *skin;
	aliasvrt_t *pose = 0;		// VBO's are null based
	maliasframedesc_t *frame;

	hdr = Cache_Get (&model->cache);

	calc_lighting (ent, &ambient, &shadelight, lightvec);

	// we need only the rotation for normals.
	VectorCopy (ent->transform + 0, norm_mat + 0);
	VectorCopy (ent->transform + 4, norm_mat + 3);
	VectorCopy (ent->transform + 8, norm_mat + 6);

	// ent model scaling and offset
	Mat4Zero (mvp_mat);
	mvp_mat[0] = hdr->mdl.scale[0];
	mvp_mat[5] = hdr->mdl.scale[1];
	mvp_mat[10] = hdr->mdl.scale[2];
	mvp_mat[15] = 1;
	VectorCopy (hdr->mdl.scale_origin, mvp_mat + 12);
	Mat4Mult (ent->transform, mvp_mat, mvp_mat);
	Mat4Mult (alias_vp, mvp_mat, mvp_mat);

	skin = R_AliasGetSkindesc (ent->skinnum, hdr);
	frame = R_AliasGetFramedesc (ent->frame, hdr);

	pose += frame->firstpose * hdr->poseverts;

	skin_size[0] = hdr->mdl.skinwidth;
	skin_size[1] = hdr->mdl.skinheight;

	qfglBindTexture (GL_TEXTURE_2D, skin->texnum);

#ifndef TETRAHEDRON
	qfglBindBuffer (GL_ARRAY_BUFFER, hdr->posedata);
	qfglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, hdr->commands);
#endif

	qfglVertexAttrib4fv (quake_mdl.color.location, color);
	qfglUniform1f (quake_mdl.ambient.location, ambient);
	qfglUniform1f (quake_mdl.shadelight.location, shadelight);
	qfglUniform3fv (quake_mdl.lightvec.location, 1, lightvec);
	qfglUniform2fv (quake_mdl.skin_size.location, 1, skin_size);
	qfglUniformMatrix4fv (quake_mdl.mvp_matrix.location, 1, false, mvp_mat);
	qfglUniformMatrix3fv (quake_mdl.norm_matrix.location, 1, false, norm_mat);

#ifndef TETRAHEDRON
	qfglVertexAttribPointer (quake_mdl.vertex.location, 3, GL_UNSIGNED_BYTE,
							 0, sizeof (aliasvrt_t),
							 (byte *) pose + field_offset (aliasvrt_t, vertex));
	qfglVertexAttribPointer (quake_mdl.stn.location, 3, GL_SHORT,
							 0, sizeof (aliasvrt_t),
							 (byte *) pose + field_offset (aliasvrt_t, stn));
	qfglDrawElements (GL_TRIANGLES, 3 * hdr->mdl.numtris, GL_UNSIGNED_SHORT, 0);
#else
	qfglVertexAttribPointer (quake_mdl.vertex.location, 3, GL_UNSIGNED_BYTE,
							 0, sizeof (aliasvrt_t),
							 &debug_verts[0].vertex);
	qfglVertexAttribPointer (quake_mdl.stn.location, 3, GL_SHORT,
							 0, sizeof (aliasvrt_t),
							 &debug_verts[0].stn);
	qfglDrawElements (GL_TRIANGLES,
					  sizeof (debug_indices) / sizeof (debug_indices[0]),
					  GL_UNSIGNED_SHORT, debug_indices);
#endif
}

// All alias models are drawn in a batch, so avoid thrashing the gl state
void
R_AliasBegin (void)
{
	// pre-multiply the view and projection matricies
	Mat4Mult (glsl_projection, glsl_view, alias_vp);

	qfglUseProgram (quake_mdl.program);
	qfglEnableVertexAttribArray (quake_mdl.vertex.location);
	qfglEnableVertexAttribArray (quake_mdl.stn.location);
	qfglDisableVertexAttribArray (quake_mdl.color.location);

	qfglUniform1i (quake_mdl.normals.location, 1);
	qfglActiveTexture (GL_TEXTURE0 + 1);
	qfglEnable (GL_TEXTURE_2D);
	qfglBindTexture (GL_TEXTURE_2D, vnorms_tex);

	qfglUniform1i (quake_mdl.colormap.location, 2);
	qfglActiveTexture (GL_TEXTURE0 + 2);
	qfglEnable (GL_TEXTURE_2D);
	qfglBindTexture (GL_TEXTURE_2D, glsl_colormap);

	qfglUniform1i (quake_mdl.palette.location, 3);
	qfglActiveTexture (GL_TEXTURE0 + 3);
	qfglEnable (GL_TEXTURE_2D);
	qfglBindTexture (GL_TEXTURE_2D, glsl_palette);

	qfglUniform1i (quake_mdl.skin.location, 0);
	qfglActiveTexture (GL_TEXTURE0 + 0);
	qfglEnable (GL_TEXTURE_2D);
}

void
R_AliasEnd (void)
{
	qfglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

	qfglDisableVertexAttribArray (quake_mdl.vertex.location);
	qfglDisableVertexAttribArray (quake_mdl.stn.location);

	qfglActiveTexture (GL_TEXTURE0 + 0);
	qfglDisable (GL_TEXTURE_2D);
	qfglActiveTexture (GL_TEXTURE0 + 1);
	qfglDisable (GL_TEXTURE_2D);
	qfglActiveTexture (GL_TEXTURE0 + 2);
	qfglDisable (GL_TEXTURE_2D);
	qfglActiveTexture (GL_TEXTURE0 + 3);
	qfglDisable (GL_TEXTURE_2D);
}
