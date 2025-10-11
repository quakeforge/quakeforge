/*
	glsl_mesh.c

	GLSL mesh rendering

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>
	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_mesh.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "mod_internal.h"
#include "r_internal.h"

#define s_dynlight (r_refdef.scene->base + scene_dynlight)

static const char *mesh_vert_effects[] =
{
	"QuakeForge.Vertex.mdl",
	0
};

static const char *mesh_frag_effects[] =
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
} meshprog = {
	.mvp_matrix  = {"mvp_mat",    1},
	.norm_matrix = {"norm_mat",   1},
	.skin_size   = {"skin_size",  1},
	.blend       = {"blend",      1},
	.colora      = {"vcolora",    0},
	.colorb      = {"vcolorb",    0},
	.sta         = {"vsta",       0},
	.stb         = {"vstb",       0},
	.normala     = {"vnormala",   0},
	.normalb     = {"vnormalb",   0},
	.vertexa     = {"vertexa",    0},
	.vertexb     = {"vertexb",    0},
	.colormap    = {"colormap",   1},
	.skin        = {"skin",       1},
	.ambient     = {"ambient",    1},
	.shadelight  = {"shadelight", 1},
	.lightvec    = {"lightvec",   1},
	.fog         = {"fog",        1},
};

static shaderparam_t *mesh_attr_map[] = {
	[qfm_position] = &meshprog.vertexa,
	[qfm_normal]   = &meshprog.normala,
//	[qfm_tangent]  = &meshprog.tangenta,
	[qfm_texcoord] = &meshprog.sta,
	[qfm_color]    = &meshprog.colora,
//	[qfm_joints]   = &meshprog.jointsa,
//	[qfm_weights]  = &meshprog.weightsa,
	[qfm_weights]  = nullptr,
};

static struct {
	GLenum type;
	bool   norm;
} mesh_type_map[] = {
	[qfm_s8]   = { .type = GL_BYTE,           .norm = false },
	[qfm_s16]  = { .type = GL_SHORT,          .norm = false },
	[qfm_s32]  = { .type = GL_INT,            .norm = false },

	[qfm_u8]   = { .type = GL_UNSIGNED_BYTE,  .norm = false },
	[qfm_u16]  = { .type = GL_UNSIGNED_SHORT, .norm = false },
	[qfm_u32]  = { .type = GL_UNSIGNED_INT,   .norm = false },

	[qfm_s8n]  = { .type = GL_BYTE,           .norm = true  },
	[qfm_s16n] = { .type = GL_SHORT,          .norm = true  },
	[qfm_u8n]  = { .type = GL_UNSIGNED_BYTE,  .norm = true  },
	[qfm_u16n] = { .type = GL_UNSIGNED_SHORT, .norm = true  },

	[qfm_f16]  = { .type = GL_HALF_FLOAT,     .norm = false },
	[qfm_f32]  = { .type = GL_FLOAT,          .norm = false },
};

static mat4f_t mesh_vp;
static uint32_t attr_state;

void
glsl_R_InitMesh (void)
{
	shader_t   *vert_shader, *frag_shader;
	int         vert;
	int         frag;

	vert_shader = GLSL_BuildShader (mesh_vert_effects);
	frag_shader = GLSL_BuildShader (mesh_frag_effects);
	vert = GLSL_CompileShader ("quakemdl.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("quakemdl.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	meshprog.program = GLSL_LinkProgram ("quakemdl", vert, frag);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.mvp_matrix);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.norm_matrix);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.skin_size);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.blend);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.colora);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.colorb);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.sta);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.stb);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.normala);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.normalb);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.vertexa);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.vertexb);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.colormap);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.skin);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.ambient);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.shadelight);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.lightvec);
	GLSL_ResolveShaderParam (meshprog.program, &meshprog.fog);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);
}

static uint32_t
set_arrays (const qfm_attrdesc_t *attrs, uint32_t num_attr, int attr_offs,
		    mesh_vrt_t *pose)
{
	byte       *pose_offs = (byte *) pose;
	uint32_t    attr_mask = 0;

	if (developer & SYS_glsl) {
		GLint size;

		qfeglGetBufferParameteriv (GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
		if (size <= (intptr_t)pose_offs) {
			Sys_Printf ("Invalid pose");
			pose = 0;
		}
	}

	for (uint32_t i = 0; i < num_attr; i++) {
		if (attrs[i].attr >= qfm_attr_count || !mesh_attr_map[attrs[i].attr]) {
			// unsupported attribute
			continue;
		}
		attr_mask |= 1 << attrs[i].attr;
		auto param = mesh_attr_map[attrs[i].attr][attr_offs];
		auto type = mesh_type_map[attrs[i].type];
		qfeglVertexAttribPointer (param.location, attrs[i].components,
								  type.type, type.norm, attrs[i].stride,
								  pose_offs + attrs[i].offset);
	}
	return attr_mask;
}

static void
enable_attributes (uint32_t attr_mask)
{
	uint32_t diff = attr_mask ^ attr_state;
	if (!diff) {
		return;
	}

	for (uint32_t i = 0; i < qfm_attr_count; i++) {
		if (diff & (1 << i)) {
			auto param = mesh_attr_map[i];
			if (attr_mask & (1 << i)) {
				qfeglEnableVertexAttribArray (param[0].location);
				qfeglEnableVertexAttribArray (param[1].location);
			} else {
				qfeglDisableVertexAttribArray (param[0].location);
				qfeglDisableVertexAttribArray (param[1].location);
			}
		}
	}
	attr_state = attr_mask;
}

static GLuint
glsl_get_skin (renderer_t *renderer, qf_mesh_t *mesh)
{
	GLuint skin_tex = 0;
	if (renderer->skin) {
		skin_t     *skin = Skin_Get (renderer->skin);
		if (skin) {
			skin_tex = skin->id;
		}
	}
	if (!skin_tex) {
		skin_tex = renderer->skindesc;
		if (!skin_tex) {
			if (!mesh->skin.numclips) {
				return 0;
			}
			auto skinframe = (keyframe_t *) ((byte *) mesh
											 + mesh->skin.keyframes);
			skin_tex = skinframe->data;
		}
	}
	return skin_tex;
}

void
glsl_R_DrawMesh (entity_t ent)
{
	static quat_t color = { 1, 1, 1, 1};
	float       skin_size[2];
	vec_t       norm_mat[9];
	mat4f_t     worldMatrix;
	alight_t    lighting;

	R_Setup_Lighting (ent, &lighting);

	auto renderer = Entity_GetRenderer (ent);
	if (renderer->onlyshadows) {
		return;
	}
	auto model = renderer->model->model;
	auto meshes = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
	auto rmesh = (glsl_mesh_t *) ((byte *) model + model->render_data);

	auto text = (const char *) ((byte *) model + model->text.offset);
	auto model_name = renderer->model->name;
	qfeglPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0,
						 strlen (model_name), model_name);

	transform_t transform = Entity_Transform (ent);
	Transform_GetWorldMatrix (transform, worldMatrix);
	// we need only the rotation for normals.
	VectorCopy (worldMatrix[0], norm_mat + 0);
	VectorCopy (worldMatrix[1], norm_mat + 3);
	VectorCopy (worldMatrix[2], norm_mat + 6);

	qfeglBindBuffer (GL_ARRAY_BUFFER, rmesh->vertices);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, rmesh->indices);

	auto animation = Entity_GetAnimation (ent);
	float blend = animation->blend;

	qfeglVertexAttrib4fv (meshprog.colora.location, color);
	qfeglVertexAttrib4fv (meshprog.colorb.location, color);
	qfeglUniform1f (meshprog.blend.location, blend);
	qfeglUniform1f (meshprog.ambient.location, lighting.ambientlight);
	qfeglUniform1f (meshprog.shadelight.location, lighting.shadelight);
	qfeglUniform3fv (meshprog.lightvec.location, 1, lighting.lightvec);
	qfeglUniformMatrix3fv (meshprog.norm_matrix.location, 1, false, norm_mat);

	GLenum index_type = GL_UNSIGNED_INT;
	if (meshes[0].index_type == qfm_u8) {
		index_type = GL_UNSIGNED_BYTE;
	} else if (meshes[0].index_type == qfm_u16) {
		index_type = GL_UNSIGNED_SHORT;
	}
	// ent model scaling and offset
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		if (renderer->submesh_mask & (1 << i)) {
			continue;
		}
		auto mesh_name = text + meshes[i].name;
		if (!meshes[i].name) {
			mesh_name = va ("meshes[%d]", i);
		}
		qfeglPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0,
							 strlen (mesh_name), mesh_name);
		mat4f_t     mvp_mat = {
			{ meshes[i].scale[0], 0, 0, 0 },
			{ 0, meshes[i].scale[1], 0, 0 },
			{ 0, 0, meshes[i].scale[2], 0 },
			{ VectorExpand (meshes[i].scale_origin), 1 },
		};
		mmulf (mvp_mat, worldMatrix, mvp_mat);
		mmulf (mvp_mat, mesh_vp, mvp_mat);
		qfeglUniformMatrix4fv (meshprog.mvp_matrix.location, 1, false,
							   (vec_t*)&mvp_mat[0]);//FIXME

		GLuint cmap_tex = glsl_colormap;
		auto colormap = Entity_GetColormap (ent);
		if (colormap) {
			cmap_tex = glsl_Skin_Colormap (colormap);
		}

		GLuint skin_tex = glsl_get_skin (renderer, &meshes[i]);;

		skin_size[0] = rmesh->skinwidth;
		skin_size[1] = rmesh->skinheight;
		qfeglUniform2fv (meshprog.skin_size.location, 1, skin_size);

		qfeglActiveTexture (GL_TEXTURE0 + 1);
		qfeglBindTexture (GL_TEXTURE_2D, cmap_tex);
		qfeglActiveTexture (GL_TEXTURE0 + 0);
		qfeglBindTexture (GL_TEXTURE_2D, skin_tex);

		void *pose1 = nullptr;
		void *pose2 = nullptr;

		if (meshes[i].morph.numclips) {
			pose1 = (void *) (intptr_t) animation->pose1;
			pose2 = (void *) (intptr_t) animation->pose2;
		}

		auto attr = (qfm_attrdesc_t *) ((byte *) &meshes[i]
										+ meshes[i].attributes.offset);
		uint32_t attr_mask = 0;
		attr_mask |= set_arrays (attr, meshes[i].attributes.count, 0, pose1);
		attr_mask |= set_arrays (attr, meshes[i].attributes.count, 1, pose2);
		enable_attributes (attr_mask);

		uint32_t index_count = 3 * meshes[i].triangle_count;
		uint32_t first_index = meshes[i].indices;
		qfeglDrawElements (GL_TRIANGLES, index_count, index_type,
						   (void *) (uintptr_t) first_index);

		qfeglPopDebugGroup ();
	}
	qfeglPopDebugGroup ();
}

// All mesh models are drawn in a batch, so avoid thrashing the gl state
void
glsl_R_MeshBegin (void)
{
	quat_t      fog;

	// pre-multiply the view and projection matricies
	mmulf (mesh_vp, glsl_projection, glsl_view);

	qfeglUseProgram (meshprog.program);
	qfeglDisableVertexAttribArray (meshprog.vertexa.location);
	qfeglDisableVertexAttribArray (meshprog.vertexb.location);
	qfeglDisableVertexAttribArray (meshprog.normala.location);
	qfeglDisableVertexAttribArray (meshprog.normalb.location);
	qfeglDisableVertexAttribArray (meshprog.sta.location);
	qfeglDisableVertexAttribArray (meshprog.stb.location);
	qfeglDisableVertexAttribArray (meshprog.colora.location);
	qfeglDisableVertexAttribArray (meshprog.colorb.location);
	attr_state = 0;

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
	qfeglUniform4fv (meshprog.fog.location, 1, fog);

	qfeglUniform1i (meshprog.colormap.location, 1);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglEnable (GL_TEXTURE_2D);

	qfeglUniform1i (meshprog.skin.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
}

void
glsl_R_MeshEnd (void)
{
	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

	qfeglDisableVertexAttribArray (meshprog.vertexa.location);
	qfeglDisableVertexAttribArray (meshprog.vertexb.location);
	qfeglDisableVertexAttribArray (meshprog.normala.location);
	qfeglDisableVertexAttribArray (meshprog.normalb.location);
	qfeglDisableVertexAttribArray (meshprog.sta.location);
	qfeglDisableVertexAttribArray (meshprog.stb.location);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);
}
