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

void R_ParseParticleEffect (void);
struct entity_s;
void R_RocketTrail (struct entity_s *ent);
void R_GrenadeTrail (struct entity_s *ent);
void R_BloodTrail (struct entity_s *ent);
void R_SlightBloodTrail (struct entity_s *ent);
void R_GreenTrail (struct entity_s *ent);
void R_FlameTrail (struct entity_s *ent);
void R_VoorTrail (struct entity_s *ent);
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void R_RunPuffEffect (vec3_t org, particle_effect_t type, byte count);
void R_RunSpikeEffect (vec3_t org, particle_effect_t type);

void R_DarkFieldParticles (struct entity_s *ent);
void R_EntityParticles (struct entity_s *ent);
void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);
void R_PushDlights (vec3_t entorigin);
struct cvar_s;
void R_MaxDlightsCheck (struct cvar_s *var);
void R_Particles_Init_Cvars (void);

void R_InitParticles (void);
inline void R_ClearParticles (void);
void R_DrawParticles (void);
struct cvar_s;
void R_MaxParticlesCheck (struct cvar_s *r_particles, 
						  struct cvar_s *cl_max_particles);

extern unsigned int r_maxparticles;
extern unsigned int numparticles;
extern struct particle_s *active_particles;
extern struct particle_s *free_particles;
extern struct particle_s *particles;
extern struct particle_s **freeparticles;

#define MAX_FIRES				128		// rocket flames

typedef struct {
	int		key;			// allows reusability
	vec3_t	origin, owner;
	float	size;
	float	die, decay;		// duration settings
	float	minlight;		// lighting threshold
	float	color[3];		// RGB
} fire_t;

extern fire_t r_fires[];

void R_AddFire (vec3_t start, vec3_t end, struct entity_s *ent);
void R_UpdateFires (void);
fire_t *R_AllocFire (int key);
void R_ClearFires (void);

#endif // _R_DYNAMIC_H
