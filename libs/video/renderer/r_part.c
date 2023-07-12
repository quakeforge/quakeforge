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
#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "compat.h"
#include "r_internal.h"

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

	size_t      size = sizeof (particle_t) + sizeof (partparm_t)
						+ sizeof (int *);

	if (ps->particles) {
		Sys_Free (ps->particles, ps->maxparticles * size);
		ps->particles = 0;
		ps->partparams = 0;
		ps->partramps = 0;
	}
	ps->maxparticles = maxparticles;

	if (ps->maxparticles) {
		ps->particles = Sys_Alloc (ps->maxparticles * size);
		ps->partparams = (partparm_t *) &ps->particles[ps->maxparticles];
		ps->partramps = (const int **) &ps->partparams[ps->maxparticles];
	}
	R_ClearParticles ();
}

void
R_ClearParticles (void)
{
	psystem_t  *ps = &r_psystem;//FIXME
	ps->numparticles = 0;
}

void
R_RunParticles (float dT)
{
	psystem_t  *ps = &r_psystem;//FIXME
	vec4f_t     gravity = ps->gravity;

	unsigned    j = 0;
	for (unsigned i = 0; i < ps->numparticles; i++) {
		particle_t *p = &ps->particles[i];
		partparm_t *parm = &ps->partparams[i];

		if (p->live <= 0 || p->ramp >= parm->ramp_max) {
			continue;
		}
		const int  *ramp = ps->partramps[i];
		if (i > j) {
			ps->particles[j] = *p;
			ps->partparams[j] = *parm;
			ps->partramps[j] = ramp;
		}
		j += 1;

		p->pos += dT * p->vel;
		p->vel += dT * (p->vel * parm->drag + gravity * parm->drag[3]);
		p->ramp += dT * parm->ramp;
		p->live -= dT;
		if (ramp) {
			p->icolor = ramp[(int)p->ramp];
		}
	}
	ps->numparticles = j;
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
