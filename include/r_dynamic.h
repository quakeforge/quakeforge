/*
	render.h

	public interface to refresh functions

	Copyright (C) 1996-1997  Id Software, Inc.

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

	$Id$
*/

#ifndef _R_DYNAMIC_H
#define _R_DYNAMIC_H

#include "QF/mathlib.h"

typedef enum {
    PE_UNKNOWN,
    PE_GUNSHOT,
    PE_BLOOD,
    PE_LIGHTNINGBLOOD,
    PE_SPIKE,
    PE_SUPERSPIKE,
    PE_KNIGHTSPIKE,
    PE_WIZSPIKE,
} particle_effect_t;

struct entity_s;

extern void (*R_RocketTrail) (const struct entity_s *ent);
extern void (*R_GrenadeTrail) (const struct entity_s *ent);
extern void (*R_BloodTrail) (const struct entity_s *ent);
extern void (*R_SlightBloodTrail) (const struct entity_s *ent);
extern void (*R_WizTrail) (const struct entity_s *ent);
extern void (*R_FlameTrail) (const struct entity_s *ent);
extern void (*R_VoorTrail) (const struct entity_s *ent);
extern void (*R_GlowTrail) (const struct entity_s *ent, int glow_color);

extern void (*R_RunParticleEffect) (const vec3_t org, const vec3_t dir,
									int color, int count);
extern void (*R_BloodPuffEffect) (const vec3_t org, int count);
extern void (*R_GunshotEffect) (const vec3_t org, int count);
extern void (*R_LightningBloodEffect) (const vec3_t org);
extern void (*R_SpikeEffect) (const vec3_t org);
extern void (*R_KnightSpikeEffect) (const vec3_t org);
extern void (*R_SuperSpikeEffect) (const vec3_t org);
extern void (*R_WizSpikeEffect) (const vec3_t org);

extern void (*R_BlobExplosion) (const vec3_t org);
extern void (*R_ParticleExplosion) (const vec3_t org);
extern void (*R_ParticleExplosion2) (const vec3_t org, int colorStart,
									 int colorLength);
extern void (*R_LavaSplash) (const vec3_t org);
extern void (*R_TeleportSplash) (const vec3_t org);
extern void (*R_DarkFieldParticles) (const struct entity_s *ent);
extern void (*R_EntityParticles) (const struct entity_s *ent);

void R_PushDlights (const vec3_t entorigin);
struct cvar_s;
void R_MaxDlightsCheck (struct cvar_s *var);
void R_Particles_Init_Cvars (void);
void R_InitBubble (void);

void R_InitParticles (void);
void R_ClearParticles (void);
void R_DrawParticles (void);
struct cvar_s;
void R_MaxParticlesCheck (struct cvar_s *r_particles,
						  struct cvar_s *r_particles_max);
void R_InitSprites (void);

extern unsigned int r_maxparticles;
extern unsigned int numparticles;
extern struct particle_s *active_particles;
extern struct particle_s *free_particles;
extern struct particle_s *particles;
extern struct particle_s **freeparticles;

#endif // _R_DYNAMIC_H
