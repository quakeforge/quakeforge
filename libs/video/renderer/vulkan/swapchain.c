#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_MATH_H
# include <math.h>
#endif

#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/cvars.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/swapchain.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"

qfv_swapchain_t *
QFV_CreateSwapchain (vulkan_ctx_t *ctx, VkSwapchainKHR old_swapchain)
{
	qfv_instfuncs_t *ifuncs = ctx->instance->funcs;
	qfv_devfuncs_t *dfuncs = ctx->device->funcs;
	qfv_queue_t *queue = &ctx->device->queue;

	VkBool32    supported;
	ifuncs->vkGetPhysicalDeviceSurfaceSupportKHR (ctx->device->physDev,
												  queue->queueFamily,
												  ctx->surface,
												  &supported);
	if (!supported) {
		Sys_Error ("unsupported surface for swapchain");
	}

	uint32_t    numModes;
	VkPresentModeKHR *modes;
	VkPresentModeKHR useMode = VK_PRESENT_MODE_FIFO_KHR;;
	ifuncs->vkGetPhysicalDeviceSurfacePresentModesKHR (ctx->device->physDev,
													   ctx->surface,
													   &numModes, 0);
	modes = alloca (numModes * sizeof (VkPresentModeKHR));
	ifuncs->vkGetPhysicalDeviceSurfacePresentModesKHR (ctx->device->physDev,
													   ctx->surface,
													   &numModes, modes);
	for (uint32_t i = 0; i < numModes; i++) {
		if ((int) modes[i] == vulkan_presentation_mode->int_val) {
			useMode = modes[i];
		}
	}
	Sys_MaskPrintf (SYS_VULKAN, "presentation mode: %d (%d)\n", useMode,
					vulkan_presentation_mode->int_val);

	VkSurfaceCapabilitiesKHR surfCaps;
	ifuncs->vkGetPhysicalDeviceSurfaceCapabilitiesKHR (ctx->device->physDev,
													   ctx->surface,
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
	ifuncs->vkGetPhysicalDeviceSurfaceFormatsKHR (ctx->device->physDev,
												  ctx->surface,
												  &numFormats, 0);
	VkSurfaceFormatKHR *formats = alloca (numFormats * sizeof (*formats));
	ifuncs->vkGetPhysicalDeviceSurfaceFormatsKHR (ctx->device->physDev,
												  ctx->surface,
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
		ctx->surface,
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
		old_swapchain
	};

	VkDevice device = ctx->device->dev;
	VkSwapchainKHR swapchain;
	dfuncs->vkCreateSwapchainKHR (device, &createInfo, 0, &swapchain);

	if (old_swapchain != swapchain) {
		dfuncs->vkDestroySwapchainKHR (device, old_swapchain, 0);
	}

	dfuncs->vkGetSwapchainImagesKHR (device, swapchain, &numImages, 0);
	qfv_swapchain_t *sc = malloc (sizeof (qfv_swapchain_t)
								  + numImages * sizeof (VkImage));
	sc->dev = device;
	sc->funcs = ctx->device->funcs;
	sc->surface = ctx->surface;
	sc->swapchain = swapchain;
	sc->numImages = numImages;
	sc->images = (VkImage *) (sc + 1);
	dfuncs->vkGetSwapchainImagesKHR (device, swapchain, &numImages,
									 sc->images);
	return sc;
}

void
QFV_DestroySwapchain (qfv_swapchain_t *swapchain)
{
	swapchain->funcs->vkDestroySwapchainKHR (swapchain->dev,
											 swapchain->swapchain, 0);
	free (swapchain);
}
