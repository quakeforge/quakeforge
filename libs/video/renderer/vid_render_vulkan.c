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

#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/dstring.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_compose.h"
#include "QF/Vulkan/qf_draw.h"
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_main.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_particles.h"
#include "QF/Vulkan/qf_sprite.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/capture.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/swapchain.h"
#include "QF/ui/view.h"

#include "QF/scene/entity.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_vulkan.h"

static vulkan_ctx_t *vulkan_ctx;

static struct psystem_s *
vulkan_ParticleSystem (void)
{
	return Vulkan_ParticleSystem (vulkan_ctx);
}

static void
vulkan_R_Init (void)
{
	r_ent_queue = EntQueue_New (mod_num_types);
	Vulkan_CreateStagingBuffers (vulkan_ctx);
	Vulkan_CreateSwapchain (vulkan_ctx);
	Vulkan_CreateFrames (vulkan_ctx);
	Vulkan_CreateCapture (vulkan_ctx);
	Vulkan_CreateRenderPass (vulkan_ctx);
	Vulkan_Texture_Init (vulkan_ctx);

	Vulkan_Matrix_Init (vulkan_ctx);
	Vulkan_Alias_Init (vulkan_ctx);
	Vulkan_Bsp_Init (vulkan_ctx);
	Vulkan_Particles_Init (vulkan_ctx);
	Vulkan_Sprite_Init (vulkan_ctx);
	Vulkan_Draw_Init (vulkan_ctx);
	Vulkan_Lighting_Init (vulkan_ctx);
	Vulkan_Compose_Init (vulkan_ctx);

	Skin_Init ();

	SCR_Init ();
}

static void
vulkan_R_ClearState (void)
{
	r_worldentity.renderer.model = 0;
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}

static void
vulkan_R_LoadSkys (const char *skyname)
{
	Vulkan_LoadSkys (skyname, vulkan_ctx);
}

static void
vulkan_R_NewMap (model_t *worldmodel, model_t **models, int num_models)
{
	Vulkan_NewMap (worldmodel, models, num_models, vulkan_ctx);
}

static void
vulkan_R_LineGraph (int x, int y, int *h_vals, int count, int height)
{
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
vulkan_R_ViewChanged (void)
{
	Vulkan_CalcProjectionMatrices (vulkan_ctx);
}

static void
vulkan_begin_frame (void)
{
	uint32_t imageIndex = 0;
	qfv_device_t *device = vulkan_ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkDevice    dev = device->dev;

	__auto_type frame = &vulkan_ctx->frames.a[vulkan_ctx->curFrame];

	dfunc->vkWaitForFences (dev, 1, &frame->fence, VK_TRUE, 2000000000);
	QFV_AcquireNextImage (vulkan_ctx->swapchain,
						  frame->imageAvailableSemaphore,
						  0, &imageIndex);
	vulkan_ctx->swapImageIndex = imageIndex;
}

static void
vulkan_render_view (void)
{
	__auto_type frame = &vulkan_ctx->frames.a[vulkan_ctx->curFrame];
	uint32_t imageIndex = vulkan_ctx->swapImageIndex;

	for (size_t i = 0; i < vulkan_ctx->renderPasses.size; i++) {
		__auto_type rp = vulkan_ctx->renderPasses.a[i];
		__auto_type rpFrame = &rp->frames.a[vulkan_ctx->curFrame];
		frame->framebuffer = rp->framebuffers->a[imageIndex];
		rp->draw (rpFrame);
	}
}

static void
vulkan_set_2d (void)
{
}

static void
vulkan_end_frame (void)
{
	qfv_device_t *device = vulkan_ctx->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_queue_t *queue = &device->queue;
	__auto_type frame = &vulkan_ctx->frames.a[vulkan_ctx->curFrame];
	uint32_t imageIndex = vulkan_ctx->swapImageIndex;

	VkCommandBufferBeginInfo beginInfo
		= { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

	VkRenderPassBeginInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderArea = { {0, 0}, vulkan_ctx->swapchain->extent },
	};

	dfunc->vkBeginCommandBuffer (frame->cmdBuffer, &beginInfo);
	for (size_t i = 0; i < vulkan_ctx->renderPasses.size; i++) {
		__auto_type rp = vulkan_ctx->renderPasses.a[i];
		__auto_type rpFrame = &rp->frames.a[vulkan_ctx->curFrame];

		if (rpFrame->renderpass) {
			renderPassInfo.framebuffer = frame->framebuffer,
			renderPassInfo.renderPass = rp->renderpass;
			renderPassInfo.clearValueCount = rp->clearValues->size;
			renderPassInfo.pClearValues = rp->clearValues->a;

			dfunc->vkCmdBeginRenderPass (frame->cmdBuffer, &renderPassInfo,
										 rpFrame->subpassContents);

			for (int j = 0; j < rpFrame->subpassCount; j++) {
				__auto_type cmdSet = &rpFrame->subpassCmdSets[j];
				if (cmdSet->size) {
					dfunc->vkCmdExecuteCommands (frame->cmdBuffer,
												 cmdSet->size, cmdSet->a);
				}
				// reset for next time around
				cmdSet->size = 0;

				//Regardless of whether any commands were submitted for this
				//subpass, must step through each and every subpass, otherwise
				//the attachments won't be transitioned correctly.
				if (j < rpFrame->subpassCount - 1) {
					dfunc->vkCmdNextSubpass (frame->cmdBuffer,
											 rpFrame->subpassContents);
				}
			}
			dfunc->vkCmdEndRenderPass (frame->cmdBuffer);
		} else {
			for (int j = 0; j < rpFrame->subpassCount; j++) {
				__auto_type cmdSet = &rpFrame->subpassCmdSets[j];
				if (cmdSet->size) {
					dfunc->vkCmdExecuteCommands (frame->cmdBuffer,
												 cmdSet->size, cmdSet->a);
				}
				// reset for next time around
				cmdSet->size = 0;
			}
		}
	}

	if (vulkan_ctx->capture_callback) {
		VkImage     srcImage = vulkan_ctx->swapchain->images->a[imageIndex];
		VkCommandBuffer cmd = QFV_CaptureImage (vulkan_ctx->capture, srcImage,
												vulkan_ctx->curFrame);
		dfunc->vkCmdExecuteCommands (frame->cmdBuffer, 1, &cmd);
	}
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

	if (vulkan_ctx->capture_callback) {
		//FIXME look into "threading" this rather than waiting here
		dfunc->vkWaitForFences (device->dev, 1, &frame->fence, VK_TRUE,
								1000000000ull);
		vulkan_ctx->capture_callback (QFV_CaptureData (vulkan_ctx->capture,
													   vulkan_ctx->curFrame),
									  vulkan_ctx->capture->extent.width,
									  vulkan_ctx->capture->extent.height);
		vulkan_ctx->capture_callback = 0;
	}

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

static int
is_bgr (VkFormat format)
{
	return (format >= VK_FORMAT_B8G8R8A8_UNORM
			&& format <= VK_FORMAT_B8G8R8A8_SRGB);
}

static void
capture_screenshot (const byte *data, int width, int height)
{
	dstring_t  *name = dstring_new ();
	// find a file name to save it to
	if (!QFS_NextFilename (name, va (vulkan_ctx->va_ctx, "%s/qf",
									 qfs_gamedir->dir.shots),
						   ".ppm")) {
		Sys_Printf ("SCR_ScreenShot_f: Couldn't create a ppm file\n");
	} else {
		QFile      *file = QFS_Open (name->str, "wb");
		if (!file) {
			Sys_Printf ("Couldn't open %s\n", name->str);
		} else {
			Qprintf (file, "P6\n%d\n%d\n255\n", width, height);
			if (vulkan_ctx->capture->canBlit ||
				!is_bgr (vulkan_ctx->swapchain->format)) {
				for (int count = width * height; count-- > 0; ) {
					Qwrite (file, data, 3);
					data += 4;
				}
			} else {
				for (int count = width * height; count-- > 0; ) {
					byte        rgb[] = { data[2], data[1], data[0] };
					Qwrite (file, rgb, 3);
					data += 4;
				}
			}
			Qclose (file);
		}
	}
	dstring_delete (name);
}

static tex_t *
vulkan_SCR_CaptureBGR (void)
{
	if (!vulkan_ctx->capture) {
		Sys_Printf ("Capture not supported\n");
		return 0;
	}
	vulkan_ctx->capture_callback = capture_screenshot;
	//FIXME async process
	return 0;
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
vulkan_Mod_SpriteLoadFrames (mod_sprite_ctx_t *sprite_ctx)
{
	Vulkan_Mod_SpriteLoadFrames (sprite_ctx, vulkan_ctx);
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

static void
set_palette (void *data, const byte *palette)
{
	//FIXME really don't want this here: need an application domain
	//so Quake can be separated from QuakeForge (ie, Quake itself becomes
	//an app using the QuakeForge engine)
}

static void
vulkan_vid_render_choose_visual (void *data)
{
	Vulkan_CreateDevice (vulkan_ctx);
	if (!vulkan_ctx->device) {
		Sys_Error ("Unable to create Vulkan device.%s",
				   vulkan_use_validation->int_val ? ""
					: "\nSet vulkan_use_validation for details");
	}
	vulkan_ctx->choose_visual (vulkan_ctx);
	vulkan_ctx->cmdpool = QFV_CreateCommandPool (vulkan_ctx->device,
									 vulkan_ctx->device->queue.queueFamily,
									 0, 1);
	__auto_type cmdset = QFV_AllocCommandBufferSet (1, alloca);
	QFV_AllocateCommandBuffers (vulkan_ctx->device, vulkan_ctx->cmdpool, 0,
								cmdset);
	vulkan_ctx->cmdbuffer = cmdset->a[0];
	vulkan_ctx->fence = QFV_CreateFence (vulkan_ctx->device, 1);
	Sys_MaskPrintf (SYS_vulkan, "vk choose visual %p %p %d %#zx\n",
					vulkan_ctx->device->dev, vulkan_ctx->device->queue.queue,
					vulkan_ctx->device->queue.queueFamily,
					(size_t) vulkan_ctx->cmdpool);
}

static void
vulkan_vid_render_create_context (void *data)
{
	vulkan_ctx->create_window (vulkan_ctx);
	vulkan_ctx->surface = vulkan_ctx->create_surface (vulkan_ctx);
	Sys_MaskPrintf (SYS_vulkan, "vk create context %#zx\n",
					(size_t) vulkan_ctx->surface);
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
	vulkan_Mod_SpriteLoadFrames,

	Skin_SetColormap,
	Skin_SetSkin,
	vulkan_Skin_SetupSkin,
	Skin_SetTranslation,
	vulkan_Skin_ProcessTranslation,
	vulkan_Skin_InitTranslations,
};

static void
vulkan_vid_render_init (void)
{
	if (!vr_data.vid->vid_internal->vulkan_context) {
		Sys_Error ("Sorry, Vulkan not supported by this program.");
	}
	vulkan_ctx = vr_data.vid->vid_internal->vulkan_context ();
	vulkan_ctx->load_vulkan (vulkan_ctx);

	Vulkan_Init_Common (vulkan_ctx);

	vr_data.vid->vid_internal->data = vulkan_ctx;
	vr_data.vid->vid_internal->set_palette = set_palette;
	vr_data.vid->vid_internal->choose_visual = vulkan_vid_render_choose_visual;
	vr_data.vid->vid_internal->create_context = vulkan_vid_render_create_context;

	vr_funcs = &vulkan_vid_render_funcs;
	m_funcs = &model_funcs;
}

static void
vulkan_vid_render_shutdown (void)
{
	if (!vulkan_ctx || !vulkan_ctx->device) {
		return;
	}
	qfv_device_t *device = vulkan_ctx->device;
	qfv_devfuncs_t *df = device->funcs;
	VkDevice    dev = device->dev;
	QFV_DeviceWaitIdle (device);
	df->vkDestroyFence (dev, vulkan_ctx->fence, 0);
	df->vkDestroyCommandPool (dev, vulkan_ctx->cmdpool, 0);

	Vulkan_Compose_Shutdown (vulkan_ctx);
	Vulkan_Lighting_Shutdown (vulkan_ctx);
	Vulkan_Draw_Shutdown (vulkan_ctx);
	Vulkan_Sprite_Shutdown (vulkan_ctx);
	Vulkan_Particles_Shutdown (vulkan_ctx);
	Vulkan_Bsp_Shutdown (vulkan_ctx);
	Vulkan_Alias_Shutdown (vulkan_ctx);
	Vulkan_Matrix_Shutdown (vulkan_ctx);

	Mod_ClearAll ();
	Vulkan_Texture_Shutdown (vulkan_ctx);
	Vulkan_DestroyRenderPasses (vulkan_ctx);
	Vulkan_Shutdown_Common (vulkan_ctx);
}

vid_render_funcs_t vulkan_vid_render_funcs = {
	vulkan_vid_render_init,
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

	vulkan_SCR_CaptureBGR,

	vulkan_ParticleSystem,
	vulkan_R_Init,
	vulkan_R_ClearState,
	vulkan_R_LoadSkys,
	vulkan_R_NewMap,
	vulkan_R_LineGraph,
	vulkan_R_ViewChanged,
	vulkan_begin_frame,
	vulkan_render_view,
	vulkan_set_2d,
	vulkan_end_frame,
	&model_funcs
};

static general_funcs_t plugin_info_general_funcs = {
	.shutdown = vulkan_vid_render_shutdown,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.vid_render = &vulkan_vid_render_funcs,
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
