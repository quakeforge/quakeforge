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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/entity.h"
#include "QF/locs.h"
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "compat.h"
#include "clview.h"
#include "d_iface.h"

#include "client/effects.h"
#include "client/temp_entities.h"

#include "qw/bothdefs.h"
#include "qw/msg_ucmd.h"
#include "qw/pmove.h"

#include "qw/include/chase.h"
#include "qw/include/cl_cam.h"
#include "qw/include/cl_ents.h"
#include "qw/include/cl_main.h"
#include "qw/include/cl_parse.h"
#include "qw/include/cl_pred.h"
#include "qw/include/host.h"

entity_t    cl_player_ents[MAX_CLIENTS];
entity_t    cl_flag_ents[MAX_CLIENTS];
entity_t    cl_entities[512];	// FIXME: magic number
byte        cl_entity_valid[2][512];

void
CL_ClearEnts (void)
{
	size_t      i;

	i = qw_entstates.num_frames * qw_entstates.num_entities;
	memset (qw_entstates.frame[0], 0, i * sizeof (entity_state_t));
	memset (cl_entity_valid, 0, sizeof (cl_entity_valid));
	for (i = 0; i < sizeof (cl_entities) / sizeof (cl_entities[0]); i++)
		CL_Init_Entity (&cl_entities[i]);
	for (i = 0; i < sizeof (cl_flag_ents) / sizeof (cl_flag_ents[0]); i++)
		CL_Init_Entity (&cl_flag_ents[i]);
	for (i = 0; i < sizeof (cl_player_ents) / sizeof (cl_player_ents[0]); i++)
		CL_Init_Entity (&cl_player_ents[i]);
}

// Hack hack hack
static inline int
is_dead_body (entity_state_t *s1)
{
	int         i = s1->frame;

	if (s1->modelindex == cl_playerindex
		&& (i == 49 || i == 60 || i == 69 || i == 84 || i == 93 || i == 102))
		return 1;
	return 0;
}

// Hack hack hack
static inline int
is_gib (entity_state_t *s1)
{
	if (s1->modelindex == cl_h_playerindex || s1->modelindex == cl_gib1index
		|| s1->modelindex == cl_gib2index || s1->modelindex == cl_gib3index)
		return 1;
	return 0;
}

static void
set_entity_model (entity_t *ent, int modelindex)
{
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
	}
	animation->nolerp = 1; // don't try to lerp when the model has changed
}

static void
CL_LinkPacketEntities (void)
{
	int         i, j, forcelink;
	float       frac, f;
	entity_t   *ent;
	entity_state_t *new, *old;
	renderer_t *renderer;
	animation_t *animation;
	vec3_t      delta;

	frac = 1;
	for (i = 0; i < 512; i++) {
		new = &qw_entstates.frame[cl.link_sequence & UPDATE_MASK][i];
		old = &qw_entstates.frame[cl.prev_sequence & UPDATE_MASK][i];
		ent = &cl_entities[i];
		renderer = &ent->renderer;
		animation = &ent->animation;
		forcelink = cl_entity_valid[0][i] != cl_entity_valid[1][i];
		cl_entity_valid[1][i] = cl_entity_valid[0][i];
		// if the object wasn't included in the last packet, remove it
		if (!cl_entity_valid[0][i]) {
			renderer->model = NULL;
			animation->pose1 = animation->pose2 = -1;
			if (ent->visibility.efrag) {
				r_funcs->R_RemoveEfrags (ent);	// just became empty
			}
			continue;
		}

		// spawn light flashes, even ones coming from invisible objects
		CL_NewDlight (i, new->origin, new->effects, new->glow_size,
					  new->glow_color, cl.time);

		// if set to invisible, skip
		if (!new->modelindex
			|| (cl_deadbodyfilter->int_val && is_dead_body (new))
			|| (cl_gibfilter->int_val && is_gib (new))) {
			if (ent->visibility.efrag) {
				r_funcs->R_RemoveEfrags (ent);
			}
			continue;
		}

		if (forcelink)
			*old = *new;

		if (forcelink || new->modelindex != old->modelindex) {
			old->modelindex = new->modelindex;
			set_entity_model (ent, new->modelindex);
		}
		animation->frame = new->frame;
		if (forcelink || new->colormap != old->colormap
			|| new->skinnum != old->skinnum) {
			old->skinnum = new->skinnum;
			renderer->skinnum = new->skinnum;
			old->colormap = new->colormap;
			if (new->colormap && (new->colormap <= MAX_CLIENTS)
				&& cl.players[new->colormap - 1].name
				&& cl.players[new->colormap - 1].name->value[0]
				&& new->modelindex == cl_playerindex) {
				player_info_t *player = &cl.players[new->colormap - 1];
				renderer->skin
					= mod_funcs->Skin_SetSkin (renderer->skin, new->colormap,
											   player->skinname->value);
				renderer->skin = mod_funcs->Skin_SetColormap (renderer->skin,
															  new->colormap);
			} else {
				renderer->skin = mod_funcs->Skin_SetColormap (renderer->skin,
															  0);
			}
		}
		ent->scale = new->scale / 16.0;

		VectorCopy (ent_colormod[new->colormod], renderer->colormod);
		renderer->colormod[3] = new->alpha / 255.0;

		renderer->min_light = 0;
		renderer->fullbright = 0;
		if (new->modelindex == cl_playerindex) {
			renderer->min_light = min (cl.fbskins, cl_fb_players->value);
			if (renderer->min_light >= 1.0) {
				renderer->fullbright = 1;
			}
		}

		if (forcelink) {
			animation->pose1 = animation->pose2 = -1;
			VectorCopy (new->origin, ent->origin);
			if (!(renderer->model->flags & EF_ROTATE))
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
				if (!(renderer->model->flags & EF_ROTATE)) {
					CL_TransformEntity (ent, new->angles);
				}
				animation->pose1 = animation->pose2 = -1;
			} else {
				vec3_t      angles, d;
				// interpolate the origin and angles
				VectorMultAdd (old->origin, f, delta, ent->origin);
				if (!(renderer->model->flags & EF_ROTATE)) {
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
		if (!ent->visibility.efrag) {
			r_funcs->R_AddEfrags (&cl.worldmodel->brush, ent);
		}

		// rotate binary objects locally
		if (renderer->model->flags & EF_ROTATE) {
			vec3_t      angles;
			angles[PITCH] = 0;
			angles[YAW] = anglemod (100 * cl.time);
			angles[ROLL] = 0;
			CL_TransformEntity (ent, angles);
		}
		//CL_EntityEffects (i, ent, new);
		//CL_NewDlight (i, ent->origin, new->effects, 0, 0, cl.time);
		if (VectorDistance_fast (old->origin, ent->origin) > (256 * 256))
			VectorCopy (ent->origin, old->origin);
		if (renderer->model->flags & ~EF_ROTATE) {
			CL_ModelEffects (ent, -new->number, new->glow_color, cl.time);
		}
	}
}

/*
	CL_AddFlagModels

	Called when the CTF flags are set. Flags are effectively temp entities.

	NOTE: this must be called /after/ the entity has been transformed as it
	uses the entity's transform matrix to get the frame vectors
*/
static void
CL_AddFlagModels (entity_t *ent, int team, int key)
{
	static float flag_offsets[] = {
		16.0, 22.0, 26.0, 25.0, 24.0, 18.0,					// 29-34 axpain
		16.0, 24.0, 24.0, 22.0, 18.0, 16.0,					// 35-40 pain
	};
	float       f;
	entity_t   *fent;

	fent = &cl_flag_ents[key];

	if (cl_flagindex == -1) {
		fent->active = 0;
		return;
	}

	fent->active = 1;
	f = 14.0;
	if (ent->animation.frame >= 29 && ent->animation.frame <= 40) {
		f = flag_offsets[ent->animation.frame - 29];
	} else if (ent->animation.frame >= 103 && ent->animation.frame <= 118) {
		if (ent->animation.frame <= 106) {			// 103-104 nailattack
			f = 20.0;								// 105-106 light
		} else {									// 107-112 rocketattack
			f = 21.0;								// 112-118 shotattack
		}
	}

	vec4f_t     position = { 22, -f, -16, 1};

	if (!Transform_GetParent (fent->transform)) {
		vec4f_t     scale = { 1, 1, 1, 1 };
		// -45 degree roll (x is forward)
		vec4f_t     rotation = { -0.382683432, 0, 0, 0.923879533 };
		Transform_SetParent (fent->transform, ent->transform);
		Transform_SetLocalTransform (fent->transform, scale, rotation,
									 position);
	} else {
		Transform_SetLocalPosition (fent->transform, position);
	}
	fent->renderer.model = cl.model_precache[cl_flagindex];
	fent->renderer.skinnum = team;

	r_funcs->R_EnqueueEntity (fent);//FIXME should use efrag (needs smarter
									// handling //in the player code)
}

/*
	CL_LinkPlayers

	Create visible entities in the correct position
	for all current players
*/
static void
CL_LinkPlayers (void)
{
	double			playertime;
	int				msec, oldphysent, i, j;
	entity_t	   *ent;
	frame_t		   *frame;
	player_info_t  *info;
	player_state_t	exact;
	player_state_t *state;
	qboolean		clientplayer;
	vec3_t			org, ang = {0, 0, 0};

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0, info = cl.players, state = frame->playerstate; j < MAX_CLIENTS;
		 j++, info++, state++) {
		ent = &cl_player_ents[j];
		if (ent->visibility.efrag)
			r_funcs->R_RemoveEfrags (ent);
		if (state->messagenum != cl.parsecount)
			continue;							// not present this frame

		if (!info->name || !info->name->value[0])
			continue;

		// spawn light flashes, even ones coming from invisible objects
		if (j == cl.playernum) {
			VectorCopy (cl.simorg, org);
			r_data->player_entity = &cl_player_ents[j];
			clientplayer = true;
		} else {
			VectorCopy (state->pls.origin, org);
			clientplayer = false;
		}
		if (info->chat && info->chat->value[0] != '0') {
			dlight_t   *dl = r_funcs->R_AllocDlight (j + 1);
			VectorCopy (org, dl->origin);
			dl->radius = 100;
			dl->die = cl.time + 0.1;
			QuatSet (0.0, 1.0, 0.0, 1.0, dl->color);
		} else {
			CL_NewDlight (j + 1, org, state->pls.effects, state->pls.glow_size,
						  state->pls.glow_color, cl.time);
		}

		// Draw player?
		if (!Cam_DrawPlayer (j))
			continue;

		if (!state->pls.modelindex)
			continue;

		// Hack hack hack
		if (cl_deadbodyfilter->int_val
			&& state->pls.modelindex == cl_playerindex
			&& ((i = state->pls.frame) == 49 || i == 60 || i == 69 || i == 84
				|| i == 93 || i == 102))
			continue;

		// predict only half the move to minimize overruns
		msec = 500 * (playertime - state->state_time);
		if (msec <= 0 || (!cl_predict_players->int_val) || cls.demoplayback2) {
			VectorCopy (state->pls.origin, ent->origin);
		} else {									// predict players movement
			state->pls.cmd.msec = msec = min (msec, 255);

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers (j);
			CL_PredictUsercmd (state, &exact, &state->pls.cmd, clientplayer);
			pmove.numphysent = oldphysent;
			VectorCopy (exact.pls.origin, ent->origin);
		}

		// angles
		if (j == cl.playernum)
		{
			ang[PITCH] = -cl.viewangles[PITCH] / 3.0;
			ang[YAW] = cl.viewangles[YAW];
		} else {
			ang[PITCH] = -state->viewangles[PITCH] / 3.0;
			ang[YAW] = state->viewangles[YAW];
		}
		ang[ROLL] = V_CalcRoll (ang, state->pls.velocity) * 4.0;

		ent->renderer.model = cl.model_precache[state->pls.modelindex];
		ent->animation.frame = state->pls.frame;
		ent->renderer.skinnum = state->pls.skinnum;

		CL_TransformEntity (ent, ang);

		ent->renderer.min_light = 0;
		ent->renderer.fullbright = 0;

		if (state->pls.modelindex == cl_playerindex) { //XXX
			// use custom skin
			ent->renderer.skin = info->skin;

			ent->renderer.min_light = min (cl.fbskins, cl_fb_players->value);

			if (ent->renderer.min_light >= 1.0) {
				ent->renderer.fullbright = 1;
			}
		} else {
			// FIXME no team colors on nonstandard player models
			ent->renderer.skin = 0;
		}

		// stuff entity in map
		r_funcs->R_AddEfrags (&cl.worldmodel->brush, ent);

		if (state->pls.effects & EF_FLAG1)
			CL_AddFlagModels (ent, 0, j);
		else if (state->pls.effects & EF_FLAG2)
			CL_AddFlagModels (ent, 1, j);
	}
}

/*
	CL_EmitEntities

	Builds the visedicts array for cl.time

	Made up of: clients, packet_entities, nails, and tents
*/
void
CL_EmitEntities (void)
{
	if (cls.state != ca_active)
		return;
	if (!cl.validsequence)
		return;

	TEntContext_t tentCtx = {
		{VectorExpand (cl.simorg), 1}, cl.worldmodel, cl.viewentity
	};

	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_UpdateTEnts (cl.time, &tentCtx);
	if (cl_draw_locs->int_val) {
		//FIXME custom ent rendering code would be nice
		dlight_t   *dl;
		location_t *nearloc;
		vec3_t      trueloc;
		int         i;

		nearloc = locs_find (cl.simorg);
		if (nearloc) {
			dl = r_funcs->R_AllocDlight (4096);
			if (dl) {
				VectorCopy (nearloc->loc, dl->origin);
				dl->radius = 200;
				dl->die = r_data->realtime + 0.1;
				dl->color[0] = 0;
				dl->color[1] = 1;
				dl->color[2] = 0;
				dl->color[3] = 0.7;
			}
			VectorCopy (nearloc->loc, trueloc);
			r_funcs->particles->R_Particle_New (pt_smokecloud, part_tex_smoke,
					trueloc, 2.0,
					vec3_origin, r_data->realtime + 9.0, 254,
					0.25 + qfrandom (0.125), 0.0);
			for (i = 0; i < 15; i++)
				r_funcs->particles->R_Particle_NewRandom (pt_fallfade,
						part_tex_dot, trueloc, 12,
						0.7, 96, r_data->realtime + 5.0,
						104 + (rand () & 7), 1.0, 0.0);
		}
	}
}

void
CL_Ents_Init (void)
{
	r_data->view_model = &cl.viewent;
}
