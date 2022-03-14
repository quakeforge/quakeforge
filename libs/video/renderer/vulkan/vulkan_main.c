/*
	vulkan_main.c

	Vulkan rendering

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/1/19

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

#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_bsp.h"
//#include "QF/Vulkan/qf_iqm.h"
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_main.h"
#include "QF/Vulkan/qf_particles.h"
#include "QF/Vulkan/qf_sprite.h"
//#include "QF/Vulkan/qf_textures.h"
#include "QF/Vulkan/renderpass.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

static void
setup_frame (vulkan_ctx_t *ctx)
{
	R_AnimateLight ();
	EntQueue_Clear (r_ent_queue);
	r_framecount++;

	R_SetFrustum ();

	vec4f_t     position = r_refdef.frame.position;
	r_refdef.viewleaf = Mod_PointInLeaf (&position[0], r_refdef.worldmodel);
}

static void
Vulkan_RenderEntities (qfv_renderframe_t *rFrame)
{
	if (!r_drawentities->int_val)
		return;
#define RE_LOOP(type_name, Type) \
	do { \
		int         begun = 0; \
		for (size_t i = 0; i < r_ent_queue->ent_queues[mod_##type_name].size; \
			 i++) { \
			entity_t   *ent = r_ent_queue->ent_queues[mod_##type_name].a[i]; \
			if (!begun) { \
				Vulkan_##Type##Begin (rFrame); \
				begun = 1; \
			} \
			/* hack the depth range to prevent view model */\
			/* from poking into walls */\
			if (ent == vr_data.view_model) { \
				Vulkan_AliasDepthRange (rFrame, 0, 0.3); \
			} \
			Vulkan_Draw##Type (ent, rFrame); \
			/* unhack in case the view_model is not the last */\
			if (ent == vr_data.view_model) { \
				Vulkan_AliasDepthRange (rFrame, 0, 1); \
			} \
		} \
		if (begun) \
			Vulkan_##Type##End (rFrame); \
	} while (0)

	RE_LOOP (alias, Alias);
	//RE_LOOP (iqm, IQM);
	RE_LOOP (sprite, Sprite);
}

static void
Vulkan_DrawViewModel (vulkan_ctx_t *ctx)
{
	entity_t   *ent = vr_data.view_model;
	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel->int_val
		|| !r_drawentities->int_val
		|| !ent->renderer.model)
		return;

	EntQueue_AddEntity (r_ent_queue, ent, ent->renderer.model->type);
}

void
Vulkan_RenderView (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	double      t[10] = {};
	int         speeds = r_speeds->int_val;

	if (!r_refdef.worldmodel) {
		return;
	}

	if (speeds)
		t[0] = Sys_DoubleTime ();
	setup_frame (ctx);
	if (speeds)
		t[1] = Sys_DoubleTime ();
	R_MarkLeaves ();
	if (speeds)
		t[2] = Sys_DoubleTime ();
	R_PushDlights (vec3_origin);
	if (speeds)
		t[3] = Sys_DoubleTime ();
	Vulkan_DrawWorld (rFrame);
	if (speeds)
		t[4] = Sys_DoubleTime ();
	Vulkan_DrawSky (rFrame);
	if (speeds)
		t[5] = Sys_DoubleTime ();
	Vulkan_DrawViewModel (ctx);
	Vulkan_RenderEntities (rFrame);
	if (speeds)
		t[6] = Sys_DoubleTime ();
	Vulkan_DrawWaterSurfaces (rFrame);
	if (speeds)
		t[7] = Sys_DoubleTime ();
	Vulkan_DrawParticles (ctx);
	if (speeds)
		t[8] = Sys_DoubleTime ();
	Vulkan_Bsp_Flush (ctx);
	if (speeds)
		t[9] = Sys_DoubleTime ();
	if (speeds) {
		double      total = (t[9]  - t[0]) * 1000;
		for (int i = 0; i < 9; i++) {
			t[i] = (t[i + 1] - t[i]) * 1000;
		}
		Sys_Printf ("frame: %g, setup: %g, mark: %g, pushdl: %g, world: %g,"
					" sky: %g, ents: %g, water: %g, flush: %g, part: %g\n",
					total,
					t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8]);
	}
}

void
Vulkan_NewMap (model_t *worldmodel, struct model_s **models, int num_models,
			   vulkan_ctx_t *ctx)
{
	int         i;

	for (i = 0; i < 256; i++) {
		d_lightstylevalue[i] = 264;		// normal light value
	}

	r_refdef.worldmodel = worldmodel;

	// Force a vis update
	r_refdef.viewleaf = NULL;
	R_MarkLeaves ();

	R_ClearParticles ();
	Vulkan_RegisterTextures (models, num_models, ctx);
	//Vulkan_BuildLightmaps (models, num_models, ctx);
	Vulkan_BuildDisplayLists (models, num_models, ctx);
	Vulkan_LoadLights (worldmodel, worldmodel->brush.entities, ctx);
}

/*void
glsl_R_LineGraph (int x, int y, int *h_vals, int count)
{
}*/

/*void
glsl_R_TimeRefresh_f (void)
{
	double      start, stop, time;
	int         i;

	glsl_ctx->end_rendering ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		Vulkan_RenderView (ctx);
		glsl_ctx->end_rendering ();
	}

	stop = Sys_DoubleTime ();
	time = stop - start;
	Sys_Printf ("%g seconds (%g fps)\n", time, 128 / time);
}*/
