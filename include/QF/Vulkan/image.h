#ifndef __QF_Vulkan_image_h
#define __QF_Vulkan_image_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/image.h"

typedef struct qfv_imageset_s
	DARRAY_TYPE (VkImage) qfv_imageset_t;

#define QFV_AllocImages(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_imageset_t, num, allocator)

typedef struct qfv_imageviewset_s
	DARRAY_TYPE (VkImageView) qfv_imageviewset_t;

#define QFV_AllocImageViews(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_imageviewset_t, num, allocator)

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
#define QFV_AllocImageBarrierSet(num, allocator) \
	DARRAY_ALLOCFIXED (qfv_imagebarrierset_t, num, allocator)

struct qfv_device_s;
VkImage QFV_CreateImage (struct qfv_device_s *device, int cubemap,
						 VkImageType type,
						 VkFormat format,
						 VkExtent3D size,
						 uint32_t num_mipmaps,
						 uint32_t num_layers,
						 VkSampleCountFlags samples,
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

size_t QFV_GetImageSize (struct qfv_device_s *device, VkImage image);

/** Generate all mipmaps for a given texture down to a 1x1 pixel.
 *
 * Uses the GPU blit command from one mip level to the next, thus the base mip
 * level data must have already been transfered to the image and the image is
 * expected to be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL. This includes any
 * array levels.
 *
 *	\param device	The device owning the command buffer.
 *	\param cmd		The command buffer to which the barrier and blit commands
 *					will be written.
 *	\param image	The image to be processed. All array layers of the base mip
 *					level must be initialized and in "transfer dst optimal"
 *					layout. All remaining mip levels must be in "undefined"
 *					oayout.
 *	\param mips		The total number of mip levels of the processed image.
 *	\param width	The pixel width of the base image.
 *	\param height	The pixel height of the base image.
 *	\param layers	The number of array layers in the base image.
 *
 *	\note	The processed image will be in "shader read only optimal" layout on
 *			completion.
 */
void QFV_GenerateMipMaps (struct qfv_device_s *device, VkCommandBuffer cmd,
						  VkImage image, unsigned mips,
						  unsigned width, unsigned height, unsigned layers);
int QFV_MipLevels (int width, int height) __attribute__((const));

/** Convert QFFormat to VkFormat
 *
 *	\param format	The format to convert.
 *	\param srgb		Select SRGB formats (non-zero) or UNORM (0).
 *	\return			The corresponding VkFormat.
 *
 *	\note For tex_palette, VK_FORMAT_R8_UINT is returned. If \a format is
 *	not a valid QFFormat, then VK_FORMAT_R8_SRGB is returned.
 */
VkFormat QFV_ImageFormat (QFFormat format, int srgb) __attribute__((const));

#endif//__QF_Vulkan_image_h
