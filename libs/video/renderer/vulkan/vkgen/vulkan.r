#define __x86_64__
#include <vulkan/vulkan.h>

//FIXME copy of qfv_subpass_t in qf_renderpass.h
//except it doesn't really matter because a custom spec is used
typedef struct qfv_subpass_s {
	vec4        color;
	string      name;
} qfv_subpass_t;
