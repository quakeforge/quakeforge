#ifndef __QF_Vulkan_qf_particles_h
#define __QF_Vulkan_qf_particles_h

#include "QF/darray.h"

typedef struct psystem_s psystem_t;

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

// Doubles as VkDrawIndirectCommand
typedef struct qfv_particle_system_s {
	uint32_t    vertexCount;	// always 1
	uint32_t    particleCount;
	uint32_t    firstVertex;	// always 0
	uint32_t    firstInstance;	// always 0
	uint32_t    part_ramps[];
} qfv_particle_system_t;

static_assert (sizeof (qfv_particle_system_t) == 4 * sizeof (uint32_t));

typedef struct particleframe_s {
	VkEvent     physicsEvent;
	VkEvent     updateEvent;
	VkBuffer    states;
	VkBuffer    params;
	VkBuffer    system;

	VkDescriptorSet curDescriptors;
	VkDescriptorSet inDescriptors;
	VkDescriptorSet newDescriptors;
} particleframe_t;

typedef struct particleframeset_s
	DARRAY_TYPE (particleframe_t) particleframeset_t;

typedef struct particlectx_s {
	particleframeset_t frames;

	struct qfv_resource_s *resources;

	psystem_t  *psystem;

	mat4f_t    *mat;
	vec4f_t    *fog;
	vec4f_t    *center;
	float      *gravity;
	float      *min_dist;
	float      *dT;
} particlectx_t;

struct vulkan_ctx_s;

psystem_t *Vulkan_ParticleSystem (struct vulkan_ctx_s *ctx);
void Vulkan_Particles_Init (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_particles_h
