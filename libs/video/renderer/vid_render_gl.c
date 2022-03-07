/*
	vid_render_gl.c

	GL version of the renderer

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

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
#include "gl/namehack.h"

#include "QF/cvar.h"

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/GL/funcs.h"
#include "QF/GL/qf_draw.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_vid.h"

#include "mod_internal.h"
#include "r_cvar.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_gl.h"

#include "gl/namehack.h"

gl_ctx_t *gl_ctx;

static void
gl_vid_render_choose_visual (void *data)
{
	gl_ctx->choose_visual (gl_ctx);
}

static void
gl_vid_render_create_context (void *data)
{
	gl_ctx->create_context (gl_ctx);
}

static vid_model_funcs_t model_funcs = {
	sizeof (gltex_t),
	gl_Mod_LoadLighting,
	gl_Mod_SubdivideSurface,
	gl_Mod_ProcessTexture,

	Mod_LoadIQM,
	Mod_LoadAliasModel,
	Mod_LoadSpriteModel,

	gl_Mod_MakeAliasModelDisplayLists,
	gl_Mod_LoadSkin,
	gl_Mod_FinalizeAliasModel,
	gl_Mod_LoadExternalSkins,
	gl_Mod_IQMFinish,
	1,
	gl_Mod_SpriteLoadFrames,

	Skin_SetColormap,
	Skin_SetSkin,
	gl_Skin_SetupSkin,
	Skin_SetTranslation,
	gl_Skin_ProcessTranslation,
	gl_Skin_InitTranslations,
};

static void
gl_vid_render_init (void)
{
	if (!vr_data.vid->vid_internal->sw_context) {
		Sys_Error ("Sorry, OpenGL not supported by this program.");
	}
	gl_ctx = vr_data.vid->vid_internal->gl_context ();
	gl_ctx->init_gl = GL_Init_Common;
	gl_ctx->load_gl ();

	vr_data.vid->vid_internal->data = gl_ctx;
	vr_data.vid->vid_internal->set_palette = GL_SetPalette;
	vr_data.vid->vid_internal->choose_visual = gl_vid_render_choose_visual;
	vr_data.vid->vid_internal->create_context = gl_vid_render_create_context;

	vr_funcs = &gl_vid_render_funcs;
	m_funcs = &model_funcs;
}

static void
gl_vid_render_shutdown (void)
{
}

static void
gl_begin_frame (void)
{
	if (gl_ctx->begun) {
		gl_ctx->end_rendering ();
		gl_ctx->begun = 0;
	}

	//FIXME forces the status bar to redraw. needed because it does not fully
	//update in sw modes but must in gl mode
	vr_data.scr_copyeverything = 1;

	if (gl_clear->int_val) {
		qfglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	} else {
		qfglClear (GL_DEPTH_BUFFER_BIT);
	}

	gl_ctx->begun = 1;

	if (r_speeds->int_val) {
		gl_ctx->start_time = Sys_DoubleTime ();
		gl_ctx->brush_polys = 0;
		gl_ctx->alias_polys = 0;
	}

	GL_Set2D ();
	GL_DrawReset ();

	// draw any areas not covered by the refresh
	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - vr_data.lineadj);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
						vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
						vid.height - vr_data.lineadj);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear (r_refdef.vrect.x, 0,
						r_refdef.vrect.x + r_refdef.vrect.width,
						r_refdef.vrect.y);
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
						r_refdef.vrect.y + r_refdef.vrect.height,
						r_refdef.vrect.width,
						vid.height - vr_data.lineadj -
						(r_refdef.vrect.height + r_refdef.vrect.y));
	}

	gl_Fog_SetupFrame ();
}

static void
gl_render_view (void)
{
	// do 3D refresh drawing, and then update the screen
	gl_R_RenderView ();
}

static void
gl_set_2d (void)
{
	GL_Set2DScaled ();
}

static void
gl_end_frame (void)
{
	if (r_speeds->int_val) {
//		qfglFinish ();
		double      start_time = gl_ctx->start_time;
		double      end_time = Sys_DoubleTime ();
		Sys_MaskPrintf (SYS_dev, "%3i ms  %4i wpoly %4i epoly %4i parts\n",
						(int) ((end_time - start_time) * 1000),
						gl_ctx->brush_polys, gl_ctx->alias_polys,
						r_psystem.numparticles);
	}

	GL_FlushText ();
	qfglFlush ();

	if (gl_finish->int_val) {
		gl_ctx->end_rendering ();
		gl_ctx->begun = 0;
	}
}

vid_render_funcs_t gl_vid_render_funcs = {
	gl_vid_render_init,
	gl_Draw_Character,
	gl_Draw_String,
	gl_Draw_nString,
	gl_Draw_AltString,
	gl_Draw_ConsoleBackground,
	gl_Draw_Crosshair,
	gl_Draw_CrosshairAt,
	gl_Draw_TileClear,
	gl_Draw_Fill,
	gl_Draw_TextBox,
	gl_Draw_FadeScreen,
	gl_Draw_BlendScreen,
	gl_Draw_CachePic,
	gl_Draw_UncachePic,
	gl_Draw_MakePic,
	gl_Draw_DestroyPic,
	gl_Draw_PicFromWad,
	gl_Draw_Pic,
	gl_Draw_Picf,
	gl_Draw_SubPic,

	gl_SCR_CaptureBGR,

	gl_ParticleSystem,
	gl_R_Init,
	gl_R_ClearState,
	gl_R_LoadSkys,
	gl_R_NewMap,
	gl_R_LineGraph,
	gl_R_ViewChanged,
	gl_begin_frame,
	gl_render_view,
	gl_set_2d,
	gl_end_frame,
	&model_funcs
};

static general_funcs_t plugin_info_general_funcs = {
	.shutdown = gl_vid_render_shutdown,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.vid_render = &gl_vid_render_funcs,
};

static plugin_data_t plugin_info_data = {
	.general = &plugin_info_general_data,
	.vid_render = &vid_render_data,
};

static plugin_t plugin_info = {
	qfp_vid_render,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"GL Renderer",
	"Copyright (C) 1996-1997  Id Software, Inc.\n"
	"Copyright (C) 1999-2012  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(vid_render, gl)
{
	return &plugin_info;
}
