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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_alias.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

static const char *alias_vert_effects[] =
{
	"QuakeForge.Vertex.mdl",
	0
};

static const char *alias_frag_effects[] =
{
	"QuakeForge.Fragment.fog",
	"QuakeForge.Fragment.colormap",
	"QuakeForge.Fragment.mdl",
	0
};

static struct {
	int         program;
	shaderparam_t mvp_matrix;
	shaderparam_t norm_matrix;
	shaderparam_t skin_size;
	shaderparam_t blend;
	shaderparam_t colora;
	shaderparam_t colorb;
	shaderparam_t sta;
	shaderparam_t stb;
	shaderparam_t normala;
	shaderparam_t normalb;
	shaderparam_t vertexa;
	shaderparam_t vertexb;
	shaderparam_t colormap;
	shaderparam_t skin;
	shaderparam_t ambient;
	shaderparam_t shadelight;
	shaderparam_t lightvec;
	shaderparam_t fog;
} quake_mdl = {
	0,
	{"mvp_mat", 1},
	{"norm_mat", 1},
	{"skin_size", 1},
	{"blend", 1},
	{"vcolora", 0},
	{"vcolorb", 0},
	{"vsta", 0},
	{"vstb", 0},
	{"vnormala", 0},
	{"vnormalb", 0},
	{"vertexa", 0},
	{"vertexb", 0},
	{"colormap", 1},
	{"skin", 1},
	{"ambient", 1},
	{"shadelight", 1},
	{"lightvec", 1},
	{"fog", 1},
};

static mat4f_t alias_vp;

void
glsl_R_InitAlias (void)
{
	shader_t   *vert_shader, *frag_shader;
	int         vert;
	int         frag;

	vert_shader = GLSL_BuildShader (alias_vert_effects);
	frag_shader = GLSL_BuildShader (alias_frag_effects);
	vert = GLSL_CompileShader ("quakemdl.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("quakemdl.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_mdl.program = GLSL_LinkProgram ("quakemdl", vert, frag);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.mvp_matrix);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.norm_matrix);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.skin_size);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.blend);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.colora);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.colorb);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.sta);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.stb);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.normala);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.normalb);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.vertexa);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.vertexb);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.colormap);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.skin);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.ambient);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.shadelight);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.lightvec);
	GLSL_ResolveShaderParam (quake_mdl.program, &quake_mdl.fog);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);
}

static void
calc_lighting (entity_t ent, float *ambient, float *shadelight,
			   vec3_t lightvec)
{
	unsigned    i;
	float       add;
	vec3_t      dist;
	int         light;

	transform_t transform = Entity_Transform (ent);
	vec4f_t     entorigin = Transform_GetWorldPosition (transform);

	VectorSet ( -1, 0, 0, lightvec);	//FIXME
	light = R_LightPoint (&r_refdef.worldmodel->brush, entorigin);
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	*ambient = max (light, max (renderer->model->min_light,
								renderer->min_light) * 128);
	*shadelight = *ambient;

	for (i = 0; i < r_maxdlights; i++) {
		if (r_dlights[i].die >= vr_data.realtime) {
			VectorSubtract (entorigin, r_dlights[i].origin, dist);
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

static void
set_arrays (const shaderparam_t *vert, const shaderparam_t *norm,
		    const shaderparam_t *st, aliasvrt_t *pose)
{
	byte       *pose_offs = (byte *) pose;

	if (developer & SYS_glsl) {
		GLint size;

		qfeglGetBufferParameteriv (GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
		if (size <= (intptr_t)pose_offs) {
			Sys_Printf ("Invalid pose");
			pose = 0;
		}
	}

	qfeglVertexAttribPointer (vert->location, 3, GL_UNSIGNED_SHORT,
							 0, sizeof (aliasvrt_t),
							 pose_offs + field_offset (aliasvrt_t, vertex));
	qfeglVertexAttribPointer (norm->location, 3, GL_SHORT,
							 1, sizeof (aliasvrt_t),
							 pose_offs + field_offset (aliasvrt_t, normal));
	qfeglVertexAttribPointer (st->location, 2, GL_SHORT,
							 0, sizeof (aliasvrt_t),
							 pose_offs + field_offset (aliasvrt_t, st));
}
//#define TETRAHEDRON
void
glsl_R_DrawAlias (entity_t ent)
{
#ifdef TETRAHEDRON
	static aliasvrt_t debug_verts[] = {
		{{  0,  0}, {-18918,-18918,-18918}, {  0,  0,  0}},
		{{  0,300}, { 18918, 18918,-18918}, {255,255,  0}},
		{{300,300}, {-18918, 18918, 18918}, {  0,255,255}},
		{{300,  0}, { 18918,-18918, 18918}, {255,  0,255}},
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
	float       blend;
	aliashdr_t *hdr;
	vec_t       norm_mat[9];
	int         skin_tex;
	int         colormap;
	aliasvrt_t *pose1 = 0;		// VBO's are null based
	aliasvrt_t *pose2 = 0;		// VBO's are null based
	mat4f_t     worldMatrix;

	calc_lighting (ent, &ambient, &shadelight, lightvec);

	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	model_t    *model = renderer->model;
	if (!(hdr = model->aliashdr))
		hdr = Cache_Get (&model->cache);

	transform_t transform = Entity_Transform (ent);
	Transform_GetWorldMatrix (transform, worldMatrix);
	// we need only the rotation for normals.
	VectorCopy (worldMatrix[0], norm_mat + 0);
	VectorCopy (worldMatrix[1], norm_mat + 3);
	VectorCopy (worldMatrix[2], norm_mat + 6);

	// ent model scaling and offset
	mat4f_t     mvp_mat = {
		{ hdr->mdl.scale[0], 0, 0, 0 },
		{ 0, hdr->mdl.scale[1], 0, 0 },
		{ 0, 0, hdr->mdl.scale[2], 0 },
		{ hdr->mdl.scale_origin[0], hdr->mdl.scale_origin[1],
		  hdr->mdl.scale_origin[2], 1 },
	};
	mmulf (mvp_mat, worldMatrix, mvp_mat);
	mmulf (mvp_mat, alias_vp, mvp_mat);

	animation_t *animation = Ent_GetComponent (ent.id, scene_animation,
											   ent.reg);
	colormap = glsl_colormap;
	if (renderer->skin && renderer->skin->auxtex)
		colormap = renderer->skin->auxtex;
	if (renderer->skin && renderer->skin->texnum) {
		skin_t     *skin = renderer->skin;
		skin_tex = skin->texnum;
	} else {
		maliasskindesc_t *skindesc;
		skindesc = R_AliasGetSkindesc (animation, renderer->skinnum, hdr);
		skin_tex = skindesc->texnum;
	}
	blend = R_AliasGetLerpedFrames (animation, hdr);

	pose1 += animation->pose1 * hdr->poseverts;
	pose2 += animation->pose2 * hdr->poseverts;

	skin_size[0] = hdr->mdl.skinwidth;
	skin_size[1] = hdr->mdl.skinheight;

	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglBindTexture (GL_TEXTURE_2D, colormap);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglBindTexture (GL_TEXTURE_2D, skin_tex);

#ifndef TETRAHEDRON
	qfeglBindBuffer (GL_ARRAY_BUFFER, hdr->posedata);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, hdr->commands);
#endif

	qfeglVertexAttrib4fv (quake_mdl.colora.location, color);
	qfeglVertexAttrib4fv (quake_mdl.colorb.location, color);
	qfeglUniform1f (quake_mdl.blend.location, blend);
	qfeglUniform1f (quake_mdl.ambient.location, ambient);
	qfeglUniform1f (quake_mdl.shadelight.location, shadelight);
	qfeglUniform3fv (quake_mdl.lightvec.location, 1, lightvec);
	qfeglUniform2fv (quake_mdl.skin_size.location, 1, skin_size);
	qfeglUniformMatrix4fv (quake_mdl.mvp_matrix.location, 1, false,
						   (vec_t*)&mvp_mat[0]);//FIXME
	qfeglUniformMatrix3fv (quake_mdl.norm_matrix.location, 1, false, norm_mat);

#ifndef TETRAHEDRON
	set_arrays (&quake_mdl.vertexa, &quake_mdl.normala, &quake_mdl.sta, pose1);
	set_arrays (&quake_mdl.vertexb, &quake_mdl.normalb, &quake_mdl.stb, pose2);
	qfeglDrawElements (GL_TRIANGLES, 3 * hdr->mdl.numtris,
					  GL_UNSIGNED_SHORT, 0);
#else
	set_arrays (&quake_mdl.vertexa, &quake_mdl.normala, &quake_mdl.sta,
				debug_verts);
	set_arrays (&quake_mdl.vertexb, &quake_mdl.normalb, &quake_mdl.stb,
				debug_verts);
	qfeglDrawElements (GL_TRIANGLES,
					  sizeof (debug_indices) / sizeof (debug_indices[0]),
					  GL_UNSIGNED_SHORT, debug_indices);
#endif
	if (!model->aliashdr)
		Cache_Release (&model->cache);
}

// All alias models are drawn in a batch, so avoid thrashing the gl state
void
glsl_R_AliasBegin (void)
{
	quat_t      fog;

	// pre-multiply the view and projection matricies
	mmulf (alias_vp, glsl_projection, glsl_view);

	qfeglUseProgram (quake_mdl.program);
	qfeglEnableVertexAttribArray (quake_mdl.vertexa.location);
	qfeglEnableVertexAttribArray (quake_mdl.vertexb.location);
	qfeglEnableVertexAttribArray (quake_mdl.normala.location);
	qfeglEnableVertexAttribArray (quake_mdl.normalb.location);
	qfeglEnableVertexAttribArray (quake_mdl.sta.location);
	qfeglEnableVertexAttribArray (quake_mdl.stb.location);
	qfeglDisableVertexAttribArray (quake_mdl.colora.location);
	qfeglDisableVertexAttribArray (quake_mdl.colorb.location);

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_mdl.fog.location, 1, fog);

	qfeglUniform1i (quake_mdl.colormap.location, 1);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglEnable (GL_TEXTURE_2D);

	qfeglUniform1i (quake_mdl.skin.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
}

void
glsl_R_AliasEnd (void)
{
	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

	qfeglDisableVertexAttribArray (quake_mdl.vertexa.location);
	qfeglDisableVertexAttribArray (quake_mdl.vertexb.location);
	qfeglDisableVertexAttribArray (quake_mdl.normala.location);
	qfeglDisableVertexAttribArray (quake_mdl.normalb.location);
	qfeglDisableVertexAttribArray (quake_mdl.sta.location);
	qfeglDisableVertexAttribArray (quake_mdl.stb.location);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);
}
