/*
	glsl_iqm.c

	GLSL IQM rendering

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/5/11

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

#define NH_DEFINE
#include "namehack.h"

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
#include "QF/GLSL/qf_iqm.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

static const char *iqm_vert_effects[] =
{
	"QuakeForge.Math.quaternion",
	"QuakeForge.Vertex.iqm",
	0
};

static const char *iqm_frag_effects[] =
{
	"QuakeForge.Fragment.fog",
	"QuakeForge.Fragment.iqm",
	0
};

typedef struct {
	shaderparam_t position;
	shaderparam_t color;
} lightpar_t;

#define MAX_IQM_LIGHTS 8
#define MAX_IQM_BONES 80

static struct {
	int         program;
	shaderparam_t mvp_matrix;
	shaderparam_t norm_matrix;
	shaderparam_t bonemats;
	shaderparam_t vcolor;
	shaderparam_t vweights;
	shaderparam_t vbones;
	shaderparam_t texcoord;
	shaderparam_t vtangent;
	shaderparam_t vnormal;
	shaderparam_t vposition;
	shaderparam_t ambient;
	lightpar_t    lights[MAX_IQM_LIGHTS];
	shaderparam_t texture;
	shaderparam_t normalmap;
	shaderparam_t fog;
} iqm_shader = {
	0,
	{"mvp_mat", 1},
	{"norm_mat", 1},
	{"bonemats", 1},
	{"vcolor", 0},
	{"vweights", 0},
	{"vbones", 0},
	{"texcoord", 0},
	{"vtangent", 0},
	{"vnormal", 0},
	{"vposition", 0},
	{"ambient", 1},
	{
		{{"lights[0].position", 1}, {"lights[0].color", 1}},
		{{"lights[1].position", 1}, {"lights[1].color", 1}},
		{{"lights[2].position", 1}, {"lights[2].color", 1}},
		{{"lights[3].position", 1}, {"lights[3].color", 1}},
		{{"lights[4].position", 1}, {"lights[4].color", 1}},
		{{"lights[5].position", 1}, {"lights[5].color", 1}},
		{{"lights[6].position", 1}, {"lights[6].color", 1}},
		{{"lights[7].position", 1}, {"lights[7].color", 1}},
	},
	{"texture", 1},
	{"normalmap", 1},
	{"fog", 1},
};

static struct va_attr_s {
	shaderparam_t *attr;
	GLint       size;
	GLenum      type;
	GLboolean   normalized;
} vertex_attribs[] = {
	{&iqm_shader.vposition, 3, GL_FLOAT, 0},
	{&iqm_shader.texcoord, 2, GL_FLOAT, 0},
	{&iqm_shader.vnormal,  3, GL_FLOAT, 0},
	{&iqm_shader.vtangent, 4, GL_FLOAT, 0},
	{&iqm_shader.vbones,   4, GL_UNSIGNED_BYTE, 0},
	{&iqm_shader.vweights, 4, GL_UNSIGNED_BYTE, 1},
	{&iqm_shader.vcolor,   4, GL_UNSIGNED_BYTE, 1},
};

static mat4f_t iqm_vp;

void
glsl_R_InitIQM (void)
{
	shader_t   *vert_shader, *frag_shader;
	int         vert;
	int         frag;
	int         i;

	vert_shader = GLSL_BuildShader (iqm_vert_effects);
	frag_shader = GLSL_BuildShader (iqm_frag_effects);
	vert = GLSL_CompileShader ("iqm.vert", vert_shader, GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("iqm.frag", frag_shader, GL_FRAGMENT_SHADER);
	iqm_shader.program = GLSL_LinkProgram ("iqm", vert, frag);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.mvp_matrix);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.norm_matrix);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.bonemats);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vcolor);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vweights);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vbones);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.texcoord);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vtangent);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vnormal);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vposition);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.ambient);
	for (i = 0; i < MAX_IQM_LIGHTS; i++) {
		GLSL_ResolveShaderParam (iqm_shader.program,
								 &iqm_shader.lights[i].position);
		GLSL_ResolveShaderParam (iqm_shader.program,
								 &iqm_shader.lights[i].color);
	}
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.texture);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.normalmap);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.fog);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);
}

static void
set_arrays (iqm_t *iqm)
{
	int         i;
	uint32_t    j;
	struct va_attr_s *attr;
	iqmvertexarray *va;
	for (i = 0, j = 0; i < iqm->num_arrays; i++) {
		va = &iqm->vertexarrays[i];
		if (va->type > IQM_COLOR)
			Sys_Error ("iqm: unknown array type");
		if (j > va->type)
			Sys_Error ("iqm: array order bogus");
		while (j < va->type)
			qfeglDisableVertexAttribArray (vertex_attribs[j++].attr->location);
		attr = &vertex_attribs[j++];
		qfeglEnableVertexAttribArray (attr->attr->location);
		qfeglVertexAttribPointer (attr->attr->location, attr->size,
								  attr->type, attr->normalized, iqm->stride,
								  iqm->vertices + va->offset);
								  //(byte *) 0 + va->offset);
	}
	while (j <= IQM_COLOR)
		qfeglDisableVertexAttribArray (vertex_attribs[j++].attr->location);
}

void
glsl_R_DrawIQM (entity_t *ent)
{
	static quat_t color = { 1, 1, 1, 1};
	model_t    *model = ent->renderer.model;
	iqm_t      *iqm = (iqm_t *) model->aliashdr;
	glsliqm_t  *glsl = (glsliqm_t *) iqm->extra_data;
	dlight_t   *lights[MAX_IQM_LIGHTS];
	int         i;
	vec_t       norm_mat[9];
	vec4f_t     entorigin;
	mat4f_t     mvp_mat;
	float       blend;
	iqmframe_t *frame;

	// we need only the rotation for normals.
	mat4f_t mat;
	Transform_GetWorldMatrix (ent->transform, mat);
	VectorCopy (mat[0], norm_mat + 0);
	VectorCopy (mat[1], norm_mat + 3);
	VectorCopy (mat[2], norm_mat + 6);
	entorigin = mat[3];
	mmulf (mvp_mat, iqm_vp, mat);

	R_LightPoint (&r_worldentity.renderer.model->brush, &entorigin[0]);//FIXME min_light?
	VectorScale (ambientcolor, 1/255.0, ambientcolor);
	R_FindNearLights (&entorigin[0], MAX_IQM_LIGHTS, lights);//FIXME

	blend = R_IQMGetLerpedFrames (ent, iqm);
	frame = R_IQMBlendFrames (iqm, ent->animation.pose1, ent->animation.pose2,
							  blend, 0);

	qfeglUniform3fv (iqm_shader.ambient.location, 1, ambientcolor);
	for (i = 0; i < MAX_IQM_LIGHTS; i++) {
		quat_t      val;
		lightpar_t *l = &iqm_shader.lights[i];
		if (!lights[i])
			break;
		VectorSubtract (lights[i]->origin, entorigin, val);
		val[3] = lights[i]->radius;
		qfeglUniform4fv (l->position.location, 1, val);
		qfeglUniform4fv (l->color.location, 1, lights[i]->color);
	}
	for (; i < MAX_IQM_LIGHTS; i++) {
		lightpar_t *l = &iqm_shader.lights[i];
		qfeglUniform4fv (l->position.location, 1, quat_origin);
		qfeglUniform4fv (l->color.location, 1, quat_origin);
	}
	qfeglBindBuffer (GL_ARRAY_BUFFER, glsl->vertex_array);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, glsl->element_array);

	qfeglUniformMatrix4fv (iqm_shader.mvp_matrix.location, 1, false,
						   &mvp_mat[0][0]);
	qfeglUniformMatrix3fv (iqm_shader.norm_matrix.location, 1, false,
						   norm_mat);
	qfeglUniformMatrix4fv (iqm_shader.bonemats.location, iqm->num_joints,
						   false, (float *) frame);
	qfeglVertexAttrib4fv (iqm_shader.vcolor.location, color);
	set_arrays (iqm);
	for (i = 0; i < iqm->num_meshes; i++) {
		qfeglActiveTexture (GL_TEXTURE0 + 0);
		qfeglBindTexture (GL_TEXTURE_2D, glsl->textures[i]);
		qfeglActiveTexture (GL_TEXTURE0 + 1);
		qfeglBindTexture (GL_TEXTURE_2D, glsl->normmaps[i]);
		qfeglDrawElements (GL_TRIANGLES, 3 * iqm->meshes[i].num_triangles,
						   GL_UNSIGNED_SHORT,
						   iqm->elements + 3 * iqm->meshes[i].first_triangle);
	}
}

// All iqm models are drawn in a batch, so avoid thrashing the gl state
void
glsl_R_IQMBegin (void)
{
	quat_t      fog;

	// pre-multiply the view and projection matricies
	mmulf (iqm_vp, glsl_projection, glsl_view);

	qfeglUseProgram (iqm_shader.program);

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
	qfeglUniform4fv (iqm_shader.fog.location, 1, fog);

	qfeglUniform1i (iqm_shader.texture.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);

	qfeglUniform1i (iqm_shader.normalmap.location, 1);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglEnable (GL_TEXTURE_2D);
}

void
glsl_R_IQMEnd (void)
{
	int         i;

	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

	for (i = 0; i <= IQM_COLOR; i++)
		qfeglDisableVertexAttribArray (vertex_attribs[i].attr->location);
}
