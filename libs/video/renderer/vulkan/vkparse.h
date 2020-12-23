#ifndef __vkparse_h
#define __vkparse_h

#include "QF/cexpr.h"
#include "QF/Vulkan/renderpass.h"
#include "libs/video/renderer/vulkan/vkparse.hinc"

typedef struct parsectx_s {
	struct exprctx_s *ectx;
	struct vulkan_ctx_s *vctx;
} parsectx_t;

VkRenderPass QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist);
void QFV_InitParse (void);

#endif//__vkparse_h
