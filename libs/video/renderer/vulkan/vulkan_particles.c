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

#include "QF/particle.h"
#include "QF/render.h"
#include "QF/va.h"

#include "QF/Vulkan/barrier.h"
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

	auto ps = pctx->psystem;
	size_t system_size = offsetof (qfv_particle_system_t,
								   part_ramps[ps->partramps_count]);
	for (size_t i = 0; i < frames; i++) {
		state_objs[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "states:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = mp * sizeof (qfv_particle_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		param_objs[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "param:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = mp * sizeof (qfv_parameters_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		system_objs[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "system:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = system_size,
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
			},
		};
	}
	QFV_CreateResource (device, pctx->resources);

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
	auto pipeline = taskctx->pipeline;
	auto layout = pipeline->layout;
	auto cmd = taskctx->cmd;

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		Vulkan_Palette_Descriptor (ctx),
		Vulkan_Translucent_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 3, sets, 0, 0);

	mat4fidentity (*pctx->mat);
	*pctx->fog = Fog_Get ();
	QFV_PushBlackboard (ctx, cmd, pipeline);

	VkDeviceSize offsets[] = { 0 };
	VkBuffer    buffers[] = {
		pframe->states,
	};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, buffers, offsets);
	dfunc->vkCmdDrawIndirect (cmd, pframe->system, 0, 1,
							  sizeof (qfv_particle_system_t));
	taskctx->subpass->call_count++;
}

static void
memory_barrier (vulkan_ctx_t *ctx, VkCommandBuffer cmd)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto bb = bufferBarriers[qfv_BB_ShaderWrite_to_ShaderRW];
	VkMemoryBarrier2 mb = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = bb.srcStageMask,
		.srcAccessMask = bb.srcAccessMask,
		.dstStageMask = bb.dstStageMask,
		.dstAccessMask = bb.dstAccessMask,
	};
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &mb,
	};
	dfunc->vkCmdPipelineBarrier2 (cmd, &dep);
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

	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging, "particles.update");

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
		.vertexCount = 4,
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

	VkBufferMemoryBarrier2 pl_barrier = {
		.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
		.buffer = packet->stage->buffer,
		.offset = sysoffs,
		.size = paramoffs + paramsize,
	};
	dfunc->vkCmdPipelineBarrier2 (packet->cmd, &(VkDependencyInfo) {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.bufferMemoryBarrierCount = 1,
				.pBufferMemoryBarriers = &pl_barrier,
			});

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
	memory_barrier (ctx, packet->cmd);
	QFV_PacketSubmit (packet);

	pctx->psystem->numparticles = 0;
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
	auto pipeline = taskctx->pipeline;
	auto layout = pipeline->layout;
	auto cmd = taskctx->cmd;

	VkDescriptorSet set[] = {
		pframe->curDescriptors,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
									layout, 0, 1, set, 0, 0);

	*pctx->palette_size = pctx->psystem->palette_size;
	*pctx->center = pctx->psystem->center;
	*pctx->gravity = pctx->psystem->gravity;
	*pctx->min_dist = pctx->psystem->min_dist;
	*pctx->dT = vr_data.frametime;
	QFV_PushBlackboard (ctx, cmd, pipeline);

	dfunc->vkCmdDispatch (cmd, (MaxParticles + 63) / 64, 1, 1);
	memory_barrier (ctx, cmd);
}

static void
particle_wait_physics (const exprval_t **params, exprval_t *result,
					   exprctx_t *ectx)
{
//	qfZoneNamed (zone, true);
//	auto taskctx = (qfv_taskctx_t *) ectx;
//	auto ctx = taskctx->ctx;
//	auto device = ctx->device;
//	auto dfunc = device->funcs;
//	auto pctx = ctx->particle_context;
//	auto pframe = &pctx->frames.a[ctx->curFrame];
//
//	auto cmd = QFV_GetCmdBuffer (ctx, false);
//	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
//						 vac (ctx->va_ctx, "cmd:particle_wait_physics:%d",
//							  ctx->curFrame));
//	QFV_AppendCmdBuffer (taskctx->job, cmd);
//	VkCommandBufferBeginInfo beginInfo = {
//		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
//		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
//	};
//	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);
//
//	wait_on_event (pframe->states, pframe->params, pframe->system,
//				   pframe->physicsEvent, true, cmd, dfunc);
//
//	dfunc->vkEndCommandBuffer (cmd);
}

static void
particle_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfv_device_t *device = ctx->device;
	particlectx_t *pctx = ctx->particle_context;

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

	particlectx_t *pctx = malloc (sizeof (particlectx_t));
	ctx->particle_context = pctx;

	*pctx = (particlectx_t) {
		.mat          = QFV_GetBlackboardVar (ctx, "Model"),
		.palette_size = QFV_GetBlackboardVar (ctx, "palette_size"),
		.fog          = QFV_GetBlackboardVar (ctx, "fog"),
		.center       = QFV_GetBlackboardVar (ctx, "center"),
		.gravity      = QFV_GetBlackboardVar (ctx, "gravity"),
		.min_dist     = QFV_GetBlackboardVar (ctx, "min_dist"),
		.dT           = QFV_GetBlackboardVar (ctx, "dT"),
	};
}

static void
particles_newscene (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto pctx = ctx->particle_context;

	QFV_DestroyResource (device, pctx->resources);
	create_buffers (ctx);

	auto packet = QFV_PacketAcquire (ctx->staging, "particles.newscene");
	auto ps = pctx->psystem;
	size_t system_size = offsetof (qfv_particle_system_t,
								   part_ramps[ps->partramps_count]);
	qfv_particle_system_t *system = QFV_PacketExtend (packet, system_size);
	memset (system, 0, sizeof (qfv_particle_system_t));
	memcpy (system->part_ramps, ps->partramps,
			sizeof (uint32_t[ps->partramps_count]));
	size_t      count = pctx->frames.size;
	for (size_t i = 0; i < count; i++) {
		auto frame = &pctx->frames.a[i];
		QFV_PacketCopyBuffer (packet, frame->system, 0,
			  &bufferBarriers[qfv_BB_Unknown_to_TransferWrite],
			  &bufferBarriers[qfv_BB_TransferWrite_to_ShaderRW]);
	}
	QFV_PacketSubmit (packet);
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

static exprfunc_t particles_newscene_func[] = {
	{ .func = particles_newscene },
	{}
};

static exprsym_t particles_task_syms[] = {
	{ "particles_draw",        &cexpr_function, particles_draw_func },
	{ "update_particles",      &cexpr_function, update_particles_func },
	{ "particle_physics",      &cexpr_function, particle_physics_func },
	{ "particle_wait_physics", &cexpr_function, particle_wait_physics_func },
	{ "particle_init",         &cexpr_function, particle_init_func },
	{ "particles_newscene",    &cexpr_function, particles_newscene_func },
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
