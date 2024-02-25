/*
	glsl_sprite.c

	Sprite drawing support for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/30

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

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/scene/entity.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_sprite.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

static const char *sprite_vert_effects[] =
{
	"QuakeForge.Vertex.sprite",
	0
};

static const char *sprite_frag_effects[] =
{
	"QuakeForge.Fragment.fog",
	"QuakeForge.Fragment.palette",
	"QuakeForge.Fragment.sprite",
	0
};

//static float proj_matrix[16];

static struct {
	int         program;
	shaderparam_t sprite;
	shaderparam_t palette;
	shaderparam_t matrix;
	shaderparam_t vertex;
	shaderparam_t uv;
	shaderparam_t color;
	shaderparam_t fog;
} quake_sprite = {
	0,
	{"sprite", 1},
	{"palette", 1},
	{"mvp_mat", 1},
	{"vertex", 0},
	{"uv", 0},
	{"vcolor", 0},
	{"fog", 1},
};

void
glsl_R_InitSprites (void)
{
	shader_t   *vert_shader, *frag_shader;
	int         frag, vert;

	vert_shader = GLSL_BuildShader (sprite_vert_effects);
	frag_shader = GLSL_BuildShader (sprite_frag_effects);
	vert = GLSL_CompileShader ("quakespr.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("quakespr.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_sprite.program = GLSL_LinkProgram ("quakespr", vert, frag);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.sprite);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.palette);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.matrix);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.vertex);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.color);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.uv);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.fog);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);
}

static void
make_quad (mspriteframe_t *frame, vec4f_t origin, vec4f_t sright, vec4f_t sup, float verts[6][3])
{
	vec4f_t     left, up, right, down;
	vec4f_t     ul, ur, ll, lr;

	// build the sprite poster in worldspace
	// first, rotate the sprite axes into world space
	right = frame->right * sright;
	up = frame->up * sup;
	left = frame->left * sright;
	down = frame->down * sup;
	// next, build the sprite corners from the axes
	ul = up + left;
	ur = up + right;
	ll = down + left;
	lr = down + right;
	// finally, translate the sprite corners, creating two triangles
	VectorAdd (origin, ul, verts[0]);	// first triangle
	VectorAdd (origin, ur, verts[1]);
	VectorAdd (origin, lr, verts[2]);
	VectorAdd (origin, ul, verts[3]);	// second triangle
	VectorAdd (origin, lr, verts[4]);
	VectorAdd (origin, ll, verts[5]);
}

void
glsl_R_DrawSprite (entity_t ent)
{
	auto renderer = Entity_GetRenderer (ent);
	msprite_t  *sprite = (msprite_t *) renderer->model->cache.data;
	vec4f_t     cameravec = {};
	vec4f_t     spn = {}, sright = {}, sup = {};
	static quat_t color = { 1, 1, 1, 1};
	float       verts[6][3];
	static float uv[6][2] = {
		{ 0, 0, }, { 1, 0, }, { 1, 1, },
		{ 0, 0, }, { 1, 1, }, { 0, 1, },
	};

	transform_t transform = Entity_Transform (ent);
	vec4f_t     origin = Transform_GetWorldPosition (transform);
	cameravec = r_refdef.frame.position - origin;

	if (!R_BillboardFrame (transform, sprite->type, cameravec,
						   &sup, &sright, &spn)) {
		// the orientation is undefined so can't draw the sprite
		return;
	}

	auto animation = Entity_GetAnimation (ent);
	auto frame = R_GetSpriteFrame (sprite, animation);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	auto texnum = (GLuint *) &frame[1];
	qfeglBindTexture (GL_TEXTURE_2D, *texnum);

	qfeglVertexAttrib4fv (quake_sprite.color.location, color);

	make_quad (frame, origin, sright, sup, verts);

	qfeglVertexAttribPointer (quake_sprite.vertex.location, 3, GL_FLOAT,
							  0, 0, verts);
	qfeglVertexAttribPointer (quake_sprite.uv.location, 2, GL_FLOAT, 0, 0, uv);
	qfeglDrawArrays (GL_TRIANGLES, 0, 6);
}

// All sprites are drawn in a batch, so avoid thrashing the gl state
void
glsl_R_SpriteBegin (void)
{
	mat4f_t     mat;
	quat_t      fog;

	qfeglUseProgram (quake_sprite.program);
	qfeglEnableVertexAttribArray (quake_sprite.vertex.location);
	qfeglEnableVertexAttribArray (quake_sprite.uv.location);
	qfeglDisableVertexAttribArray (quake_sprite.color.location);

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_sprite.fog.location, 1, fog);

	qfeglUniform1i (quake_sprite.sprite.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);

	qfeglUniform1i (quake_sprite.palette.location, 1);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);

	mmulf (mat, glsl_projection, glsl_view);
	qfeglUniformMatrix4fv (quake_sprite.matrix.location, 1, false, (vec_t*)&mat[0]);//FIXME
}

void
glsl_R_SpriteEnd (void)
{
	qfeglDisableVertexAttribArray (quake_sprite.vertex.location);
	qfeglDisableVertexAttribArray (quake_sprite.uv.location);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);
}
