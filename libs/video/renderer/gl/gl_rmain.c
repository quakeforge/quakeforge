/*
	gl_rmain.c

	(no description)

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/scene/entity.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_alias.h"
#include "QF/GL/qf_draw.h"
#include "QF/GL/qf_iqm.h"
#include "QF/GL/qf_particles.h"
#include "QF/GL/qf_rlight.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_sprite.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"
#include "varrays.h"
#include "vid_gl.h"

float       gl_r_world_matrix[16];
//FIXME static float r_base_world_matrix[16];

//vec3_t		gl_shadecolor;					// Ender (Extend) Colormod
float		gl_modelalpha;					// Ender (Extend) Alpha

void
glrmain_init (void)
{
	gldepthmin = 0;
	gldepthmax = 1;
	qfglDepthFunc (GL_LEQUAL);
	qfglDepthRange (gldepthmin, gldepthmax);

	gl_overbright_f (0, 0);
	gl_multitexture_f (0, 0);
}

void
gl_R_RotateForEntity (entity_t *e)
{
	mat4f_t     mat;
	Transform_GetWorldMatrix (e->transform, mat);
	qfglMultMatrixf ((vec_t*)&mat[0]);//FIXME
}

void
gl_R_RenderEntities (entqueue_t *queue)
{
	if (!r_drawentities)
		return;

	// LordHavoc: split into 3 loops to simplify state changes

	if (gl_mtex_active_tmus >= 2) {
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		qfglDisable (GL_TEXTURE_2D);
		qglActiveTexture (gl_mtex_enum + 0);
	}
	if (gl_affinemodels)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	if (gl_tess)
		qfglEnable (GL_PN_TRIANGLES_ATI);
	qfglEnable (GL_CULL_FACE);

	if (gl_vector_light) {
		qfglEnable (GL_LIGHTING);
		qfglEnable (GL_NORMALIZE);
	} else if (gl_tess) {
		qfglEnable (GL_NORMALIZE);
	}

	for (size_t i = 0; i < queue->ent_queues[mod_alias].size; i++) { \
		entity_t   *ent = queue->ent_queues[mod_alias].a[i]; \
		gl_R_DrawAliasModel (ent);
	}
	qfglColor3ubv (color_white);

	qfglDisable (GL_NORMALIZE);
	qfglDisable (GL_LIGHTING);

	if (gl_tess)
		qfglDisable (GL_PN_TRIANGLES_ATI);
	if (gl_affinemodels)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);
	if (gl_mtex_active_tmus >= 2) { // FIXME: Ugly, but faster than cleaning
									// up in every R_DrawAliasModel()!
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		if (gl_combine_capable && gl_overbright) {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE, gl_rgb_scale);
		} else {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		qfglDisable (GL_TEXTURE_2D);

		qglActiveTexture (gl_mtex_enum + 0);
	}

	for (size_t i = 0; i < queue->ent_queues[mod_iqm].size; i++) { \
		entity_t   *ent = queue->ent_queues[mod_iqm].a[i]; \
		gl_R_DrawIQMModel (ent);
	}
	qfglColor3ubv (color_white);

	qfglDisable (GL_CULL_FACE);
	qfglEnable (GL_ALPHA_TEST);
	if (gl_va_capable)
		qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, gl_spriteVertexArray);
	for (size_t i = 0; i < queue->ent_queues[mod_sprite].size; i++) { \
		entity_t   *ent = queue->ent_queues[mod_sprite].a[i]; \
		gl_R_DrawSpriteModel (ent);
	}
	qfglDisable (GL_ALPHA_TEST);
}

static void
R_DrawViewModel (void)
{
	entity_t   *ent = vr_data.view_model;
	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel
		|| !r_drawentities
		|| !ent->renderer.model)
		return;

	// hack the depth range to prevent view model from poking into walls
	qfglDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	qfglEnable (GL_CULL_FACE);

	if (gl_vector_light) {
		qfglEnable (GL_LIGHTING);
		qfglEnable (GL_NORMALIZE);
	} else if (gl_tess) {
		qfglEnable (GL_NORMALIZE);
	}

	if (gl_affinemodels)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	if (gl_mtex_active_tmus >= 2) {
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		qfglDisable (GL_TEXTURE_2D);
		qglActiveTexture (gl_mtex_enum + 0);
	}

	gl_R_DrawAliasModel (ent);

	qfglColor3ubv (color_white);
	if (gl_mtex_active_tmus >= 2) { // FIXME: Ugly, but faster than cleaning
									// up in every R_DrawAliasModel()!
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		if (gl_combine_capable && gl_overbright) {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE, gl_rgb_scale);
		} else {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		qfglDisable (GL_TEXTURE_2D);

		qglActiveTexture (gl_mtex_enum + 0);
	}
	if (gl_affinemodels)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);

	qfglDisable (GL_NORMALIZE);
	qfglDisable (GL_LIGHTING);


	qfglDisable (GL_CULL_FACE);
	qfglDepthRange (gldepthmin, gldepthmax);
}

static void
R_SetupGL (void)
{
	qfglMatrixMode (GL_PROJECTION);
	qfglLoadMatrixf ((vec_t*)&gl_ctx->projection[0]);//FIXME

	qfglFrontFace (GL_CW);

	qfglMatrixMode (GL_MODELVIEW);
	qfglLoadIdentity ();

	static mat4f_t z_up = {
		{ 0, 0,  1, 0},
		{-1, 0,  0, 0},
		{ 0, 1,  0, 0},
		{ 0, 0,  0, 1},
	};
	mat4f_t view;
	mmulf (view, z_up, r_refdef.camera_inverse);
	qfglLoadMatrixf ((vec_t*)&view[0]);//FIXME

	qfglGetFloatv (GL_MODELVIEW_MATRIX, gl_r_world_matrix);

	// set drawing parms
//	qfglEnable (GL_CULL_FACE);
	qfglDisable (GL_ALPHA_TEST);
	qfglAlphaFunc (GL_GREATER, 0.5);
	qfglEnable (GL_DEPTH_TEST);
	if (gl_dlight_smooth)
		qfglShadeModel (GL_SMOOTH);
	else
		qfglShadeModel (GL_FLAT);
}

void
gl_R_RenderView (void)
{
	if (r_norefresh) {
		return;
	}
	if (!r_refdef.worldmodel) {
		return;
	}

	R_SetupGL ();
	gl_Fog_EnableGFog ();

	gl_R_DrawWorld ();
	S_ExtraUpdate ();			// don't let sound get messed up if going slow
	gl_R_RenderDlights ();
	R_DrawViewModel ();

	gl_Fog_DisableGFog ();
}

void
gl_R_ClearState (void)
{
	r_refdef.worldmodel = 0;
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
