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

#include "QF/plugin/vid_render.h"

#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/qf_particles.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static const char * __attribute__((used)) particle_pass_names[] = {
	"draw",
};

void
Vulkan_ClearParticles (vulkan_ctx_t *ctx)
{
}

void
Vulkan_InitParticles (vulkan_ctx_t *ctx)
{
}

static void
R_RocketTrail_QF (vec4f_t start, vec4f_t end)
{
}

static void
R_GrenadeTrail_QF (vec4f_t start, vec4f_t end)
{
}

static void
R_BloodTrail_QF (vec4f_t start, vec4f_t end)
{
}

static void
R_SlightBloodTrail_QF (vec4f_t start, vec4f_t end)
{
}

static void
R_WizTrail_QF (vec4f_t start, vec4f_t end)
{
}

static void
R_FlameTrail_QF (vec4f_t start, vec4f_t end)
{
}

static void
R_VoorTrail_QF (vec4f_t start, vec4f_t end)
{
}

static void
R_GlowTrail_QF (vec4f_t start, vec4f_t end, int glow_color)
{
}

static void
R_RunParticleEffect_QF (vec4f_t org, vec4f_t dir, int color,
					    int count)
{
}

static void
R_BloodPuffEffect_QF (vec4f_t org, int count)
{
}

static void
R_GunshotEffect_QF (vec4f_t org, int count)
{
}

static void
R_LightningBloodEffect_QF (vec4f_t org)
{
}

static void
R_SpikeEffect_QF (vec4f_t org)
{
}

static void
R_KnightSpikeEffect_QF (vec4f_t org)
{
}

static void
R_SuperSpikeEffect_QF (vec4f_t org)
{
}

static void
R_WizSpikeEffect_QF (vec4f_t org)
{
}

static void
R_BlobExplosion_QF (vec4f_t org)
{
}

static void
R_ParticleExplosion_QF (vec4f_t org)
{
}

static void
R_ParticleExplosion2_QF (vec4f_t org, int colorStart, int colorLength)
{
}

static void
R_LavaSplash_QF (vec4f_t org)
{
}

static void
R_TeleportSplash_QF (vec4f_t org)
{
}

static void
R_DarkFieldParticles_ID (vec4f_t org)
{
}

static void
R_EntityParticles_ID (vec4f_t org)
{
}

static void
R_Particle_New (ptype_t type, int texnum, vec4f_t org,
				float scale, vec4f_t vel, float die,
				int color, float alpha, float ramp)
{
}

static void
R_Particle_NewRandom (ptype_t type, int texnum, vec4f_t org,
					  int org_fuzz, float scale, int vel_fuzz,
					  float die, int color, float alpha,
					  float ramp)
{
}

static vid_particle_funcs_t vulkan_particles_QF = {
	R_RocketTrail_QF,
	R_GrenadeTrail_QF,
	R_BloodTrail_QF,
	R_SlightBloodTrail_QF,
	R_WizTrail_QF,
	R_FlameTrail_QF,
	R_VoorTrail_QF,
	R_GlowTrail_QF,
	R_RunParticleEffect_QF,
	R_BloodPuffEffect_QF,
	R_GunshotEffect_QF,
	R_LightningBloodEffect_QF,
	R_SpikeEffect_QF,
	R_KnightSpikeEffect_QF,
	R_SuperSpikeEffect_QF,
	R_WizSpikeEffect_QF,
	R_BlobExplosion_QF,
	R_ParticleExplosion_QF,
	R_ParticleExplosion2_QF,
	R_LavaSplash_QF,
	R_TeleportSplash_QF,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

void
Vulkan_r_easter_eggs_f (cvar_t *var, vulkan_ctx_t *ctx)
{
	if (!easter_eggs || !r_particles_style) {
		return;
	}
	if (easter_eggs->int_val) {
		if (r_particles_style->int_val) {
			//vulkan_vid_render_funcs.particles = &vulkan_particles_QF_egg;
		} else {
			//vulkan_vid_render_funcs.particles = &vulkan_particles_ID_egg;
		}
	} else {
		if (r_particles_style->int_val) {
			//vulkan_vid_render_funcs.particles = &vulkan_particles_QF;
		} else {
			//vulkan_vid_render_funcs.particles = &vulkan_particles_ID;
		}
	}
	vulkan_vid_render_funcs.particles = &vulkan_particles_QF;
}

void
Vulkan_r_particles_style_f (cvar_t *var, vulkan_ctx_t *ctx)
{
	Vulkan_r_easter_eggs_f (var, ctx);
}

void
Vulkan_DrawParticles (vulkan_ctx_t *ctx)
{
}

void
Vulkan_Particles_Init (vulkan_ctx_t *ctx)
{
	vulkan_vid_render_funcs.particles = &vulkan_particles_QF;

	qfv_device_t *device = ctx->device;

	qfvPushDebug (ctx, "particles init");

	particlectx_t *pctx = calloc (1, sizeof (particlectx_t));
	ctx->particle_context = pctx;

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
	pctx->draw_layout = Vulkan_CreatePipelineLayout (ctx, "draw_layout");

	pctx->pool = Vulkan_CreateDescriptorPool (ctx, "particle_pool");
	pctx->setLayout = Vulkan_CreateDescriptorSetLayout (ctx, "particle_set");

	for (size_t i = 0; i < frames; i++) {
		__auto_type pframe = &pctx->frames.a[i];

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
	}
	qfvPopDebug (ctx);
}

void
Vulkan_Particles_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	particlectx_t *pctx = ctx->particle_context;

	for (size_t i = 0; i < pctx->frames.size; i++) {
		__auto_type pframe = &pctx->frames.a[i];
		free (pframe->cmdSet.a);
	}

	dfunc->vkDestroyPipeline (device->dev, pctx->physics, 0);
	dfunc->vkDestroyPipeline (device->dev, pctx->update, 0);
	dfunc->vkDestroyPipeline (device->dev, pctx->draw, 0);
	free (pctx->frames.a);
	free (pctx);
}
