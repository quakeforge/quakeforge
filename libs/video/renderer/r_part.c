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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/render.h"

#include "compat.h"
#include "r_dynamic.h"
#include "r_local.h"

unsigned int	r_maxparticles, numparticles;
particle_t	   *active_particles, *free_particles, *particles, **freeparticles;
vec3_t			r_pright, r_pup, r_ppn;

void (*R_RocketTrail) (const struct entity_s *ent);
void (*R_GrenadeTrail) (const struct entity_s *ent);
void (*R_BloodTrail) (const struct entity_s *ent);
void (*R_SlightBloodTrail) (const struct entity_s *ent);
void (*R_WizTrail) (const struct entity_s *ent);
void (*R_FlameTrail) (const struct entity_s *ent);
void (*R_VoorTrail) (const struct entity_s *ent);
void (*R_GlowTrail) (const struct entity_s *ent, int glow_color);
void (*R_RunParticleEffect) (const vec3_t org, const vec3_t dir, int color, int count);
void (*R_BloodPuffEffect) (const vec3_t org, int count);
void (*R_GunshotEffect) (const vec3_t org, int count);
void (*R_LightningBloodEffect) (const vec3_t org);
void (*R_SpikeEffect) (const vec3_t org);
void (*R_KnightSpikeEffect) (const vec3_t org);
void (*R_SuperSpikeEffect) (const vec3_t org);
void (*R_WizSpikeEffect) (const vec3_t org);
void (*R_BlobExplosion) (const vec3_t org);
void (*R_ParticleExplosion) (const vec3_t org);
void (*R_ParticleExplosion2) (const vec3_t org, int colorStart, int colorLength);
void (*R_LavaSplash) (const vec3_t org);
void (*R_TeleportSplash) (const vec3_t org);
void (*R_DarkFieldParticles) (const struct entity_s *ent);
void (*R_EntityParticles) (const struct entity_s *ent);


/*
  R_MaxParticlesCheck

  Misty-chan: Dynamically change the maximum amount of particles on the fly.
  Thanks to a LOT of help from Taniwha, Deek, Mercury, Lordhavoc, and lots of
  others.
*/
void
R_MaxParticlesCheck (cvar_t *r_particles, cvar_t *r_particles_max)
{
/*
	Catchall. If the user changed the setting to a number less than zero *or*
	if we had a wacky cfg get past the init code check, this will make sure we
	don't have problems. Also note that grabbing the var->int_val is IMPORTANT:
	Prevents a segfault since if we grabbed the int_val of r_particles_max
	we'd sig11 right here at startup.
*/
	if (r_particles && r_particles->int_val)
		r_maxparticles = r_particles_max ? r_particles_max->int_val : 0;
	else
		r_maxparticles = 0;

/*
	Be very careful the next time we do something like this. calloc/free are
	IMPORTANT and the compiler doesn't know when we do bad things with them.
*/
	if (particles)
		free (particles);
	if (freeparticles)
		free (freeparticles);

	particles = 0;
	freeparticles = 0;

	if (r_maxparticles) {
		particles = (particle_t *) calloc (r_maxparticles, sizeof (particle_t));
		freeparticles = (particle_t **) calloc (r_maxparticles,
												sizeof (particle_t *));
	}

	R_ClearParticles ();

	if (r_init)
		R_InitParticles ();
}
