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

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "mod_internal.h"
#include "r_internal.h"

#include "gl/namehack.h"

static vid_model_funcs_t model_funcs = {
    Mod_LoadExternalTextures,
	Mod_LoadLighting,
	Mod_SubdivideSurface,
	Mod_ProcessTexture,
	Mod_LoadAliasModel,
	Mod_LoadSpriteModel,

	Skin_SetColormap,
	Skin_SetSkin,
	Skin_SetupSkin,
	Skin_SetTranslation,
	Skin_ProcessTranslation,
	Skin_InitTranslations,
	Skin_Init_Textures,
	Skin_SetPlayerSkin,
};

vid_render_funcs_t gl_vid_render_funcs = {
	gl_Draw_Init,
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

	gl_SCR_UpdateScreen,
	SCR_DrawRam,
	SCR_DrawTurtle,
	SCR_DrawPause,
	gl_SCR_CaptureBGR,
	gl_SCR_ScreenShot,
	SCR_DrawStringToSnap,

	gl_Fog_Update,
	gl_Fog_ParseWorldspawn,

	gl_R_ClearState,
	gl_R_LoadSkys,
	gl_R_NewMap,
	R_AddEfrags,
	R_RemoveEfrags,
	R_EnqueueEntity,
	gl_R_LineGraph,
	R_AllocDlight,
	R_AllocEntity,
	gl_R_RenderView,
	R_DecayLights,
	0,
	0,
	&model_funcs
};

static general_funcs_t plugin_info_general_funcs = {
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	0,
	0,
	0,
	0,
	&gl_vid_render_funcs,
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
	qfp_snd_render,
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
