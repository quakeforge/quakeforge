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

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_iqm.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

static const char iqm_vert[] =
#include "iqm.vc"
;

static const char iqm_frag[] =
#include "iqm.fc"
;

static struct {
	int         program;
	shaderparam_t mvp_matrix;
	shaderparam_t bonemats;
	shaderparam_t vcolor;
	shaderparam_t vweights;
	shaderparam_t vbones;
	shaderparam_t texcoord;
	shaderparam_t vtangent;
	shaderparam_t vnormal;
	shaderparam_t position;
	shaderparam_t fog;
} iqm_shader = {
	0,
	{"mvp_mat", 1},
	{"bonemats", 1},
	{"vcolor", 0},
	{"vweights", 0},
	{"vbones", 0},
	{"texcoord", 0},
	{"vtangent", 0},
	{"vnormal", 0},
	{"position", 0},
	{"fog", 1},
};

static struct va_attr_s {
	shaderparam_t *attr;
	GLint       size;
	GLenum      type;
	GLboolean   normalized;
} vertex_attribs[] = {
	{&iqm_shader.position, 3, GL_FLOAT, 0},
	{&iqm_shader.texcoord, 2, GL_FLOAT, 0},
	{&iqm_shader.vnormal,  3, GL_FLOAT, 0},
	{&iqm_shader.vtangent, 4, GL_FLOAT, 0},
	{&iqm_shader.vbones,   4, GL_UNSIGNED_BYTE, 0},
	{&iqm_shader.vweights, 4, GL_UNSIGNED_BYTE, 1},
	{&iqm_shader.vcolor,   4, GL_UNSIGNED_BYTE, 1},
};

static mat4_t iqm_vp;

void
glsl_R_InitIQM (void)
{
	int         vert;
	int         frag;

	vert = GLSL_CompileShader ("iqm.vert", iqm_vert, GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("iqm.frag", iqm_frag, GL_FRAGMENT_SHADER);
	iqm_shader.program = GLSL_LinkProgram ("iqm", vert, frag);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.mvp_matrix);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.bonemats);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vcolor);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vweights);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vbones);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.texcoord);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vtangent);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.vnormal);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.position);
	GLSL_ResolveShaderParam (iqm_shader.program, &iqm_shader.fog);
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
		attr = &vertex_attribs[j];
		qfeglEnableVertexAttribArray (attr->attr->location);
		qfeglVertexAttribPointer (attr->attr->location, attr->size,
								  attr->type, attr->normalized, iqm->stride,
								  (byte *) 0 + va->offset);
	}
	while (j <= IQM_COLOR)
		qfeglDisableVertexAttribArray (vertex_attribs[j++].attr->location);
}

void
glsl_R_DrawIQM (void)
{
	entity_t   *ent = currententity;
	model_t    *model = ent->model;
	iqm_t      *iqm = (iqm_t *) model->aliashdr;

	set_arrays (iqm);
}

// All iqm models are drawn in a batch, so avoid thrashing the gl state
void
glsl_R_IQMBegin (void)
{
	quat_t      fog;

	// pre-multiply the view and projection matricies
	Mat4Mult (glsl_projection, glsl_view, iqm_vp);

	qfeglUseProgram (iqm_shader.program);

	VectorCopy (glsl_Fog_GetColor (), fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	qfeglUniform4fv (iqm_shader.fog.location, 1, fog);
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
