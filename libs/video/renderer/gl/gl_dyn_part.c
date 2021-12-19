/*
	gl_dyn_part.c

	OpenGL particle system.

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/mersenne.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_explosions.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"
#include "varrays.h"

static int						partUseVA;
static int						pVAsize;
static int					   *pVAindices;
static varray_t2f_c4ub_v3f_t   *particleVertexArray;

void
gl_R_InitParticles (void)
{
	int		i;

	if (r_maxparticles && r_init) {
		if (vaelements) {
			partUseVA = 0;
			pVAsize = r_maxparticles * 4;
			Sys_MaskPrintf (SYS_dev,
							"Particles: Vertex Array use disabled.\n");
		} else {
			if (vaelements > 3)
				pVAsize = min ((unsigned int) (vaelements - (vaelements % 4)),
							   r_maxparticles * 4);
			else if (vaelements >= 0)
				pVAsize = r_maxparticles * 4;
			Sys_MaskPrintf (SYS_dev,
							"Particles: %i maximum vertex elements.\n",
							pVAsize);
		}
		if (particleVertexArray)
			free (particleVertexArray);
		particleVertexArray = (varray_t2f_c4ub_v3f_t *)
			calloc (pVAsize, sizeof (varray_t2f_c4ub_v3f_t));

		if (partUseVA)
			qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, particleVertexArray);

		if (pVAindices)
			free (pVAindices);
		pVAindices = (int *) calloc (pVAsize, sizeof (int));
		for (i = 0; i < pVAsize; i++)
			pVAindices[i] = i;
	} else {
		if (particleVertexArray) {
			free (particleVertexArray);
			particleVertexArray = 0;
		}
		if (pVAindices) {
			free (pVAindices);
			pVAindices = 0;
		}
	}
}

void
gl_R_DrawParticles (void)
{
	unsigned char  *at;
	int				vacount;
	float		minparticledist, scale;
	vec3_t			up_scale, right_scale, up_right_scale, down_right_scale;
	varray_t2f_c4ub_v3f_t		*VA;

	if (!r_particles->int_val)
		return;

	R_RunParticles (vr_data.frametime);

	qfglBindTexture (GL_TEXTURE_2D, gl_part_tex);
	// LordHavoc: particles should not affect zbuffer
	qfglDepthMask (GL_FALSE);
	qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, particleVertexArray);

	minparticledist = DotProduct (r_refdef.viewposition, vpn) +
		r_particles_nearclip->value;

	vacount = 0;
	VA = particleVertexArray;

	for (unsigned i = 0; i < numparticles; i++) {
		particle_t *p = &particles[i];
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (p->pos, vpn) < minparticledist)) {
			at = (byte *) &d_8to24table[(byte) p->icolor];
			VA[0].color[0] = at[0];
			VA[0].color[1] = at[1];
			VA[0].color[2] = at[2];
			VA[0].color[3] = p->alpha * 255;
			memcpy (VA[1].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[2].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[3].color, VA[0].color, sizeof (VA[0].color));

			switch (p->tex) {
				case part_tex_dot:
					VA[0].texcoord[0] = 0.0;
					VA[0].texcoord[1] = 0.0;
					VA[1].texcoord[0] = 0.5;
					VA[1].texcoord[1] = 0.0;
					VA[2].texcoord[0] = 0.5;
					VA[2].texcoord[1] = 0.5;
					VA[3].texcoord[0] = 0.0;
					VA[3].texcoord[1] = 0.5;
					break;
				case part_tex_spark:
					VA[0].texcoord[0] = 0.5;
					VA[0].texcoord[1] = 0.0;
					VA[1].texcoord[0] = 1.0;
					VA[1].texcoord[1] = 0.0;
					VA[2].texcoord[0] = 1.0;
					VA[2].texcoord[1] = 0.5;
					VA[3].texcoord[0] = 0.5;
					VA[3].texcoord[1] = 0.5;
					break;
				case part_tex_smoke:
					VA[0].texcoord[0] = 0.0;
					VA[0].texcoord[1] = 0.5;
					VA[1].texcoord[0] = 0.5;
					VA[1].texcoord[1] = 0.5;
					VA[2].texcoord[0] = 0.5;
					VA[2].texcoord[1] = 1.0;
					VA[3].texcoord[0] = 0.0;
					VA[3].texcoord[1] = 1.0;
					break;
			}

			scale = p->scale;

			VectorScale (vup, scale, up_scale);
			VectorScale (vright, scale, right_scale);

			VectorAdd (right_scale, up_scale, up_right_scale);
			VectorSubtract (right_scale, up_scale, down_right_scale);

			VectorAdd (p->pos, down_right_scale, VA[0].vertex);
			VectorSubtract (p->pos, up_right_scale, VA[1].vertex);
			VectorSubtract (p->pos, down_right_scale, VA[2].vertex);
			VectorAdd (p->pos, up_right_scale, VA[3].vertex);

			VA += 4;
			vacount += 4;
			if (vacount + 4 > pVAsize) {
				// never reached if partUseVA is false
				qfglDrawElements (GL_QUADS, vacount, GL_UNSIGNED_INT,
								  pVAindices);
				vacount = 0;
				VA = particleVertexArray;
			}
		}
	}

	if (vacount) {
		if (partUseVA) {
			qfglDrawElements (GL_QUADS, vacount, GL_UNSIGNED_INT, pVAindices);
		} else {
			varray_t2f_c4ub_v3f_t *va = particleVertexArray;
			int         i;

			qfglBegin (GL_QUADS);
			for (i = 0; i < vacount; i++, va++) {
				qfglTexCoord2fv (va->texcoord);
				qfglColor4ubv (va->color);
				qfglVertex3fv (va->vertex);
			}
			qfglEnd ();
		}
	}

	qfglColor3ubv (color_white);
	qfglDepthMask (GL_TRUE);
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
gl_R_Particles_Init_Cvars (void)
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
