/*
	glsl_main.c

	GLSL rendering

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_alias.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_draw.h"
#include "QF/GLSL/qf_fisheye.h"
#include "QF/GLSL/qf_iqm.h"
#include "QF/GLSL/qf_lightmap.h"
#include "QF/GLSL/qf_main.h"
#include "QF/GLSL/qf_particles.h"
#include "QF/GLSL/qf_sprite.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_warp.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_gl.h"

mat4f_t glsl_projection;
mat4f_t glsl_view;

static void
R_SetupView (void)
{
	static mat4f_t z_up = {
		{ 0, 0,  1, 0},
		{-1, 0,  0, 0},
		{ 0, 1,  0, 0},
		{ 0, 0,  0, 1},
	};

	mmulf (glsl_view, z_up, r_refdef.camera_inverse);

	qfeglEnable (GL_CULL_FACE);
	qfeglEnable (GL_DEPTH_TEST);
}

void
glsl_R_RenderEntities (entqueue_t *queue)
{
	int         begun;

	if (!r_drawentities)
		return;
#define RE_LOOP(type_name, Type) \
	do { \
		begun = 0; \
		for (size_t i = 0; i < queue->ent_queues[mod_##type_name].size; \
			 i++) { \
			entity_t   *ent = queue->ent_queues[mod_##type_name].a[i]; \
			if (!begun) { \
				glsl_R_##Type##Begin (); \
				begun = 1; \
			} \
			glsl_R_Draw##Type (ent); \
		} \
		if (begun) \
			glsl_R_##Type##End (); \
	} while (0)

	RE_LOOP (alias, Alias);
	RE_LOOP (iqm, IQM);
	RE_LOOP (sprite, Sprite);
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
	qfeglDepthRangef (0, 0.3);
	glsl_R_AliasBegin ();
	glsl_R_DrawAlias (ent);
	glsl_R_AliasEnd ();
	qfeglDepthRangef (0, 1);
}

void
glsl_R_RenderView (void)
{
	if (!r_refdef.worldmodel) {
		return;
	}

	memcpy (glsl_projection, glsl_ctx->projection, sizeof (mat4f_t));

	R_SetupView ();
	glsl_R_DrawWorld ();
	glsl_R_DrawSky ();
	if (vr_data.view_model) {
		R_DrawViewModel ();
	}
}

static void
glsl_R_TimeRefresh_f (void)
{
/* FIXME update for simd
	double      start, stop, time;
	int         i;

	glsl_ctx->end_rendering ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		glsl_R_RenderView ();
		glsl_ctx->end_rendering ();
	}

	stop = Sys_DoubleTime ();
	time = stop - start;
	Sys_Printf ("%g seconds (%g fps)\n", time, 128 / time);
*/
}

void
glsl_R_Init (void)
{
	Cmd_AddCommand ("timerefresh", glsl_R_TimeRefresh_f,
					"Test the current refresh rate for the current location.");
	R_Init_Cvars ();
	glsl_Draw_Init ();
	SCR_Init ();
	glsl_R_InitBsp ();
	glsl_R_InitAlias ();
	glsl_R_InitIQM ();
	glsl_R_InitSprites ();
	glsl_R_InitParticles ();
	glsl_InitFisheye ();
	glsl_InitWarp ();
	Skin_Init ();
}

void
glsl_R_NewScene (scene_t *scene)
{
	int         i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	r_refdef.worldmodel = scene->worldmodel;

	// Force a vis update
	r_refdef.viewleaf = NULL;
	R_MarkLeaves ();

	R_ClearParticles ();
	glsl_R_RegisterTextures (scene->models, scene->num_models);
	glsl_R_BuildLightmaps (scene->models, scene->num_models);
	glsl_R_BuildDisplayLists (scene->models, scene->num_models);
}

void
glsl_R_LineGraph (int x, int y, int *h_vals, int count, int height)
{
}

void
glsl_R_ClearState (void)
{
	r_refdef.worldmodel = 0;
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
