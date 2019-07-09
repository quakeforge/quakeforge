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

static VulkanInstance_t *vulkan_instance;
static VulkanDevice_t *vulkan_device;

void
Vulkan_Init_Cvars ()
{
	vulkan_use_validation = Cvar_Get ("vulkan_use_validation", "1", CVAR_NONE,
									  0,
									  "enable LunarG Standard Validation "
									  "Layer if available (requires instance "
									  "restart).");
}

static const char *instance_extensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	0,
};

static const char *device_extensions[] = {
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

#define DEVICE_LEVEL_VULKAN_FUNCTION_EXTENSION(name) \
	dev->name = (PFN_##name) inst->vkGetDeviceProcAddr (dev->device, #name); \
	if (!dev->name) { \
		Sys_Printf ("Couldn't find device level function %s", #name); \
	}

#include "QF/Vulkan/funclist.h"
}

static VulkanDevice_t *
create_suitable_device (VulkanInstance_t *instance)
{
	for (uint32_t i = 0; i < instance->numDevices; i++) {
		VulkanPhysDevice_t *phys = &instance->devices[i];
		if (!Vulkan_ExtensionsSupported (phys->extensions, phys->numExtensions,
										 device_extensions)) {
			return 0;
		}
		int family = find_queue_family (phys, VK_QUEUE_GRAPHICS_BIT);
		if (family < 0) {
			return 0;
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
			0, 0,
			0, device_extensions,
			&features
		};
		memset (&features, 0, sizeof (features));
		VulkanDevice_t *device = calloc (1, sizeof (VulkanDevice_t));
		if (instance->vkCreateDevice (phys->device, &dCreateInfo, 0,
									  &device->device) == VK_SUCCESS) {
			load_device_funcs (instance, device);
			device->vkGetDeviceQueue (device->device, family,
									  0, &device->queue);
			return device;
		}
	}
	return 0;
}

int x = 1;

void
Vulkan_Init_Common (void)
{
	Sys_Printf ("Vulkan_Init_Common\n");
	Vulkan_Init_Cvars ();

	vulkan_instance = Vulkan_CreateInstance (PACKAGE_STRING, 0x000702ff, 0, instance_extensions);//FIXME version
	vulkan_device = create_suitable_device (vulkan_instance);
	if (!vulkan_device) {
		Sys_Error ("no suitable vulkan device found");
	}
	// only for now...
	Sys_Printf ("%p %p\n", vulkan_device->device, vulkan_device->queue);
	Vulkan_Shutdown_Common ();
	if (x)
	Sys_Quit();
}

void
Vulkan_Shutdown_Common (void)
{
	if (vulkan_device) {
		vulkan_device->vkDestroyDevice (vulkan_device->device, 0);
		free (vulkan_device);
		vulkan_device = 0;
	}
	Vulkan_DestroyInstance (vulkan_instance);
	vulkan_instance = 0;
	vulkan_ctx->unload_vulkan (vulkan_ctx);
}
