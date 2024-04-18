/*
	vulkan_particles.c

	Quake specific Vulkan particle manager

	Copyright (C) 2021      Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/va.h"

#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_palette.h"
#include "QF/Vulkan/qf_particles.h"
#include "QF/Vulkan/qf_translucent.h"

#include "r_internal.h"
#include "vid_vulkan.h"

//FIXME make dynamic
#define MaxParticles 2048

typedef struct {
	vec4f_t     gravity;
	float       dT;
} particle_push_constants_t;

static void
create_buffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	particlectx_t *pctx = ctx->particle_context;
	size_t      mp = MaxParticles;
	size_t      frames = pctx->frames.size;

	pctx->resources = malloc (sizeof (qfv_resource_t)
							  // states buffer
							  + frames * sizeof (qfv_resobj_t)
							  // params buffer
							  + frames * sizeof (qfv_resobj_t)
							  // system buffer
							  + frames * sizeof (qfv_resobj_t));
	__auto_type state_objs = (qfv_resobj_t *) &pctx->resources[1];
	__auto_type param_objs = &state_objs[frames];
	__auto_type system_objs = &param_objs[frames];
	pctx->resources[0] = (qfv_resource_t) {
		.name = "particles",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 3 * frames,
		.objects = state_objs,
	};
	for (size_t i = 0; i < frames; i++) {
		state_objs[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "states:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = mp * sizeof (qfv_particle_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		param_objs[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "param:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = mp * sizeof (qfv_parameters_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		system_objs[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "system:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (qfv_particle_system_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
			},
		};
	}
	QFV_CreateResource (device, pctx->resources);

	size_t      stageSize = (pctx->resources->size / frames)*(frames + 1);
	pctx->stage = QFV_CreateStagingBuffer (device, "particles", stageSize,
										   ctx->cmdpool);
	for (size_t i = 0; i < frames; i++) {
		__auto_type pframe = &pctx->frames.a[i];
		pframe->states = state_objs[i].buffer.buffer;
		pframe->params = param_objs[i].buffer.buffer;
		pframe->system = system_objs[i].buffer.buffer;
	}

	for (size_t i = 0; i < frames; i++) {
		__auto_type curr = &pctx->frames.a[i];
		__auto_type prev = &pctx->frames.a[(i + frames - 1) % frames];
		VkDescriptorBufferInfo bufferInfo[] = {
			{ curr->states, 0, VK_WHOLE_SIZE },
			{ curr->params, 0, VK_WHOLE_SIZE },
			{ curr->system, 0, VK_WHOLE_SIZE },
			{ prev->states, 0, VK_WHOLE_SIZE },
			{ prev->params, 0, VK_WHOLE_SIZE },
			{ prev->system, 0, VK_WHOLE_SIZE },
		};
		VkWriteDescriptorSet write[] = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
				curr->curDescriptors, 0, 0, 3,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = bufferInfo + 0
			},
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
				curr->inDescriptors, 0, 0, 3,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = bufferInfo + 3
			},
		};
		dfunc->vkUpdateDescriptorSets (device->dev, 2, write, 0, 0);
	}
}

static void
particles_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto pctx = ctx->particle_context;
	auto pframe = &pctx->frames.a[ctx->curFrame];
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		Vulkan_Palette_Descriptor (ctx),
		Vulkan_Translucent_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 3, sets, 0, 0);

	mat4f_t     mat;
	mat4fidentity (mat);
	vec4f_t     fog = Fog_Get ();
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (mat4f_t), &mat },
		{ VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof (fog), &fog },
	};
	QFV_PushConstants (device, cmd, layout, 2, push_constants);
	VkDeviceSize offsets[] = { 0 };
	VkBuffer    buffers[] = {
		pframe->states,
	};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, buffers, offsets);
	dfunc->vkCmdDrawIndirect (cmd, pframe->system, 0, 1,
							  sizeof (qfv_particle_system_t));
}

static void
update_particles (const exprval_t **p, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto pctx = ctx->particle_context;
	auto pframe = &pctx->frames.a[ctx->curFrame];
	auto layout = taskctx->pipeline->layout;

	qfv_packet_t *packet = QFV_PacketAcquire (pctx->stage);

	__auto_type limits = &device->physDev->p.properties.limits;
	VkMemoryRequirements req = {
		.alignment = limits->minStorageBufferOffsetAlignment
	};
	uint32_t    numParticles = min (MaxParticles, pctx->psystem->numparticles);
	size_t      syssize = sizeof (qfv_particle_system_t);
	size_t      partoffs = QFV_NextOffset (syssize, &req);
	size_t      partsize = sizeof (qfv_particle_t) * numParticles;
	size_t      paramoffs = QFV_NextOffset (partoffs + partsize, &req);
	size_t      paramsize = sizeof (qfv_parameters_t) * numParticles;
	size_t      size = paramoffs + paramsize;

	qfv_particle_system_t *system = QFV_PacketExtend (packet, size);
	*system = (qfv_particle_system_t) {
		.vertexCount = 1,
		.particleCount = numParticles,
	};
	auto particles = (qfv_particle_t *) ((byte *) system + partoffs);
	memcpy (particles, pctx->psystem->particles, partsize);
	auto params = (qfv_parameters_t *) ((byte *) system + paramoffs);
	memcpy (params, pctx->psystem->partparams, paramsize);

	if (!numParticles) {
		// if there are no particles, then no space for the particle states or
		// parameters has been allocated in the staging buffer, so map the
		// two buffers over the system buffer. This avoids either buffer being
		// just past the end of the staging buffer (which the validation layers
		// (correctly) do not like).
		// This is fine because the two buffers are only read by the compute
		// shader.
		partsize = paramsize = syssize;
		partoffs = paramoffs = 0;
	}

	size_t      sysoffs = packet->offset;
	VkDescriptorBufferInfo bufferInfo[] = {
		{ packet->stage->buffer, sysoffs + partoffs, partsize},
		{ packet->stage->buffer, sysoffs + paramoffs, paramsize},
		{ packet->stage->buffer, sysoffs, syssize },
	};
	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
			pframe->newDescriptors, 0, 0, 3,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = bufferInfo
		},
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);

	dfunc->vkResetEvent (device->dev, pframe->updateEvent);

	VkBufferMemoryBarrier pl_barrier[] = {
		{ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.buffer = packet->stage->buffer,
			.offset = sysoffs,
			.size = paramoffs + paramsize,
		},
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_HOST_BIT,
								 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								 0,
								 0, 0,
								 1, pl_barrier,
								 0, 0);

	dfunc->vkCmdBindPipeline (packet->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
							  taskctx->pipeline->pipeline);
	VkDescriptorSet set[3] = {
		pframe->curDescriptors,
		pframe->inDescriptors,
		pframe->newDescriptors,
	};
	dfunc->vkCmdBindDescriptorSets (packet->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
									layout, 0, 3, set, 0, 0);
	dfunc->vkCmdDispatch (packet->cmd, 1, 1, 1);
	dfunc->vkCmdSetEvent (packet->cmd, pframe->updateEvent,
						  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	QFV_PacketSubmit (packet);

	pctx->psystem->numparticles = 0;
}

static void
wait_on_event (VkBuffer states, VkBuffer params, VkBuffer system,
			   VkEvent event, bool draw, VkCommandBuffer cmd,
			   qfv_devfuncs_t *dfunc)
{
	VkStructureType type = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	VkAccessFlags srcAccess = draw
		? VK_ACCESS_SHADER_READ_BIT
			| VK_ACCESS_SHADER_WRITE_BIT
		: VK_ACCESS_SHADER_WRITE_BIT;
	VkAccessFlags dstAccess = draw
		? VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
		: VK_ACCESS_SHADER_READ_BIT
			| VK_ACCESS_SHADER_WRITE_BIT;
	VkBufferMemoryBarrier barrier[] = {
		{ .sType = type,
			.srcAccessMask = srcAccess, .dstAccessMask = dstAccess,
			.buffer = states, .offset = 0, .size = VK_WHOLE_SIZE },
		{ .sType = type,
			.srcAccessMask = srcAccess, .dstAccessMask = dstAccess,
			.buffer = params, .offset = 0, .size = VK_WHOLE_SIZE },
		{ .sType = type,
			.srcAccessMask = srcAccess, .dstAccessMask = dstAccess,
			.buffer = system, .offset = 0, .size = VK_WHOLE_SIZE },
	};
	VkAccessFlags srcStage = draw
		? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		: VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	VkAccessFlags dstStage = draw
		? VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
			| VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
		: VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	dfunc->vkCmdWaitEvents (cmd, 1, &event, srcStage, dstStage,
							0, 0, 3, barrier, 0, 0);
}

static void
particle_physics (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto pctx = ctx->particle_context;
	auto pframe = &pctx->frames.a[ctx->curFrame];
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	dfunc->vkResetEvent (device->dev, pframe->physicsEvent);
	wait_on_event (pframe->states, pframe->params, pframe->system,
				   pframe->updateEvent, false, cmd, dfunc);

	VkDescriptorSet set[] = {
		pframe->curDescriptors,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
									layout, 0, 1, set, 0, 0);

	particle_push_constants_t constants = {
		.gravity = pctx->psystem->gravity,
		.dT = vr_data.frametime,
	};
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_COMPUTE_BIT,
			field_offset (particle_push_constants_t, gravity),
			sizeof (vec4f_t), &constants.gravity },
		{ VK_SHADER_STAGE_COMPUTE_BIT,
			field_offset (particle_push_constants_t, dT),
			sizeof (float), &constants.dT },
	};
	QFV_PushConstants (device, cmd, layout, 2, push_constants);
	dfunc->vkCmdDispatch (cmd, MaxParticles, 1, 1);
	dfunc->vkCmdSetEvent (cmd, pframe->physicsEvent,
						  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

static void
particle_wait_physics (const exprval_t **params, exprval_t *result,
					   exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto pctx = ctx->particle_context;
	auto pframe = &pctx->frames.a[ctx->curFrame];

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 va (ctx->va_ctx, "cmd:particle_wait_physics:%d",
							 ctx->curFrame));
	QFV_AppendCmdBuffer (ctx, cmd);
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	wait_on_event (pframe->states, pframe->params, pframe->system,
				   pframe->physicsEvent, true, cmd, dfunc);

	dfunc->vkEndCommandBuffer (cmd);
}

static void
particle_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	particlectx_t *pctx = ctx->particle_context;
	size_t      frames = pctx->frames.size;

	for (size_t i = 0; i < frames; i++) {
		__auto_type pframe = &pctx->frames.a[i];
		dfunc->vkDestroyEvent (device->dev, pframe->updateEvent, 0);
		dfunc->vkDestroyEvent (device->dev, pframe->physicsEvent, 0);
	}

	QFV_DestroyStagingBuffer (pctx->stage);
	QFV_DestroyResource (device, pctx->resources);
	free (pctx->resources);

	free (pctx->frames.a);
	free (pctx);
}

static void
particle_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "particles init");
	auto pctx = ctx->particle_context;
	pctx->psystem = &r_psystem;
	auto device = ctx->device;
	auto dfunc = device->funcs;

	size_t      frames = ctx->render_context->frames.size;
	DARRAY_INIT (&pctx->frames, frames);
	DARRAY_RESIZE (&pctx->frames, frames);
	pctx->frames.grow = 0;

	auto dsmanager = QFV_Render_DSManager (ctx, "particle_set");

	for (size_t i = 0; i < frames; i++) {
		__auto_type pframe = &pctx->frames.a[i];

		pframe->curDescriptors = QFV_DSManager_AllocSet (dsmanager);
		pframe->inDescriptors  = QFV_DSManager_AllocSet (dsmanager);
		pframe->newDescriptors = QFV_DSManager_AllocSet (dsmanager);

		VkEventCreateInfo event = { VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
		dfunc->vkCreateEvent (device->dev, &event, 0, &pframe->physicsEvent);
		dfunc->vkCreateEvent (device->dev, &event, 0, &pframe->updateEvent);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_EVENT,
							 pframe->physicsEvent,
							 va (ctx->va_ctx, "event:particle:physics:%zd", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_EVENT,
							 pframe->updateEvent,
							 va (ctx->va_ctx, "event:particle:update:%zd", i));
	}
	create_buffers (ctx);
	qfvPopDebug (ctx);
}

static void
particle_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, particle_shutdown);
	QFV_Render_AddStartup (ctx, particle_startup);

	particlectx_t *pctx = calloc (1, sizeof (particlectx_t));
	ctx->particle_context = pctx;
}

static exprfunc_t particles_draw_func[] = {
	{ .func = particles_draw },
	{}
};
static exprfunc_t update_particles_func[] = {
	{ .func = update_particles },
	{}
};
static exprfunc_t particle_physics_func[] = {
	{ .func = particle_physics },
	{}
};
static exprfunc_t particle_wait_physics_func[] = {
	{ .func = particle_wait_physics },
	{}
};

static exprfunc_t particle_init_func[] = {
	{ .func = particle_init },
	{}
};

static exprsym_t particles_task_syms[] = {
	{ "particles_draw", &cexpr_function, particles_draw_func },
	{ "update_particles", &cexpr_function, update_particles_func },
	{ "particle_physics", &cexpr_function, particle_physics_func },
	{ "particle_wait_physics", &cexpr_function, particle_wait_physics_func },
	{ "particle_init", &cexpr_function, particle_init_func },
	{}
};

void
Vulkan_Particles_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, particles_task_syms);
}

psystem_t *__attribute__((pure))//FIXME?
Vulkan_ParticleSystem (vulkan_ctx_t *ctx)
{
	return ctx->particle_context->psystem;	//FIXME support more
}
