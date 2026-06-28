/*
	r_part.c

	Interface for particles

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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/ecs.h"
#include "QF/mersenne.h"
#include "QF/particle.h"

#include "r_internal.h"

typedef bool (*pemitter_func_f) (ecs_system_t *sys, ecs_pool_t *pool,
								 uint32_t ind, float dT);

static const component_t pemitter_components[pemitter_comp_count] = {
	[pemitter_plane] = {
		.size = sizeof (pe_plane_t),
		.name = "plane",
	},
};
static ecs_system_t pemitter_sys;
static ecs_system_t scene_sys;
static mtstate_t pemitter_mt;

psystem_t   r_psystem;	//FIXME singleton
int r_particles;
static cvar_t r_particles_cvar = {
	.name = "r_particles",
	.description =
		"Toggles drawing of particles.",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &r_particles },
};
int r_particles_max;
static cvar_t r_particles_max_cvar = {
	.name = "r_particles_max",
	.description =
		"Maximum amount of particles to display. No maximum, minimum is 0.",
	.default_value = "2048",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &r_particles_max },
};
float r_particles_nearclip;
static cvar_t r_particles_nearclip_cvar = {
	.name = "r_particles_nearclip",
	.description =
		"Distance of the particle near clipping plane from the player.",
	.default_value = "32",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &r_particles_nearclip },
};

/*
  R_MaxParticlesCheck

  Misty-chan: Dynamically change the maximum amount of particles on the fly.
  Thanks to a LOT of help from Taniwha, Deek, Mercury, Lordhavoc, and lots of
  others.
*/
static void
R_MaxParticlesCheck (void)
{
	psystem_t  *ps = &r_psystem;//FIXME
	unsigned    maxparticles = 0;

	maxparticles = r_particles ? r_particles_max : 0;

	if (ps->maxparticles == maxparticles) {
		return;
	}

	size_t      size = sizeof (particle_t) + sizeof (partparm_t);

	if (ps->particles) {
		Sys_Free (ps->particles, ps->maxparticles * size);
		ps->particles = nullptr;
		ps->partparams = nullptr;
		ps->partramps = nullptr;
	}
	ps->maxparticles = maxparticles;

	if (ps->maxparticles) {
		ps->particles = Sys_Alloc (ps->maxparticles * size);
		ps->partparams = (partparm_t *) &ps->particles[ps->maxparticles];
		ps->partramps = nullptr;
	}
	R_ClearParticles ();
}

static void
r_particles_nearclip_f (void *data, const cvar_t *cvar)
{
	r_particles_nearclip = bound (r_nearclip, r_particles_nearclip, r_farclip);
}

static void
r_particles_f (void *data, const cvar_t *cvar)
{
	R_MaxParticlesCheck ();
}

static void
r_particles_max_f (void *data, const cvar_t *cvar)
{
	R_MaxParticlesCheck ();
}

void
R_Particles_Init_Cvars (void)
{
	Cvar_Register (&r_particles_cvar, r_particles_f, 0);
	Cvar_Register (&r_particles_max_cvar, r_particles_max_f, 0);
	Cvar_Register (&r_particles_nearclip_cvar, r_particles_nearclip_f, 0);
}

void
R_Particles_Init (ecs_registry_t *reg)
{
	pemitter_sys = (ecs_system_t) {
		.reg = reg,
		.base = ECS_RegisterComponents (reg, pemitter_components,
										pemitter_comp_count),
	};
	mtwist_seed (&pemitter_mt, 0xdeadbeef);
}

void
R_Particles_NewScene (scene_t *scene)
{
	if (scene) {
		scene_sys = (ecs_system_t) {
			.reg = scene->reg,
			.base = scene->base,
		};
		scene->psys_base = pemitter_sys.base;
	} else {
		scene_sys = (ecs_system_t) {};
	}
}

void
R_ClearParticles (void)
{
	psystem_t  *ps = &r_psystem;//FIXME
	ps->numparticles = 0;
}

static void
psystem_run (psystem_t *ps, float dT)
{
	vec4f_t     center = ps->center;
	float       gravity = ps->gravity;
	float       min_dist = ps->min_dist * ps->min_dist;

	unsigned    j = 0;
	const int  *ramp = ps->partramps;
	for (unsigned i = 0; i < ps->numparticles; i++) {
		particle_t *p = &ps->particles[i];
		partparm_t *parm = &ps->partparams[i];

		if (p->live <= 0 || p->ramp >= parm->ramp_max) {
			continue;
		}
		if (i > j) {
			ps->particles[j] = *p;
			ps->partparams[j] = *parm;
		}

		p->pos += dT * p->vel;
		vec4f_t d = loadxyzf (center[3] * p->pos - center);
		float d2 = dotf(d, d)[0];
		if (d2 < min_dist) {
			p->live = 0;
			continue;
		}
		vec4f_t acc = -d * gravity * parm->drag[3] / (d2 * sqrtf (d2));
		p->vel += dT * (acc + p->vel * parm->drag);
		p->ramp += dT * parm->ramp_rate;
		p->scale += dT * parm->scale_rate;
		p->alpha -= dT * parm->alpha_rate;
		p->live -= dT;
		if (p->ramp_base >= 0) {
			p->color = ramp[p->ramp_base + (int)p->ramp];
		}

		j += 1;
	}
	ps->numparticles = j;
}

static inline int psystem_new (psystem_t *ps)
{
	if (ps->numparticles >= ps->maxparticles) {
		return -1;
	}
	return ps->numparticles++;
}

static inline bool
pemitter_update (pemitter_t *emitter, float dT)
{
	if (emitter->rate <= 0) {
		return false;
	}
	return true;
}

static inline uint32_t
pemitter_count (pemitter_t *emitter, float dT)
{
	if (emitter->rate <= 0) {
		return -emitter->rate;
	}
	uint32_t count = 0;
	emitter->timer += dT;
	if (emitter->timer > 0) {
		count = emitter->timer * emitter->rate;
		emitter->timer -= count / emitter->rate;
	}
	return count;
}

static inline particle_t *
spawn_particle (psystem_t *ps, peparticle_t *particle,
				int ramp_base, int ramp_range)
{
	int pind = psystem_new (ps);
	if (pind < 0) {
		return nullptr;
	}

	auto p = &ps->particles[pind];
	auto param = &ps->partparams[pind];

	*p = (particle_t) {
		.color = particle->color,
		.ramp_base = ramp_base,
		.alpha = particle->alpha,
		.tex = particle->tex,
		.ramp = mtwist_rand_0_1 (&pemitter_mt) * ramp_range,
		.scale = particle->scale,
		.live = particle->live,
	};
	*param = (partparm_t) {
		.drag = { VectorExpand (particle->drag), particle->grav_scale },
		.ramp_rate = particle->ramp_rate,
		.ramp_max = particle->ramp_max,
		.scale_rate = particle->scale_rate,
		.alpha_rate = particle->alpha_rate,
	};
	return p;
}

static vec4f_t
part_vel (vec3_t vel_scale)
{
	vec4f_t vel = {};
	for (int i = 0; i < 3; i++) {
		if (vel_scale[i]) {
			vel[i] = mtwist_rand_m1_1 (&pemitter_mt) * vel_scale[i];
		}
	}
	return vel;
}

static bool
pemit_plane (ecs_system_t *sys, ecs_pool_t *pool, uint32_t ind, float dT)
{
	auto plane = &((pe_plane_t *) pool->data) [ind];
	psystem_t  *ps = &r_psystem;//FIXME
	uint32_t count = pemitter_count (&plane->base, dT);

	entity_t ent = {
		.reg = scene_sys.reg,
		.id = pool->dense[ind],
		.base = scene_sys.base,
	};
	auto xform = Entity_Transform (ent);
	auto mat = Transform_GetWorldMatrixPtr (xform);

	while (count-- > 0) {
		auto p = spawn_particle (ps, &plane->particle, plane->base.ramp_base,
								 plane->base.ramp_range);
		if (!p) {
			break;
		}

		vec4f_t pos;
		if (plane->square) {
			float u, v;
			// FIXME figure out how to do in between 0 and 1
			if (plane->solid) {
				u = mtwist_rand_m1_1 (&pemitter_mt);
				v = mtwist_rand_m1_1 (&pemitter_mt);
			} else {
				float a = 4 * mtwist_rand_m1_1 (&pemitter_mt);
				u = (a < 0 ? max (-a - 2, 0) : min (a, 2)) - 1;
				v = (a < 0 ? min (-a, 2) : max (a - 2, 0)) - 1;
			}
			pos = u * loadvec3f (plane->u) + v * loadvec3f (plane->v);
		} else {
			float a = M_PI * mtwist_rand_m1_1 (&pemitter_mt);
			float s = plane->solid;
			float r = 1;
			if (s) {
				r = mtwist_rand_0_1 (&pemitter_mt);
				r = (1 - s) + sqrtf (r) * s;
			}
			float x = r * cos(a);
			float y = r * sin(a);
			pos = x * loadvec3f (plane->u) + y * loadvec3f (plane->v);
		}
		pos[3] = 1;
		vec4f_t vel = part_vel (plane->vel);
		p->pos = mvmulf (mat, pos);
		p->vel = mvmulf (mat, vel);
	}

	return pemitter_update (&plane->base, dT);
}

void
R_Particles_RunEmitters (float dT)
{
	static pemitter_func_f emit_func[pemitter_comp_count] = {
		[pemitter_plane] = pemit_plane,
	};
	auto reg = pemitter_sys.reg;
	for (int i = 0; i < pemitter_comp_count; i++) {
		uint32_t c = pemitter_sys.base + i;
		auto pool = &reg->comp_pools[c];
		for (uint32_t j = 0; j < pool->count; j++) {
			while (!emit_func[i] (&pemitter_sys, pool, j, dT)) {
				uint32_t ent = pool->dense[j];
				Ent_RemoveComponent (ent, c, reg);
			}
		}
	}
}

void
R_RunParticles (float dT)
{
	psystem_t  *ps = &r_psystem;//FIXME
	psystem_run (ps, dT);
}
