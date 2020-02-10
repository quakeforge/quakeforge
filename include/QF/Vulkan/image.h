#ifndef __QF_Vulkan_image_h
#define __QF_Vulkan_image_h

typedef struct qfv_image_s {
	struct qfv_device_s *device;
	VkImage    image;
} qfv_image_t;

typedef struct qfv_imagetransition_s {
	qfv_image_t *image;
	VkAccessFlags srcAccess;
	VkAccessFlags dstAccess;
	VkImageLayout oldLayout;
	VkImageLayout newLayout;
	uint32_t      srcQueueFamily;
	uint32_t      dstQueueFamily;
	VkImageAspectFlags aspect;
} qfv_imagetransition_t;

typedef struct qfv_imagebarrierset_s {
	struct qfv_device_s *device;
	uint32_t    numBarriers;
	VkImageMemoryBarrier *barriers;
} qfv_imagebarrierset_t;

typedef struct qfv_imageview_s {
	struct qfv_device_s *device;
	VkImageView view;
	qfv_image_t *image;
	VkImageViewType type;
	VkFormat    format;
	VkImageAspectFlags aspect;
} qfv_imageview_t;

struct qfv_device_s;
qfv_image_t *QFV_CreateImage (struct qfv_device_s *device, int cubemap,
							  VkImageType type,
							  VkFormat format,
							  VkExtent3D size,
							  uint32_t num_mipmaps,
							  uint32_t num_layers,
							  VkSampleCountFlagBits samples,
							  VkImageUsageFlags usage_scenarios);

struct qfv_memory_s *QFV_AllocImageMemory (qfv_image_t *image,
									 VkMemoryPropertyFlags properties,
									 VkDeviceSize size, VkDeviceSize offset);

int QFV_BindImageMemory (qfv_image_t *image, struct qfv_memory_s *memory,
						  VkDeviceSize offset);

qfv_imagebarrierset_t *
QFV_CreateImageTransitionSet (qfv_imagetransition_t **transitions,
							   int numTransitions);

qfv_imageview_t *QFV_CreateImageView (qfv_image_t *image, VkImageViewType type,
									  VkFormat format,
									  VkImageAspectFlags aspect);

#endif//__QF_Vulkan_image_h
