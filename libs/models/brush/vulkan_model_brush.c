/*
	vulkan_model_brush.c

	Vulkan support routines for model loading and caching

	Copyright (C) 1996-1997  Id Software, Inc.

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
// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_model.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"

#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_vulkan.h"

static vulktex_t vulkan_notexture = { };

static void vulkan_brush_clear (model_t *model, void *data)
{
	modelctx_t *mctx = data;
	vulkan_ctx_t *ctx = mctx->ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	for (int i = 0; i < model->numtextures; i++) {
		texture_t  *tx = model->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;
		dfunc->vkDestroyImage (device->dev, tex->tex->image, 0);
		dfunc->vkDestroyImageView (device->dev, tex->tex->view, 0);
		if (tex->glow) {
			dfunc->vkDestroyImage (device->dev, tex->glow->image, 0);
			dfunc->vkDestroyImageView (device->dev, tex->glow->view, 0);
		}
	}
	dfunc->vkFreeMemory (device->dev, mctx->texture_memory, 0);
}

static size_t
get_image_size (VkImage image, qfv_device_t *device)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	size_t      size;
	size_t      align;

	VkMemoryRequirements requirements;
	dfunc->vkGetImageMemoryRequirements (device->dev, image, &requirements);
	size = requirements.size;
	align = requirements.alignment - 1;
	size = (size + align) & ~(align);
	return size;
}

static void
load_textures (model_t *model, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	modelctx_t *mctx = model->data;
	VkImage     image = 0;

	size_t      memsize = 0;
	for (int i = 0; i < model->numtextures; i++) {
		texture_t  *tx = model->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;
		tex->tex->offset = memsize;
		memsize += get_image_size (tex->tex->image, device);
		// just so we have one in the end
		image = tex->tex->image;
		if (tex->glow) {
			tex->glow->offset = memsize;
			memsize += get_image_size (tex->glow->image, device);
		}
	}
	VkDeviceMemory mem;
	mem = QFV_AllocImageMemory (device, image,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								memsize, 0);
	mctx->texture_memory = mem;

	for (int i = 0; i < model->numtextures; i++) {
		texture_t  *tx = model->textures[i];
		if (!tx) {
			continue;
		}
		vulktex_t  *tex = tx->render;

		dfunc->vkBindImageMemory (device->dev, tex->tex->image, mem,
								  tex->tex->offset);
		VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
		if (strncmp (tx->name, "sky", 3) == 0) {
			type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		}
		tex->tex->view = QFV_CreateImageView (device, tex->tex->image,
											  type, VK_FORMAT_R8G8B8A8_UNORM,
											  VK_IMAGE_ASPECT_COLOR_BIT);
		if (tex->glow) {
			dfunc->vkBindImageMemory (device->dev, tex->glow->image, mem,
									  tex->glow->offset);
			// skys are unlit so never have a glow texture thus glow
			// textures are always simple 2D
			tex->glow->view = QFV_CreateImageView (device, tex->tex->image,
												   VK_IMAGE_VIEW_TYPE_2D,
												   VK_FORMAT_R8G8B8A8_UNORM,
												   VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}
}

void
Vulkan_Mod_ProcessTexture (texture_t *tx, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	if (!tx) {
		modelctx_t *mctx = Hunk_AllocName (sizeof (modelctx_t),
										   loadmodel->name);
		mctx->ctx = ctx;
		loadmodel->clear = vulkan_brush_clear;
		loadmodel->data = mctx;

		r_notexture_mip->render = &vulkan_notexture;
		load_textures (loadmodel, ctx);
		return;
	}

	vulktex_t    *tex = tx->render;
	tex->tex = (qfv_tex_t *) (tx + 1);
	VkExtent3D extent = { tx->width, tx->height, 1 };

	int layers = 1;
	if (strncmp (tx->name, "sky", 3) == 0) {
		layers = 2;
	}

	tex->tex->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
									   VK_FORMAT_R8G8B8A8_UNORM,
									   extent, 4, layers,
									   VK_SAMPLE_COUNT_1_BIT,
									   VK_IMAGE_USAGE_TRANSFER_DST_BIT
									   | VK_IMAGE_USAGE_SAMPLED_BIT);
	if (layers > 1) {
		// skys are unlit, so no fullbrights
		return;
	}

	const char *name = va ("fb_%s", tx->name);
	int         size = (tx->width * tx->height * 85) / 64;
	int         fullbright_mark = Hunk_LowMark ();
	byte       *pixels = Hunk_AllocName (size, name);

	if (!Mod_CalcFullbright ((byte *) (tx + 1), pixels, size)) {
		Hunk_FreeToLowMark (fullbright_mark);
		return;
	}
	tex->glow = tex->tex + 1;
	tex->glow->image = QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
										VK_FORMAT_R8G8B8A8_UNORM,
										extent, 4, 1,
										VK_SAMPLE_COUNT_1_BIT,
										VK_IMAGE_USAGE_TRANSFER_DST_BIT
										| VK_IMAGE_USAGE_SAMPLED_BIT);
	// store the pointer to the fullbright data: memory will never be set to
	// actual device memory because all of the textures will be loaded in one
	// big buffer
	tex->glow->memory = (VkDeviceMemory) pixels;
}

void
Vulkan_Mod_LoadLighting (bsp_t *bsp, vulkan_ctx_t *ctx)
{
	mod_lightmap_bytes = 1;
	if (!bsp->lightdatasize) {
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Hunk_AllocName (bsp->lightdatasize, loadname);
	memcpy (loadmodel->lightdata, bsp->lightdata, bsp->lightdatasize);
}
