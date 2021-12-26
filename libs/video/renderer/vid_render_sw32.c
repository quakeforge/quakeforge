/*
	vid_render_gl.c

	SW32 version of the renderer

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
#include "sw32/namehack.h"

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

#include "sw32/namehack.h"

sw_ctx_t *sw32_ctx;

static void
sw32_vid_render_choose_visual (void *data)
{
    sw32_ctx->choose_visual (sw32_ctx);
}

static void
sw32_vid_render_create_context (void *data)
{
    sw32_ctx->create_context (sw32_ctx);
}

static void
sw32_vid_render_set_palette (void *data, const byte *palette)
{
    sw32_ctx->set_palette (sw32_ctx, palette);
}

static vid_model_funcs_t model_funcs = {
	0,
	sw_Mod_LoadLighting,
	0,//Mod_SubdivideSurface,
	0,//Mod_ProcessTexture,

	Mod_LoadIQM,
	Mod_LoadAliasModel,
	Mod_LoadSpriteModel,

	sw_Mod_MakeAliasModelDisplayLists,
	sw_Mod_LoadSkin,
	0,
	0,
	sw_Mod_IQMFinish,
	1,
	sw_Mod_SpriteLoadFrames,

	Skin_SetColormap,
	Skin_SetSkin,
	sw_Skin_SetupSkin,
	Skin_SetTranslation,
	sw_Skin_ProcessTranslation,
	sw_Skin_InitTranslations,
};

static void
sw32_vid_render_init (void)
{
	if (!vr_data.vid->vid_internal->sw_context) {
		Sys_Error ("Sorry, software rendering not supported by this program.");
	}
	sw32_ctx = vr_data.vid->vid_internal->sw32_context ();

	vr_data.vid->vid_internal->data = sw32_ctx;
	vr_data.vid->vid_internal->set_palette = sw32_vid_render_set_palette;
	vr_data.vid->vid_internal->choose_visual = sw32_vid_render_choose_visual;
	vr_data.vid->vid_internal->create_context = sw32_vid_render_create_context;

	vr_funcs = &sw32_vid_render_funcs;
	m_funcs = &model_funcs;
}

static void
sw32_vid_render_shutdown (void)
{
}

vid_render_funcs_t sw32_vid_render_funcs = {
	sw32_vid_render_init,
	sw32_Draw_Character,
	sw32_Draw_String,
	sw32_Draw_nString,
	sw32_Draw_AltString,
	sw32_Draw_ConsoleBackground,
	sw32_Draw_Crosshair,
	sw32_Draw_CrosshairAt,
	sw32_Draw_TileClear,
	sw32_Draw_Fill,
	sw32_Draw_FillRGBA,
	sw32_Draw_TextBox,
	sw32_Draw_FadeScreen,
	sw32_Draw_BlendScreen,
	sw32_Draw_CachePic,
	sw32_Draw_UncachePic,
	sw32_Draw_MakePic,
	sw32_Draw_DestroyPic,
	sw32_Draw_PicFromWad,
	sw32_Draw_Pic,
	sw32_Draw_Picf,
	sw32_Draw_SubPic,

	SCR_SetFOV,
	SCR_DrawRam,
	SCR_DrawTurtle,
	SCR_DrawPause,
	sw32_SCR_CaptureBGR,
	sw32_SCR_ScreenShot,
	SCR_DrawStringToSnap,

	0,
	0,

	sw32_ParticleSystem,
	sw32_R_Init,
	sw32_R_RenderFrame,
	sw32_R_ClearState,
	sw32_R_LoadSkys,
	sw32_R_NewMap,
	R_AddEfrags,
	R_RemoveEfrags,
	sw32_R_LineGraph,
	R_AllocDlight,
	R_AllocEntity,
	R_MaxDlightsCheck,
	R_DecayLights,
	sw32_R_ViewChanged,
	sw32_SCR_ScreenShot_f,
	&model_funcs
};

static general_funcs_t plugin_info_general_funcs = {
	.shutdown = sw32_vid_render_shutdown,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.vid_render = &sw32_vid_render_funcs,
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
	"SW32 Renderer",
	"Copyright (C) 1996-1997  Id Software, Inc.\n"
	"Copyright (C) 1999-2012  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(vid_render, sw32)
{
	return &plugin_info;
}
