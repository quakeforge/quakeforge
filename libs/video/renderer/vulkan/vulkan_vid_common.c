/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2019      Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/init.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

static cvar_t *vulkan_presentation_mode;

static void
vulkan_presentation_mode_f (cvar_t *var)
{
	if (!strcmp (var->string, "immediate")) {
		var->int_val = VK_PRESENT_MODE_IMMEDIATE_KHR;
	} else if (!strcmp (var->string, "fifo")) {
		var->int_val = VK_PRESENT_MODE_FIFO_KHR;
	} else if (!strcmp (var->string, "relaxed")) {
		var->int_val = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	} else if (!strcmp (var->string, "mailbox")) {
		var->int_val = VK_PRESENT_MODE_MAILBOX_KHR;
	} else {
		Sys_Printf ("Invalid presentation mode, using fifo\n");
		var->int_val = VK_PRESENT_MODE_FIFO_KHR;
	}
}

void
Vulkan_Init_Cvars ()
{
	vulkan_use_validation = Cvar_Get ("vulkan_use_validation", "1", CVAR_NONE,
									  0,
									  "enable LunarG Standard Validation "
									  "Layer if available (requires instance "
									  "restart).");
	// FIXME implement fallback choices (instead of just fifo)
	vulkan_presentation_mode = Cvar_Get ("vulkan_presentation_mode", "mailbox",
										 CVAR_NONE, vulkan_presentation_mode_f,
										 "desired presentation mode (may fall "
										 "back to fifo).");
}

static const char *instance_extensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	0,
};

static const char *device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	0,
};

static int
count_bits (uint32_t val)
{
	int         bits = 0;
	while (val) {
		bits += val & 1;
		val >>= 1;
	}
	return bits;
}

static int
find_queue_family (VulkanPhysDevice_t *dev, uint32_t flags)
{
	int         best_diff = 32;
	uint32_t    family = -1;

	for (uint32_t i = 0; i < dev->numQueueFamilies; i++) {
		VkQueueFamilyProperties *queue = &dev->queueFamilies[i];

		if ((queue->queueFlags & flags) == flags) {
			int diff = count_bits (queue->queueFlags & ~flags);
			if (diff < best_diff) {
				best_diff = diff;
				family = i;
			}
		}
	}
	return family;
}

static void
load_device_funcs (VulkanInstance_t *inst, VulkanDevice_t *dev)
{
#define DEVICE_LEVEL_VULKAN_FUNCTION(name) \
	dev->name = (PFN_##name) inst->vkGetDeviceProcAddr (dev->device, #name); \
	if (!dev->name) { \
		Sys_Error ("Couldn't find device level function %s", #name); \
	}

#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION(name, ext) \
	if (inst->extension_enabled (inst, ext)) { \
		dev->name = (PFN_##name) inst->vkGetDeviceProcAddr (dev->device, \
															#name); \
		if (!dev->name) { \
			Sys_Printf ("Couldn't find device level function %s", #name); \
		} \
	}

#include "QF/Vulkan/funclist.h"
}

void
Vulkan_Init_Common (vulkan_ctx_t *ctx)
{
	Sys_Printf ("Vulkan_Init_Common\n");
	Vulkan_Init_Cvars ();

	ctx->vtx = Vulkan_CreateInstance (ctx, PACKAGE_STRING, 0x000702ff, 0, instance_extensions);//FIXME version
	ctx->instance = ctx->vtx->instance;
}

void
Vulkan_Shutdown_Common (vulkan_ctx_t *ctx)
{
	Vulkan_DestroyInstance (ctx->vtx);
	ctx->vtx = 0;
	ctx->unload_vulkan (ctx);
}

void
Vulkan_CreateDevice (vulkan_ctx_t *ctx)
{
	VulkanInstance_t *inst = ctx->vtx;

	uint32_t nlay = 1;	// ensure alloca doesn't see 0 and terminated
	uint32_t next = count_strings (device_extensions) + 1; // ensure terminated
	if (vulkan_use_validation->int_val) {
		nlay += count_strings (vulkanValidationLayers);
	}
	const char **lay = alloca (nlay * sizeof (const char *));
	const char **ext = alloca (next * sizeof (const char *));
	// ensure there are null pointers so merge_strings can act as append
	// since it does not add a null, but also make sure the counts reflect
	// actual numbers
	memset (lay, 0, nlay-- * sizeof (const char *));
	memset (ext, 0, next-- * sizeof (const char *));
	merge_strings (ext, device_extensions, 0);
	if (vulkan_use_validation->int_val) {
		merge_strings (lay, lay, vulkanValidationLayers);
	}

	for (uint32_t i = 0; i < inst->numDevices; i++) {
		VulkanPhysDevice_t *phys = &inst->devices[i];
		if (!Vulkan_LayersSupported (phys->layers, phys->numLayers, lay)) {
			continue;
		}
		if (!Vulkan_ExtensionsSupported (phys->extensions, phys->numExtensions,
										 ext)) {
			continue;
		}
		int family = find_queue_family (phys, VK_QUEUE_GRAPHICS_BIT);
		if (family < 0) {
			continue;
		}
		if (!ctx->get_presentation_support (ctx, phys->device, family)) {
			continue;
		}
		float priority = 1;
		VkDeviceQueueCreateInfo qCreateInfo = {
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, 0,
			family, 1, &priority
		};
		VkPhysicalDeviceFeatures features;
		VkDeviceCreateInfo dCreateInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, 0, 0,
			1, &qCreateInfo,
			nlay, lay,
			next, ext,
			&features
		};
		memset (&features, 0, sizeof (features));
		VulkanDevice_t *device = calloc (1, sizeof (VulkanDevice_t));
		if (inst->vkCreateDevice (phys->device, &dCreateInfo, 0,
									  &device->device) == VK_SUCCESS) {
			load_device_funcs (inst, device);
			device->vkGetDeviceQueue (device->device, family,
									  0, &device->queue);
			ctx->dev = device;
			ctx->device = device->device;
			ctx->physDevice = phys->device;
			device->queueFamily = family;
			return;
		}
	}
}

void
Vulkan_CreateSwapchain (vulkan_ctx_t *ctx)
{
	VkBool32    supported;
	ctx->vtx->vkGetPhysicalDeviceSurfaceSupportKHR (ctx->physDevice,
													ctx->dev->queueFamily,
													ctx->dev->surface,
													&supported);
	if (!supported) {
		Sys_Error ("unsupported surface for swapchain");
	}
	uint32_t    numModes;
	VkPresentModeKHR *modes;
	VkPresentModeKHR useMode = VK_PRESENT_MODE_FIFO_KHR;;
	ctx->vtx->vkGetPhysicalDeviceSurfacePresentModesKHR (ctx->physDevice,
														 ctx->dev->surface,
														 &numModes, 0);
	modes = alloca (numModes * sizeof (VkPresentModeKHR));
	ctx->vtx->vkGetPhysicalDeviceSurfacePresentModesKHR (ctx->physDevice,
														 ctx->dev->surface,
														 &numModes, modes);
	for (uint32_t i = 0; i < numModes; i++) {
		if ((int) modes[i] == vulkan_presentation_mode->int_val) {
			useMode = modes[i];
		}
	}
	Sys_MaskPrintf (SYS_VULKAN, "presentation mode: %d (%d)\n", useMode,
					vulkan_presentation_mode->int_val);

	VkSurfaceCapabilitiesKHR surfCaps;
	ctx->vtx->vkGetPhysicalDeviceSurfaceCapabilitiesKHR (ctx->physDevice,
														 ctx->dev->surface,
														 &surfCaps);
	uint32_t numImages = surfCaps.minImageCount + 1;
	if (surfCaps.maxImageCount > 0 && numImages > surfCaps.maxImageCount) {
		numImages = surfCaps.maxImageCount;
	}

	VkExtent2D imageSize = {viddef.width, viddef.height};
	if (surfCaps.currentExtent.width == ~0u) {
		imageSize.width = bound (surfCaps.minImageExtent.width,
								 imageSize.width,
								 surfCaps.maxImageExtent.width);
		imageSize.height = bound (surfCaps.minImageExtent.height,
								  imageSize.height,
								  surfCaps.maxImageExtent.height);
	} else {
		imageSize = surfCaps.currentExtent;
	}
	Sys_MaskPrintf (SYS_VULKAN, "%d [%d, %d]\n", numImages,
					imageSize.width, imageSize.height);

	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageUsage &= surfCaps.supportedUsageFlags;

	VkSurfaceTransformFlagBitsKHR surfTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	uint32_t numFormats;
	ctx->vtx->vkGetPhysicalDeviceSurfaceFormatsKHR (ctx->physDevice,
													ctx->dev->surface,
													&numFormats, 0);
	VkSurfaceFormatKHR *formats = alloca (numFormats * sizeof (*formats));
	ctx->vtx->vkGetPhysicalDeviceSurfaceFormatsKHR (ctx->physDevice,
													ctx->dev->surface,
													&numFormats, formats);
	VkSurfaceFormatKHR useFormat = {VK_FORMAT_R8G8B8A8_UNORM,
									VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	if (numFormats > 1) {
		uint32_t i;
		for (i = 0; i < numFormats; i++) {
			if (formats[i].format == useFormat.format
				&& formats[i].colorSpace == useFormat.colorSpace) {
				break;
			}
		}
		if (i == numFormats) {
			useFormat = formats[0];
		}
	} else if (numFormats == 1 && formats[0].format != VK_FORMAT_UNDEFINED) {
		useFormat = formats[0];
	}

	VkSwapchainCreateInfoKHR createInfo = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, 0, 0,
		ctx->dev->surface,
		numImages,
		useFormat.format, useFormat.colorSpace,
		imageSize,
		1, // array layers
		imageUsage,
		VK_SHARING_MODE_EXCLUSIVE,
		0, 0,
		surfTransform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		useMode,
		VK_TRUE,
		ctx->swapchain
	};
	VkSwapchainKHR swapchain;
	ctx->dev->vkCreateSwapchainKHR (ctx->device, &createInfo, 0, &swapchain);
	if (ctx->swapchain != swapchain) {
		ctx->dev->vkDestroySwapchainKHR (ctx->device, ctx->swapchain, 0);
	}
	ctx->swapchain = swapchain;

	ctx->dev->vkGetSwapchainImagesKHR (ctx->device, swapchain, &numImages, 0);
	ctx->swapchainImages = malloc (numImages * sizeof (*ctx->swapchainImages));
	ctx->dev->vkGetSwapchainImagesKHR (ctx->device, swapchain, &numImages,
									   ctx->swapchainImages);
	ctx->numSwapchainImages = numImages;
}
