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
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_particles.h"
#include "QF/Vulkan/qf_renderpass.h"

#include "r_internal.h"
#include "vid_vulkan.h"

//FIXME make dynamic
#define MaxParticles 2048

typedef struct {
	vec4f_t     gravity;
	float       dT;
} particle_push_constants_t;

static const char * __attribute__((used)) particle_pass_names[] = {
	"draw",
};

static void
particle_begin_subpass (VkPipeline pipeline, qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	particlectx_t *pctx = ctx->particle_context;
	uint32_t    curFrame = ctx->curFrame;
	particleframe_t *pframe = &pctx->frames.a[curFrame];
	VkCommandBuffer cmd = pframe->cmdSet.a[0];

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		rFrame->renderpass->renderpass, QFV_passTranslucent,
		rFrame->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);
	QFV_duCmdBeginLabel (device, cmd, va (ctx->va_ctx, "particles:%s", "draw"),
						 { 0.6, 0.5, 0, 1});

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									pctx->draw_layout, 0, 1, sets, 0, 0);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);
}

static void
particle_end_subpass (VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

void
Vulkan_DrawParticles (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	particlectx_t *pctx = ctx->particle_context;
	uint32_t    curFrame = ctx->curFrame;
	particleframe_t *pframe = &pctx->frames.a[curFrame];
	VkCommandBuffer cmd = pframe->cmdSet.a[0];

	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passTranslucent],
				   pframe->cmdSet.a[0]);

	particle_begin_subpass (pctx->draw, rFrame);

	VkBufferMemoryBarrier barrier[] = {
		{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_SHADER_READ_BIT
			| VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			0, 0,
			pframe->states, 0, VK_WHOLE_SIZE },
		{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_SHADER_READ_BIT
			| VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			0, 0,
			pframe->params, 0, VK_WHOLE_SIZE },
		{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_SHADER_READ_BIT
			| VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			0, 0,
			pframe->system, 0, VK_WHOLE_SIZE },
	};
	dfunc->vkCmdWaitEvents (cmd, 1, &pframe->physicsEvent,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
							| VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
							0, 0,
							3, barrier,
							0, 0);

	mat4f_t     mat;
	mat4fidentity (mat);
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (mat4f_t), &mat },
	};
	QFV_PushConstants (device, cmd, pctx->draw_layout, 1, push_constants);
	VkDeviceSize offsets[] = { 0 };
	VkBuffer    buffers[] = {
		pframe->states,
	};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, buffers, offsets);
	dfunc->vkCmdDrawIndirect (cmd, pframe->system, 0, 1,
							  sizeof (qfv_particle_system_t));
	particle_end_subpass (cmd, ctx);
}

static void
create_buffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	particlectx_t *pctx = ctx->particle_context;
	size_t      mp = MaxParticles;
	size_t      frames = ctx->frames.size;

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
			.name = "states",
			.type = qfv_res_buffer,
			.buffer = {
				.size = mp * sizeof (qfv_particle_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
		param_objs[i] = (qfv_resobj_t) {
			.name = "params",
			.type = qfv_res_buffer,
			.buffer = {
				.size = mp * sizeof (qfv_parameters_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		system_objs[i] = (qfv_resobj_t) {
			.name = "system",
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

void
Vulkan_Particles_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	qfvPushDebug (ctx, "particles init");

	particlectx_t *pctx = calloc (1, sizeof (particlectx_t));
	ctx->particle_context = pctx;
	pctx->psystem = &r_psystem;

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&pctx->frames, frames);
	DARRAY_RESIZE (&pctx->frames, frames);
	pctx->frames.grow = 0;

	pctx->physics = Vulkan_CreateComputePipeline (ctx, "partphysics");
	pctx->update = Vulkan_CreateComputePipeline (ctx, "partupdate");
	pctx->draw = Vulkan_CreateGraphicsPipeline (ctx, "partdraw");
	pctx->physics_layout = Vulkan_CreatePipelineLayout (ctx,
														"partphysics_layout");
	pctx->update_layout = Vulkan_CreatePipelineLayout (ctx,
													   "partupdate_layout");
	pctx->draw_layout = Vulkan_CreatePipelineLayout (ctx, "partdraw_layout");

	pctx->pool = Vulkan_CreateDescriptorPool (ctx, "particle_pool");
	pctx->setLayout = Vulkan_CreateDescriptorSetLayout (ctx, "particle_set");

	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (3 * frames, alloca);
	for (size_t i = 0; i < layouts->size; i++) {
		layouts->a[i] = pctx->setLayout;
	}
	__auto_type sets = QFV_AllocateDescriptorSet (device, pctx->pool, layouts);

	for (size_t i = 0; i < frames; i++) {
		__auto_type pframe = &pctx->frames.a[i];

		pframe->curDescriptors = sets->a[i * 3 + 0];
		pframe->inDescriptors  = sets->a[i * 3 + 1];
		pframe->newDescriptors = sets->a[i * 3 + 2];

		DARRAY_INIT (&pframe->cmdSet, QFV_particleNumPasses);
		DARRAY_RESIZE (&pframe->cmdSet, QFV_particleNumPasses);
		pframe->cmdSet.grow = 0;

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, &pframe->cmdSet);

		for (int j = 0; j < QFV_particleNumPasses; j++) {
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
								 pframe->cmdSet.a[j],
								 va (ctx->va_ctx, "cmd:particle:%zd:%s", i,
									 particle_pass_names[j]));
		}

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
	free (sets);
	create_buffers (ctx);
	qfvPopDebug (ctx);
}

void
Vulkan_Particles_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	particlectx_t *pctx = ctx->particle_context;
	size_t      frames = ctx->frames.size;

	for (size_t i = 0; i < frames; i++) {
		__auto_type pframe = &pctx->frames.a[i];
		dfunc->vkDestroyEvent (device->dev, pframe->updateEvent, 0);
		dfunc->vkDestroyEvent (device->dev, pframe->physicsEvent, 0);
	}

	QFV_DestroyStagingBuffer (pctx->stage);
	QFV_DestroyResource (device, pctx->resources);
	free (pctx->resources);

	dfunc->vkDestroyPipeline (device->dev, pctx->physics, 0);
	dfunc->vkDestroyPipeline (device->dev, pctx->update, 0);
	dfunc->vkDestroyPipeline (device->dev, pctx->draw, 0);
	free (pctx->frames.a);
	free (pctx);
}

psystem_t *__attribute__((pure))//FIXME?
Vulkan_ParticleSystem (vulkan_ctx_t *ctx)
{
	return ctx->particle_context->psystem;	//FIXME support more
}

static void
particles_update (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	particlectx_t *pctx = ctx->particle_context;
	__auto_type pframe = &pctx->frames.a[ctx->curFrame];

	qfv_packet_t *packet = QFV_PacketAcquire (pctx->stage);

	__auto_type limits = &device->physDev->properties->limits;
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
	__auto_type particles = (qfv_particle_t *) ((byte *)system + partoffs);
	memcpy (particles, pctx->psystem->particles, partsize);
	qfv_parameters_t *params = (qfv_parameters_t *)((byte *)system + paramoffs);
	memcpy (params, pctx->psystem->partparams, paramsize);

	partsize = max (1, partsize);
	paramsize = max (1, paramsize);

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
	dfunc->vkResetEvent (device->dev, pframe->physicsEvent);

	VkBufferMemoryBarrier pl_barrier[] = {
		{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			0,
			VK_ACCESS_SHADER_READ_BIT,
			0, 0,
			packet->stage->buffer, sysoffs, paramoffs + paramsize },
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								 0,
								 0, 0,
								 1, pl_barrier,
								 0, 0);

	dfunc->vkCmdBindPipeline (packet->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
							  pctx->update);
	VkDescriptorSet set[3] = {
		pframe->curDescriptors,
		pframe->inDescriptors,
		pframe->newDescriptors,
	};
	dfunc->vkCmdBindDescriptorSets (packet->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
									pctx->update_layout, 0, 3, set, 0, 0);
	dfunc->vkCmdDispatch (packet->cmd, 1, 1, 1);
	dfunc->vkCmdSetEvent (packet->cmd, pframe->updateEvent,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	VkBufferMemoryBarrier ev_barrier[] = {
		{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT
			| VK_ACCESS_SHADER_WRITE_BIT,
			0, 0,
			pframe->states, 0, VK_WHOLE_SIZE },
		{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT
			| VK_ACCESS_SHADER_WRITE_BIT,
			0, 0,
			pframe->params, 0, VK_WHOLE_SIZE },
		{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT
			| VK_ACCESS_SHADER_WRITE_BIT,
			0, 0,
			pframe->system, 0, VK_WHOLE_SIZE },
	};
	dfunc->vkCmdWaitEvents (packet->cmd, 1, &pframe->updateEvent,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							0, 0,
							3, ev_barrier,
							0, 0);
	dfunc->vkCmdBindPipeline (packet->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
							  pctx->physics);
	dfunc->vkCmdBindDescriptorSets (packet->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
									pctx->physics_layout, 0, 1, set, 0, 0);

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
	QFV_PushConstants (device, packet->cmd, pctx->physics_layout,
					   2, push_constants);
	dfunc->vkCmdDispatch (packet->cmd, MaxParticles, 1, 1);
	dfunc->vkCmdSetEvent (packet->cmd, pframe->physicsEvent,
						  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	QFV_PacketSubmit (packet);

	pctx->psystem->numparticles = 0;
}

void
Vulkan_Particles_CreateRenderPasses (vulkan_ctx_t *ctx)
{
	__auto_type rp = QFV_RenderPass_New (ctx, "particles", particles_update);
	rp->order = QFV_rp_particles;
	DARRAY_APPEND (&ctx->renderPasses, rp);
}
