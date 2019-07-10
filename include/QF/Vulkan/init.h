/*
	Copyright (C) 2019 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2019/6/29

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
#ifndef __QF_Vulkan_init_h
#define __QF_Vulkan_init_h

#include <vulkan/vulkan.h>

#include "QF/qtypes.h"

typedef struct VulkanDevice_s {
	VkDevice device;
	int32_t queueFamily;
	VkQueue queue;
	VkSurfaceKHR surface;

#define DEVICE_LEVEL_VULKAN_FUNCTION(name) PFN_##name name;
#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION(name,ext) PFN_##name name;
#include "QF/Vulkan/funclist.h"
} VulkanDevice_t;

typedef struct {
	VkPhysicalDevice device;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceProperties properties;
	uint32_t    numLayers;
	VkLayerProperties *layers;
	uint32_t    numExtensions;
	VkExtensionProperties *extensions;
	VkPhysicalDeviceMemoryProperties memory;
	uint32_t    numQueueFamilies;
	VkQueueFamilyProperties *queueFamilies;
} VulkanPhysDevice_t;

typedef struct VulkanInstance_s {
	VkInstance  instance;
	struct strset_s *enabled_extensions;
	int         (*extension_enabled) (struct VulkanInstance_s *inst,
									  const char *ext);
	VkDebugUtilsMessengerEXT debug_callback;
	uint32_t    numDevices;
	VulkanPhysDevice_t *devices;

#define INSTANCE_LEVEL_VULKAN_FUNCTION(name) PFN_##name name;
#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION(name,ext) PFN_##name name;
#include "QF/Vulkan/funclist.h"
} VulkanInstance_t;

extern const char * const vulkanValidationLayers[];

void Vulkan_Init_Cvars (void);
struct vulkan_ctx_s;
VulkanInstance_t *Vulkan_CreateInstance (struct vulkan_ctx_s *ctx,
										 const char *appName,
										 uint32_t appVersion,
										 const char **layers,
										 const char **extensions);
void Vulkan_DestroyInstance (VulkanInstance_t *instance);
int Vulkan_ExtensionsSupported (const VkExtensionProperties *extensions,
								int numExtensions,
								const char * const *requested);
int Vulkan_LayersSupported (const VkLayerProperties *extensions,
								int numLayers,
								const char * const *requested);

#endif // __QF_Vulkan_init_h
