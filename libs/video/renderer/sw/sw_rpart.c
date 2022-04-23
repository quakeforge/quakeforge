/*
	sw_rpart.c

	Software renderer particle effects code.

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

vec3_t          r_pright, r_pup, r_ppn, r_porigin;

void
R_DrawParticles (psystem_t *psystem)
{
	if (!psystem->numparticles) {
		return;
	}
	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vfwd, r_ppn);
	VectorCopy (r_refdef.frame.position, r_porigin);

	for (unsigned i = 0; i < psystem->numparticles; i++) {
		particle_t *p = &psystem->particles[i];
		D_DrawParticle (p);
	}
}

psystem_t * __attribute__((const))//FIXME
sw_ParticleSystem (void)
{
	return &r_psystem;
}
