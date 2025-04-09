#ifndef __QF_Vulkan_device_h
#define __QF_Vulkan_device_h

#include "QF/Vulkan/qf_vid.h"

typedef struct qfv_devfuncs_s {
#define DEVICE_LEVEL_VULKAN_FUNCTION(name) PFN_##name name;
#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION(name,ext) PFN_##name name;
#include "QF/Vulkan/funclist.h"
} qfv_devfuncs_t;

typedef struct qfv_queue_s {
	struct qfv_device_s *device;

	uint32_t    queueFamily;
	VkQueue     queue;
} qfv_queue_t;

struct qfv_instance_s;
typedef struct qfv_device_s {
	VkDevice    dev;
	struct qfv_physdev_s *physDev;
	qfv_devfuncs_t *funcs;
	qfv_queue_t queue;
	struct strset_s *enabled_extensions;
	int         (*extension_enabled) (struct qfv_device_s *inst,
									  const char *ext);
} qfv_device_t;

struct vulkan_ctx_s;
qfv_device_t *QFV_CreateDevice (struct vulkan_ctx_s *ctx,
								const char **extensions);
void QFV_DestroyDevice (qfv_device_t *device);
int QFV_DeviceWaitIdle (qfv_device_t *device);

VkFormat QFV_FindSupportedFormat (qfv_device_t *device, VkImageTiling tiling,
								  VkFormatFeatureFlags features,
								  int numCandidates,
								  const VkFormat *candidates);

#endif//__QF_Vulkan_device_h
