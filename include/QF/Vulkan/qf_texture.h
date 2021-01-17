#ifndef __QF_Vulkan_qf_texture_h
#define __QF_Vulkan_qf_texture_h

#include "QF/image.h"

typedef struct qfv_tex_s qfv_tex_t;
struct vulkan_ctx_s;

qfv_tex_t *Vulkan_LoadTex (struct vulkan_ctx_s *ctx, tex_t *tex, int mip);
VkImageView Vulkan_TexImageView (qfv_tex_t *tex) __attribute__((pure));
void Vulkan_UnloadTex (struct vulkan_ctx_s *ctx, qfv_tex_t *tex);

#endif//__QF_Vulkan_qf_texture_h
