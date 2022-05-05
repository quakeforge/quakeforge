#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_MATH_H
//# include <math.h>
#endif

#include "QF/cvar.h"
#include "QF/mathlib.h"

#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/swapchain.h"

#include "r_internal.h"
#include "vid_vulkan.h"

qfv_swapchain_t *
QFV_CreateSwapchain (vulkan_ctx_t *ctx, VkSwapchainKHR old_swapchain)
{
	qfv_instfuncs_t *ifuncs = ctx->instance->funcs;
	qfv_devfuncs_t *dfuncs = ctx->device->funcs;
	qfv_queue_t *queue = &ctx->device->queue;
	VkPhysicalDevice physDev = ctx->device->physDev->dev;

	VkBool32    supported;
	ifuncs->vkGetPhysicalDeviceSurfaceSupportKHR (physDev,
												  queue->queueFamily,
												  ctx->surface,
												  &supported);
	if (!supported) {
		Sys_Error ("unsupported surface for swapchain");
	}

	uint32_t    numModes;
	VkPresentModeKHR *modes;
	VkPresentModeKHR useMode = VK_PRESENT_MODE_FIFO_KHR;
	ifuncs->vkGetPhysicalDeviceSurfacePresentModesKHR (physDev,
													   ctx->surface,
													   &numModes, 0);
	modes = alloca (numModes * sizeof (VkPresentModeKHR));
	ifuncs->vkGetPhysicalDeviceSurfacePresentModesKHR (physDev,
													   ctx->surface,
													   &numModes, modes);
	for (uint32_t i = 0; i < numModes; i++) {
		if ((int) modes[i] == vulkan_presentation_mode) {
			useMode = modes[i];
		}
	}
	Sys_MaskPrintf (SYS_vulkan, "presentation mode: %d (%d)\n", useMode,
					vulkan_presentation_mode);

	VkSurfaceCapabilitiesKHR surfCaps;
	ifuncs->vkGetPhysicalDeviceSurfaceCapabilitiesKHR (physDev,
													   ctx->surface,
													   &surfCaps);
	uint32_t numImages = surfCaps.minImageCount + 1;
	if (surfCaps.maxImageCount > 0 && numImages > surfCaps.maxImageCount) {
		numImages = surfCaps.maxImageCount;
	}

	VkExtent2D imageSize = {ctx->window_width, ctx->window_height};
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
	Sys_MaskPrintf (SYS_vulkan, "%d [%d, %d]\n", numImages,
					imageSize.width, imageSize.height);

	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageUsage &= surfCaps.supportedUsageFlags;
	Sys_MaskPrintf (SYS_vulkan, "%x %x\n", imageUsage,
					surfCaps.supportedUsageFlags);

	VkSurfaceTransformFlagBitsKHR surfTransform
		= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	uint32_t numFormats;
	ifuncs->vkGetPhysicalDeviceSurfaceFormatsKHR (physDev,
												  ctx->surface,
												  &numFormats, 0);
	VkSurfaceFormatKHR *formats = alloca (numFormats * sizeof (*formats));
	ifuncs->vkGetPhysicalDeviceSurfaceFormatsKHR (physDev,
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

	VkDevice dev = ctx->device->dev;
	VkSwapchainKHR swapchain;
	dfuncs->vkCreateSwapchainKHR (dev, &createInfo, 0, &swapchain);

	if (old_swapchain != swapchain) {
		dfuncs->vkDestroySwapchainKHR (dev, old_swapchain, 0);
	}

	dfuncs->vkGetSwapchainImagesKHR (dev, swapchain, &numImages, 0);
	qfv_swapchain_t *sc = malloc (sizeof (qfv_swapchain_t));
	sc->device = ctx->device;
	sc->surface = ctx->surface;
	sc->swapchain = swapchain;
	sc->format = useFormat.format;
	sc->extent = imageSize;
	sc->numImages = numImages;
	sc->usage = imageUsage;
	sc->images = DARRAY_ALLOCFIXED (qfv_imageset_t, numImages, malloc);
	sc->imageViews = DARRAY_ALLOCFIXED (qfv_imageviewset_t, numImages, malloc);
	dfuncs->vkGetSwapchainImagesKHR (dev, swapchain, &numImages, sc->images->a);
	for (uint32_t i = 0; i < numImages; i++) {
		sc->imageViews->a[i]
			= QFV_CreateImageView (ctx->device, sc->images->a[i],
								   VK_IMAGE_VIEW_TYPE_2D, sc->format,
								   VK_IMAGE_ASPECT_COLOR_BIT);
	}
	return sc;
}

void
QFV_DestroySwapchain (qfv_swapchain_t *swapchain)
{
	qfv_device_t *device = swapchain->device;
	VkDevice dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	for (size_t i = 0; i < swapchain->imageViews->size; i++) {
		dfunc->vkDestroyImageView (dev, swapchain->imageViews->a[i], 0);
	}
	free (swapchain->images);
	free (swapchain->imageViews);
	dfunc->vkDestroySwapchainKHR (dev, swapchain->swapchain, 0);
	free (swapchain);
}

int
QFV_AcquireNextImage (qfv_swapchain_t *swapchain, VkSemaphore semaphore,
					  VkFence fence, uint32_t *imageIndex)
{
	qfv_device_t *device = swapchain->device;
	VkDevice dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;
	uint64_t timeout = 2000000000;
	*imageIndex = ~0u;
	VkResult res = dfunc->vkAcquireNextImageKHR (dev, swapchain->swapchain,
												 timeout, semaphore, fence,
												 imageIndex);
	switch (res) {
		case VK_SUCCESS:
		case VK_TIMEOUT:
		case VK_NOT_READY:
			return 1;
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
			return 0;
		default:
			Sys_Error ("vkAcquireNextImageKHR failed: %d", res);
	}
}
