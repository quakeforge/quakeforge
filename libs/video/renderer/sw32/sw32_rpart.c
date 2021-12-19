/*
	sw32_rpart.c

	24 bit color software renderer particle effects.

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

#define NH_DEFINE
#include "namehack.h"

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/mersenne.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "compat.h"
#include "r_internal.h"

void
sw32_R_DrawParticles (void)
{
	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vpn, r_ppn);

	vec4f_t     gravity = {0, 0, -vr_data.gravity, 0};

	unsigned    j = 0;
	for (unsigned i = 0; i < numparticles; i++) {
		particle_t *p = &particles[i];
		partparm_t *parm = &partparams[i];

		if (p->live <= 0 || p->ramp >= parm->ramp_max) {
			continue;
		}
		const int  *ramp = partramps[j];
		if (i > j) {
			particles[j] = *p;
			partparams[j] = *parm;
			partramps[j] = ramp;
		}
		p = &particles[j];
		parm = &partparams[j];
		j += 1;

		sw32_D_DrawParticle (p);

		float       dT = vr_data.frametime;
		p->pos += dT * p->vel;
		p->vel += dT * (p->vel * parm->drag + gravity * parm->drag[3]);
		p->ramp += dT * parm->ramp;
		p->live -= dT;
		if (ramp) {
			p->icolor = ramp[(int)p->ramp];
		}
	}
	numparticles = j;
}

static void
r_particles_nearclip_f (cvar_t *var)
{
	Cvar_SetValue (r_particles_nearclip, bound (r_nearclip->value, var->value,
												r_farclip->value));
}

static void
r_particles_f (cvar_t *var)
{
	R_MaxParticlesCheck (var, r_particles_max);
}

static void
r_particles_max_f (cvar_t *var)
{
	R_MaxParticlesCheck (r_particles, var);
}

void
sw32_R_Particles_Init_Cvars (void)
{
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
}
