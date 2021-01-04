#ifndef __vkparse_h
#define __vkparse_h

#include "QF/cexpr.h"
#include "QF/Vulkan/renderpass.h"
#ifdef vkparse_internal
#include "libs/video/renderer/vulkan/vkparse.hinc"
#endif

typedef struct parsectx_s {
	struct exprctx_s *ectx;
	struct vulkan_ctx_s *vctx;
} parsectx_t;

VkRenderPass QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist);
void QFV_InitParse (void);
exprenum_t *QFV_GetEnum (const char *name);

#endif//__vkparse_h
