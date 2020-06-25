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

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/qfplist.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/vid_render.h"

#include "compat.h"

#include "nq/include/chase.h"
#include "nq/include/client.h"
#include "nq/include/host.h"
#include "nq/include/host.h"
#include "nq/include/server.h"

entity_t        cl_entities[MAX_EDICTS];
double          cl_msgtime[MAX_EDICTS];
byte            cl_forcelink[MAX_EDICTS];

void
CL_ClearEnts (void)
{
	size_t      i;

	// clear other arrays
	memset (cl_entities, 0, sizeof (cl_entities));
	i = nq_entstates.num_frames * nq_entstates.num_entities;
	memset (nq_entstates.frame[0], 0, i * sizeof (entity_state_t));
	memset (cl_msgtime, 0, sizeof (cl_msgtime));
	memset (cl_forcelink, 0, sizeof (cl_forcelink));

	for (i = 0; i < MAX_EDICTS; i++)
		CL_Init_Entity (cl_entities + i);
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

	dl = r_funcs->R_AllocDlight (key);
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
CL_TransformEntity (entity_t *ent, const vec3_t angles, qboolean force)
{
	vec3_t      ang;
	vec_t      *forward, *left, *up;

	if (VectorIsZero (angles)) {
		VectorSet (1, 0, 0, ent->transform + 0);
		VectorSet (0, 1, 0, ent->transform + 4);
		VectorSet (0, 0, 1, ent->transform + 8);
	} else if (force || !VectorCompare (angles, ent->angles)) {
		forward = ent->transform + 0;
		left = ent->transform + 4;
		up = ent->transform + 8;
		VectorCopy (angles, ang);
		if (ent->model && ent->model->type == mod_alias) {
			// stupid quake bug
			// why, oh, why, do alias models pitch in the opposite direction
			// to everything else?
			ang[PITCH] = -ang[PITCH];
		}
		AngleVectors (ang, forward, left, up);
		VectorNegate (left, left);  // AngleVectors is right-handed
	}
	VectorCopy (angles, ent->angles);
	ent->transform[3] = 0;
	ent->transform[7] = 0;
	ent->transform[11] = 0;
	VectorCopy (ent->origin, ent->transform + 12);
	ent->transform[15] = 1;
}

static void
CL_ModelEffects (entity_t *ent, int num, int glow_color)
{
	dlight_t   *dl;
	model_t    *model = ent->model;

	if (model->flags & EF_ROCKET) {
		dl = r_funcs->R_AllocDlight (num);
		if (dl) {
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200.0;
			dl->die = cl.time + 0.1;
			//FIXME VectorCopy (r_firecolor->vec, dl->color);
			VectorSet (0.9, 0.7, 0.0, dl->color);
			dl->color[3] = 0.7;
		}
		r_funcs->particles->R_RocketTrail (ent);
	} else if (model->flags & EF_GRENADE)
		r_funcs->particles->R_GrenadeTrail (ent);
	else if (model->flags & EF_GIB)
		r_funcs->particles->R_BloodTrail (ent);
	else if (model->flags & EF_ZOMGIB)
		r_funcs->particles->R_SlightBloodTrail (ent);
	else if (model->flags & EF_TRACER)
		r_funcs->particles->R_WizTrail (ent);
	else if (model->flags & EF_TRACER2)
		r_funcs->particles->R_FlameTrail (ent);
	else if (model->flags & EF_TRACER3)
		r_funcs->particles->R_VoorTrail (ent);
	else if (model->flags & EF_GLOWTRAIL)
		if (r_funcs->particles->R_GlowTrail)
			r_funcs->particles->R_GlowTrail (ent, glow_color);
}

static void
CL_EntityEffects (int num, entity_t *ent, entity_state_t *state)
{
	dlight_t   *dl;

	if (state->effects & EF_BRIGHTFIELD)
		r_funcs->particles->R_EntityParticles (ent);
	if (state->effects & EF_MUZZLEFLASH) {
		vec_t      *fv = ent->transform;

		dl = r_funcs->R_AllocDlight (num);
		if (dl) {
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
	}
}

static void
set_entity_model (entity_t *ent, int modelindex)
{
	int         i = ent - cl_entities;
	ent->model = cl.model_precache[modelindex];
	// automatic animation (torches, etc) can be either all together
	// or randomized
	if (ent->model) {
		if (ent->model->synctype == ST_RAND)
			ent->syncbase = (float) (rand () & 0x7fff) / 0x7fff;
		else
			ent->syncbase = 0.0;
	} else {
		cl_forcelink[i] = true;	// hack to make null model players work
	}
	if (i <= cl.maxclients)
		ent->skin = mod_funcs->Skin_SetColormap (ent->skin, i);
}

void
CL_RelinkEntities (void)
{
	entity_t   *ent;
	entity_state_t *new, *old;
	float       bobjrotate, frac, f, d;
	int         i, j;
	int         entvalid;
	vec3_t      delta;
	int         model_flags;

	r_data->player_entity = &cl_entities[cl.viewentity];

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
	for (i = 1; i < cl.num_entities; i++) {
		new = &nq_entstates.frame[0 + cl.mindex][i];
		old = &nq_entstates.frame[1 - cl.mindex][i];
		ent = &cl_entities[i];
		// if the object wasn't included in the last packet, remove it
		entvalid = cl_msgtime[i] == cl.mtime[0];
		if (entvalid && !new->modelindex) {
			VectorCopy (new->origin, ent->origin);
			VectorCopy (new->angles, ent->angles);
			entvalid = 0;
		}
		if (!entvalid) {
			ent->model = NULL;
			ent->pose1 = ent->pose2 = -1;
			if (ent->efrag)
				r_funcs->R_RemoveEfrags (ent);	// just became empty
			continue;
		}

		if (cl_forcelink[i])
			*old = *new;

		if (cl_forcelink[i] || new->modelindex != old->modelindex) {
			old->modelindex = new->modelindex;
			set_entity_model (ent, new->modelindex);
		}
		ent->frame = new->frame;
		if (cl_forcelink[i] || new->colormap != old->colormap) {
			old->colormap = new->colormap;
			ent->skin = mod_funcs->Skin_SetColormap (ent->skin, new->colormap);
		}
		if (cl_forcelink[i] || new->skinnum != old->skinnum) {
			old->skinnum = new->skinnum;
			ent->skinnum = new->skinnum;
			if (i <= cl.maxclients) {
				ent->skin = mod_funcs->Skin_SetColormap (ent->skin, i);
				mod_funcs->Skin_SetTranslation (i, cl.scores[i - 1].topcolor,
												cl.scores[i - 1].bottomcolor);
			}
		}
		ent->scale = new->scale / 16.0;

		VectorCopy (ent_colormod[new->colormod], ent->colormod);
		ent->colormod[3] = ENTALPHA_DECODE (new->alpha);

		model_flags = 0;
		if (ent->model)
			model_flags = ent->model->flags;

		if (cl_forcelink[i]) {
			// The entity was not updated in the last message so move to the
			// final spot
			ent->pose1 = ent->pose2 = -1;
			VectorCopy (new->origin, ent->origin);
			if (!(model_flags & EF_ROTATE))
				CL_TransformEntity (ent, new->angles, true);
			if (i != cl.viewentity || chase_active->int_val) {
				if (ent->efrag)
					r_funcs->R_RemoveEfrags (ent);
				r_funcs->R_AddEfrags (ent);
			}
			VectorCopy (ent->origin, ent->old_origin);
		} else {
			f = frac;
			VectorCopy (ent->origin, ent->old_origin);
			VectorSubtract (new->origin, old->origin, delta);
			// If the delta is large, assume a teleport and don't lerp
			if (fabs (delta[0]) > 100 || fabs (delta[1] > 100)
				|| fabs (delta[2]) > 100) {
				// assume a teleportation, not a motion
				VectorCopy (new->origin, ent->origin);
				if (!(model_flags & EF_ROTATE))
					CL_TransformEntity (ent, new->angles, true);
				ent->pose1 = ent->pose2 = -1;
			} else {
				vec3_t      angles, d;
				// interpolate the origin and angles
				VectorMultAdd (old->origin, f, delta, ent->origin);
				if (!(model_flags & EF_ROTATE)) {
					VectorSubtract (new->angles, old->angles, d);
					for (j = 0; j < 3; j++) {
						if (d[j] > 180)
							d[j] -= 360;
						else if (d[j] < -180)
							d[j] += 360;
					}
					VectorMultAdd (old->angles, f, d, angles);
					CL_TransformEntity (ent, angles, false);
				}
			}
			if (i != cl.viewentity || chase_active->int_val) {
				if (ent->efrag) {
					if (!VectorCompare (ent->origin, ent->old_origin)) {
						r_funcs->R_RemoveEfrags (ent);
						r_funcs->R_AddEfrags (ent);
					}
				} else {
					r_funcs->R_AddEfrags (ent);
				}
			}
		}

		// rotate binary objects locally
		if (model_flags & EF_ROTATE) {
			vec3_t      angles;
			VectorCopy (new->angles, angles);
			angles[YAW] = bobjrotate;
			CL_TransformEntity (ent, angles, false);
		}
		CL_EntityEffects (i, ent, new);
		CL_NewDlight (i, ent->origin, new->effects, new->glow_size,
					  new->glow_color);
		if (VectorDistance_fast (old->origin, ent->origin) > (256 * 256))
			VectorCopy (ent->origin, old->origin);
		if (model_flags & ~EF_ROTATE)
			CL_ModelEffects (ent, i, new->glow_color);

		cl_forcelink[i] = false;
	}
}
