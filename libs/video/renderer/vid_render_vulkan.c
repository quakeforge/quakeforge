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

//#define NH_DEFINE
//#include "vulkan/namehack.h"

#include "QF/darray.h"
#include "QF/sys.h"

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/swapchain.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_vulkan.h"

#include "vulkan/namehack.h"

static vulkan_ctx_t *vulkan_ctx;

static void
vulkan_R_Init (void)
{
	qfv_device_t *device = vulkan_ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_cmdbufferset_t *cmdBuffers;

	Vulkan_CreateSwapchain (vulkan_ctx);
	cmdBuffers = QFV_AllocateCommandBuffers (device, vulkan_ctx->cmdpool, 0,
											 vulkan_ctx->swapchain->numImages);
	vulkan_ctx->frameset.cmdBuffers = cmdBuffers;
	VkCommandBufferBeginInfo beginInfo
		= { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VkClearColorValue clearColor = { {0.7294, 0.8549, 0.3333, 1.0} };
	VkImageSubresourceRange image_subresource_range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, 1, 0, 1,
	};
	VkImageMemoryBarrier pc_barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		device->queue.queueFamily,
		device->queue.queueFamily,
		0,	// filled in later
		image_subresource_range
    };
	VkImageMemoryBarrier cp_barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		device->queue.queueFamily,
		device->queue.queueFamily,
		0,	// filled in later
		image_subresource_range
    };
	for (size_t i = 0; i < cmdBuffers->size; i++) {
		pc_barrier.image = vulkan_ctx->swapchain->images->a[i];
		cp_barrier.image = vulkan_ctx->swapchain->images->a[i];
		dfunc->vkBeginCommandBuffer (cmdBuffers->a[i], &beginInfo);
		dfunc->vkCmdPipelineBarrier (cmdBuffers->a[i],
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, 0,	// memory barriers
				0, 0,	// buffer barriers
				1, &pc_barrier);
		dfunc->vkCmdClearColorImage (cmdBuffers->a[i], pc_barrier.image,
									 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									 &clearColor, 1,
									 &image_subresource_range);
		dfunc->vkCmdPipelineBarrier (cmdBuffers->a[i],
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				0,
				0, 0,
				0, 0,
				1, &cp_barrier);
		dfunc->vkEndCommandBuffer (cmdBuffers->a[i]);
	}
	Sys_Printf ("R_Init %p %d", vulkan_ctx->swapchain->swapchain,
				vulkan_ctx->swapchain->numImages);
	for (int32_t i = 0; i < vulkan_ctx->swapchain->numImages; i++) {
		Sys_Printf (" %p", vulkan_ctx->swapchain->images->a[i]);
	}
	Sys_Printf ("\n");
}

static void
vulkan_SCR_UpdateScreen (double time,  void (*f)(void), void (**g)(void))
{
	static int count = 0;
	static double startTime;
	uint32_t imageIndex = 0;
	qfv_device_t *device = vulkan_ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkDevice    dev = device->dev;
	qfv_queue_t *queue = &vulkan_ctx->device->queue;
	vulkan_frameset_t *frameset = &vulkan_ctx->frameset;

	dfunc->vkWaitForFences (dev, 1, & frameset->fences->a[frameset->curFrame],
							VK_TRUE, 2000000000);
	QFV_AcquireNextImage (vulkan_ctx->swapchain,
				  frameset->imageSemaphores->a[frameset->curFrame],
				  0, &imageIndex);

	VkPipelineStageFlags waitStage
		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
		1,
		&frameset->imageSemaphores->a[frameset->curFrame],
		&waitStage,
		1, &frameset->cmdBuffers->a[imageIndex],
		1, &frameset->renderDoneSemaphores->a[frameset->curFrame],
	};
	dfunc->vkResetFences (dev, 1, &frameset->fences->a[frameset->curFrame]);
	dfunc->vkQueueSubmit (queue->queue, 1, &submitInfo,
						  frameset->fences->a[frameset->curFrame]);

	VkPresentInfoKHR presentInfo = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, 0,
		1, &frameset->renderDoneSemaphores->a[frameset->curFrame],
		1, &vulkan_ctx->swapchain->swapchain, &imageIndex,
		0
	};
	dfunc->vkQueuePresentKHR (queue->queue, &presentInfo);

	frameset->curFrame++;
	frameset->curFrame %= frameset->fences->size;

	if (++count >= 100) {
		double currenTime = Sys_DoubleTime ();
		double time = currenTime - startTime;
		startTime = currenTime;
		printf ("%d frames in %g s: %g fps     \r", count, time, count / time);
		fflush (stdout);
		count = 0;
	}
}

static qpic_t *
vulkan_Draw_CachePic (const char *path, qboolean alpha)
{
	return 0;
}

static qpic_t qpic = { 1, 1, {0} };

static qpic_t *
vulkan_Draw_MakePic (int width, int height, const byte *data)
{
	return &qpic;
}

static vid_model_funcs_t model_funcs = {
	0,//vulkan_Mod_LoadExternalTextures,
	0,//vulkan_Mod_LoadLighting,
	0,//vulkan_Mod_SubdivideSurface,
	0,//vulkan_Mod_ProcessTexture,

	Mod_LoadIQM,
	Mod_LoadAliasModel,
	Mod_LoadSpriteModel,

	0,//vulkan_Mod_MakeAliasModelDisplayLists,
	0,//vulkan_Mod_LoadSkin,
	0,//vulkan_Mod_FinalizeAliasModel,
	0,//vulkan_Mod_LoadExternalSkins,
	0,//vulkan_Mod_IQMFinish,
	0,
	0,//vulkan_Mod_SpriteLoadTexture,

	Skin_SetColormap,
	Skin_SetSkin,
	0,//vulkan_Skin_SetupSkin,
	Skin_SetTranslation,
	0,//vulkan_Skin_ProcessTranslation,
	0,//vulkan_Skin_InitTranslations,
};

vid_render_funcs_t vulkan_vid_render_funcs = {
	0,//vulkan_Draw_Init,
	0,//vulkan_Draw_Character,
	0,//vulkan_Draw_String,
	0,//vulkan_Draw_nString,
	0,//vulkan_Draw_AltString,
	0,//vulkan_Draw_ConsoleBackground,
	0,//vulkan_Draw_Crosshair,
	0,//vulkan_Draw_CrosshairAt,
	0,//vulkan_Draw_TileClear,
	0,//vulkan_Draw_Fill,
	0,//vulkan_Draw_TextBox,
	0,//vulkan_Draw_FadeScreen,
	0,//vulkan_Draw_BlendScreen,
	vulkan_Draw_CachePic,
	0,//vulkan_Draw_UncachePic,
	vulkan_Draw_MakePic,
	0,//vulkan_Draw_DestroyPic,
	0,//vulkan_Draw_PicFromWad,
	0,//vulkan_Draw_Pic,
	0,//vulkan_Draw_Picf,
	0,//vulkan_Draw_SubPic,

	vulkan_SCR_UpdateScreen,
	SCR_DrawRam,
	SCR_DrawTurtle,
	SCR_DrawPause,
	0,//vulkan_SCR_CaptureBGR,
	0,//vulkan_SCR_ScreenShot,
	SCR_DrawStringToSnap,

	0,//vulkan_Fog_Update,
	0,//vulkan_Fog_ParseWorldspawn,

	vulkan_R_Init,
	0,//vulkan_R_ClearState,
	0,//vulkan_R_LoadSkys,
	0,//vulkan_R_NewMap,
	R_AddEfrags,
	R_RemoveEfrags,
	R_EnqueueEntity,
	0,//vulkan_R_LineGraph,
	R_AllocDlight,
	R_AllocEntity,
	0,//vulkan_R_RenderView,
	R_DecayLights,
	0,//vulkan_R_ViewChanged,
	0,//vulkan_R_ClearParticles,
	0,//vulkan_R_InitParticles,
	0,//vulkan_SCR_ScreenShot_f,
	0,//vulkan_r_easter_eggs_f,
	0,//vulkan_r_particles_style_f,
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
									 0, 0);
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
	vulkan_ctx->frameset.curFrame = 0;
	qfv_fenceset_t *fences = DARRAY_ALLOCFIXED (*fences, 2, malloc);
	qfv_semaphoreset_t *imageSems = DARRAY_ALLOCFIXED (*imageSems, 2, malloc);
	qfv_semaphoreset_t *renderDoneSems = DARRAY_ALLOCFIXED (*renderDoneSems, 2,
															malloc);
	for (int i = 0; i < 2; i++) {
		fences->a[i]= QFV_CreateFence (vulkan_ctx->device, 1);
		imageSems->a[i] = QFV_CreateSemaphore (vulkan_ctx->device);
		renderDoneSems->a[i] = QFV_CreateSemaphore (vulkan_ctx->device);
	}
	vulkan_ctx->frameset.fences = fences;
	vulkan_ctx->frameset.imageSemaphores = imageSems;
	vulkan_ctx->frameset.renderDoneSemaphores = renderDoneSems;
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
	vulkan_frameset_t *frameset = &vulkan_ctx->frameset;
	for (size_t i = 0; i < frameset->fences->size; i++) {
		df->vkDestroyFence (dev, frameset->fences->a[i], 0);
		df->vkDestroySemaphore (dev, frameset->imageSemaphores->a[i], 0);
		df->vkDestroySemaphore (dev, frameset->renderDoneSemaphores->a[i], 0);
	}
	df->vkDestroyCommandPool (dev, vulkan_ctx->cmdpool, 0);
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
