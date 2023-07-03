#ifndef __QF_Vulkan_qf_translucent_h
#define __QF_Vulkan_qf_translucent_h

#include "QF/darray.h"

#include "QF/simd/types.h"

#include "QF/Vulkan/command.h"

typedef struct qfv_transfrag_s {
	vec4f_t     color;
	float       depth;
	int32_t     next;
} qfv_transfrag_t;

typedef struct qfv_transtate_s {
	int32_t     numFragments;
	int32_t     maxFragments;
} qfv_transtate_t;

typedef struct translucentframe_s {
	VkDescriptorSet flat;
	VkDescriptorSet cube;
	struct qfv_resobj_s *heads;
	struct qfv_resobj_s *cube_heads;
	struct qfv_resobj_s *heads_view;
	struct qfv_resobj_s *cube_heads_view;
	struct qfv_resobj_s *state;
	struct qfv_resobj_s *frags;
} translucentframe_t;

typedef struct translucentframeset_s
	DARRAY_TYPE (translucentframe_t) translucentframeset_t;

typedef struct translucentctx_s {
	translucentframeset_t frames;
	struct qfv_resource_s *resources;
	VkExtent2D  extent;

	int         maxFragments;
} translucentctx_t;

struct vulkan_ctx_s;

void Vulkan_Translucent_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Translucent_Setup (struct vulkan_ctx_s *ctx);
void Vulkan_Translucent_Shutdown (struct vulkan_ctx_s *ctx);
VkDescriptorSet Vulkan_Translucent_Descriptors (struct vulkan_ctx_s *ctx,
												int frame)__attribute__((pure));

#endif//__QF_Vulkan_qf_translucent_h
