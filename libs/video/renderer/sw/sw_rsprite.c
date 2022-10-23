/*
	sw_rsprite.c

	(description)

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
# include <string.h>
#endif

#include <math.h>

#include "QF/render.h"
#include "QF/sys.h"

#include "QF/scene/component.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "r_internal.h"

static int  clip_current;
static vec5_t clip_verts[2][MAXWORKINGVERTS];
static int  sprite_width, sprite_height;

spritedesc_t r_spritedesc;


static void
R_RotateSprite (vec4f_t relvieworg, float beamlength, vec3_t org)
{
	vec3_t      vec;

	VectorCopy (relvieworg, org);
	if (beamlength == 0.0)
		return;

	VectorScale (r_spritedesc.vfwd, -beamlength, vec);
	VectorAdd (r_entorigin, vec, r_entorigin);
	VectorSubtract (relvieworg, vec, org);
}


/*
	R_ClipSpriteFace

	Clips the winding at clip_verts[clip_current] and changes clip_current
	Throws out the back side
*/
static int
R_ClipSpriteFace (int nump, clipplane_t *pclipplane)
{
	int         i, outcount;
	float       dists[MAXWORKINGVERTS + 1];
	float       frac, clipdist, *pclipnormal;
	float      *in, *instep, *outstep, *vert2;

	clipdist = pclipplane->dist;
	pclipnormal = pclipplane->normal;

	// calc dists
	if (clip_current) {
		in = clip_verts[1][0];
		outstep = clip_verts[0][0];
		clip_current = 0;
	} else {
		in = clip_verts[0][0];
		outstep = clip_verts[1][0];
		clip_current = 1;
	}

	instep = in;
	for (i = 0; i < nump; i++, instep += sizeof (vec5_t) / sizeof (float)) {
		dists[i] = DotProduct (instep, pclipnormal) - clipdist;
	}

	// handle wraparound case
	dists[nump] = dists[0];
	memcpy (instep, in, sizeof (vec5_t));

	// clip the winding
	instep = in;
	outcount = 0;

	for (i = 0; i < nump; i++, instep += sizeof (vec5_t) / sizeof (float)) {
		if (dists[i] >= 0) {
			memcpy (outstep, instep, sizeof (vec5_t));
			outstep += sizeof (vec5_t) / sizeof (float);
			outcount++;
		}

		if (dists[i] == 0 || dists[i + 1] == 0)
			continue;
#if __APPLE_CC__ <= 1175
		// bug in gcc (GCC) 3.1 20020420 (prerelease) for darwin
		if ((dists[i] > 0) && (dists[i + 1] > 0))
			continue;
		if ((dists[i] <= 0) && (dists[i + 1] <= 0))
			continue;
#else
		if ((dists[i] > 0) == (dists[i + 1] > 0))
			continue;
#endif

		// split it into a new vertex
		frac = dists[i] / (dists[i] - dists[i + 1]);

		vert2 = instep + sizeof (vec5_t) / sizeof (float);

		outstep[0] = instep[0] + frac * (vert2[0] - instep[0]);
		outstep[1] = instep[1] + frac * (vert2[1] - instep[1]);
		outstep[2] = instep[2] + frac * (vert2[2] - instep[2]);
		outstep[3] = instep[3] + frac * (vert2[3] - instep[3]);
		outstep[4] = instep[4] + frac * (vert2[4] - instep[4]);

		outstep += sizeof (vec5_t) / sizeof (float);

		outcount++;
	}

	return outcount;
}


static void
R_SetupAndDrawSprite (const vec3_t relvieworg)
{
	int         i, nump;
	float       dot, scale, *pv;
	vec5_t     *pverts;
	vec3_t      left, up, right, down, transformed, local;
	emitpoint_t outverts[MAXWORKINGVERTS + 1], *pout;

	dot = DotProduct (r_spritedesc.vfwd, relvieworg);

	// backface cull
	if (dot >= 0)
		return;

	// build the sprite poster in worldspace
	VectorScale (r_spritedesc.vright, r_spritedesc.pspriteframe->right, right);
	VectorScale (r_spritedesc.vup, r_spritedesc.pspriteframe->up, up);
	VectorScale (r_spritedesc.vright, r_spritedesc.pspriteframe->left, left);
	VectorScale (r_spritedesc.vup, r_spritedesc.pspriteframe->down, down);

	pverts = clip_verts[0];

	pverts[0][0] = r_entorigin[0] + up[0] + left[0];
	pverts[0][1] = r_entorigin[1] + up[1] + left[1];
	pverts[0][2] = r_entorigin[2] + up[2] + left[2];
	pverts[0][3] = 0;
	pverts[0][4] = 0;

	pverts[1][0] = r_entorigin[0] + up[0] + right[0];
	pverts[1][1] = r_entorigin[1] + up[1] + right[1];
	pverts[1][2] = r_entorigin[2] + up[2] + right[2];
	pverts[1][3] = sprite_width;
	pverts[1][4] = 0;

	pverts[2][0] = r_entorigin[0] + down[0] + right[0];
	pverts[2][1] = r_entorigin[1] + down[1] + right[1];
	pverts[2][2] = r_entorigin[2] + down[2] + right[2];
	pverts[2][3] = sprite_width;
	pverts[2][4] = sprite_height;

	pverts[3][0] = r_entorigin[0] + down[0] + left[0];
	pverts[3][1] = r_entorigin[1] + down[1] + left[1];
	pverts[3][2] = r_entorigin[2] + down[2] + left[2];
	pverts[3][3] = 0;
	pverts[3][4] = sprite_height;

	// clip to the frustum in worldspace
	nump = 4;
	clip_current = 0;

	for (i = 0; i < 4; i++) {
		nump = R_ClipSpriteFace (nump, &view_clipplanes[i]);
		if (nump < 3)
			return;
		if (nump >= MAXWORKINGVERTS)
			Sys_Error ("R_SetupAndDrawSprite: too many points");
	}

	// transform vertices into viewspace and project
	pv = &clip_verts[clip_current][0][0];
	r_spritedesc.nearzi = -999999;

	for (i = 0; i < nump; i++) {
		VectorSubtract (pv, r_refdef.frame.position, local);
		TransformVector (local, transformed);

		if (transformed[2] < NEAR_CLIP)
			transformed[2] = NEAR_CLIP;

		pout = &outverts[i];
		pout->zi = 1.0 / transformed[2];
		if (pout->zi > r_spritedesc.nearzi)
			r_spritedesc.nearzi = pout->zi;

		pout->s = pv[3];
		pout->t = pv[4];

		scale = xscale * pout->zi;
		pout->u = (xcenter + scale * transformed[0]);

		scale = yscale * pout->zi;
		pout->v = (ycenter - scale * transformed[1]);

		pv += sizeof (vec5_t) / sizeof (*pv);
	}

	// draw it
	r_spritedesc.nump = nump;
	r_spritedesc.pverts = outverts;
	D_DrawSprite (relvieworg);
}

void
R_DrawSprite (entity_t ent)
{
	transform_t transform = Entity_Transform (ent);
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer,
											 r_refdef.scene->reg);
	animation_t *animation = Ent_GetComponent (ent.id, scene_animation,
											   r_refdef.scene->reg);
	msprite_t  *sprite = renderer->model->cache.data;

	vec4f_t     cameravec = r_refdef.frame.position - r_entorigin;

	r_spritedesc.pspriteframe = R_GetSpriteFrame (sprite, animation);

	sprite_width = r_spritedesc.pspriteframe->width;
	sprite_height = r_spritedesc.pspriteframe->height;

	vec4f_t     up = {};
	vec4f_t     right = {};
	vec4f_t     fwd = {};
	if (!R_BillboardFrame (transform, sprite->type, cameravec,
						   &up, &right, &fwd)) {
		// the orientation is undefined so can't draw the sprite
		return;
	}
	VectorCopy (up, r_spritedesc.vup);//FIXME
	VectorCopy (right, r_spritedesc.vright);
	VectorCopy (fwd, r_spritedesc.vfwd);

	vec3_t      org;
	R_RotateSprite (cameravec, sprite->beamlength, org);

	R_SetupAndDrawSprite (org);
}
