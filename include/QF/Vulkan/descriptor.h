#ifndef __QF_Vulkan_descriptor_h
#define __QF_Vulkan_descriptor_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"

typedef struct qfv_bindingset_s
	DARRAY_TYPE (VkDescriptorSetLayoutBinding) qfv_bindingset_t;

typedef struct qfv_descriptorsetlayoutset_s
	DARRAY_TYPE (VkDescriptorSetLayout) qfv_descriptorsetlayoutset_t;

#define QFV_AllocDescriptorSetLayoutSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_descriptorsetlayoutset_t, num, allocator)

typedef struct qfv_descriptorsets_s
	DARRAY_TYPE (VkDescriptorSet) qfv_descriptorsets_t;

#define QFV_AllocDescriptorSets(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_descriptorsets_t, num, allocator)

typedef struct qfv_writedescriptorsets_s
	DARRAY_TYPE (VkWriteDescriptorSet) qfv_writedescriptorsets_t;

#define QFV_AllocWriteDescriptorSets(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_writedescriptorsets_t, num, allocator)

typedef struct qfv_copydescriptorsets_s
	DARRAY_TYPE (VkCopyDescriptorSet) qfv_copydescriptorsets_t;

#define QFV_AllocCopyDescriptorSets(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_descriptorsetlayoutset_t, num, allocator)

struct qfv_device_s;

VkSampler QFV_CreateSampler (struct qfv_device_s *device,
							 VkFilter magFilter, VkFilter minFilter,
							 VkSamplerMipmapMode mipmapMode,
							 VkSamplerAddressMode addressModeU,
							 VkSamplerAddressMode addressModeV,
							 VkSamplerAddressMode addressModeW,
							 float mipLodBias,
							 VkBool32 anisotryEnable, float maxAnisotropy,
							 VkBool32 compareEnable, VkCompareOp compareOp,
							 float minLod, float maxLod,
							 VkBorderColor borderColor,
							 VkBool32 unnormalizedCoordinates);

VkDescriptorSetLayout
QFV_CreateDescriptorSetLayout (struct qfv_device_s *device,
							   qfv_bindingset_t *bindings);

VkDescriptorPool
QFV_CreateDescriptorPool (struct qfv_device_s *device,
						  VkDescriptorPoolCreateFlags flags, uint32_t maxSets,
						  qfv_bindingset_t *bindings);

qfv_descriptorsets_t *
QFV_AllocateDescriptorSet (struct qfv_device_s *device,
						   VkDescriptorPool pool,
						   qfv_descriptorsetlayoutset_t *layouts);

#endif//__QF_Vulkan_descriptor_h
