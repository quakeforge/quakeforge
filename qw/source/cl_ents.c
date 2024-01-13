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
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"
#include "QF/scene/light.h"

#include "compat.h"

#include "client/effects.h"
#include "client/locs.h"
#include "client/temp_entities.h"
#include "client/view.h"
#include "client/world.h"

#include "qw/bothdefs.h"
#include "qw/msg_ucmd.h"
#include "qw/pmove.h"

#include "qw/include/cl_cam.h"
#include "qw/include/cl_ents.h"
#include "qw/include/cl_main.h"
#include "qw/include/cl_parse.h"
#include "qw/include/cl_pred.h"
#include "qw/include/host.h"

entity_t    cl_flag_ents[MAX_CLIENTS];
entity_t    cl_entities[512];	// FIXME: magic number
byte        cl_entity_valid[2][512];

void
CL_ClearEnts (void)
{
	size_t      i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		cl_flag_ents[i] = nullentity;
	}

	for (i = 0; i < 512; i++) {
		if (Entity_Valid (cl_entities[i])) {
			Scene_DestroyEntity (cl_world.scene, cl_entities[i]);
			cl_entities[i] = nullentity;
		}
	}
	i = qw_entstates.num_frames * qw_entstates.num_entities;
	memset (qw_entstates.frame[0], 0, i * sizeof (entity_state_t));
	memset (cl_entity_valid, 0, sizeof (cl_entity_valid));
}

static entity_t
CL_GetInvalidEntity (int num)
{
	return cl_entities[num];
}

entity_t
CL_GetEntity (int num)
{
	if (!Entity_Valid (cl_entities[num])) {
		cl_entities[num] = Scene_CreateEntity (cl_world.scene);
		CL_Init_Entity (cl_entities[num]);
	}
	return cl_entities[num];
}

static entity_t
CL_GetFlagEnt (int key)
{
	if (!Entity_Valid (cl_flag_ents[key])) {
		cl_flag_ents[key] = Scene_CreateEntity (cl_world.scene);
		CL_Init_Entity (cl_flag_ents[key]);
	}
	return cl_flag_ents[key];
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
set_entity_model (entity_t ent, int modelindex)
{
	renderer_t *renderer = Ent_GetComponent (ent.id, ent.base + scene_renderer,
											 cl_world.scene->reg);
	animation_t *animation = Ent_GetComponent (ent.id, ent.base + scene_animation,
											   cl_world.scene->reg);
	renderer->model = cl_world.models.a[modelindex];
	// automatic animation (torches, etc) can be either all together
	// or randomized
	if (renderer->model) {
		if (renderer->model->synctype == ST_RAND) {
			animation->syncbase = (float) (rand () & 0x7fff) / 0x7fff;
		} else {
			animation->syncbase = 0.0;
		}
		renderer->noshadows = renderer->model->shadow_alpha < 0.5;
	}
	animation->nolerp = 1; // don't try to lerp when the model has changed
}

static void
CL_LinkPacketEntities (void)
{
	int         i, j, forcelink;
	float       frac, f;
	entity_t    ent;
	entity_state_t *new, *old;

	frac = 1;
	for (i = MAX_CLIENTS + 1; i < 512; i++) {
		new = &qw_entstates.frame[cl.link_sequence & UPDATE_MASK][i];
		old = &qw_entstates.frame[cl.prev_sequence & UPDATE_MASK][i];

		ent = CL_GetInvalidEntity (i);
		forcelink = cl_entity_valid[0][i] != cl_entity_valid[1][i];
		cl_entity_valid[1][i] = cl_entity_valid[0][i];
		// if the object wasn't included in the last packet, or set
		// to invisible, remove it
		if (!cl_entity_valid[0][i]
			|| !new->modelindex
			|| (cl_deadbodyfilter && is_dead_body (new))
			|| (cl_gibfilter && is_gib (new))) {
			if (Entity_Valid (ent)) {
				Scene_DestroyEntity (cl_world.scene, ent);
			}
			continue;
		}
		if (!Entity_Valid (ent)) {
			ent = CL_GetEntity (i);
			forcelink = true;
		}
		transform_t transform = Entity_Transform (ent);
		renderer_t *renderer = Ent_GetComponent (ent.id, ent.base + scene_renderer,
												 ent.reg);
		animation_t *animation = Ent_GetComponent (ent.id, ent.base + scene_animation,
												   ent.reg);
		vec4f_t    *old_origin = Ent_GetComponent (ent.id, ent.base + scene_old_origin,
												   ent.reg);

		// spawn light flashes, even ones coming from invisible objects
		CL_NewDlight (ent, new->origin, new->effects, new->glow_size,
					  new->glow_color, cl.time);

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
				colormap_t  colormap = {
					.top = player->topcolor,
					.bottom = player->bottomcolor,
				};
				Ent_SetComponent (ent.id, ent.base + scene_colormap, ent.reg, &colormap);
				renderer->skin = Skin_SetSkin (renderer->skin, new->colormap,
											   player->skinname->value);
				renderer->skin = Skin_SetColormap (renderer->skin,
												   new->colormap);
			} else {
				renderer->skin = Skin_SetColormap (renderer->skin, 0);
				Ent_RemoveComponent (ent.id, ent.base + scene_colormap, ent.reg);
			}
		}

		VectorCopy (ent_colormod[new->colormod], renderer->colormod);
		renderer->colormod[3] = new->alpha / 255.0;

		renderer->min_light = 0;
		renderer->fullbright = 0;
		if (new->modelindex == cl_playerindex) {
			renderer->min_light = min (cl.fbskins, cl_fb_players);
			if (renderer->min_light >= 1.0) {
				renderer->fullbright = 1;
			}
		}

		*old_origin = Transform_GetWorldPosition (transform);
		if (forcelink) {
			animation->pose1 = animation->pose2 = -1;
			CL_TransformEntity (ent, new->scale / 16, new->angles,
								new->origin);
			if (i != cl.viewentity || chase_active) {
				R_AddEfrags (&cl_world.scene->worldmodel->brush, ent);
			}
		} else {
			vec4f_t     delta = new->origin - old->origin;
			f = frac;
			// If the delta is large, assume a teleport and don't lerp
			if (fabs (delta[0]) > 100 || fabs (delta[1]) > 100
				|| fabs (delta[2]) > 100) {
				// assume a teleportation, not a motion
				CL_TransformEntity (ent, new->scale / 16, new->angles,
									new->origin);
				animation->pose1 = animation->pose2 = -1;
			} else if (!(renderer->model->effects & ME_ROTATE)) {
				vec3_t      angles, d;
				vec4f_t     origin = old->origin + f * delta;
				// interpolate the origin and angles
				VectorSubtract (new->angles, old->angles, d);
				for (j = 0; j < 3; j++) {
					if (d[j] > 180)
						d[j] -= 360;
					else if (d[j] < -180)
						d[j] += 360;
				}
				VectorMultAdd (old->angles, f, d, angles);
				CL_TransformEntity (ent, new->scale / 16.0, angles, origin);
			}
			if (i != cl.viewentity || chase_active) {
				vec4f_t     org = Transform_GetWorldPosition (transform);
				if (!VectorCompare (org, *old_origin)) {//FIXME
					R_AddEfrags (&cl_world.scene->worldmodel->brush, ent);
				}
			}
		}

		// rotate binary objects locally
		if (renderer->model->effects & ME_ROTATE) {
			vec3_t      angles;
			angles[PITCH] = 0;
			angles[YAW] = anglemod (100 * cl.time);
			angles[ROLL] = 0;
			CL_TransformEntity (ent, new->scale / 16.0, angles, new->origin);
		}
		//CL_EntityEffects (i, ent, new);
		//CL_NewDlight (i, ent->origin, new->effects, 0, 0, cl.time);
		vec4f_t org = Transform_GetWorldPosition (transform);
		if (VectorDistance_fast (old->origin, org) > (256 * 256))
			old->origin = org;
		if (renderer->model->effects & ~ME_ROTATE) {
			CL_ModelEffects (ent, new->glow_color, cl.time);
		}
	}
}

static void
CL_UpdateFlagModels (entity_t ent, int key)
{
	static float flag_offsets[] = {
		16.0, 22.0, 26.0, 25.0, 24.0, 18.0,					// 29-34 axpain
		16.0, 24.0, 24.0, 22.0, 18.0, 16.0,					// 35-40 pain
	};
	float       f;
	entity_t    fent = CL_GetFlagEnt (key);
	byte       *active = Ent_GetComponent (fent.id, fent.base + scene_active,
										   cl_world.scene->reg);

	if (!*active) {
		return;
	}
	animation_t *animation = Ent_GetComponent (ent.id, ent.base + scene_animation,
											   cl_world.scene->reg);

	f = 14.0;
	if (animation->frame >= 29 && animation->frame <= 40) {
		f = flag_offsets[animation->frame - 29];
	} else if (animation->frame >= 103 && animation->frame <= 118) {
		if (animation->frame <= 106) {			// 103-104 nailattack
			f = 20.0;								// 105-106 light
		} else {									// 107-112 rocketattack
			f = 21.0;								// 112-118 shotattack
		}
	}

	vec4f_t     scale = { 1, 1, 1, 1 };
	// -45 degree roll (x is forward)
	vec4f_t     rotation = { -0.382683432, 0, 0, 0.923879533 };
	vec4f_t     position = { -f, -22, -16, 1};

	transform_t transform = Entity_Transform (fent);
	Transform_SetLocalTransform (transform, scale, rotation, position);
}

static entity_t
CL_AddFlagModels (entity_t ent, int team, int key)
{
	entity_t    fent;

	fent = CL_GetFlagEnt (key);
	byte       *active = Ent_GetComponent (fent.id, fent.base + scene_active,
										   cl_world.scene->reg);

	if (cl_flagindex == -1) {
		*active = 0;
		return nullentity;
	}

	*active = 1;

	transform_t ftransform = Entity_Transform (fent);
	transform_t transform = Entity_Transform (ent);
	if (!Transform_Valid (Transform_GetParent (ftransform))) {
		Transform_SetParent (ftransform, transform);
	}
	CL_UpdateFlagModels (ent, key);

	renderer_t *renderer = Ent_GetComponent (fent.id, fent.base + scene_renderer,
											 cl_world.scene->reg);
	renderer->model = cl_world.models.a[cl_flagindex];
	renderer->skinnum = team;

	return fent;
}

static void
CL_RemoveFlagModels (int key)
{
	entity_t    fent;

	fent = CL_GetFlagEnt (key);
	byte       *active = Ent_GetComponent (fent.id, fent.base + scene_active,
										   cl_world.scene->reg);
	transform_t transform = Entity_Transform (fent);
	*active = 0;
	Transform_SetParent (transform, nulltransform);
}

static void
muzzle_flash (entity_t ent, player_state_t *state, bool is_player)
{
	vec3_t		f, r, u;
	vec4f_t     position = { 0, 0, 0, 1}, fv = {};
	if (is_player)
		AngleVectors (cl.viewstate.player_angles, f, r, u);
	else
		AngleVectors (state->viewangles, f, r, u);

	VectorCopy (f, fv);
	VectorCopy (state->pls.es.origin, position);
	CL_MuzzleFlash (ent, position, fv, 0, cl.time);
	state->muzzle_flash = false;
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
	int				msec, oldphysent, j;
	entity_t	    ent;
	frame_t		   *frame;
	player_info_t  *player;
	player_state_t	exact;
	player_state_t *state;
	bool			clientplayer;
	vec3_t			ang = {0, 0, 0};
	vec4f_t         org;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0, player = cl.players, state = frame->playerstate;
		 j < MAX_CLIENTS; j++, player++, state++) {
		ent = CL_GetInvalidEntity (j + 1);
		if (state->messagenum != cl.parsecount
			|| !player->name || !player->name->value[0]) {
			// not present this frame
			if (Entity_Valid (ent)) {
				Scene_DestroyEntity (cl_world.scene, ent);
			}
			continue;
		}

		if (!Entity_Valid (ent)) {
			ent = CL_GetEntity (j + 1);
		}
		renderer_t *renderer = Ent_GetComponent (ent.id, ent.base + scene_renderer,
												 cl_world.scene->reg);
		animation_t *animation = Ent_GetComponent (ent.id, ent.base + scene_animation,
												   cl_world.scene->reg);

		// spawn light flashes, even ones coming from invisible objects
		if (j == cl.playernum) {
			org = cl.viewstate.player_origin;
			cl.viewstate.player_entity = ent;
			clientplayer = true;
		} else {
			org = state->pls.es.origin;
			clientplayer = false;
		}
		if (player->chat && player->chat->value[0] != '0') {
			Ent_SetComponent (ent.id, ent.base + scene_dynlight, ent.reg, &(dlight_t) {
				.origin = org,
				.color = {0.0, 1.0, 0.0, 1.0},
				.radius = 100,
				.die = cl.time + 0.1,
			});
			Light_LinkLight (cl_world.scene->lights, ent.id);
		} else {
			CL_NewDlight (ent, org, state->pls.es.effects,
						  state->pls.es.glow_size, state->pls.es.glow_color,
						  cl.time);
		}
		if (state->muzzle_flash) {
			muzzle_flash (ent, state, j == cl.playernum);
		}

		// Draw player?
		if (!Cam_DrawPlayer (j))
			continue;

		if (!state->pls.es.modelindex)
			continue;

		// Hack hack hack
		if (cl_deadbodyfilter
			&& state->pls.es.modelindex == cl_playerindex
			&& is_dead_body (&state->pls.es))
			continue;

		renderer->onlyshadows = (cl_player_shadows && j == cl.playernum
								 && !chase_active);

		colormap_t  colormap = {
			.top = player->topcolor,
			.bottom = player->bottomcolor,
		};
		Ent_SetComponent (ent.id, ent.base + scene_colormap, ent.reg, &colormap);

		// predict only half the move to minimize overruns
		msec = 500 * (playertime - state->state_time);
		if (msec <= 0 || (!cl_predict_players) || cls.demoplayback2) {
			Sys_Printf("a\n");
			exact.pls.es.origin = state->pls.es.origin;
		} else {									// predict players movement
			state->pls.cmd.msec = msec = min (msec, 255);

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers (j);
			exact.pls.es.origin[3] = 1;//FIXME should be done by prediction
			CL_PredictUsercmd (state, &exact, &state->pls.cmd, clientplayer);
			pmove.numphysent = oldphysent;
		}

		// angles
		if (j == cl.playernum)
		{
			ang[PITCH] = -cl.viewstate.player_angles[PITCH] / 3.0;
			ang[YAW] = cl.viewstate.player_angles[YAW];
		} else {
			ang[PITCH] = -state->viewangles[PITCH] / 3.0;
			ang[YAW] = state->viewangles[YAW];
		}
		ang[ROLL] = V_CalcRoll (ang, state->pls.es.velocity) * 4.0;

		if (renderer->model
			!= cl_world.models.a[state->pls.es.modelindex]) {
			renderer->model = cl_world.models.a[state->pls.es.modelindex];
			animation->nolerp = 1;
		}
		animation->frame = state->pls.es.frame;
		renderer->skinnum = state->pls.es.skinnum;

		//FIXME scale
		CL_TransformEntity (ent, 1, ang, exact.pls.es.origin);

		renderer->min_light = 0;
		renderer->fullbright = 0;

		if (state->pls.es.modelindex == cl_playerindex) { //XXX
			// use custom skin
			renderer->skin = player->skin;

			renderer->min_light = min (cl.fbskins, cl_fb_players);

			if (renderer->min_light >= 1.0) {
				renderer->fullbright = 1;
			}
		} else {
			// FIXME no team colors on nonstandard player models
			renderer->skin = 0;
		}

		int     flag_state = state->pls.es.effects & (EF_FLAG1 | EF_FLAG2);
		if (Entity_Valid (player->flag_ent) && !flag_state) {
			CL_RemoveFlagModels (j);
			player->flag_ent = nullentity;
		} else if (!Entity_Valid (player->flag_ent) && flag_state) {
			if (flag_state & EF_FLAG1)
				player->flag_ent = CL_AddFlagModels (ent, 0, j);
			else if (flag_state & EF_FLAG2)
				player->flag_ent = CL_AddFlagModels (ent, 1, j);
		}

		// stuff entity in map
		R_AddEfrags (&cl_world.scene->worldmodel->brush, ent);
		if (Entity_Valid (player->flag_ent)) {
			CL_UpdateFlagModels (ent, j);
			entity_t    fent = player->flag_ent;
			R_AddEfrags (&cl_world.scene->worldmodel->brush, fent);
		}
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
		cl.viewstate.player_origin, cl.viewentity
	};

	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_UpdateTEnts (cl.time, &tentCtx);
	if (cl_draw_locs) {
		locs_draw (cl.time, cl.viewstate.player_origin);
	}
}

void
CL_Ents_Init (void)
{
}
