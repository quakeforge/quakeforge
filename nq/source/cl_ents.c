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
#include "QF/plist.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/vid_render.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "compat.h"

#include "client/effects.h"
#include "client/temp_entities.h"
#include "client/world.h"

#include "client/chase.h"

#include "nq/include/client.h"
#include "nq/include/host.h"
#include "nq/include/host.h"
#include "nq/include/server.h"

entity_t   *cl_entities[MAX_EDICTS];
double      cl_msgtime[MAX_EDICTS];
static byte forcelink_bytes[SET_SIZE(MAX_EDICTS)];
#define alloc_forcelink(s) (set_bits_t *)forcelink_bytes
set_t       cl_forcelink = SET_STATIC_INIT (MAX_EDICTS, alloc_forcelink);
#undef alloc_forcelink

void
CL_ClearEnts (void)
{
	size_t      i;

	for (i = 0; i < MAX_EDICTS; i++) {
		if (cl_entities[i]) {
			Scene_DestroyEntity (cl_world.scene, cl_entities[i]);
			cl_entities[i] = 0;
		}
	}

	// clear other arrays
	i = nq_entstates.num_frames * nq_entstates.num_entities;
	memset (nq_entstates.frame[0], 0, i * sizeof (entity_state_t));
	memset (cl_msgtime, 0, sizeof (cl_msgtime));
	set_empty (&cl_forcelink);
}

entity_t *
CL_GetEntity (int num)
{
	if (!cl_entities[num]) {
		cl_entities[num] = Scene_CreateEntity (cl_world.scene);
		CL_Init_Entity (cl_entities[num]);
	}
	return cl_entities[num];
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
set_entity_model (int ent_ind, int modelindex)
{
	entity_t   *ent = cl_entities[ent_ind];
	renderer_t *renderer = &ent->renderer;
	animation_t *animation = &ent->animation;
	renderer->model = cl_world.models.a[modelindex];
	// automatic animation (torches, etc) can be either all together
	// or randomized
	if (renderer->model) {
		if (renderer->model->synctype == ST_RAND) {
			animation->syncbase = (float) (rand () & 0x7fff) / 0x7fff;
		} else {
			animation->syncbase = 0.0;
		}
	} else {
		// hack to make null model players work
		SET_ADD (&cl_forcelink, ent_ind);
	}
	animation->nolerp = 1; // don't try to lerp when the model has changed
	if (ent_ind <= cl.maxclients) {
		renderer->skin = mod_funcs->Skin_SetColormap (renderer->skin, ent_ind);
	}
}

void
CL_RelinkEntities (void)
{
	entity_t   *ent;
	entity_state_t *new, *old;
	renderer_t *renderer;
	animation_t *animation;
	float       bobjrotate, frac, f;
	int         i, j;
	int         entvalid;
	int         model_flags;

	// determine partial update time
	frac = CL_LerpPoint ();

	// interpolate player info
	cl.viewstate.velocity = cl.frameVelocity[1]
		+ frac * (cl.frameVelocity[0] - cl.frameVelocity[1]);

	if (cls.demoplayback) {
		// interpolate the angles
		vec3_t      d;
		VectorSubtract (cl.frameViewAngles[0], cl.frameViewAngles[1], d);
		for (j = 0; j < 3; j++) {
			if (d[j] > 180) {
				d[j] -= 360;
			} else if (d[j] < -180) {
				d[j] += 360;
			}
		}
		VectorMultAdd (cl.frameViewAngles[1], frac, d,
					   cl.viewstate.player_angles);
	}

	bobjrotate = anglemod (100 * cl.time);

	// start on the entity after the world
	for (i = 1; i < cl.num_entities; i++) {
		new = &nq_entstates.frame[0 + cl.frameIndex][i];
		old = &nq_entstates.frame[1 - cl.frameIndex][i];
		ent = CL_GetEntity (i);
		renderer = &ent->renderer;
		animation = &ent->animation;

		// if the object wasn't included in the last packet, remove it
		entvalid = cl_msgtime[i] == cl.mtime[0];
		if (entvalid && !new->modelindex) {
			CL_TransformEntity (ent, new->scale / 16.0, new->angles,
								new->origin);
			entvalid = 0;
		}
		if (!entvalid) {
			renderer->model = NULL;
			animation->pose1 = animation->pose2 = -1;
			if (ent->visibility.efrag) {
				R_RemoveEfrags (ent);	// just became empty
			}
			continue;
		}

		if (SET_TEST_MEMBER (&cl_forcelink, i)) {
			*old = *new;
		}

		if (SET_TEST_MEMBER (&cl_forcelink, i)
			|| new->modelindex != old->modelindex) {
			old->modelindex = new->modelindex;
			set_entity_model (i, new->modelindex);
		}
		animation->frame = new->frame;
		if (SET_TEST_MEMBER (&cl_forcelink, i)
			|| new->colormap != old->colormap) {
			old->colormap = new->colormap;
			renderer->skin = mod_funcs->Skin_SetColormap (renderer->skin,
														  new->colormap);
		}
		if (SET_TEST_MEMBER (&cl_forcelink, i)
			|| new->skinnum != old->skinnum) {
			old->skinnum = new->skinnum;
			renderer->skinnum = new->skinnum;
			if (i <= cl.maxclients) {
				renderer->skin = mod_funcs->Skin_SetColormap (renderer->skin,
															  i);
				mod_funcs->Skin_SetTranslation (i, cl.players[i - 1].topcolor,
												cl.players[i - 1].bottomcolor);
			}
		}

		VectorCopy (ent_colormod[new->colormod], renderer->colormod);
		renderer->colormod[3] = ENTALPHA_DECODE (new->alpha);

		model_flags = 0;
		if (renderer->model) {
			model_flags = renderer->model->flags;
		}

		if (SET_TEST_MEMBER (&cl_forcelink, i)) {
			// The entity was not updated in the last message so move to the
			// final spot
			animation->pose1 = animation->pose2 = -1;
			CL_TransformEntity (ent, new->scale / 16.0, new->angles,
								new->origin);
			if (i != cl.viewentity || chase_active->int_val) {
				if (ent->visibility.efrag) {
					R_RemoveEfrags (ent);
				}
				R_AddEfrags (&cl_world.worldmodel->brush, ent);
			}
			ent->old_origin = new->origin;
		} else {
			vec4f_t     delta = new->origin - old->origin;
			f = frac;
			ent->old_origin = Transform_GetWorldPosition (ent->transform);
			// If the delta is large, assume a teleport and don't lerp
			if (fabs (delta[0]) > 100 || fabs (delta[1] > 100)
				|| fabs (delta[2]) > 100) {
				// assume a teleportation, not a motion
				CL_TransformEntity (ent, new->scale / 16.0, new->angles,
									new->origin);
				animation->pose1 = animation->pose2 = -1;
			} else {
				// interpolate the origin and angles
				vec3_t      angles, d;
				vec4f_t     origin = old->origin + f * delta;
				if (!(model_flags & EF_ROTATE)) {
					VectorSubtract (new->angles, old->angles, d);
					for (j = 0; j < 3; j++) {
						if (d[j] > 180)
							d[j] -= 360;
						else if (d[j] < -180)
							d[j] += 360;
					}
					VectorMultAdd (old->angles, f, d, angles);
				}
				CL_TransformEntity (ent, new->scale / 16.0, angles, origin);
			}
			if (i != cl.viewentity || chase_active->int_val) {
				if (ent->visibility.efrag) {
					vec4f_t     org
						= Transform_GetWorldPosition (ent->transform);
					if (!VectorCompare (org, ent->old_origin)) {//FIXME
						R_RemoveEfrags (ent);
						R_AddEfrags (&cl_world.worldmodel->brush, ent);
					}
				} else {
					R_AddEfrags (&cl_world.worldmodel->brush, ent);
				}
			}
		}

		// rotate binary objects locally
		if (model_flags & EF_ROTATE) {
			vec3_t      angles;
			VectorCopy (new->angles, angles);
			angles[YAW] = bobjrotate;
			CL_TransformEntity (ent, new->scale / 16.0, angles, new->origin);
		}
		CL_EntityEffects (i, ent, new, cl.time);
		vec4f_t org = Transform_GetWorldPosition (ent->transform);
		CL_NewDlight (i, org, new->effects, new->glow_size,
					  new->glow_color, cl.time);
		if (VectorDistance_fast (old->origin, org) > (256 * 256)) {
			old->origin = org;
		}
		if (model_flags & ~EF_ROTATE)
			CL_ModelEffects (ent, i, new->glow_color, cl.time);

		SET_REMOVE (&cl_forcelink, i);
	}
	cl.viewstate.player_entity = CL_GetEntity (cl.viewentity);
	cl.viewstate.player_origin
		= Transform_GetWorldPosition (cl.viewstate.player_entity->transform);
}
