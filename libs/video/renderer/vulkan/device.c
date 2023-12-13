/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

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

#include <string.h>

#include "QF/sys.h"

#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"

#include "vid_vulkan.h"

#include "util.h"

static int __attribute__((const))
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
find_queue_family (qfv_instance_t *instance, VkPhysicalDevice dev,
				   uint32_t flags)
{
	qfv_instfuncs_t *funcs = instance->funcs;
	uint32_t    numFamilies;
	VkQueueFamilyProperties *queueFamilies;
	int         best_diff = 32;
	uint32_t    family = -1;

	funcs->vkGetPhysicalDeviceQueueFamilyProperties (dev, &numFamilies, 0);
	queueFamilies = alloca (numFamilies * sizeof (*queueFamilies));
	funcs->vkGetPhysicalDeviceQueueFamilyProperties (dev, &numFamilies,
													 queueFamilies);

	for (uint32_t i = 0; i < numFamilies; i++) {
		VkQueueFamilyProperties *queue = &queueFamilies[i];

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
load_device_funcs (qfv_instance_t *inst, qfv_device_t *dev)
{
	qfv_instfuncs_t *ifunc = inst->funcs;
	qfv_devfuncs_t *dfunc = dev->funcs;
	VkDevice    device = dev->dev;
#define DEVICE_LEVEL_VULKAN_FUNCTION(name) \
	dfunc->name = (PFN_##name) ifunc->vkGetDeviceProcAddr (device, #name); \
	if (!dfunc->name) { \
		Sys_Error ("Couldn't find device level function %s", #name); \
	}

#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION(name, ext) \
	if (!ext || dev->extension_enabled (dev, ext)) { \
		dfunc->name = (PFN_##name) ifunc->vkGetDeviceProcAddr (device, #name); \
		if (!dfunc->name) { \
			Sys_MaskPrintf (SYS_vulkan_parse, \
							"Couldn't find device level function %s", #name); \
		} else { \
			Sys_MaskPrintf (SYS_vulkan_parse, \
							"Found device level function %s\n", #name); \
		} \
	}

#include "QF/Vulkan/funclist.h"
}

static int
device_extension_enabled (qfv_device_t *device, const char *ext)
{
	return strset_contains (device->enabled_extensions, ext);
}

qfv_device_t *
QFV_CreateDevice (vulkan_ctx_t *ctx, const char **extensions)
{
	uint32_t nlay = 1;	// ensure alloca doesn't see 0 and terminated
	uint32_t next = count_strings (extensions) + 1; // ensure terminated
	const char **lay = alloca (nlay * sizeof (const char *));
	const char **ext = alloca (next * sizeof (const char *));
	// ensure there are null pointers so merge_strings can act as append
	// since it does not add a null, but also make sure the counts reflect
	// actual numbers
	memset (lay, 0, nlay-- * sizeof (const char *));
	memset (ext, 0, next-- * sizeof (const char *));
	merge_strings (ext, extensions, 0);

	qfv_instance_t *inst = ctx->instance;
	qfv_instfuncs_t *ifunc = inst->funcs;

	for (uint32_t i = 0; i < inst->numDevices; i++) {
		VkPhysicalDevice physdev = inst->devices[i].dev;
		/*
		if (!Vulkan_ExtensionsSupported (phys->extensions, phys->numExtensions,
										 ext)) {
			continue;
		}
		*/
		int family = find_queue_family (inst, physdev, VK_QUEUE_GRAPHICS_BIT);
		if (family < 0) {
			continue;
		}
		if (!ctx->get_presentation_support (ctx, physdev, family)) {
			continue;
		}
		float priority = 1;
		VkDeviceQueueCreateInfo qCreateInfo = {
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, 0,
			family, 1, &priority
		};
		VkPhysicalDeviceVulkan12Features features12 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.hostQueryReset = 1,
		};
		VkPhysicalDeviceVulkan11Features features11 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			.pNext = &features12,
			.multiview = 1,
			.multiviewGeometryShader = 1,
		};
		VkPhysicalDeviceFeatures2 features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &features11,
			.features = {
				.imageCubeArray = 1,
				.independentBlend = 1,
				.geometryShader = 1,
				.multiViewport = 1,
				.fragmentStoresAndAtomics = 1,
				.fillModeNonSolid = 1,
			},
		};
		VkDeviceCreateInfo dCreateInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &features, 0,
			1, &qCreateInfo,
			nlay, lay,
			next, ext,
			0
		};
		qfv_device_t *device = calloc (1, sizeof (qfv_device_t)
										  + sizeof (qfv_devfuncs_t));
		device->funcs = (qfv_devfuncs_t *) (device + 1);
		if (ifunc->vkCreateDevice (physdev, &dCreateInfo, 0,
								   &device->dev) == VK_SUCCESS) {
			qfv_devfuncs_t *dfunc = device->funcs;
			device->enabled_extensions = new_strset (ext);
			device->extension_enabled = device_extension_enabled;

			device->physDev = &inst->devices[i];
			load_device_funcs (inst, device);
			device->queue.device = device;
			device->queue.queueFamily = family;
			dfunc->vkGetDeviceQueue (device->dev, family, 0,
									 &device->queue.queue);
			ctx->device = device;
			return device;
		}
		free (device);
	}
	return 0;
}

void
QFV_DestroyDevice (qfv_device_t *device)
{
	device->funcs->vkDestroyDevice (device->dev, 0);
	del_strset (device->enabled_extensions);
	free (device);
}

int
QFV_DeviceWaitIdle (qfv_device_t *device)
{
	qfZoneScoped (true);
	qfv_devfuncs_t *dfunc = device->funcs;
	return dfunc->vkDeviceWaitIdle (device->dev) == VK_SUCCESS;
}

VkFormat
QFV_FindSupportedFormat (qfv_device_t *device,
						 VkImageTiling tiling, VkFormatFeatureFlags features,
						 int numCandidates, const VkFormat *candidates)
{
	VkPhysicalDevice pdev = device->physDev->dev;
	qfv_instfuncs_t *ifuncs = device->physDev->instance->funcs;
	for (int i = 0; i < numCandidates; i++) {
		VkFormat    format = candidates[i];
		VkFormatProperties props;
		ifuncs->vkGetPhysicalDeviceFormatProperties (pdev, format, &props);
		if ((tiling == VK_IMAGE_TILING_LINEAR
			 && (props.linearTilingFeatures & features) == features)
			|| (tiling == VK_IMAGE_TILING_OPTIMAL
				&& (props.optimalTilingFeatures & features) == features)) {
			return format;
		}
	}
	Sys_Error ("no supported format");
}
