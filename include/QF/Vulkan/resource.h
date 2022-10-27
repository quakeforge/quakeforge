#ifndef __QF_Vulkan_resource_h
#define __QF_Vulkan_resource_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

typedef enum {
	qfv_res_buffer = 1,
	qfv_res_buffer_view,
	qfv_res_image,
	qfv_res_image_view,
} qfv_res_type;

typedef struct qfv_resobj_s {
	const char *name;
	qfv_res_type type;
	union {
		struct {
			VkDeviceSize size;
			VkBufferUsageFlags usage;
			VkBuffer    buffer;
			VkDeviceSize offset;
		}       buffer;
		struct {
			unsigned    buffer;
			VkFormat    format;
			VkDeviceSize offset;
			VkDeviceSize size;
			VkBufferView view;
		}       buffer_view;
		struct {
			int         cubemap;
			VkImageType type;
			VkFormat format;
			VkExtent3D extent;
			uint32_t num_mipmaps;
			uint32_t num_layers;
			VkSampleCountFlags samples;
			VkImageUsageFlags usage;
			VkImage     image;
			VkDeviceSize offset;
		}       image;
		struct {
			unsigned    image;
			VkImageViewType type;
			VkFormat    format;
			VkImageAspectFlags aspect;
			VkImageView view;
		}       image_view;
	};
} qfv_resobj_t;

typedef struct qfv_resource_s {
	const char *name;
	struct va_ctx_s *va_ctx;
	VkMemoryPropertyFlags memory_properties;
	unsigned    num_objects;
	qfv_resobj_t *objects;
	VkDeviceMemory memory;
	VkDeviceSize size;
} qfv_resource_t;

struct qfv_device_s;

int QFV_CreateResource (struct qfv_device_s *device, qfv_resource_t *resource);
void QFV_DestroyResource (struct qfv_device_s *device,
						  qfv_resource_t *resource);
struct tex_s;
void QFV_ResourceInitTexImage (qfv_resobj_t *image, const char *name,
							   int mips, const struct tex_s *tex);

#endif//__QF_Vulkan_resource_h
