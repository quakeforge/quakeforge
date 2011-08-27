/*
	cl_ents.c

	entity parsing and management

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/plugin.h"
#include "QF/qfplist.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "chase.h"
#include "client.h"
#include "compat.h"
#include "host.h"
#include "host.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "server.h"

// FIXME: put these on hunk?
entity_t    cl_entities[MAX_EDICTS];
cl_entity_state_t    cl_baselines[MAX_EDICTS];


void
CL_ClearEnts (void)
{
	size_t      i;

	// clear other arrays   
	memset (cl_entities, 0, sizeof (cl_entities));
	memset (cl_baselines, 0, sizeof (cl_baselines));
	memset (r_lightstyle, 0, sizeof (r_lightstyle));

	for (i = 0; i < MAX_EDICTS; i++) {
		cl_baselines[i].ent = &cl_entities[i];
		CL_Init_Entity (cl_entities + i);
	}
}

static void
CL_NewDlight (int key, vec3_t org, int effects, byte glow_size,
			  byte glow_color)
{
	float       radius;
	dlight_t   *dl;
	static quat_t normal = {0.4, 0.2, 0.05, 0.7};
	static quat_t red = {0.5, 0.05, 0.05, 0.7};
	static quat_t blue = {0.05, 0.05, 0.5, 0.7};
	static quat_t purple = {0.5, 0.05, 0.5, 0.7};

	effects &= EF_BLUE | EF_RED | EF_BRIGHTLIGHT | EF_DIMLIGHT;
	if (!effects) {
		if (!glow_size)
			return;
	}

	dl = R_AllocDlight (key);
	if (!dl)
		return;
	VectorCopy (org, dl->origin);

	if (effects & (EF_BLUE | EF_RED | EF_BRIGHTLIGHT | EF_DIMLIGHT)) {
		radius = 200 + (rand () & 31);
		if (effects & EF_BRIGHTLIGHT) {
			radius += 200;
			dl->origin[2] += 16;
		}
		if (effects & EF_DIMLIGHT)
			if (effects & ~EF_DIMLIGHT)
				radius -= 100;
		dl->radius = radius;
		dl->die = cl.time + 0.1;

		switch (effects & (EF_RED | EF_BLUE)) {
			case EF_RED | EF_BLUE:
				QuatCopy (purple, dl->color);
				break;
			case EF_RED:
				QuatCopy (red, dl->color);
				break;
			case EF_BLUE:
				QuatCopy (blue, dl->color);
				break;
			default:
				QuatCopy (normal, dl->color);
				break;
		}
	}

	if (glow_size) {
		dl->radius += glow_size < 128 ? glow_size * 8.0 :
			(glow_size - 256) * 8.0;
		dl->die = cl.time + 0.1;
		if (glow_color) {
			if (glow_color == 255) {
				dl->color[0] = dl->color[1] = dl->color[2] = 1.0;
			} else {
				byte        *tempcolor;

				tempcolor = (byte *) &d_8to24table[glow_color];
				VectorScale (tempcolor, 1 / 255.0, dl->color);
			}
		}
	}
}

/*
	CL_LerpPoint

	Determines the fraction between the last two messages at which the
	objects should be put.
*/
static float
CL_LerpPoint (void)
{
	float       f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cl_nolerp->int_val || cls.timedemo || sv.active) {
		cl.time = cl.mtime[0];
		return 1;
	}

	if (f > 0.1) {						// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - cl.mtime[1]) / f;

	if (frac < 0) {
		if (frac < -0.01)
			cl.time = cl.mtime[1];
		frac = 0;
	} else if (frac > 1) {
		if (frac > 1.01)
			cl.time = cl.mtime[0];
		frac = 1;
	}

	return frac;
}

void
CL_RelinkEntities (void)
{
	entity_t   *ent;
	cl_entity_state_t *state;
	dlight_t   *dl;
	float       bobjrotate, frac, f, d;
	int         i, j;
	vec3_t      delta;
	
	r_player_entity = &cl_entities[cl.viewentity];

	// determine partial update time    
	frac = CL_LerpPoint ();

	// interpolate player info
	for (i = 0; i < 3; i++)
		cl.velocity[i] = cl.mvelocity[1][i] +
			frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if (cls.demoplayback) {
		// interpolate the angles   
		for (j = 0; j < 3; j++) {
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.viewangles[j] = cl.mviewangles[1][j] + frac * d;
		}
	}

	bobjrotate = anglemod (100 * cl.time);

	// start on the entity after the world
	for (i = 1, state = cl_baselines + 1; i < cl.num_entities; i++, state++) {
		ent = state->ent;
		if (!ent->model) {				// empty slot
			if (ent->efrag)
				R_RemoveEfrags (ent);	// just became empty
			continue;
		}
		// if the object wasn't included in the last packet, remove it
		if (state->msgtime != cl.mtime[0]) {
			ent->model = NULL;
			//johnfitz -- next time this entity slot is reused, the lerp will
			//need to be reset
			ent->lerpflags |= LERP_RESETMOVE|LERP_RESETANIM;
			if (ent->efrag)
				R_RemoveEfrags (ent);	// just became empty
			continue;
		}

		ent->colormod[3] = ENTALPHA_DECODE (state->alpha);

		VectorCopy (ent->origin, ent->old_origin);

		if (state->forcelink) {
			// The entity was not updated in the last message so move to the
			// final spot
			VectorCopy (state->msg_origins[0], ent->origin);
			VectorCopy (state->msg_angles[0], ent->angles);
			if (i != cl.viewentity || chase_active->int_val) {
				if (ent->efrag)
					R_RemoveEfrags (ent);
				R_AddEfrags (ent);
			}
		} else {
			// If the delta is large, assume a teleport and don't lerp
			f = frac;
			VectorSubtract (state->msg_origins[0],
							state->msg_origins[1], delta);
			if (fabs (delta[0]) > 100 || fabs (delta[1] > 100)
				|| fabs (delta[2]) > 100) {
				// assume a teleportation, not a motion
				VectorCopy (state->msg_origins[0], ent->origin);
				VectorCopy (state->msg_angles[0], ent->angles);
				ent->lerpflags |= LERP_RESETMOVE;
			} else {
				// interpolate the origin and angles
				// FIXME r_lerpmove.value &&
				if (ent->lerpflags & LERP_MOVESTEP)
					f = 1;
				VectorMultAdd (state->msg_origins[1], f, delta, ent->origin);
				for (j = 0; j < 3; j++) {
					d = state->msg_angles[0][j] - state->msg_angles[1][j];
					if (d > 180)
						d -= 360;
					else if (d < -180)
						d += 360;
					ent->angles[j] = state->msg_angles[1][j] + f * d;
				}
			}
			if (i != cl.viewentity || chase_active->int_val) {
				if (ent->efrag) {
					if (!VectorCompare (ent->origin, ent->old_origin)) {
						R_RemoveEfrags (ent);
						R_AddEfrags (ent);
					}
				} else {
					R_AddEfrags (ent);
				}
			}
		}

		// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
			ent->angles[1] = bobjrotate;

		if (state->effects & EF_BRIGHTFIELD)
			R_EntityParticles (ent);
		if (state->effects & EF_MUZZLEFLASH) {
			vec3_t      fv, rv, uv;

			dl = R_AllocDlight (i);
			if (dl) {
				AngleVectors (ent->angles, fv, rv, uv);

				VectorMultAdd (ent->origin, 18, fv, dl->origin);
				dl->origin[2] += 16;
				dl->radius = 200 + (rand () & 31);
				dl->die = cl.time + 0.1;
				dl->minlight = 32;
				dl->color[0] = 0.2;
				dl->color[1] = 0.1;
				dl->color[2] = 0.05;
				dl->color[3] = 0.7;
			}
#if 0		//FIXME how much do we want this?
			//johnfitz -- assume muzzle flash accompanied by muzzle flare,
			//which looks bad when lerped
			if (ent == &cl_entities[cl.viewentity])
				cl.viewent.lerpflags |= LERP_RESETANIM | LERP_RESETANIM2;
			else
				ent->lerpflags |= LERP_RESETANIM | LERP_RESETANIM2;
#endif
		}
		CL_NewDlight (i, ent->origin, state->effects, 0, 0);
		if (VectorDistance_fast (state->msg_origins[1], ent->origin)
			> (256 * 256))
			VectorCopy (ent->origin, state->msg_origins[1]);
		if (ent->model->flags & EF_ROCKET) {
			dl = R_AllocDlight (i);
			if (dl) {
				VectorCopy (ent->origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.1;
				VectorCopy (r_firecolor->vec, dl->color);
				dl->color[3] = 0.7;
			}
			R_RocketTrail (ent);
		} else if (ent->model->flags & EF_GRENADE)
			R_GrenadeTrail (ent);
		else if (ent->model->flags & EF_GIB)
			R_BloodTrail (ent);
		else if (ent->model->flags & EF_ZOMGIB)
			R_SlightBloodTrail (ent);
		else if (ent->model->flags & EF_TRACER)
			R_WizTrail (ent);
		else if (ent->model->flags & EF_TRACER2)
			R_FlameTrail (ent);
		else if (ent->model->flags & EF_TRACER3)
			R_VoorTrail (ent);
		else if (ent->model->flags & EF_GLOWTRAIL)
			R_GlowTrail (ent, state->glow_color);

		state->forcelink = false;
	}
}
