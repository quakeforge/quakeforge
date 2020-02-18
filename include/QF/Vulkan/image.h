#ifndef __QF_Vulkan_image_h
#define __QF_Vulkan_image_h

#include "QF/darray.h"

typedef struct qfv_imageset_s DARRAY_TYPE (VkImage) qfv_imageset_t;
typedef struct qfv_imageviewset_s DARRAY_TYPE (VkImageView) qfv_imageviewset_t;

typedef struct qfv_imageresource_s {
	struct qfv_device_s *device;
	VkImage     image;
	VkDeviceMemory object;
	VkImageView view;
} qfv_imageresource_t;

typedef struct qfv_imagetransition_s {
	VkImage       image;
	VkAccessFlags srcAccess;
	VkAccessFlags dstAccess;
	VkImageLayout oldLayout;
	VkImageLayout newLayout;
	uint32_t      srcQueueFamily;
	uint32_t      dstQueueFamily;
	VkImageAspectFlags aspect;
} qfv_imagetransition_t;

typedef struct qfv_imagetransitionset_s
	DARRAY_TYPE (qfv_imagetransition_t) qfv_imagetransitionset_t;
typedef struct qfv_imagebarrierset_s
	DARRAY_TYPE (VkImageMemoryBarrier) qfv_imagebarrierset_t;

struct qfv_device_s;
VkImage QFV_CreateImage (struct qfv_device_s *device, int cubemap,
						 VkImageType type,
						 VkFormat format,
						 VkExtent3D size,
						 uint32_t num_mipmaps,
						 uint32_t num_layers,
						 VkSampleCountFlagBits samples,
						 VkImageUsageFlags usage_scenarios);

VkDeviceMemory QFV_AllocImageMemory (struct qfv_device_s *device,
									 VkImage image,
									 VkMemoryPropertyFlags properties,
									 VkDeviceSize size, VkDeviceSize offset);

int QFV_BindImageMemory (struct qfv_device_s *device, VkImage image,
						 VkDeviceMemory object, VkDeviceSize offset);

qfv_imagebarrierset_t *
QFV_CreateImageTransitionSet (qfv_imagetransition_t *transitions,
							   int numTransitions);

VkImageView QFV_CreateImageView (struct qfv_device_s *device,
								 VkImage image, VkImageViewType type,
								 VkFormat format, VkImageAspectFlags aspect);

#endif//__QF_Vulkan_image_h
