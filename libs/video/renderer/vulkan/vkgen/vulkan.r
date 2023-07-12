#define __x86_64__
#include <vulkan/vulkan.h>

typedef vec4 vec4f_t;
#define __QF_simd_types_h
#include "QF/Vulkan/render.h"

//FIXME copy of qfv_subpass_t in qf_renderpass.h
//except it doesn't really matter because a custom spec is used
typedef struct qfv_osubpass_s {
	vec4        color;
	string      name;
} qfv_osubpass_t;
