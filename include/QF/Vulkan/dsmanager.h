#ifndef __QF_Vulkan_dsmanager_h
#define __QF_Vulkan_dsmanager_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/darray.h"

typedef struct qfv_descriptorpoolset_s
	DARRAY_TYPE (VkDescriptorPool) qfv_descriptorpoolset_t;
typedef struct qfv_descriptorsetset_s
	DARRAY_TYPE (VkDescriptorSet) qfv_descriptorsetset_t;

typedef struct qfv_dsmanager_s {
	const char *name;
	struct qfv_device_s *device;
	VkDescriptorPoolCreateInfo poolCreateInfo;
	VkDescriptorPool activePool;
	qfv_descriptorpoolset_t freePools;
	qfv_descriptorpoolset_t usedPools;
	qfv_descriptorsetset_t freeSets;
	VkDescriptorSetLayout layout;
} qfv_dsmanager_t;

struct qfv_descriptorsetlayoutinfo_s;

qfv_dsmanager_t *
QFV_DSManager_Create (const struct qfv_descriptorsetlayoutinfo_s *setLayoutInfo,
					  uint32_t maxSets, struct vulkan_ctx_s *ctx);
void QFV_DSManager_Destroy (qfv_dsmanager_t *setManager);
VkDescriptorSet QFV_DSManager_AllocSet (qfv_dsmanager_t *setManager);
void QFV_DSManager_FreeSet (qfv_dsmanager_t *setManager, VkDescriptorSet set);

#endif//__QF_Vulkan_dsmanager_h
