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
#include "QF/entity.h"
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

#include "client/effects.h"
#include "client/temp_entities.h"

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

static void
set_entity_model (entity_t *ent, int modelindex)
{
	int         i = ent - cl_entities;
	renderer_t *renderer = &ent->renderer;
	animation_t *animation = &ent->animation;
	renderer->model = cl.model_precache[modelindex];
	// automatic animation (torches, etc) can be either all together
	// or randomized
	if (renderer->model) {
		if (renderer->model->synctype == ST_RAND) {
			animation->syncbase = (float) (rand () & 0x7fff) / 0x7fff;
		} else {
			animation->syncbase = 0.0;
		}
	} else {
		cl_forcelink[i] = true;	// hack to make null model players work
	}
	animation->nolerp = 1; // don't try to lerp when the model has changed
	if (i <= cl.maxclients) {
		renderer->skin = mod_funcs->Skin_SetColormap (renderer->skin, i);
	}
}

void
CL_RelinkEntities (void)
{
	entity_t   *ent;
	entity_state_t *new, *old;
	renderer_t *renderer;
	animation_t *animation;
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
		renderer = &ent->renderer;
		animation = &ent->animation;

		// if the object wasn't included in the last packet, remove it
		entvalid = cl_msgtime[i] == cl.mtime[0];
		if (entvalid && !new->modelindex) {
			VectorCopy (new->origin, ent->origin);
			VectorCopy (new->angles, ent->angles);
			entvalid = 0;
		}
		if (!entvalid) {
			renderer->model = NULL;
			animation->pose1 = animation->pose2 = -1;
			if (ent->visibility.efrag) {
				r_funcs->R_RemoveEfrags (ent);	// just became empty
			}
			continue;
		}

		if (cl_forcelink[i])
			*old = *new;

		if (cl_forcelink[i] || new->modelindex != old->modelindex) {
			old->modelindex = new->modelindex;
			set_entity_model (ent, new->modelindex);
		}
		animation->frame = new->frame;
		if (cl_forcelink[i] || new->colormap != old->colormap) {
			old->colormap = new->colormap;
			renderer->skin = mod_funcs->Skin_SetColormap (renderer->skin,
														  new->colormap);
		}
		if (cl_forcelink[i] || new->skinnum != old->skinnum) {
			old->skinnum = new->skinnum;
			renderer->skinnum = new->skinnum;
			if (i <= cl.maxclients) {
				renderer->skin = mod_funcs->Skin_SetColormap (renderer->skin,
															  i);
				mod_funcs->Skin_SetTranslation (i, cl.scores[i - 1].topcolor,
												cl.scores[i - 1].bottomcolor);
			}
		}
		ent->scale = new->scale / 16.0;

		VectorCopy (ent_colormod[new->colormod], renderer->colormod);
		renderer->colormod[3] = ENTALPHA_DECODE (new->alpha);

		model_flags = 0;
		if (renderer->model) {
			model_flags = renderer->model->flags;
		}

		if (cl_forcelink[i]) {
			// The entity was not updated in the last message so move to the
			// final spot
			animation->pose1 = animation->pose2 = -1;
			VectorCopy (new->origin, ent->origin);
			if (!(model_flags & EF_ROTATE))
				CL_TransformEntity (ent, new->angles);
			if (i != cl.viewentity || chase_active->int_val) {
				if (ent->visibility.efrag) {
					r_funcs->R_RemoveEfrags (ent);
				}
				r_funcs->R_AddEfrags (&cl.worldmodel->brush, ent);
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
					CL_TransformEntity (ent, new->angles);
				animation->pose1 = animation->pose2 = -1;
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
					CL_TransformEntity (ent, angles);
				}
			}
			if (i != cl.viewentity || chase_active->int_val) {
				if (ent->visibility.efrag) {
					if (!VectorCompare (ent->origin, ent->old_origin)) {
						r_funcs->R_RemoveEfrags (ent);
						r_funcs->R_AddEfrags (&cl.worldmodel->brush, ent);
					}
				} else {
					r_funcs->R_AddEfrags (&cl.worldmodel->brush, ent);
				}
			}
		}

		// rotate binary objects locally
		if (model_flags & EF_ROTATE) {
			vec3_t      angles;
			VectorCopy (new->angles, angles);
			angles[YAW] = bobjrotate;
			CL_TransformEntity (ent, angles);
		}
		CL_EntityEffects (i, ent, new, cl.time);
		CL_NewDlight (i, ent->origin, new->effects, new->glow_size,
					  new->glow_color, cl.time);
		if (VectorDistance_fast (old->origin, ent->origin) > (256 * 256))
			VectorCopy (ent->origin, old->origin);
		if (model_flags & ~EF_ROTATE)
			CL_ModelEffects (ent, i, new->glow_color, cl.time);

		cl_forcelink[i] = false;
	}
}
