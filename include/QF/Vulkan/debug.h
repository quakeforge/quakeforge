#ifndef __QF_Vulkan_debug_h
#define __QF_Vulkan_debug_h

#define QFV_duCmdBeginLabel(device, cmd, name...)\
	do { \
		qfv_devfuncs_t *dfunc = device->funcs; \
		if (dfunc->vkCmdBeginDebugUtilsLabelEXT) { \
			VkDebugUtilsLabelEXT label = { \
				VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, 0, name \
			}; \
			dfunc->vkCmdBeginDebugUtilsLabelEXT (cmd, &label); \
		} \
	} while (0)

#define QFV_duCmdEndLabel(device, cmd) \
	do { \
		qfv_devfuncs_t *dfunc = device->funcs; \
		if (dfunc->vkCmdEndDebugUtilsLabelEXT) { \
			dfunc->vkCmdEndDebugUtilsLabelEXT (cmd); \
		} \
	} while (0)

#define QFV_duCmdInsertLabel(device, cmd, name...) \
	do { \
		qfv_devfuncs_t *dfunc = device->funcs; \
		if (dfunc->vkCmdInsertDebugUtilsLabelEXT) { \
			VkDebugUtilsLabelEXT label = { \
				VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, 0, name \
			}; \
			dfunc->vkCmdInsertDebugUtilsLabelEXT (cmd, &label); \
		} \
	} while (0)

#define QFV_duCreateMessenger(inst, severity, type, callback, data, messenger)\
	do { \
		if (inst->funcs->vkCreateDebugUtilsMessengerEXT) { \
			VkDebugUtilsMessengerCreateInfoEXT createInfo = { \
				VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, 0, 0,\
				severity, type, callback, data \
			}; \
			inst->funcs->vkCreateDebugUtilsMessengerEXT (inst, &createInfo, 0,\
														 messenger); \
		} \
	} while (0)

#define QFV_duDestroyMessenger(inst, messenger) \
	do { \
		if (inst->funcs->vkDestroyDebugUtilsMessengerEXT) { \
			inst->funcs->vkDestroyDebugUtilsMessengerEXT (inst, messenger, 0);\
		} \
	} while (0)

#define QFV_duQueueBeginLabel(device, queue, name...) \
	do { \
		qfv_devfuncs_t *dfunc = device->funcs; \
		if (dfunc->vkQueueBeginDebugUtilsLabelEXT) { \
			VkDebugUtilsLabelEXT label = { \
				VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, 0, name \
			}; \
			dfunc->vkQueueBeginDebugUtilsLabelEXT (queue, &label); \
		} \
	} while (0)

#define QFV_duQueueEndLabel(device, queue) \
	do { \
		qfv_devfuncs_t *dfunc = device->funcs; \
		if (dfunc->vkQueueEndDebugUtilsLabelEXT) { \
			dfunc->vkQueueEndDebugUtilsLabelEXT (queue); \
		} \
	} while (0)

#define QFV_duQueueInsertLabel(device, queue, name...) \
	do { \
		qfv_devfuncs_t *dfunc = device->funcs; \
		if (dfunc->vkQueueInsertDebugUtilsLabelEXT) { \
			VkDebugUtilsLabelEXT label = { \
				VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, 0, name \
			}; \
			dfunc->vkQueueInsertDebugUtilsLabelEXT (queue, &label); \
		} \
	} while (0)

#define QFV_duSetObjectName(device, type, handle, name) \
	do { \
		qfv_devfuncs_t *dfunc = device->funcs; \
		if (dfunc->vkSetDebugUtilsObjectNameEXT) { \
			VkDebugUtilsObjectNameInfoEXT nameInfo = { \
				VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, 0, \
				type, (VkObjectType) handle, name \
			}; \
			dfunc->vkSetDebugUtilsObjectNameEXT (device->dev, &nameInfo); \
		} \
	} while (0)

#define QFV_duSetObjectTag(device, type, handle, name, size, tag) \
	do { \
		qfv_devfuncs_t *dfunc = device->funcs; \
		if (dfunc->vkSetDebugUtilsObjectTagEXT) { \
			VkDebugUtilsObjectTagInfoEXT tagInfo = { \
				VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT, 0, \
				type, handle, name, size, tag \
			}; \
			dfunc->vkSetDebugUtilsObjectTagEXT (device->dev, &tagInfo); \
		} \
	} while (0)

#define QFV_duSubmitMessage(inst, severity, types, data) \
	do { \
		if (inst->funcs->vkSubmitDebugUtilsMessageEXT) { \
			inst->funcs->vkSubmitDebugUtilsMessageEXT (inst, severity, types, \
													   data); \
		} \
	} while (0)

#endif//__QF_Vulkan_debug_h
