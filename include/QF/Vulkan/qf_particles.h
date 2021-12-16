#ifndef __QF_Vulkan_qf_particles_h
#define __QF_Vulkan_qf_particles_h

#include "QF/darray.h"
#include "QF/image.h"

#include "QF/Vulkan/command.h"

typedef struct qfv_particle_s {
	vec4f_t     pos;
	vec4f_t     vel;
	vec4f_t     color;
	float       texture;
	float       ramp;
	float       scale;
	float       live;
} qfv_particle_t;

typedef struct qfv_parameters_s {
	vec4f_t     drag;
	vec4f_t     ramp;
} qfv_parameters_t;

typedef enum {
	QFV_particleTranslucent,

	QFV_particleNumPasses
} QFV_ParticleSubpass;

typedef struct particleframe_s {
	VkCommandBuffer compute;
	VkSemaphore physSem;
	VkSemaphore drawSem;
	VkSemaphore updateSem;
	VkBuffer    state;
	VkBuffer    params;
	VkBuffer    system;

	VkDescriptorSet descriptors;

	qfv_cmdbufferset_t cmdSet;
} particleframe_t;

typedef struct particleframeset_s
	DARRAY_TYPE (particleframe_t) particleframeset_t;

typedef struct particlectx_s {
	particleframeset_t frames;
	VkPipeline  physics;
	VkPipeline  update;
	VkPipeline  draw;

	VkDescriptorPool pool;
	VkDescriptorSetLayout setLayout;
	VkPipelineLayout physics_layout;
	VkPipelineLayout update_layout;
	VkPipelineLayout draw_layout;
} particlectx_t;

struct cvar_s;
struct vulkan_ctx_s;;

void Vulkan_ClearParticles (struct vulkan_ctx_s *ctx);
void Vulkan_InitParticles (struct vulkan_ctx_s *ctx);
void Vulkan_r_easter_eggs_f (struct cvar_s *var, struct vulkan_ctx_s *ctx);
void Vulkan_r_particles_style_f (struct cvar_s *var, struct vulkan_ctx_s *ctx);
void Vulkan_Particles_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Particles_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_DrawParticles (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_particles_h
