/*
	vid_render_gl.c

	SW version of the renderer

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

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "mod_internal.h"
#include "r_internal.h"

static general_funcs_t plugin_info_general_funcs = {
};

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

static vid_render_funcs_t vid_render_funcs = {
	Draw_Init,
	Draw_Character,
	Draw_String,
	Draw_nString,
	Draw_AltString,
	Draw_ConsoleBackground,
	Draw_Crosshair,
	Draw_CrosshairAt,
	Draw_TileClear,
	Draw_Fill,
	Draw_TextBox,
	Draw_FadeScreen,
	Draw_BlendScreen,
	Draw_CachePic,
	Draw_UncachePic,
	Draw_MakePic,
	Draw_DestroyPic,
	Draw_PicFromWad,
	Draw_Pic,
	Draw_Picf,
	Draw_SubPic,

	SCR_UpdateScreen,
	SCR_DrawRam,
	SCR_DrawFPS,
	SCR_DrawTime,
	SCR_DrawTurtle,
	SCR_DrawPause,
	SCR_CaptureBGR,
	SCR_ScreenShot,
	SCR_DrawStringToSnap,

	Fog_Update,
	Fog_ParseWorldspawn,

	R_ClearState,
	R_LoadSkys,
	R_NewMap,
	R_AddEfrags,
	R_RemoveEfrags,
	R_EnqueueEntity,
	R_LineGraph,
	R_AllocDlight,
	R_AllocEntity,
	R_RenderView,
	R_DecayLights,
	D_FlushCaches,
	0,
	&model_funcs
};

static vid_render_data_t vid_render_data = {
	&vid, &r_refdef, &scr_vrect,
	0, 0, 0,
	0,
	0, 0,
	0.0,
	true, false, false, false,
	0,
	0, 0,
	0,
	0.0, 0.0,
	0,
	r_origin, vpn, vright, vup
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	0,
	0,
	0,
	0,
	&vid_render_funcs,
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
	"SW Renderer",
	"Copyright (C) 1996-1997  Id Software, Inc.\n"
	"Copyright (C) 1999-2012  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(vid_render, sw)
{
	return &plugin_info;
}
