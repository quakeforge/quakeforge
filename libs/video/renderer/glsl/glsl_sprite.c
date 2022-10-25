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

#include "QF/scene/component.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

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
	shaderparam_t spritea;
	shaderparam_t spriteb;
	shaderparam_t palette;
	shaderparam_t matrix;
	shaderparam_t vertexa;
	shaderparam_t vertexb;
	shaderparam_t uvab;
	shaderparam_t colora;
	shaderparam_t colorb;
	shaderparam_t blend;
	shaderparam_t fog;
} quake_sprite = {
	0,
	{"spritea", 1},
	{"spriteb", 1},
	{"palette", 1},
	{"mvp_mat", 1},
	{"vertexa", 0},
	{"vertexb", 0},
	{"uvab", 0},
	{"vcolora", 0},
	{"vcolorb", 0},
	{"vblend", 0},
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
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.spritea);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.spriteb);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.palette);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.matrix);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.vertexa);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.vertexb);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.colora);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.colorb);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.uvab);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.blend);
	GLSL_ResolveShaderParam (quake_sprite.program, &quake_sprite.fog);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);
}

static void
R_GetSpriteFrames (entity_t ent, msprite_t *sprite, mspriteframe_t **frame1,
				   mspriteframe_t **frame2, float *blend)
{
	animation_t *animation = Ent_GetComponent (ent.id, scene_animation,
											   ent.reg);
	int         framenum = animation->frame;
	int         pose;
	int         i, numframes;
	float      *intervals;
	float       frame_interval;
	float       fullinterval, targettime, time;
	mspritegroup_t *group = 0;
	mspriteframedesc_t *framedesc;

	if (framenum >= sprite->numframes || framenum < 0)
		framenum = 0;

	framedesc = &sprite->frames[framenum];
	if (framedesc->type == SPR_SINGLE) {
		frame_interval = 0.1;
		pose = framenum;
	} else {
		group = framedesc->group;
		intervals = group->intervals;
		numframes = group->numframes;
		fullinterval = intervals[numframes - 1];

		time = vr_data.realtime + animation->syncbase;
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < numframes - 1; i++) {
			if (intervals[i] > targettime)
				break;
		}
		frame_interval = intervals[i];
		if (i)
			frame_interval = intervals[i - 1];
		pose = i;
	}

	//FIXME this will break if the sprite changes between single frames and
	//group frames.
	*blend = R_EntityBlend (animation, pose, frame_interval);
	if (group) {
		*frame1 = group->frames[animation->pose1];
		*frame2 = group->frames[animation->pose2];
	} else {
		*frame1 = sprite->frames[animation->pose1].frame;
		*frame2 = sprite->frames[animation->pose2].frame;
	}
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
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	msprite_t  *sprite = (msprite_t *) renderer->model->cache.data;
	mspriteframe_t *frame1, *frame2;
	float       blend;
	vec4f_t     cameravec = {};
	vec4f_t     spn = {}, sright = {}, sup = {};
	static quat_t color = { 1, 1, 1, 1};
	float       vertsa[6][3], vertsb[6][3];
	static float uvab[6][4] = {
		{ 0, 0, 0, 0 },
		{ 1, 0, 1, 0 },
		{ 1, 1, 1, 1 },
		{ 0, 0, 0, 0 },
		{ 1, 1, 1, 1 },
		{ 0, 1, 0, 1 },
	};

	transform_t transform = Entity_Transform (ent);
	vec4f_t     origin = Transform_GetWorldPosition (transform);
	cameravec = r_refdef.frame.position - origin;

	if (!R_BillboardFrame (transform, sprite->type, cameravec,
						   &sup, &sright, &spn)) {
		// the orientation is undefined so can't draw the sprite
		return;
	}

	R_GetSpriteFrames (ent, sprite, &frame1, &frame2, &blend);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglBindTexture (GL_TEXTURE_2D, frame1->gl_texturenum);

	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglBindTexture (GL_TEXTURE_2D, frame2->gl_texturenum);

	qfeglVertexAttrib4fv (quake_sprite.colora.location, color);
	qfeglVertexAttrib4fv (quake_sprite.colorb.location, color);
	qfeglVertexAttrib1f (quake_sprite.blend.location, blend);

	make_quad (frame1, origin, sright, sup, vertsa);
	make_quad (frame2, origin, sright, sup, vertsb);

	qfeglVertexAttribPointer (quake_sprite.vertexa.location, 3, GL_FLOAT,
							 0, 0, vertsa);
	qfeglVertexAttribPointer (quake_sprite.vertexb.location, 3, GL_FLOAT,
							 0, 0, vertsb);
	qfeglVertexAttribPointer (quake_sprite.uvab.location, 4, GL_FLOAT,
							 0, 0, uvab);
	qfeglDrawArrays (GL_TRIANGLES, 0, 6);
}

// All sprites are drawn in a batch, so avoid thrashing the gl state
void
glsl_R_SpriteBegin (void)
{
	mat4f_t     mat;
	quat_t      fog;

	qfeglUseProgram (quake_sprite.program);
	qfeglEnableVertexAttribArray (quake_sprite.vertexa.location);
	qfeglEnableVertexAttribArray (quake_sprite.vertexb.location);
	qfeglEnableVertexAttribArray (quake_sprite.uvab.location);
	qfeglDisableVertexAttribArray (quake_sprite.colora.location);
	qfeglDisableVertexAttribArray (quake_sprite.colorb.location);
	qfeglDisableVertexAttribArray (quake_sprite.blend.location);

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_sprite.fog.location, 1, fog);

	qfeglUniform1i (quake_sprite.spritea.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);

	qfeglUniform1i (quake_sprite.spriteb.location, 1);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglEnable (GL_TEXTURE_2D);

	qfeglUniform1i (quake_sprite.palette.location, 2);
	qfeglActiveTexture (GL_TEXTURE0 + 2);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);

	mmulf (mat, glsl_projection, glsl_view);
	qfeglUniformMatrix4fv (quake_sprite.matrix.location, 1, false, (vec_t*)&mat[0]);//FIXME
}

void
glsl_R_SpriteEnd (void)
{
	qfeglDisableVertexAttribArray (quake_sprite.vertexa.location);
	qfeglDisableVertexAttribArray (quake_sprite.vertexb.location);
	qfeglDisableVertexAttribArray (quake_sprite.uvab.location);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 1);
	qfeglDisable (GL_TEXTURE_2D);
	qfeglActiveTexture (GL_TEXTURE0 + 2);
	qfeglDisable (GL_TEXTURE_2D);
}
