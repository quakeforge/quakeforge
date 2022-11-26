#define __x86_64__
#include <vulkan/vulkan.h>

//FIXME copy of qfv_subpass_t in qf_renderpass.h
//except it doesn't really matter because a custom spec is used
typedef struct qfv_subpass_s {
	vec4        color;
	string      name;
} qfv_subpass_t;

//FIXME copy of qfv_output_t in qf_renderpass.h
//except it doesn't really matter because a custom spec is used
typedef struct qfv_output_s {
	VkExtent2D  extent;
	VkImage     image;
	VkImageView view;
	VkFormat    format;
	uint32_t    frames;
	VkImageView *view_list;
	VkImageLayout finalLayout;
} qfv_output_t;
