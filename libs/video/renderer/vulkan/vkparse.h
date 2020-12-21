#ifndef __vkparse_h
#define __vkparse_h

#include "QF/Vulkan/renderpass.h"
#include "libs/video/renderer/vulkan/vkparse.hinc"

VkRenderPass QFV_ParseRenderPass (vulkan_ctx_t *ctx, plitem_t *plist);
void QFV_InitParse (void);

#endif//__vkparse_h
