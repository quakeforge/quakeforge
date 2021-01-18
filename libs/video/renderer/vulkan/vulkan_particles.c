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
#include "QF/plugin/vid_render.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/qf_particles.h"

#include "r_internal.h"
#include "vid_vulkan.h"

void
Vulkan_R_ClearParticles (struct vulkan_ctx_s *ctx)
{
}

void
Vulkan_R_InitParticles (struct vulkan_ctx_s *ctx)
{
}

static void
R_RocketTrail_QF (const entity_t *ent)
{
}

static void
R_GrenadeTrail_QF (const entity_t *ent)
{
}

static void
R_BloodTrail_QF (const entity_t *ent)
{
}

static void
R_SlightBloodTrail_QF (const entity_t *ent)
{
}

static void
R_WizTrail_QF (const entity_t *ent)
{
}

static void
R_FlameTrail_QF (const entity_t *ent)
{
}

static void
R_VoorTrail_QF (const entity_t *ent)
{
}

static void
R_GlowTrail_QF (const entity_t *ent, int glow_color)
{
}

static void
R_RunParticleEffect_QF (const vec3_t org, const vec3_t dir, int color,
					    int count)
{
}

static void
R_BloodPuffEffect_QF (const vec3_t org, int count)
{
}

static void
R_GunshotEffect_QF (const vec3_t org, int count)
{
}

static void
R_LightningBloodEffect_QF (const vec3_t org)
{
}

static void
R_SpikeEffect_QF (const vec3_t org)
{
}

static void
R_KnightSpikeEffect_QF (const vec3_t org)
{
}

static void
R_SuperSpikeEffect_QF (const vec3_t org)
{
}

static void
R_WizSpikeEffect_QF (const vec3_t org)
{
}

static void
R_BlobExplosion_QF (const vec3_t org)
{
}

static void
R_ParticleExplosion_QF (const vec3_t org)
{
}

static void
R_ParticleExplosion2_QF (const vec3_t org, int colorStart, int colorLength)
{
}

static void
R_LavaSplash_QF (const vec3_t org)
{
}

static void
R_TeleportSplash_QF (const vec3_t org)
{
}

static void
R_DarkFieldParticles_ID (const entity_t *ent)
{
}

static void
R_EntityParticles_ID (const entity_t *ent)
{
}

static void
R_Particle_New (ptype_t type, int texnum, const vec3_t org,
				float scale, const vec3_t vel, float die,
				int color, float alpha, float ramp)
{
}

static void
R_Particle_NewRandom (ptype_t type, int texnum, const vec3_t org,
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
Vulkan_r_easter_eggs_f (cvar_t *var, struct vulkan_ctx_s *ctx)
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
Vulkan_r_particles_style_f (cvar_t *var, struct vulkan_ctx_s *ctx)
{
	Vulkan_r_particles_style_f (var, ctx);
}

void
Vulkan_Particles_Init (struct vulkan_ctx_s *ctx)
{
	/*
	easter_eggs = Cvar_Get ("easter_eggs", "0", CVAR_NONE, r_easter_eggs_f,
							"Enables easter eggs.");
	r_particles = Cvar_Get ("r_particles", "1", CVAR_ARCHIVE, r_particles_f,
							"Toggles drawing of particles.");
	r_particles_max = Cvar_Get ("r_particles_max", "2048", CVAR_ARCHIVE,
								r_particles_max_f, "Maximum amount of "
								"particles to display. No maximum, minimum "
								"is 0.");
	r_particles_nearclip = Cvar_Get ("r_particles_nearclip", "32",
									 CVAR_ARCHIVE, r_particles_nearclip_f,
									 "Distance of the particle near clipping "
									 "plane from the player.");
	r_particles_style = Cvar_Get ("r_particles_style", "1", CVAR_ARCHIVE,
								  r_particles_style_f, "Sets particle style. "
								  "0 for Id, 1 for QF.");
	*/
	vulkan_vid_render_funcs.particles = &vulkan_particles_QF;
}
