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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

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

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

static const char quakesprite_vert[] =
#include "quakespr.vc"
;

static const char quakesprite_frag[] =
#include "quakespr.fc"
;

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

VISIBLE void
R_InitSprites (void)
{
	int         frag, vert;

	vert = GLSL_CompileShader ("quakespr.vert", quakesprite_vert,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("quakespr.frag", quakesprite_frag,
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
}

static void
R_GetSpriteFrames (entity_t *ent, msprite_t *sprite, mspriteframe_t **frame1,
				   mspriteframe_t **frame2, float *blend)
{
	int         framenum = currententity->frame;
	int         pose;
	int         i, numframes;
	float      *intervals;
	float       frame_interval;
	float       fullinterval, targettime, time;
	mspritegroup_t *group = 0;
	mspriteframedesc_t *framedesc;

	if (framenum >= sprite->numframes || framenum < 0)
		framenum = 0;

	numframes = sprite->numframes;
	framedesc = &sprite->frames[framenum];
	if (framedesc->type == SPR_SINGLE) {
		frame_interval = 0.1;
		pose = framenum;
	} else {
		group = (mspritegroup_t *) framedesc->frameptr;
		intervals = group->intervals;
		numframes = group->numframes;
		fullinterval = intervals[numframes - 1];

		time = vr_data.realtime + currententity->syncbase;
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
	*blend = R_EntityBlend (ent, pose, frame_interval);
	if (group) {
		*frame1 = group->frames[ent->pose1];
		*frame2 = group->frames[ent->pose2];
	} else {
		*frame1 = sprite->frames[ent->pose1].frameptr;
		*frame2 = sprite->frames[ent->pose2].frameptr;
	}
}

static void
make_quad (mspriteframe_t *frame, const vec3_t vpn, const vec3_t vright,
		   const vec3_t vup, float verts[6][3])
{
	vec3_t      left, up, right, down;
	vec3_t      ul, ur, ll, lr;

	// build the sprite poster in worldspace
	// first, rotate the sprite axes into world space
	VectorScale (vright, frame->right, right);
	VectorScale (vup, frame->up, up);
	VectorScale (vright, frame->left, left);
	VectorScale (vup, frame->down, down);
	// next, build the sprite corners from the axes
	VectorAdd (up, left, ul);
	VectorAdd (up, right, ur);
	VectorAdd (down, left, ll);
	VectorAdd (down, right, lr);
	// finally, translate the sprite corners, creating two triangles
	VectorAdd (currententity->origin, ul, verts[0]);	// first triangle
	VectorAdd (currententity->origin, ur, verts[1]);
	VectorAdd (currententity->origin, lr, verts[2]);
	VectorAdd (currententity->origin, ul, verts[3]);	// second triangle
	VectorAdd (currententity->origin, lr, verts[4]);
	VectorAdd (currententity->origin, ll, verts[5]);
}

void
R_DrawSprite (void)
{
	entity_t   *ent = currententity;
	msprite_t  *sprite = (msprite_t *) ent->model->cache.data;
	mspriteframe_t *frame1, *frame2;
	float       blend, sr, cr, dot, angle;
	vec3_t      tvec;
	vec3_t      svpn, svright, svup;
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

	switch (sprite->type) {
		case SPR_FACING_UPRIGHT:
			// generate the sprite's exes with svup straight up in worldspace
			// and svright perpendicular to r_origin. This will not work if the
			// view direction is very close to straight up or down because the
			// cross product will be between two nearly parallel vectors and
			// starts to approach an undefined staate, so we don't draw if the
			// two vectors are less than 1 degree apart
			VectorNegate (r_origin, tvec);
			VectorNormalize (tvec);
			dot = tvec[2];			// same as DotProcut (tvec, svup) because
									// svup is 0, 0, 1
			if ((dot > 0.999848) || (dot < -0.99848)) // cos (1 degree)
				return;
			VectorSet (0, 0, 1, svup);
			// CrossProduct (svup, -r_origin, svright)
			VectorSet (tvec[1], -tvec[0], 0, svright);
			VectorNormalize (svright);
			// CrossProduct (svright, svup, svpn);
			VectorSet (-svright[1], svright[0], 0, svpn);
			break;
		case SPR_VP_PARALLEL:
			// generate the prite's axes completely parallel to the viewplane.
			// There are no problem situations, because the prite is always in
			// the same position relative to the viewer.
			VectorCopy (vup, svup);
			VectorCopy (vright, svright);
			VectorCopy (vpn, svpn);
			break;
		case SPR_VP_PARALLEL_UPRIGHT:
			// generate the sprite's axes with svup straight up in worldspace,
			// and svright parallel to the viewplane. This will not work if the
			// view diretion iss very close to straight up or down because the
			// cross prodcut will be between two nearly parallel vectors and
			// starts to approach an undefined state, so we don't draw if the
			// two vectros are less that 1 degree apart
			dot = vpn[2];
			if ((dot > 0.999848) || (dot < -0.99848)) // cos (1 degree)
				return;
			VectorSet (0, 0, 1, svup);
			// CrossProduct (svup, -r_origin, svright)
			VectorSet (vpn[1], -vpn[0], 0, svright);
			VectorNormalize (svright);
			// CrossProduct (svright, svup, svpn);
			VectorSet (-svright[1], svright[0], 0, svpn);
			break;
		case SPR_ORIENTED:
			// generate the prite's axes according to the sprite's world
			// orientation
			VectorCopy (currententity->transform + 0, svpn);
			VectorNegate (currententity->transform + 4, svright);
			VectorCopy (currententity->transform + 8, svup);
			break;
		case SPR_VP_PARALLEL_ORIENTED:
			// generate the sprite's axes parallel to the viewplane, but
			// rotated in that plane round the center according to the sprite
			// entity's roll angle. Thus svpn stays the same, but svright and
			// svup rotate
			angle = currententity->angles[ROLL] * (M_PI / 180);
			sr = sin (angle);
			cr = cos (angle);
			VectorCopy (vpn, svpn);
			VectorScale (vright, cr, svright);
			VectorMultAdd (svright, sr, vup, svright);
			VectorScale (vup, cr, svup);
			VectorMultAdd (svup, -sr, vright, svup);
			break;
		default:
			Sys_Error ("R_DrawSprite: Bad sprite type %d", sprite->type);
	}

	R_GetSpriteFrames (ent, sprite, &frame1, &frame2, &blend);

	qfglActiveTexture (GL_TEXTURE0 + 0);
	qfglBindTexture (GL_TEXTURE_2D, frame1->gl_texturenum);

	qfglActiveTexture (GL_TEXTURE0 + 1);
	qfglBindTexture (GL_TEXTURE_2D, frame2->gl_texturenum);

	qfglVertexAttrib4fv (quake_sprite.colora.location, color);
	qfglVertexAttrib4fv (quake_sprite.colorb.location, color);
	qfglVertexAttrib1f (quake_sprite.blend.location, blend);

	make_quad (frame1, svpn, svright, svup, vertsa);
	make_quad (frame2, svpn, svright, svup, vertsb);

	qfglVertexAttribPointer (quake_sprite.vertexa.location, 3, GL_FLOAT,
							 0, 0, vertsa);
	qfglVertexAttribPointer (quake_sprite.vertexb.location, 3, GL_FLOAT,
							 0, 0, vertsb);
	qfglVertexAttribPointer (quake_sprite.uvab.location, 4, GL_FLOAT,
							 0, 0, uvab);
	qfglDrawArrays (GL_TRIANGLES, 0, 6);
}

// All sprites are drawn in a batch, so avoid thrashing the gl state
void
R_SpriteBegin (void)
{
	mat4_t      mat;
	quat_t      fog;

	qfglUseProgram (quake_sprite.program);
	qfglEnableVertexAttribArray (quake_sprite.vertexa.location);
	qfglEnableVertexAttribArray (quake_sprite.vertexb.location);
	qfglEnableVertexAttribArray (quake_sprite.uvab.location);
	qfglDisableVertexAttribArray (quake_sprite.colora.location);
	qfglDisableVertexAttribArray (quake_sprite.colorb.location);
	qfglDisableVertexAttribArray (quake_sprite.blend.location);

	VectorCopy (Fog_GetColor (), fog);
	fog[3] = Fog_GetDensity () / 64.0;
	qfglUniform4fv (quake_sprite.fog.location, 1, fog);

	qfglUniform1i (quake_sprite.spritea.location, 0);
	qfglActiveTexture (GL_TEXTURE0 + 0);
	qfglEnable (GL_TEXTURE_2D);

	qfglUniform1i (quake_sprite.spriteb.location, 1);
	qfglActiveTexture (GL_TEXTURE0 + 1);
	qfglEnable (GL_TEXTURE_2D);

	qfglUniform1i (quake_sprite.palette.location, 2);
	qfglActiveTexture (GL_TEXTURE0 + 2);
	qfglEnable (GL_TEXTURE_2D);
	qfglBindTexture (GL_TEXTURE_2D, glsl_palette);

	Mat4Mult (glsl_projection, glsl_view, mat);
	qfglUniformMatrix4fv (quake_sprite.matrix.location, 1, false, mat);
}

void
R_SpriteEnd (void)
{
	qfglDisableVertexAttribArray (quake_sprite.vertexa.location);
	qfglDisableVertexAttribArray (quake_sprite.vertexb.location);
	qfglDisableVertexAttribArray (quake_sprite.uvab.location);

	qfglActiveTexture (GL_TEXTURE0 + 0);
	qfglDisable (GL_TEXTURE_2D);
	qfglActiveTexture (GL_TEXTURE0 + 1);
	qfglDisable (GL_TEXTURE_2D);
	qfglActiveTexture (GL_TEXTURE0 + 2);
	qfglDisable (GL_TEXTURE_2D);
}
