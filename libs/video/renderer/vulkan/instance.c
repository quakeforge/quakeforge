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

#include <string.h>

#include "QF/cvar.h"
#include "QF/mathlib.h"

#include "QF/Vulkan/instance.h"

#include "vid_vulkan.h"

#include "util.h"

cvar_t *vulkan_use_validation;

static uint32_t numLayers;
static VkLayerProperties *instanceLayerProperties;
static strset_t *instanceLayers;

static uint32_t numExtensions;
static VkExtensionProperties *instanceExtensionProperties;
static strset_t *instanceExtensions;

const char * const vulkanValidationLayers[] = {
	"VK_LAYER_KHRONOS_validation",
	0,
};

static const char * const debugExtensions[] = {
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	0,
};

static void
get_instance_layers_and_extensions  (vulkan_ctx_t *ctx)
{
	uint32_t    i;
	VkLayerProperties *layers;
	VkExtensionProperties *extensions;

	ctx->vkEnumerateInstanceLayerProperties (&numLayers, 0);
	layers = malloc (numLayers * sizeof (VkLayerProperties));
	ctx->vkEnumerateInstanceLayerProperties (&numLayers, layers);
	instanceLayers = new_strset (0);
	for (i = 0; i < numLayers; i++) {
		strset_add (instanceLayers, layers[i].layerName);
	}

	ctx->vkEnumerateInstanceExtensionProperties (0, &numExtensions, 0);
	extensions = malloc (numExtensions * sizeof (VkLayerProperties));
	ctx->vkEnumerateInstanceExtensionProperties (0, &numExtensions,
														extensions);
	instanceExtensions = new_strset (0);
	for (i = 0; i < numExtensions; i++) {
		strset_add (instanceExtensions, extensions[i].extensionName);
	}

	if (developer->int_val & SYS_vulkan) {
		for (i = 0; i < numLayers; i++) {
			Sys_Printf ("%s %x %u %s\n",
						layers[i].layerName,
						layers[i].specVersion,
						layers[i].implementationVersion,
						layers[i].description);
		}
		for (i = 0; i < numExtensions; i++) {
			Sys_Printf ("%d %s\n",
						extensions[i].specVersion,
						extensions[i].extensionName);
		}
	}
	instanceLayerProperties = layers;
	instanceExtensionProperties = extensions;
}

static int
instance_extension_enabled (qfv_instance_t *inst, const char *ext)
{
	return strset_contains (inst->enabled_extensions, ext);
}

static int message_severities =
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
static int message_types =
	VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

static void
debug_breakpoint (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity)
{
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT messageType,
				const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
				void *data)
{
	qfv_instance_t *instance = data;
	if (!(messageSeverity & vulkan_use_validation->int_val)) {
		return 0;
	}
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
	fprintf (stderr, "validation layer: %s%s\n", msgSev,
			 callbackData->pMessage);
	for (size_t i = instance->debug_stack.size; i-- > 0; ) {
		fprintf (stderr, "    %s\n", instance->debug_stack.a[i]);
	}
	debug_breakpoint (messageSeverity);
	return VK_FALSE;
}

static void
setup_debug_callback (qfv_instance_t *instance)
{
	VkDebugUtilsMessengerEXT debug_handle;
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = message_severities,
		.messageType = message_types,
		.pfnUserCallback = debug_callback,
		.pUserData = instance,
	};
	if (!instance->funcs->vkCreateDebugUtilsMessengerEXT) {
		Sys_Printf ("Cound not set up Vulkan validation debug callback\n");
		return;
	}
	instance->funcs->vkCreateDebugUtilsMessengerEXT(instance->instance,
													&createInfo, 0,
													&debug_handle);
	instance->debug_handle = debug_handle;
}

static void
load_instance_funcs (vulkan_ctx_t *ctx)
{
	qfv_instance_t *instance = ctx->instance;
	qfv_instfuncs_t *funcs = instance->funcs;
	VkInstance inst = instance->instance;
#define INSTANCE_LEVEL_VULKAN_FUNCTION(name) \
	funcs->name = (PFN_##name) ctx->vkGetInstanceProcAddr (inst, #name); \
	if (!funcs->name) { \
		Sys_Error ("Couldn't find instance level function %s", #name); \
	}

#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION(name, ext) \
	if (instance->extension_enabled (instance, ext)) { \
		funcs->name = (PFN_##name) ctx->vkGetInstanceProcAddr (inst, #name); \
		if (!funcs->name) { \
			Sys_Error ("Couldn't find instance level function %s", #name); \
		} \
	}

#include "QF/Vulkan/funclist.h"
}

qfv_instance_t *
QFV_CreateInstance (vulkan_ctx_t *ctx,
					const char *appName, uint32_t appVersion,
					const char **layers, const char **extensions)
{
	VkApplicationInfo appInfo = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO, 0,
		appName, appVersion,
		PACKAGE_STRING, 0x000702ff, //FIXME version
		VK_API_VERSION_1_1,
	};
	VkInstanceCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0,
		&appInfo,
		0, 0,
		0, 0,
	};
	VkResult    res;
	VkInstance  instance;

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
	prune_strings (instanceLayers, lay, &nlay);
	prune_strings (instanceExtensions, ext, &next);
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
	qfv_instance_t *inst = calloc (1, sizeof(qfv_instance_t)
								   + sizeof (qfv_instfuncs_t));
	inst->instance = instance;
	inst->funcs = (qfv_instfuncs_t *)(inst + 1);
	inst->enabled_extensions = new_strset (ext);
	inst->extension_enabled = instance_extension_enabled;
	DARRAY_INIT (&inst->debug_stack, 8);
	ctx->instance = inst;
	load_instance_funcs (ctx);

	if (vulkan_use_validation->int_val) {
		setup_debug_callback (inst);
	}

	qfv_instfuncs_t *ifunc = inst->funcs;
	ifunc->vkEnumeratePhysicalDevices (instance, &inst->numDevices, 0);
	inst->devices = malloc (inst->numDevices * sizeof (*inst->devices));
	VkPhysicalDevice *devices = alloca (inst->numDevices * sizeof (*devices));
	ifunc->vkEnumeratePhysicalDevices (instance, &inst->numDevices, devices);
	for (uint32_t i = 0; i < inst->numDevices; i++) {
		VkPhysicalDevice physDev = devices[i];
		qfv_physdev_t *dev = &inst->devices[i];
		dev->instance = inst;
		dev->dev = physDev;
		ifunc->vkGetPhysicalDeviceProperties (physDev, &dev->properties);
		ifunc->vkGetPhysicalDeviceMemoryProperties (physDev,
													&dev->memory_properties);
	}
	return inst;
}

void
QFV_DestroyInstance (qfv_instance_t *instance)
{
	qfv_instfuncs_t *ifunc = instance->funcs;

	if (instance->debug_handle) {
		ifunc->vkDestroyDebugUtilsMessengerEXT (instance->instance,
												instance->debug_handle, 0);
	}
	instance->funcs->vkDestroyInstance (instance->instance, 0);
	del_strset (instance->enabled_extensions);
	free (instance->devices);
	free (instance);
}

VkSampleCountFlagBits
QFV_GetMaxSampleCount (qfv_physdev_t *physdev)
{
	VkSampleCountFlagBits maxSamples = VK_SAMPLE_COUNT_64_BIT;
	VkSampleCountFlagBits counts;
	counts = min (physdev->properties.limits.framebufferColorSampleCounts,
				  physdev->properties.limits.framebufferDepthSampleCounts);
	while (maxSamples && maxSamples > counts) {
		maxSamples >>= 1;
	}
	Sys_MaskPrintf (SYS_vulkan, "Max samples: %x (%d)\n", maxSamples, counts);
	return maxSamples;
}
