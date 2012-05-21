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
#include "QF/locs.h"
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "qw/msg_ucmd.h"

#include "qw/bothdefs.h"
#include "cl_cam.h"
#include "cl_ents.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "cl_pred.h"
#include "cl_tent.h"
#include "compat.h"
#include "d_iface.h"
#include "host.h"
#include "qw/pmove.h"
#include "clview.h"

entity_t    cl_player_ents[MAX_CLIENTS];
entity_t    cl_flag_ents[MAX_CLIENTS];
entity_t    cl_packet_ents[512];	// FIXME: magic number

void
CL_ClearEnts (void)
{
	size_t      i;

	for (i = 0; i < sizeof (cl_packet_ents) / sizeof (cl_packet_ents[0]); i++)
		CL_Init_Entity (&cl_packet_ents[i]);
	for (i = 0; i < sizeof (cl_flag_ents) / sizeof (cl_flag_ents[0]); i++)
		CL_Init_Entity (&cl_flag_ents[i]);
	for (i = 0; i < sizeof (cl_player_ents) / sizeof (cl_player_ents[0]); i++)
		CL_Init_Entity (&cl_player_ents[i]);
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
CL_LinkPacketEntities (void)
{
	int				pnum;
	dlight_t	   *dl;
	entity_t	   *ent;
	entity_state_t *s1;
	model_t		   *model;
	packet_entities_t *pack;

	pack = &cl.frames[cls.netchan.incoming_sequence &
					  UPDATE_MASK].packet_entities;

	for (pnum = 0; pnum < pack->num_entities; pnum++) {
		s1 = &pack->entities[pnum];

		// spawn light flashes, even ones coming from invisible objects
		CL_NewDlight (s1->number, s1->origin, s1->effects, s1->glow_size,
					  s1->glow_color);

		ent = &cl_packet_ents[s1->number];

		// if set to invisible, skip
		if (!s1->modelindex
			|| (cl_deadbodyfilter->int_val && is_dead_body (s1))
			|| (cl_gibfilter->int_val && is_gib (s1))) {
			if (ent->efrag)
				r_funcs->R_RemoveEfrags (ent);
			continue;
		}

		ent->model = model = cl.model_precache[s1->modelindex];

		ent->min_light = 0;
		ent->fullbright = 0;

		if (s1->modelindex == cl_playerindex) {
			ent->min_light = min (cl.fbskins, cl_fb_players->value);
			if (ent->min_light >= 1.0)
				ent->fullbright = 1;
		}

		// set colormap
		ent->skin = s1->skin;

		// LordHavoc: cleaned up Endy's coding style, and fixed Endy's bugs
		// Ender: Extend (Colormod) [QSG - Begin]
		// N.B: All messy code below is the sole fault of LordHavoc and
		// his futile attempts to save bandwidth. :)
		if (s1->colormod == 255) {
			ent->colormod[0] = ent->colormod[1] = ent->colormod[2] = 1.0;
		} else {
			ent->colormod[0] = (float) ((s1->colormod >> 5) & 7) * (1.0 / 7.0);
			ent->colormod[1] = (float) ((s1->colormod >> 2) & 7) * (1.0 / 7.0);
			ent->colormod[2] = (float) (s1->colormod & 3) * (1.0 / 3.0);
		}
		ent->colormod[3] = s1->alpha / 255.0;
		ent->scale = s1->scale / 16.0;
		// Ender: Extend (Colormod) [QSG - End]

		// set skin
		ent->skinnum = s1->skinnum;

		// set frame
		ent->frame = s1->frame;

		if (!ent->efrag) {
			ent->pose1 = ent->pose2 = -1;

			// No trail if new this frame
			VectorCopy (s1->origin, ent->origin);
			VectorCopy (ent->origin, ent->old_origin);
		} else {
			VectorCopy (ent->origin, ent->old_origin);
			VectorCopy (s1->origin, ent->origin);
			if (!VectorCompare (ent->origin, ent->old_origin)) {
				// the entity moved, it must be relinked
				r_funcs->R_RemoveEfrags (ent);
			}
		}
		if (!ent->efrag)
			r_funcs->R_AddEfrags (ent);

		if (model->flags & EF_ROTATE) {		// rotate binary objects locally
			vec3_t      ang;
			ang[PITCH] = 0;
			ang[YAW] = anglemod (100 * cl.time);
			ang[ROLL] = 0;
			CL_TransformEntity (ent, ang, false);
		} else {
			CL_TransformEntity (ent, s1->angles, false);
		}

		// add automatic particle trails
		if (!model->flags)
			continue;

		if (model->flags & EF_ROCKET) {
			dl = r_funcs->R_AllocDlight (-s1->number);
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
				r_funcs->particles->R_GlowTrail (ent, s1->glow_color);
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
	vec_t      *v_forward, *v_left;
	vec3_t      ang;

	if (cl_flagindex == -1)
		return;

	f = 14.0;
	if (ent->frame >= 29 && ent->frame <= 40) {
		f = flag_offsets[ent->frame - 29];
	} else if (ent->frame >= 103 && ent->frame <= 118) {
		if (ent->frame <= 106)							// 103-104 nailattack
			f = 20.0;									// 105-106 light
		else											// 107-112 rocketattack
			f = 21.0;									// 112-118 shotattack
	}

	fent = &cl_flag_ents[key];
	fent->model = cl.model_precache[cl_flagindex];
	fent->skinnum = team;

	v_forward = ent->transform + 0;
	v_left = ent->transform + 4;

	VectorMultAdd (ent->origin, -f, v_forward, fent->origin);
	VectorMultAdd (fent->origin, -22, v_left, fent->origin);
	fent->origin[2] -= 16.0;

	VectorCopy (ent->angles, ang);
	ang[2] -= 45.0;
	CL_TransformEntity (fent, ang, false);

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
		if (ent->efrag)
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
		CL_NewDlight (j + 1, org, state->pls.effects, state->pls.glow_size,
					  state->pls.glow_color);

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

		ent->model = cl.model_precache[state->pls.modelindex];
		ent->frame = state->pls.frame;
		ent->skinnum = state->pls.skinnum;

		CL_TransformEntity (ent, ang, false);

		ent->min_light = 0;
		ent->fullbright = 0;

		if (state->pls.modelindex == cl_playerindex) { //XXX
			// use custom skin
			ent->skin = info->skin;

			ent->min_light = min (cl.fbskins, cl_fb_players->value);

			if (ent->min_light >= 1.0)
				ent->fullbright = 1;
		} else {
			// FIXME no team colors on nonstandard player models
			ent->skin = 0;
		}

		// stuff entity in map
		r_funcs->R_AddEfrags (ent);

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

	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_UpdateTEnts ();
#if 0 //FIXME
	if (!r_drawentities->int_val) {
		dlight_t   *dl;
		location_t *nearloc;
		vec3_t      trueloc;
		int         i;

		nearloc = locs_find (r_origin);
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
#endif
}

void
CL_Ents_Init (void)
{
	r_data->view_model = &cl.viewent;
}
