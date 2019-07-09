/*
	vid_render_vulkan.c

	Vulkan version of the renderer

	Copyright (C) 2019 Bill Currie <bill@taniwha.org>

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
#include "vulkan/namehack.h"

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/Vulkan/qf_vid.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_vulkan.h"

#include "vulkan/namehack.h"

static vulkan_ctx_t *vulkan_ctx;

static vid_model_funcs_t model_funcs = {
/*	vulkan_Mod_LoadExternalTextures,
	vulkan_Mod_LoadLighting,
	vulkan_Mod_SubdivideSurface,
	vulkan_Mod_ProcessTexture,

	Mod_LoadIQM,
	Mod_LoadAliasModel,
	Mod_LoadSpriteModel,

	vulkan_Mod_MakeAliasModelDisplayLists,
	vulkan_Mod_LoadSkin,
	vulkan_Mod_FinalizeAliasModel,
	vulkan_Mod_LoadExternalSkins,
	vulkan_Mod_IQMFinish,
	0,
	vulkan_Mod_SpriteLoadTexture,

	Skin_SetColormap,
	Skin_SetSkin,
	vulkan_Skin_SetupSkin,
	Skin_SetTranslation,
	vulkan_Skin_ProcessTranslation,
	vulkan_Skin_InitTranslations,*/
};

vid_render_funcs_t vulkan_vid_render_funcs = {
	vulkan_Draw_Init,
	vulkan_Draw_Character,
	vulkan_Draw_String,
	vulkan_Draw_nString,
	vulkan_Draw_AltString,
	vulkan_Draw_ConsoleBackground,
	vulkan_Draw_Crosshair,
	vulkan_Draw_CrosshairAt,
	vulkan_Draw_TileClear,
	vulkan_Draw_Fill,
	vulkan_Draw_TextBox,
	vulkan_Draw_FadeScreen,
	vulkan_Draw_BlendScreen,
	vulkan_Draw_CachePic,
	vulkan_Draw_UncachePic,
	vulkan_Draw_MakePic,
	vulkan_Draw_DestroyPic,
	vulkan_Draw_PicFromWad,
	vulkan_Draw_Pic,
	vulkan_Draw_Picf,
	vulkan_Draw_SubPic,

/*	vulkan_SCR_UpdateScreen,
	SCR_DrawRam,
	SCR_DrawTurtle,
	SCR_DrawPause,
	vulkan_SCR_CaptureBGR,
	vulkan_SCR_ScreenShot,
	SCR_DrawStringToSnap,

	vulkan_Fog_Update,
	vulkan_Fog_ParseWorldspawn,

	vulkan_R_Init,
	vulkan_R_ClearState,
	vulkan_R_LoadSkys,
	vulkan_R_NewMap,
	R_AddEfrags,
	R_RemoveEfrags,
	R_EnqueueEntity,
	vulkan_R_LineGraph,
	R_AllocDlight,
	R_AllocEntity,
	vulkan_R_RenderView,
	R_DecayLights,
	vulkan_R_ViewChanged,
	vulkan_R_ClearParticles,
	vulkan_R_InitParticles,
	vulkan_SCR_ScreenShot_f,
	vulkan_r_easter_eggs_f,
	vulkan_r_particles_style_f,
	0,
	&model_funcs*/
};

static void
set_palette (const byte *palette)
{
	//FIXME really don't want this here: need an application domain
	//so Quake can be separated from QuakeForge (ie, Quake itself becomes
	//an app using the QuakeForge engine)
}

static void
vulkan_vid_render_choose_visual (void)
{
	//vulkan_ctx->coose_visual (vulkan_ctx);
}

static void
vulkan_vid_render_create_context (void)
{
	vulkan_ctx->create_surface (vulkan_ctx);
}

static void
vulkan_vid_render_init (void)
{
	vulkan_ctx = vr_data.vid->vid_internal->vulkan_context ();
	vulkan_ctx->load_vulkan (vulkan_ctx);

	Vulkan_Init_Common (vulkan_ctx);

	vr_data.vid->vid_internal->set_palette = set_palette;
	vr_data.vid->vid_internal->choose_visual = vulkan_vid_render_choose_visual;
	vr_data.vid->vid_internal->create_context = vulkan_vid_render_create_context;

	vr_funcs = &vulkan_vid_render_funcs;
	m_funcs = &model_funcs;
}

static void
vulkan_vid_render_shutdown (void)
{
	Vulkan_Shutdown_Common (vulkan_ctx);
}

static general_funcs_t plugin_info_general_funcs = {
	vulkan_vid_render_init,
	vulkan_vid_render_shutdown,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	0,
	0,
	0,
	0,
	&vulkan_vid_render_funcs,
};

static plugin_data_t plugin_info_data = {
	&plugin_info_general_data,
	0,
	0,
	0,
	0,
	0,
	&vid_render_data,
};

static plugin_t plugin_info = {
	qfp_vid_render,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"Vulkan Renderer",
	"Copyright (C) 1996-1997  Id Software, Inc.\n"
	"Copyright (C) 1999-2019  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(vid_render, vulkan)
{
	return &plugin_info;
}
