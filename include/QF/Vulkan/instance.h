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
#ifndef __QF_Vulkan_instance_h
#define __QF_Vulkan_instance_h

#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/qtypes.h"

typedef struct qfv_instfuncs_s {
#define INSTANCE_LEVEL_VULKAN_FUNCTION(name) PFN_##name name;
#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION(name,ext) PFN_##name name;
#include "QF/Vulkan/funclist.h"
} qfv_instfuncs_t;

typedef struct DARRAY_TYPE(const char *) qfv_debugstack_t;

typedef struct qfv_physdev_s {
	struct qfv_instance_s *instance;
	VkPhysicalDevice dev;
	VkPhysicalDeviceProperties2 p;
	VkPhysicalDeviceVulkan11Properties v11Properties;
	VkPhysicalDeviceVulkan12Properties v12Properties;
	VkPhysicalDeviceVulkan13Properties v13Properties;
	VkPhysicalDeviceMemoryProperties memory_properties;
} qfv_physdev_t;

typedef struct qfv_instance_s {
	VkInstance  instance;
	qfv_instfuncs_t *funcs;
	struct strset_s *enabled_extensions;
	int         (*extension_enabled) (struct qfv_instance_s *inst,
									  const char *ext);
	VkDebugUtilsMessengerEXT debug_handle;
	uint32_t    numDevices;
	qfv_physdev_t *devices;
	qfv_debugstack_t debug_stack;
} qfv_instance_t;

struct vulkan_ctx_s;
qfv_instance_t *QFV_CreateInstance (struct vulkan_ctx_s *ctx,
									const char *appName,
									uint32_t appVersion,
									const char **layers,
									const char **extensions);
void QFV_DestroyInstance (qfv_instance_t *instance);

VkSampleCountFlagBits QFV_GetMaxSampleCount (qfv_physdev_t *physdev);

#endif // __QF_Vulkan_instance_h
