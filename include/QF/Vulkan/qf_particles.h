#ifndef __QF_Vulkan_qf_particles_h
#define __QF_Vulkan_qf_particles_h

#include "QF/image.h"

struct cvar_s;
struct vulkan_ctx_s;;

void Vulkan_ClearParticles (struct vulkan_ctx_s *ctx);
void Vulkan_InitParticles (struct vulkan_ctx_s *ctx);
void Vulkan_r_easter_eggs_f (struct cvar_s *var, struct vulkan_ctx_s *ctx);
void Vulkan_r_particles_style_f (struct cvar_s *var, struct vulkan_ctx_s *ctx);
void Vulkan_Particles_Init (struct vulkan_ctx_s *ctx);
void Vulkan_DrawParticles (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_particles_h
