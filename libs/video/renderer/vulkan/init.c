/*
	init.c

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
#include "QF/Vulkan/init.h"

#include "vid_vulkan.h"

#include "util.h"

cvar_t *vulkan_use_validation;

static uint32_t numLayers;
static VkLayerProperties *instanceLayerProperties;
static const char **instanceLayerNames;
static uint32_t numExtensions;
static VkExtensionProperties *instanceExtensionProperties;
static const char **instanceExtensionNames;

const char * const vulkanValidationLayers[] = {
	"VK_LAYER_LUNARG_standard_validation",
	0,
};

static const char * const debugExtensions[] = {
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	0,
};

static const char * const device_types[] = {
	"other",
	"integrated gpu",
	"discrete gpu",
	"virtual gpu",
	"cpu",
};

static void
get_instance_layers_and_extensions  (vulkan_ctx_t *ctx)
{
	uint32_t    i;
	VkLayerProperties *properties;
	VkExtensionProperties *extensions;

	ctx->vkEnumerateInstanceLayerProperties (&numLayers, 0);
	properties = malloc (numLayers * sizeof (VkLayerProperties));
	ctx->vkEnumerateInstanceLayerProperties (&numLayers, properties);
	instanceLayerNames = (const char **) malloc ((numLayers + 1)
												 * sizeof (const char **));
	for (i = 0; i < numLayers; i++) {
		instanceLayerNames[i] = properties[i].layerName;
	}
	instanceLayerNames[i] = 0;

	ctx->vkEnumerateInstanceExtensionProperties (0, &numExtensions, 0);
	extensions = malloc (numExtensions * sizeof (VkLayerProperties));
	ctx->vkEnumerateInstanceExtensionProperties (0, &numExtensions,
														extensions);
	instanceExtensionNames = (const char **) malloc ((numExtensions + 1)
													 * sizeof (const char **));
	for (i = 0; i < numExtensions; i++) {
		instanceExtensionNames[i] = extensions[i].extensionName;
	}
	instanceExtensionNames[i] = 0;

	if (developer->int_val & SYS_VULKAN) {
		for (i = 0; i < numLayers; i++) {
			Sys_Printf ("%s %x %u %s\n",
						properties[i].layerName,
						properties[i].specVersion,
						properties[i].implementationVersion,
						properties[i].description);
		}
		for (i = 0; i < numExtensions; i++) {
			Sys_Printf ("%d %s\n",
						extensions[i].specVersion,
						extensions[i].extensionName);
		}
	}
	instanceLayerProperties = properties;
	instanceExtensionProperties = extensions;
}

static void
init_physdev (VulkanInstance_t *instance, VkPhysicalDevice dev, VulkanPhysDevice_t *physdev)
{
	physdev->device = dev;

	instance->vkGetPhysicalDeviceProperties (dev, &physdev->properties);

	instance->vkEnumerateDeviceLayerProperties (dev, &physdev->numLayers, 0);
	physdev->layers = malloc (physdev->numLayers * sizeof (VkLayerProperties));
	instance->vkEnumerateDeviceLayerProperties (dev, &physdev->numLayers,
												physdev->layers);

	instance->vkEnumerateDeviceExtensionProperties (dev, 0,
													&physdev->numExtensions,
													0);
	physdev->extensions = malloc (physdev->numExtensions
								  * sizeof (VkExtensionProperties));
	instance->vkEnumerateDeviceExtensionProperties (dev, 0,
													&physdev->numExtensions,
													physdev->extensions);

	instance->vkGetPhysicalDeviceFeatures (dev, &physdev->features);

	instance->vkGetPhysicalDeviceMemoryProperties (dev, &physdev->memory);

	instance->vkGetPhysicalDeviceQueueFamilyProperties (dev,
														&physdev->numQueueFamilies,
														0);
	physdev->queueFamilies = malloc (physdev->numQueueFamilies
								  * sizeof (VkQueueFamilyProperties));
	instance->vkGetPhysicalDeviceQueueFamilyProperties (dev,
														&physdev->numQueueFamilies,
														physdev->queueFamilies);

	if (developer->int_val & SYS_VULKAN) {
		VkPhysicalDeviceProperties *prop = &physdev->properties;
		Sys_Printf ("dev: %p\n", dev);
		Sys_Printf ("  %x %x\n", prop->apiVersion, prop->driverVersion);
		Sys_Printf ("  %x %x\n", prop->vendorID, prop->deviceID);
		Sys_Printf ("  %s: %s\n", device_types[prop->deviceType],
					prop->deviceName);
		for (uint32_t i = 0; i < physdev->numLayers; i++) {
			Sys_Printf ("  %s %x %u %s\n",
						physdev->layers[i].layerName,
						physdev->layers[i].specVersion,
						physdev->layers[i].implementationVersion,
						physdev->layers[i].description);
		}
		for (uint32_t i = 0; i < physdev->numExtensions; i++) {
			Sys_Printf ("  %u %s\n",
						physdev->extensions[i].specVersion,
						physdev->extensions[i].extensionName);
		}
		Sys_Printf ("  memory types:\n");
		for (uint32_t i = 0; i < physdev->memory.memoryTypeCount; i++) {
			Sys_Printf ("    %x %d\n",
						physdev->memory.memoryTypes[i].propertyFlags,
						physdev->memory.memoryTypes[i].heapIndex);
		}
		Sys_Printf ("  memory heaps:\n");
		for (uint32_t i = 0; i < physdev->memory.memoryHeapCount; i++) {
			Sys_Printf ("    %x %ld\n",
						physdev->memory.memoryHeaps[i].flags,
						physdev->memory.memoryHeaps[i].size);
		}
		Sys_Printf ("  queue families:\n");
		for (uint32_t i = 0; i < physdev->numQueueFamilies; i++) {
			VkQueueFamilyProperties *queue = &physdev->queueFamilies[i];
			VkExtent3D  gran = queue->minImageTransferGranularity;
			Sys_Printf ("    %x %3d %3d [%d %d %d]\n",
						queue->queueFlags,
						queue->queueCount,
						queue->timestampValidBits,
						gran.width, gran.height, gran.depth);
		}
	}
}

static int
instance_extension_enabled (VulkanInstance_t *inst, const char *ext)
{
	return strset_contains (inst->enabled_extensions, ext);
}

static int message_severities =
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
static int message_types =
	VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT messageType,
				const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
				void *data)
{
	const char *msgSev = "";
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		msgSev = "verbose: ";
	}
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		msgSev = "info: ";
	}
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		msgSev = "warning: ";
	}
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		msgSev = "error: ";
	}
	fprintf (stderr, "validation layer: %s%s\n", msgSev, callbackData->pMessage);
	return VK_FALSE;
}

static void
setup_debug_callback (VulkanInstance_t *instance)
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = message_severities,
		.messageType = message_types,
		.pfnUserCallback = debug_callback,
		.pUserData = instance,
	};
	instance->vkCreateDebugUtilsMessengerEXT(instance->instance, &createInfo,
											 0, &instance->debug_callback);
}

static void
load_instance_funcs (vulkan_ctx_t *ctx)
{
	VulkanInstance_t *vtx = ctx->vtx;
	VkInstance instance = vtx->instance;
#define INSTANCE_LEVEL_VULKAN_FUNCTION(name) \
	vtx->name = (PFN_##name) ctx->vkGetInstanceProcAddr (instance, #name); \
	if (!vtx->name) { \
		Sys_Error ("Couldn't find instance level function %s", #name); \
	}

#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION(name, ext) \
	if (vtx->extension_enabled (vtx, ext)) { \
		vtx->name = (PFN_##name) ctx->vkGetInstanceProcAddr (instance, \
															 #name); \
		if (!vtx->name) { \
			Sys_Error ("Couldn't find instance level function %s", #name); \
		} \
	}

#include "QF/Vulkan/funclist.h"
}

VulkanInstance_t *
Vulkan_CreateInstance (vulkan_ctx_t *ctx,
					   const char *appName, uint32_t appVersion,
					   const char **layers, const char **extensions)
{
	VkApplicationInfo appInfo = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO, 0,
		appName, appVersion,
		PACKAGE_STRING, 0x000702ff, //FIXME version
		VK_API_VERSION_1_0,
	};
	VkInstanceCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0,
		&appInfo,
		0, 0,
		0, 0,
	};
	VkResult    res;
	VkInstance  instance;
	uint32_t    numDev, i;
	VkPhysicalDevice *devices;
	VulkanInstance_t *inst;

	if (!instanceLayerProperties) {
		get_instance_layers_and_extensions (ctx);
	}

	uint32_t nlay = count_strings (layers) + 1;
	uint32_t next = count_strings (extensions)
					+ count_strings (ctx->required_extensions) + 1;
	if (vulkan_use_validation->int_val) {
		nlay += count_strings (vulkanValidationLayers);
		next += count_strings (debugExtensions);
	}
	const char **lay = alloca (nlay * sizeof (const char *));
	const char **ext = alloca (next * sizeof (const char *));
	// ensure there are null pointers so merge_strings can act as append
	// since it does not add a null
	memset (lay, 0, nlay-- * sizeof (const char *));
	memset (ext, 0, next-- * sizeof (const char *));
	merge_strings (lay, layers, 0);
	merge_strings (ext, extensions, ctx->required_extensions);
	if (vulkan_use_validation->int_val) {
		merge_strings (lay, lay, vulkanValidationLayers);
		merge_strings (ext, ext, debugExtensions);
	}
	prune_strings (instanceLayerNames, lay, &nlay);
	prune_strings (instanceExtensionNames, ext, &next);
	lay[nlay] = 0;
	ext[next] = 0;
	createInfo.enabledLayerCount = nlay;
	createInfo.ppEnabledLayerNames = lay;
	createInfo.enabledExtensionCount = next;
	createInfo.ppEnabledExtensionNames = ext;

	res = ctx->vkCreateInstance (&createInfo, 0, &instance);
	if (res != VK_SUCCESS) {
		Sys_Error ("unable to create vulkan instance\n");
	}
	inst = malloc (sizeof(VulkanInstance_t));
	inst->instance = instance;
	inst->enabled_extensions = new_strset (ext);
	inst->extension_enabled = instance_extension_enabled;
	ctx->extension_enabled = instance_extension_enabled;
	ctx->vtx = inst;
	load_instance_funcs (ctx);

	if (vulkan_use_validation->int_val) {
		setup_debug_callback (inst);
	}

	res = inst->vkEnumeratePhysicalDevices (instance, &numDev, 0);
	if (res != VK_SUCCESS) {
		Sys_Error ("unable to query vulkan device count: %d\n", res);
	}
	devices = malloc(numDev * sizeof (VkPhysicalDevice));
	res = inst->vkEnumeratePhysicalDevices (instance, &numDev, devices);
	if (res != VK_SUCCESS) {
		Sys_Error ("unable to query vulkan device properties: %d\n", res);
	}

	inst->numDevices = numDev;
	inst->devices = malloc (numDev * sizeof (VulkanPhysDevice_t));

	for (i = 0; i < numDev; i++) {
		init_physdev (inst, devices[i], &inst->devices[i]);
	}

	return inst;
}

void
Vulkan_DestroyInstance (VulkanInstance_t *instance)
{
	for (uint32_t i = 0; i < instance->numDevices; i++) {
		free (instance->devices[i].queueFamilies);
		free (instance->devices[i].extensions);
		free (instance->devices[i].layers);
	}
	free (instance->devices);
	instance->vkDestroyInstance (instance->instance, 0);
	free (instance);
}

int
Vulkan_LayersSupported (const VkLayerProperties *layers,
						int numLayers,
						const char * const *requested)
{
	while (*requested) {
		int         i;
		for (i = 0; i < numLayers; i++) {
			if (!strcmp (*requested, layers[i].layerName)) {
				break;
			}
		}
		if (i == numLayers) {
			// requested layer not found
			break;
		}
		requested++;
	}
	return !*requested;
}

int
Vulkan_ExtensionsSupported (const VkExtensionProperties *extensions,
							int numExtensions,
							const char * const *requested)
{
	while (*requested) {
		int         i;
		for (i = 0; i < numExtensions; i++) {
			if (!strcmp (*requested, extensions[i].extensionName)) {
				break;
			}
		}
		if (i == numExtensions) {
			// requested extension not found
			break;
		}
		requested++;
	}
	return !*requested;
}
