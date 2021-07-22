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

#define NH_DEFINE
#include "namehack.h"

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

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_alias.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_iqm.h"
#include "QF/GLSL/qf_lightmap.h"
#include "QF/GLSL/qf_sprite.h"
#include "QF/GLSL/qf_textures.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_gl.h"

mat4f_t glsl_projection;
mat4f_t glsl_view;

void
glsl_R_ViewChanged (void)
{
	float       aspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
	float       f = 1 / tan (r_refdef.fov_y * M_PI / 360);
	float       neard, fard;
	vec4f_t    *proj = glsl_projection;

	neard = r_nearclip->value;
	fard = r_farclip->value;

	// NOTE columns!
	proj[0] = (vec4f_t) { f / aspect, 0, 0, 0 };
	proj[1] = (vec4f_t) { 0, f, 0, 0 };
	proj[2] = (vec4f_t) { 0, 0, (fard + neard) / (neard - fard), -1 };
	proj[3] = (vec4f_t) { 0, 0, (2 * fard * neard) / (neard - fard), 0 };
}

void
glsl_R_SetupFrame (void)
{
	R_AnimateLight ();
	R_ClearEnts ();
	r_framecount++;

	VectorCopy (r_refdef.viewposition, r_origin);
	VectorCopy (qvmulf (r_refdef.viewrotation, (vec4f_t) { 1, 0, 0, 0 }), vpn);
	VectorCopy (qvmulf (r_refdef.viewrotation, (vec4f_t) { 0, -1, 0, 0 }), vright);
	VectorCopy (qvmulf (r_refdef.viewrotation, (vec4f_t) { 0, 0, 1, 0 }), vup);


	R_SetFrustum ();

	r_viewleaf = Mod_PointInLeaf (r_origin, r_worldentity.renderer.model);
}

static void
R_SetupView (void)
{
	float       x, y, w, h;
	static mat4f_t z_up = {
		{ 0, 0, -1, 0},
		{-1, 0,  0, 0},
		{ 0, 1,  0, 0},
		{ 0, 0,  0, 1},
	};
	vec4f_t     offset = { 0, 0, 0, 1 };

	x = r_refdef.vrect.x;
	y = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height));
	w = r_refdef.vrect.width;
	h = r_refdef.vrect.height;
	qfeglViewport (x, y, w, h);

	mat4fquat (glsl_view, qconjf (r_refdef.viewrotation));
	mmulf (glsl_view, z_up, glsl_view);
	offset = -r_refdef.viewposition;
	offset[3] = 1;
	glsl_view[3] = mvmulf (glsl_view, offset);

	qfeglEnable (GL_CULL_FACE);
	qfeglEnable (GL_DEPTH_TEST);
}

static void
R_RenderEntities (void)
{
	entity_t   *ent;
	int         begun;

	if (!r_drawentities->int_val)
		return;
#define RE_LOOP(type_name, Type) \
	do { \
		begun = 0; \
		for (ent = r_ent_queue; ent; ent = ent->next) { \
			if (ent->renderer.model->type != mod_##type_name) \
				continue; \
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
		|| !r_drawviewmodel->int_val
		|| !r_drawentities->int_val
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
	double      t[10] = {};
	int         speeds = r_speeds->int_val;

	if (speeds)
		t[0] = Sys_DoubleTime ();
	glsl_R_SetupFrame ();
	R_SetupView ();
	if (speeds)
		t[1] = Sys_DoubleTime ();
	R_MarkLeaves ();
	if (speeds)
		t[2] = Sys_DoubleTime ();
	R_PushDlights (vec3_origin);
	if (speeds)
		t[3] = Sys_DoubleTime ();
	glsl_R_DrawWorld ();
	if (speeds)
		t[4] = Sys_DoubleTime ();
	glsl_R_DrawSky ();
	if (speeds)
		t[5] = Sys_DoubleTime ();
	R_RenderEntities ();
	if (speeds)
		t[6] = Sys_DoubleTime ();
	glsl_R_DrawWaterSurfaces ();
	if (speeds)
		t[7] = Sys_DoubleTime ();
	glsl_R_DrawParticles ();
	if (speeds)
		t[8] = Sys_DoubleTime ();
	R_DrawViewModel ();
	if (speeds)
		t[9] = Sys_DoubleTime ();
	if (speeds) {
		Sys_Printf ("frame: %g, setup: %g, mark: %g, pushdl: %g, world: %g,"
					" sky: %g, ents: %g, water: %g, part: %g, view: %g\n",
					(t[9] - t[0]) * 1000, (t[1] - t[0]) * 1000,
					(t[2] - t[1]) * 1000, (t[3] - t[2]) * 1000,
					(t[4] - t[3]) * 1000, (t[5] - t[4]) * 1000,
					(t[6] - t[5]) * 1000, (t[7] - t[6]) * 1000,
					(t[8] - t[7]) * 1000, (t[9] - t[8]) * 1000);
	}
}

void
glsl_R_Init (void)
{
	Cmd_AddCommand ("pointfile", glsl_R_ReadPointFile_f,
					"Load a pointfile to determine map leaks.");
	Cmd_AddCommand ("timerefresh", glsl_R_TimeRefresh_f,
					"Test the current refresh rate for the current location.");
	R_Init_Cvars ();
	glsl_R_Particles_Init_Cvars ();
	glsl_Draw_Init ();
	SCR_Init ();
	glsl_R_InitBsp ();
	glsl_R_InitAlias ();
	glsl_R_InitIQM ();
	glsl_R_InitSprites ();
	glsl_R_InitParticles ();
	glsl_Fog_Init ();
	Skin_Init ();
}

void
glsl_R_NewMap (model_t *worldmodel, struct model_s **models, int num_models)
{
	int         i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.renderer.model = worldmodel;

	// Force a vis update
	r_viewleaf = NULL;
	R_MarkLeaves ();

	R_FreeAllEntities ();
	glsl_R_ClearParticles ();
	glsl_R_RegisterTextures (models, num_models);
	glsl_R_BuildLightmaps (models, num_models);
	glsl_R_BuildDisplayLists (models, num_models);
}

void
glsl_R_LineGraph (int x, int y, int *h_vals, int count, int height)
{
}

void
glsl_R_ClearState (void)
{
	R_ClearEfrags ();
	R_ClearDlights ();
	glsl_R_ClearParticles ();
}

void
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
