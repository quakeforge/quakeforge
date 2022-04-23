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
#include "QF/GL/qf_particles.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"
#include "varrays.h"

static int						partUseVA;
static int						pVAsize;
static int					   *pVAindices;
static varray_t2f_c4ub_v3f_t   *particleVertexArray;

static void
alloc_arrays (psystem_t *ps)
{
	int		i;

	if (ps->maxparticles && r_init) {
		if (vaelements) {
			partUseVA = 0;
			pVAsize = ps->maxparticles * 4;
			Sys_MaskPrintf (SYS_dev,
							"Particles: Vertex Array use disabled.\n");
		} else {
			if (vaelements > 3)
				pVAsize = min ((unsigned int) (vaelements - (vaelements % 4)),
							   ps->maxparticles * 4);
			else if (vaelements >= 0)
				pVAsize = ps->maxparticles * 4;
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

static void
gl_particles_f (void *data, const cvar_t *cvar)
{
	alloc_arrays (&r_psystem);//FIXME
}

void
gl_R_InitParticles (void)
{
	Cvar_AddListener (Cvar_FindVar ("r_particles"), gl_particles_f, 0);
	Cvar_AddListener (Cvar_FindVar ("r_particles_max"), gl_particles_f, 0);
	alloc_arrays (&r_psystem);
}

void
gl_R_DrawParticles (psystem_t *psystem)
{
	unsigned char  *at;
	int				vacount;
	float		minparticledist, scale;
	vec3_t			up_scale, right_scale, up_right_scale, down_right_scale;
	varray_t2f_c4ub_v3f_t		*VA;

	if (!r_psystem.numparticles) {
		return;
	}

	qfglBindTexture (GL_TEXTURE_2D, gl_part_tex);
	// LordHavoc: particles should not affect zbuffer
	qfglDepthMask (GL_FALSE);
	qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, particleVertexArray);

	minparticledist = DotProduct (r_refdef.frame.position,
								  r_refdef.frame.forward)
		+ r_particles_nearclip;

	vacount = 0;
	VA = particleVertexArray;

	for (unsigned i = 0; i < r_psystem.numparticles; i++) {
		particle_t *p = &r_psystem.particles[i];
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (p->pos, r_refdef.frame.forward) < minparticledist)) {
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

			VectorScale (r_refdef.frame.up, scale, up_scale);
			VectorScale (r_refdef.frame.right, scale, right_scale);

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

psystem_t * __attribute__((const))//FIXME
gl_ParticleSystem (void)
{
	return &r_psystem;
}
