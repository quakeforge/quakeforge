/*
	particles.h

	Client particles handling

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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
#ifndef __client_particles_h_
#define __client_particles_h_

#include "QF/render.h"

typedef enum {
	pt_static,
	pt_grav,
	pt_slowgrav,
	pt_fire,
	pt_explode,
	pt_explode2,
	pt_blob,
	pt_blob2,
	pt_smoke,
	pt_smokecloud,
	pt_bloodcloud,
	pt_fadespark,
	pt_fadespark2,
	pt_fallfade,
	pt_fallfadespark,
	pt_flame
} ptype_t;

typedef struct cl_particle_funcs_s {
	void (*RocketTrail) (vec4f_t start, vec4f_t end);
	void (*GrenadeTrail) (vec4f_t start, vec4f_t end);
	void (*BloodTrail) (vec4f_t start, vec4f_t end);
	void (*SlightBloodTrail) (vec4f_t start, vec4f_t end);
	void (*WizTrail) (vec4f_t start, vec4f_t end);
	void (*FlameTrail) (vec4f_t start, vec4f_t end);
	void (*VoorTrail) (vec4f_t start, vec4f_t end);
	void (*GlowTrail) (vec4f_t start, vec4f_t end, int glow_color);

	void (*RunParticleEffect) (vec4f_t org, vec4f_t dir, int color, int count);
	void (*BloodPuffEffect) (vec4f_t org, int count);
	void (*GunshotEffect) (vec4f_t org, int count);
	void (*LightningBloodEffect) (vec4f_t org);
	void (*SpikeEffect) (vec4f_t org);
	void (*KnightSpikeEffect) (vec4f_t org);
	void (*SuperSpikeEffect) (vec4f_t org);
	void (*WizSpikeEffect) (vec4f_t org);

	void (*BlobExplosion) (vec4f_t org);
	void (*ParticleExplosion) (vec4f_t org);
	void (*ParticleExplosion2) (vec4f_t org, int colorStart, int colorLength);
	void (*LavaSplash) (vec4f_t org);
	void (*TeleportSplash) (vec4f_t org);
	void (*DarkFieldParticles) (vec4f_t org);
	void (*EntityParticles) (vec4f_t org);

	void (*Particle_New) (ptype_t type, int texnum, vec4f_t org,
							float scale, vec4f_t vel, float die,
							int color, float alpha, float ramp);
	void (*Particle_NewRandom) (ptype_t type, int texnum, vec4f_t org,
								  int org_fuzz, float scale, int vel_fuzz,
								  float die, int color, float alpha,
								  float ramp);
} cl_particle_funcs_t;

extern cl_particle_funcs_t *clp_funcs;
extern float cl_frametime;
extern float cl_realtime;

void CL_Particles_Init (void);
void CL_ParticlesGravity (float gravity);

struct model_s;
void CL_LoadPointFile (const struct model_s *model);

#endif // __client_particles_h_
