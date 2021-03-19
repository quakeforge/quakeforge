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

#include <stdlib.h>

#include "QF/darray.h"
#include "QF/sys.h"

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_compose.h"
#include "QF/Vulkan/qf_draw.h"
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_main.h"
#include "QF/Vulkan/qf_particles.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/swapchain.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_vulkan.h"

static vulkan_ctx_t *vulkan_ctx;

static tex_t *
vulkan_SCR_CaptureBGR (void)
{
	return 0;
}

static tex_t *
vulkan_SCR_ScreenShot (int width, int height)
{
	return 0;
}

static void
vulkan_Fog_Update (float density, float red, float green, float blue,
				   float time)
{
}

static void
vulkan_Fog_ParseWorldspawn (struct plitem_s *worldspawn)
{
}

static void
vulkan_R_Init (void)
{
	Vulkan_CreateStagingBuffers (vulkan_ctx);
	Vulkan_CreateMatrices (vulkan_ctx);
	Vulkan_CreateSwapchain (vulkan_ctx);
	Vulkan_CreateFrames (vulkan_ctx);
	Vulkan_CreateRenderPass (vulkan_ctx);
	Vulkan_CreateFramebuffers (vulkan_ctx);
	// FIXME this should be staged so screen updates can begin while pipelines
	// are being built
	vulkan_ctx->pipeline = Vulkan_CreatePipeline (vulkan_ctx, "pipeline");
	Vulkan_Texture_Init (vulkan_ctx);
	Vulkan_Alias_Init (vulkan_ctx);
	Vulkan_Bsp_Init (vulkan_ctx);
	Vulkan_Draw_Init (vulkan_ctx);
	Vulkan_Particles_Init (vulkan_ctx);
	Vulkan_Lighting_Init (vulkan_ctx);
	Vulkan_Compose_Init (vulkan_ctx);
	Skin_Init ();

	Sys_Printf ("R_Init %p %d", vulkan_ctx->swapchain->swapchain,
				vulkan_ctx->swapchain->numImages);
	for (int32_t i = 0; i < vulkan_ctx->swapchain->numImages; i++) {
		Sys_Printf (" %p", vulkan_ctx->swapchain->images->a[i]);
	}
	Sys_Printf ("\n");

	SCR_Init ();
}

static void
vulkan_R_RenderFrame (SCR_Func scr_3dfunc, SCR_Func *scr_funcs)
{
	const VkSubpassContents subpassContents
		= VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
	uint32_t imageIndex = 0;
	qfv_device_t *device = vulkan_ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkDevice    dev = device->dev;
	qfv_queue_t *queue = &vulkan_ctx->device->queue;

	__auto_type frame = &vulkan_ctx->frames.a[vulkan_ctx->curFrame];

	dfunc->vkWaitForFences (dev, 1, &frame->fence, VK_TRUE, 2000000000);
	QFV_AcquireNextImage (vulkan_ctx->swapchain,
						  frame->imageAvailableSemaphore,
						  0, &imageIndex);
	vulkan_ctx->swapImageIndex = imageIndex;

	frame->framebuffer = vulkan_ctx->framebuffers->a[imageIndex];

	scr_3dfunc ();
	while (*scr_funcs) {
		(*scr_funcs) ();
		scr_funcs++;
	}

	Vulkan_FlushText (vulkan_ctx);

	Vulkan_Lighting_Draw (vulkan_ctx);
	Vulkan_Compose_Draw (vulkan_ctx);

	VkCommandBufferBeginInfo beginInfo
		= { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VkRenderPassBeginInfo renderPassInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, 0,
		vulkan_ctx->renderpass, 0,
		{ {0, 0}, vulkan_ctx->swapchain->extent },
		vulkan_ctx->clearValues->size, vulkan_ctx->clearValues->a
	};

	dfunc->vkBeginCommandBuffer (frame->cmdBuffer, &beginInfo);
	renderPassInfo.framebuffer = frame->framebuffer;
	dfunc->vkCmdBeginRenderPass (frame->cmdBuffer, &renderPassInfo,
								 subpassContents);

	for (int i = 0; i < frame->cmdSetCount; i++) {
		if (frame->cmdSets[i].size) {
			dfunc->vkCmdExecuteCommands (frame->cmdBuffer,
										 frame->cmdSets[i].size,
										 frame->cmdSets[i].a);
		}
		// reset for next time around
		frame->cmdSets[i].size = 0;

		if (i < frame->cmdSetCount - 1) {
			dfunc->vkCmdNextSubpass (frame->cmdBuffer, subpassContents);
		}
	}

	dfunc->vkCmdEndRenderPass (frame->cmdBuffer);
	dfunc->vkEndCommandBuffer (frame->cmdBuffer);

	VkPipelineStageFlags waitStage
		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
		1, &frame->imageAvailableSemaphore, &waitStage,
		1, &frame->cmdBuffer,
		1, &frame->renderDoneSemaphore,
	};
	dfunc->vkResetFences (dev, 1, &frame->fence);
	dfunc->vkQueueSubmit (queue->queue, 1, &submitInfo, frame->fence);

	VkPresentInfoKHR presentInfo = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, 0,
		1, &frame->renderDoneSemaphore,
		1, &vulkan_ctx->swapchain->swapchain, &imageIndex,
		0
	};
	dfunc->vkQueuePresentKHR (queue->queue, &presentInfo);

	vulkan_ctx->curFrame++;
	vulkan_ctx->curFrame %= vulkan_ctx->frames.size;
}

static void
vulkan_R_ClearState (void)
{
	R_ClearEfrags ();
	R_ClearDlights ();
	Vulkan_ClearParticles (vulkan_ctx);
}

static void
vulkan_R_LoadSkys (const char *skyname)
{
}

static void
vulkan_R_NewMap (model_t *worldmodel, model_t **models, int num_models)
{
	Vulkan_NewMap (worldmodel, models, num_models, vulkan_ctx);
}

static void
vulkan_R_LineGraph (int x, int y, int *h_vals, int count)
{
}

static void
vulkan_R_RenderView (void)
{
	Vulkan_RenderView (vulkan_ctx);
}

static void
vulkan_Draw_Character (int x, int y, unsigned ch)
{
	Vulkan_Draw_Character (x, y, ch, vulkan_ctx);
}

static void
vulkan_Draw_String (int x, int y, const char *str)
{
	Vulkan_Draw_String (x, y, str, vulkan_ctx);
}

static void
vulkan_Draw_nString (int x, int y, const char *str, int count)
{
	Vulkan_Draw_nString (x, y, str, count, vulkan_ctx);
}

static void
vulkan_Draw_AltString (int x, int y, const char *str)
{
	Vulkan_Draw_AltString (x, y, str, vulkan_ctx);
}

static void
vulkan_Draw_ConsoleBackground (int lines, byte alpha)
{
	Vulkan_Draw_ConsoleBackground (lines, alpha, vulkan_ctx);
}

static void
vulkan_Draw_Crosshair (void)
{
	Vulkan_Draw_Crosshair (vulkan_ctx);
}

static void
vulkan_Draw_CrosshairAt (int ch, int x, int y)
{
	Vulkan_Draw_CrosshairAt (ch, x, y, vulkan_ctx);
}

static void
vulkan_Draw_TileClear (int x, int y, int w, int h)
{
	Vulkan_Draw_TileClear (x, y, w, h, vulkan_ctx);
}

static void
vulkan_Draw_Fill (int x, int y, int w, int h, int c)
{
	Vulkan_Draw_Fill (x, y, w, h, c, vulkan_ctx);
}

static void
vulkan_Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
	Vulkan_Draw_TextBox (x, y, width, lines, alpha, vulkan_ctx);
}

static void
vulkan_Draw_FadeScreen (void)
{
	Vulkan_Draw_FadeScreen (vulkan_ctx);
}

static void
vulkan_Draw_BlendScreen (quat_t color)
{
	Vulkan_Draw_BlendScreen (color, vulkan_ctx);
}

static qpic_t *
vulkan_Draw_CachePic (const char *path, qboolean alpha)
{
	return Vulkan_Draw_CachePic (path, alpha, vulkan_ctx);
}

static void
vulkan_Draw_UncachePic (const char *path)
{
	Vulkan_Draw_UncachePic (path, vulkan_ctx);
}

static qpic_t *
vulkan_Draw_MakePic (int width, int height, const byte *data)
{
	return Vulkan_Draw_MakePic (width, height, data, vulkan_ctx);
}

static void
vulkan_Draw_DestroyPic (qpic_t *pic)
{
	Vulkan_Draw_DestroyPic (pic, vulkan_ctx);
}

static qpic_t *
vulkan_Draw_PicFromWad (const char *name)
{
	return Vulkan_Draw_PicFromWad (name, vulkan_ctx);
}

static void
vulkan_Draw_Pic (int x, int y, qpic_t *pic)
{
	Vulkan_Draw_Pic (x, y, pic, vulkan_ctx);
}

static void
vulkan_Draw_Picf (float x, float y, qpic_t *pic)
{
	Vulkan_Draw_Picf (x, y, pic, vulkan_ctx);
}

static void
vulkan_Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height)
{
	Vulkan_Draw_SubPic (x, y, pic, srcx, srcy, width, height, vulkan_ctx);
}

static void
vulkan_R_ViewChanged (float aspect)
{
	Vulkan_CalcProjectionMatrices (vulkan_ctx, aspect);
}

static void
vulkan_R_ClearParticles (void)
{
	Vulkan_ClearParticles (vulkan_ctx);
}

static void
vulkan_R_InitParticles (void)
{
	Vulkan_InitParticles (vulkan_ctx);
}

static void
vulkan_SCR_ScreenShot_f (void)
{
}

static void
vulkan_r_easter_eggs_f (struct cvar_s *var)
{
	Vulkan_r_easter_eggs_f (var, vulkan_ctx);
}

static void
vulkan_r_particles_style_f (struct cvar_s *var)
{
	Vulkan_r_particles_style_f (var, vulkan_ctx);
}

static void
vulkan_Mod_LoadLighting (model_t *mod, bsp_t *bsp)
{
	Vulkan_Mod_LoadLighting (mod, bsp, vulkan_ctx);
}

static void
vulkan_Mod_SubdivideSurface (model_t *mod, msurface_t *fa)
{
}

static void
vulkan_Mod_ProcessTexture (model_t *mod, texture_t *tx)
{
	Vulkan_Mod_ProcessTexture (mod, tx, vulkan_ctx);
}

static void
vulkan_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx,
									   void *_m, int _s, int extra)
{
	Vulkan_Mod_MakeAliasModelDisplayLists (alias_ctx, _m, _s, extra,
										   vulkan_ctx);
}

static void *
vulkan_Mod_LoadSkin (mod_alias_ctx_t *alias_ctx, byte *skin, int skinsize,
					 int snum, int gnum, qboolean group,
					 maliasskindesc_t *skindesc)
{
	return Vulkan_Mod_LoadSkin (alias_ctx, skin, skinsize, snum, gnum, group,
								skindesc, vulkan_ctx);
}

static void
vulkan_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx)
{
	Vulkan_Mod_FinalizeAliasModel (alias_ctx, vulkan_ctx);
}

static void
vulkan_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx)
{
}

static void
vulkan_Mod_IQMFinish (model_t *mod)
{
}

static void
vulkan_Mod_SpriteLoadTexture (model_t *mod, mspriteframe_t *pspriteframe,
							  int framenum)
{
}

static void
vulkan_Skin_SetupSkin (struct skin_s *skin, int cmap)
{
}

static void
vulkan_Skin_ProcessTranslation (int cmap, const byte *translation)
{
}

static void
vulkan_Skin_InitTranslations (void)
{
}

static vid_model_funcs_t model_funcs = {
	sizeof (vulktex_t) + 2 * sizeof (qfv_tex_t),
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
	vulkan_Skin_InitTranslations,
};

vid_render_funcs_t vulkan_vid_render_funcs = {
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

	SCR_DrawRam,
	SCR_DrawTurtle,
	SCR_DrawPause,
	vulkan_SCR_CaptureBGR,
	vulkan_SCR_ScreenShot,
	SCR_DrawStringToSnap,

	vulkan_Fog_Update,
	vulkan_Fog_ParseWorldspawn,

	vulkan_R_Init,
	vulkan_R_RenderFrame,
	vulkan_R_ClearState,
	vulkan_R_LoadSkys,
	vulkan_R_NewMap,
	R_AddEfrags,
	R_RemoveEfrags,
	R_EnqueueEntity,
	vulkan_R_LineGraph,
	R_AllocDlight,
	R_AllocEntity,
	R_MaxDlightsCheck,
	vulkan_R_RenderView,
	R_DecayLights,
	vulkan_R_ViewChanged,
	vulkan_R_ClearParticles,
	vulkan_R_InitParticles,
	vulkan_SCR_ScreenShot_f,
	vulkan_r_easter_eggs_f,
	vulkan_r_particles_style_f,
	0,
	&model_funcs
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
	Vulkan_CreateDevice (vulkan_ctx);
	vulkan_ctx->choose_visual (vulkan_ctx);
	vulkan_ctx->cmdpool = QFV_CreateCommandPool (vulkan_ctx->device,
									 vulkan_ctx->device->queue.queueFamily,
									 0, 1);
	__auto_type cmdset = QFV_AllocCommandBufferSet (1, alloca);
	QFV_AllocateCommandBuffers (vulkan_ctx->device, vulkan_ctx->cmdpool, 0,
								cmdset);
	vulkan_ctx->cmdbuffer = cmdset->a[0];
	vulkan_ctx->fence = QFV_CreateFence (vulkan_ctx->device, 1);
	Sys_Printf ("vk choose visual %p %p %d %p\n", vulkan_ctx->device->dev,
				vulkan_ctx->device->queue.queue,
				vulkan_ctx->device->queue.queueFamily,
				vulkan_ctx->cmdpool);
}

static void
vulkan_vid_render_create_context (void)
{
	vulkan_ctx->create_window (vulkan_ctx);
	vulkan_ctx->surface = vulkan_ctx->create_surface (vulkan_ctx);
	Sys_Printf ("vk create context %p\n", vulkan_ctx->surface);
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
	qfv_device_t *device = vulkan_ctx->device;
	qfv_devfuncs_t *df = device->funcs;
	VkDevice    dev = device->dev;
	QFV_DeviceWaitIdle (device);
	df->vkDestroyFence (dev, vulkan_ctx->fence, 0);
	df->vkDestroyCommandPool (dev, vulkan_ctx->cmdpool, 0);
	Vulkan_Compose_Shutdown (vulkan_ctx);
	Vulkan_Lighting_Shutdown (vulkan_ctx);
	Vulkan_Draw_Shutdown (vulkan_ctx);
	Vulkan_Bsp_Shutdown (vulkan_ctx);
	Vulkan_Alias_Shutdown (vulkan_ctx);
	Mod_ClearAll ();
	Vulkan_Texture_Shutdown (vulkan_ctx);
	Vulkan_DestroyFramebuffers (vulkan_ctx);
	Vulkan_DestroyRenderPass (vulkan_ctx);
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
