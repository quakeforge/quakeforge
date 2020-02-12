#ifndef __QF_Vulkan_descriptor_h
#define __QF_Vulkan_descriptor_h

typedef struct qfv_sampler_s {
	struct qfv_device_s *device;
	VkSampler   sampler;
} qfv_sampler_t;

typedef struct qfv_bindingset_s {
	int         numBindings;
	VkDescriptorSetLayoutBinding bindings[];
} qfv_bindingset_t;

typedef struct qfv_descriptorsetlayout_s {
	struct qfv_device_s *device;
	VkDescriptorSetLayout layout;
} qfv_descriptorsetlayout_t;

typedef struct qfv_descriptorsetlayoutset_s {
	uint32_t    numLayouts;
	qfv_descriptorsetlayout_t *layouts[];
} qfv_descriptorsetlayoutset_t;

#define QFV_AllocDescriptorSetLayoutSet(num, allocator) \
    allocator (field_offset (qfv_descriptorsetlayoutset_t, layouts[num]))


typedef struct qfv_descriptorpool_s {
	struct qfv_device_s *device;
	VkDescriptorPool pool;
} qfv_descriptorpool_t;

typedef struct qfv_descriptorset_s {
	struct qfv_device_s *device;
	qfv_descriptorpool_t *pool;
	VkDescriptorSet set;
} qfv_descriptorset_t;

typedef struct qfv_imagedescriptorinfo_s {
	qfv_descriptorset_t *descriptorset;
	uint32_t    binding;
	uint32_t    arrayElement;
	VkDescriptorType type;
	uint32_t    numInfo;
	VkDescriptorImageInfo infos[];
} qfv_imagedescriptorinfo_t;

typedef struct qfv_bufferdescriptorinfo_s {
	qfv_descriptorset_t *descriptorset;
	uint32_t    binding;
	uint32_t    arrayElement;
	VkDescriptorType type;
	uint32_t    numInfo;
	VkDescriptorBufferInfo infos[];
} qfv_bufferdescriptorinfo_t;

typedef struct qfv_texelbufferdescriptorinfo_s {
	qfv_descriptorset_t *descriptorset;
	uint32_t    binding;
	uint32_t    arrayElement;
	VkDescriptorType type;
	uint32_t    numInfo;
	VkBufferView infos[];
} qfv_texelbufferdescriptorinfo_t;

typedef struct qfv_copydescriptorinfo_s {
	qfv_descriptorset_t *dstSet;
	uint32_t    dstBinding;
	uint32_t    dstArrayElement;
	qfv_descriptorset_t *srcSet;
	uint32_t    srcBinding;
	uint32_t    srcArrayElement;
	uint32_t    descriptorCount;
} qfv_copydescriptorinfo_t;

qfv_sampler_t *QFV_CreateSampler (struct qfv_device_s *device,
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

qfv_bindingset_t *QFV_CreateBindingSet (int numBindings);
void QFV_DestroyBindingSet (qfv_bindingset_t *bindingset);

qfv_descriptorsetlayout_t *
QFV_CreateDescriptorSetLayout (struct qfv_device_s *device,
							   qfv_bindingset_t *bindings);

qfv_descriptorpool_t *
QFV_CreateDescriptorPool (struct qfv_device_s *device,
						  VkDescriptorPoolCreateFlags flags, uint32_t maxSets,
						  qfv_bindingset_t *bindings);

qfv_descriptorset_t *
QFV_AllocateDescriptorSet (qfv_descriptorpool_t *pool,
						   qfv_descriptorsetlayout_t *layout);

#define QFV_allocateinfo(type, num, allocator) \
	allocator (field_offset (type, infos[num]))
#define QFV_ImageDescriptorInfo(num, allocator) \
	QFV_allocateinfo(qfv_imagedescriptorinfo_t, num, allocator)
#define QFV_BufferDescriptorInfo(num, allocator) \
	QFV_allocateinfo(qfv_bufferdescriptorinfo_t, num, allocator)
#define QFV_TexelBufferDescriptorInfo(num, allocator) \
	QFV_allocateinfo(qfv_texelbufferdescriptorinfo_t, num, allocator)

void
QFV_UpdateDescriptorSets (struct qfv_device_s *device,
						  uint32_t numImageInfos,
                          qfv_imagedescriptorinfo_t *imageInfos,
                          uint32_t numBufferInfos,
                          qfv_bufferdescriptorinfo_t *bufferInfos,
                          uint32_t numTexelBufferInfos,
                          qfv_texelbufferdescriptorinfo_t *texelbufferInfos,
                          uint32_t numCopyInfos,
                          qfv_copydescriptorinfo_t *copyInfos);

void QFV_FreeDescriptorSet (qfv_descriptorset_t *set);
void QFV_ResetDescriptorPool (qfv_descriptorpool_t *pool);
void QFV_DestroyDescriptorPool (qfv_descriptorpool_t *pool);
void QFV_DestroyDescriptorSetLayout (qfv_descriptorsetlayout_t *layout);
void QFV_DestroySampler (qfv_sampler_t *sampler);


#endif//__QF_Vulkan_descriptor_h
