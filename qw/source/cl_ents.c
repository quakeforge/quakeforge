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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/skin.h"

#include "bothdefs.h"
#include "cl_cam.h"
#include "cl_ents.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "cl_pred.h"
#include "cl_tent.h"
#include "compat.h"
#include "d_iface.h"
#include "host.h"
#include "msg_ucmd.h"
#include "net_svc.h"
#include "pmove.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "view.h"

static struct predicted_player {
	int         flags;
	qboolean    active;
	vec3_t      origin;					// predicted origin
} predicted_players[MAX_CLIENTS];

entity_t    cl_packet_ents[512];	// FIXME: magic number
entity_t    cl_flag_ents[MAX_CLIENTS];
entity_t    cl_player_ents[MAX_CLIENTS];

extern cvar_t *cl_predict_players;
extern cvar_t *cl_solid_players;


void
CL_ClearEnts ()
{
	int		i;

	for (i = 0; i < sizeof (cl_packet_ents) / sizeof (cl_packet_ents[0]); i++)
		CL_Init_Entity (&cl_packet_ents[i]);
	for (i = 0; i < sizeof (cl_flag_ents) / sizeof (cl_flag_ents[0]); i++)
		CL_Init_Entity (&cl_flag_ents[i]);
	for (i = 0; i < sizeof (cl_player_ents) / sizeof (cl_player_ents[0]); i++)
		CL_Init_Entity (&cl_player_ents[i]);
}

void
CL_NewDlight (int key, vec3_t org, int effects)
{
	float		radius;
	dlight_t   *dl;
	static vec3_t normal = {0.4, 0.2, 0.05};
	static vec3_t red = {0.5, 0.05, 0.05};
	static vec3_t blue = {0.05, 0.05, 0.5};
	static vec3_t purple = {0.5, 0.05, 0.5};

	if (!(effects & (EF_BLUE | EF_RED | EF_BRIGHTLIGHT | EF_DIMLIGHT)))
		return;

	radius = 200 + (rand () & 31);
	dl = R_AllocDlight (key);
	VectorCopy (org, dl->origin);
	dl->die = cl.time + 0.1;
	switch (effects & (EF_BLUE | EF_RED)) {
		case EF_BLUE | EF_RED:
			VectorCopy (purple, dl->color);
			break;
		case EF_BLUE:
			VectorCopy (blue, dl->color);
			break;
		case EF_RED:
			VectorCopy (red, dl->color);
			break;
		default:
			VectorCopy (normal, dl->color);
			break;
	}
	if (effects & EF_BRIGHTLIGHT) {
		radius += 200;
		dl->origin[2] += 16;
	}
	dl->radius = radius;
}

/* PACKET ENTITY PARSING / LINKING */

int         bitcounts[32];				// / just for protocol profiling

void
CL_EntityState_Copy (entity_state_t *src, entity_state_t *dest, int bits)
{
	if (bits & U_MODEL)
		dest->modelindex = src->modelindex;
	if (bits & U_FRAME)
		dest->frame = (dest->frame & 0xFF00) | (src->frame & 0xFF);
	if (bits & U_COLORMAP)
		dest->colormap = src->colormap;
	if (bits & U_SKIN)
		dest->skinnum = src->skinnum;
	if (bits & U_EFFECTS)
		dest->effects = (dest->effects & 0xFF00) | (src->effects & 0xFF);

	if (bits & U_ORIGIN1)
		dest->origin[0] = src->origin[0];
	if (bits & U_ANGLE1)
		dest->angles[0] = src->angles[0];
	if (bits & U_ORIGIN2)
		dest->origin[1] = src->origin[1];
	if (bits & U_ANGLE2)
		dest->angles[1] = src->angles[1];
	if (bits & U_ORIGIN3)
		dest->origin[2] = src->origin[2];
	if (bits & U_ANGLE3)
		dest->angles[2] = src->angles[2];

	if (bits & U_ALPHA)
		dest->alpha = src->alpha;
	if (bits & U_SCALE)
		dest->scale = src->scale;
	if (bits & U_EFFECTS2)
		dest->effects = (dest->effects & 0xFF) | (src->effects & 0xFF00);
	if (bits & U_GLOWSIZE)
		dest->glow_size = src->glow_size;
	if (bits & U_GLOWCOLOR)
		dest->glow_color = src->glow_color;
	if (bits & U_COLORMOD)
		dest->colormod = src->colormod;
	if (bits & U_FRAME2)
		dest->frame = (dest->frame & 0xFF) | (src->frame & 0xFF00);

	if (bits & U_SOLID) {
		// FIXME
	}
}

void
CL_ParsePacketEntities (void)
{
	int			index, packetnum;
	packet_entities_t *newp;
	net_svc_packetentities_t block;

	packetnum = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[packetnum].packet_entities;
	cl.frames[packetnum].invalid = false;

	cl.validsequence = cls.netchan.incoming_sequence;

	newp->num_entities = 0;

	if (NET_SVC_PacketEntities_Parse (&block, net_message)) {
		Host_NetError ("CL_ParsePacketEntities: Bad Read");
		return;
	}

	for (index = 0; block.vars[index].word; index++) {
		if (block.vars[index].word & U_REMOVE) {
			cl.validsequence = 0;
			Con_Printf ("CL_ParsePacketEntities: WARNING: U_REMOVE on "
						"full update\n");
			return;
		}

		if (index >= MAX_PACKET_ENTITIES) {
			Host_NetError ("CL_ParsePacketEntities: index %i >= "
						   "MAX_PACKET_ENTITIES", index);
			return;
		}

		CL_EntityState_Copy (&cl_baselines[block.vars[index].word & 511],
							 &newp->entities[index],
							 ~block.vars[index].state.flags);
		CL_EntityState_Copy (&block.vars[index].state,
							 &newp->entities[index],
							 block.vars[index].state.flags);
		newp->entities[index].number = block.vars[index].state.number;
		if (newp->entities[index].modelindex >= MAX_MODELS) {
			Host_NetError ("CL_ParsePacketEntities: modelindex %i >= "
						   "MAX_MODELS", newp->entities[index].modelindex);
			return;
		}
		continue;
	}

	newp->num_entities = index;
}

/*
	CL_ParseDeltaPacketEntities

	An svc_packetentities has just been parsed, deal with the
	rest of the data stream.
*/
void
CL_ParseDeltaPacketEntities ()
{
	int			i;
	byte		from;
	int			oldindex, newindex, newnum, oldnum, oldpacket, newpacket, word;
	packet_entities_t *oldp, *newp, dummy;
	qboolean	full;
	net_svc_deltapacketentities_t block;

	if (NET_SVC_DeltaPacketEntities_Parse (&block, net_message)) {
		Host_NetError ("CL_ParseDeltaPacketEntities: Bad Read");
		return;
	}

	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	from = block.from;

	oldpacket = cl.frames[newpacket].delta_sequence;

	if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK))
		Con_DPrintf ("CL_ParseDeltaPacketEntities: WARNING: from mismatch\n");

	full = false;
	if (oldpacket != -1) {
		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1) {
			// we can't use this, it is too old
			Con_DPrintf ("FlushEntityPacket\n");
			cl.validsequence = 0;				// can't render a frame
			return;
		}
		cl.validsequence = cls.netchan.incoming_sequence;
		oldp = &cl.frames[oldpacket & UPDATE_MASK].packet_entities;
	} else {	// a full update that we can start delta compressing from now
		oldp = &dummy;
		dummy.num_entities = 0;
		cl.validsequence = cls.netchan.incoming_sequence;
		full = true;
	}

	oldindex = 0;
	newindex = 0;
	newp->num_entities = 0;

	for (i = 0;; i++) {
		word = block.vars[i].word;
		if (!word) {	// copy rest of ents from old packet
			while (oldindex < oldp->num_entities) {	
//				Con_Printf ("copy %i\n", oldp->entities[oldindex].number);
				if (newindex >= MAX_PACKET_ENTITIES) {
					Host_NetError ("CL_ParseDeltaPacketEntities: newindex >= "
								   "MAX_PACKET_ENTITIES (1st)");
					return;
				}
				newp->entities[newindex] = oldp->entities[oldindex];
				newindex++;
				oldindex++;
			}
			break;
		}
		newnum = word & 511;
		oldnum = oldindex >= oldp->num_entities ? 9999 :
			oldp->entities[oldindex].number;

		while (newnum > oldnum) {
//		if (newnum > oldnum) {
			if (full) {
				Con_Printf ("CL_ParseDeltaPacketEntities: WARNING: "
							"oldcopy on full update");
				return;
			}
//			Con_Printf ("copy %i\n", oldnum);
			// copy one of the old entities over to the new packet unchanged
			if (newindex >= MAX_PACKET_ENTITIES) {
				Host_NetError ("CL_ParseDeltaPacketEntities: newindex >= "
							   "MAX_PACKET_ENTITIES (2nd)");
				return;
			}
			newp->entities[newindex] = oldp->entities[oldindex];
			newindex++;
			oldindex++;
			oldnum = oldindex >= oldp->num_entities ? 9999 :
				oldp->entities[oldindex].number;
		}

		if (newnum < oldnum) {			// new from baseline
//			Con_Printf ("baseline %i\n", newnum);
			if (word & U_REMOVE) {
				if (full) {
					cl.validsequence = 0;
					Con_Printf ("CL_ParseDeltaPacketEntities: "
								"WARNING: U_REMOVE on full update\n");
					return;
				}
				continue;
			}

			if (newindex >= MAX_PACKET_ENTITIES) {
				Host_NetError ("CL_ParseDeltaPacketEntities: newindex >= "
							   "MAX_PACKET_ENTITIES (3rd)");
				return;
			}
			CL_EntityState_Copy (&cl_baselines[newnum],
								 &newp->entities[newindex],
								 ~block.vars[i].state.flags);
			CL_EntityState_Copy (&block.vars[i].state,
								 &newp->entities[newindex],
								 block.vars[i].state.flags);
			newp->entities[newindex].number = block.vars[i].state.number;
			if (newp->entities[newindex].modelindex >= MAX_MODELS) {
				Host_NetError ("CL_ParsePacketEntities: modelindex %i >= "
							   "MAX_MODELS",
							   newp->entities[newindex].modelindex);
				return;
			}
			newindex++;
			continue;
		}

		if (newnum == oldnum) {			// delta from previous
			if (full) {
				cl.validsequence = 0;
				Con_Printf ("WARNING: delta on full update");
			}
			if (word & U_REMOVE) {	// Clear the entity
				entity_t	*ent = &cl_packet_ents[newnum];
				memset (ent, 0, sizeof (entity_t));
				oldindex++;
				continue;
			}
//			Con_Printf ("delta %i\n", newnum);
			CL_EntityState_Copy (&oldp->entities[oldindex],
								 &newp->entities[newindex],
								 ~block.vars[i].state.flags);
			CL_EntityState_Copy (&block.vars[i].state,
								 &newp->entities[newindex],
								 block.vars[i].state.flags);
			newp->entities[newindex].number = block.vars[i].state.number;
			if (newp->entities[newindex].modelindex >= MAX_MODELS) {
				Host_NetError ("CL_ParsePacketEntities: modelindex %i >= "
							   "MAX_MODELS",
							   newp->entities[newindex].modelindex);
				return;
			}

			newindex++;
			oldindex++;
		}
	}

	newp->num_entities = newindex;
}

void
CL_LinkPacketEntities (void)
{
	int				pnum, i;
	dlight_t	   *dl;
	entity_t	  **ent;
	entity_state_t *s1;
	model_t		   *model;
	packet_entities_t *pack;

	extern int  cl_h_playerindex, cl_gib1index, cl_gib2index, cl_gib3index;
	extern int  cl_playerindex;

	pack = &cl.frames[cls.netchan.incoming_sequence &
					  UPDATE_MASK].packet_entities;

	for (pnum = 0; pnum < pack->num_entities; pnum++) {
		s1 = &pack->entities[pnum];

		// spawn light flashes, even ones coming from invisible objects
		CL_NewDlight (s1->number, s1->origin, s1->effects);

		// if set to invisible, skip
		if (!s1->modelindex)
			continue;

		// Hack hack hack
		if (cl_deadbodyfilter->int_val && s1->modelindex == cl_playerindex &&
			((i = s1->frame) == 49 || i == 60 || i == 69 || i == 84 ||
			 i == 93 || i == 102))
			continue;

		if (cl_gibfilter->int_val &&
			(s1->modelindex == cl_h_playerindex || s1->modelindex ==
			 cl_gib1index || s1->modelindex == cl_gib2index ||
			 s1->modelindex == cl_gib3index))
			continue;

		// create a new entity
		ent = R_NewEntity ();
		if (!ent)
			break;						// object list is full

		*ent = &cl_packet_ents[s1->number];

		(*ent)->keynum = s1->number;
		(*ent)->model = model = cl.model_precache[s1->modelindex];

		// set colormap
		if (s1->colormap && (s1->colormap < MAX_CLIENTS)
			&& cl.players[s1->colormap - 1].name[0]
			&& !strcmp ((*ent)->model->name, "progs/player.mdl")) {
			(*ent)->colormap = cl.players[s1->colormap - 1].translations;
			(*ent)->scoreboard = &cl.players[s1->colormap - 1];
		} else {
			(*ent)->colormap = vid.colormap8;
			(*ent)->scoreboard = NULL;
		}

		if ((*ent)->scoreboard && !(*ent)->scoreboard->skin)
			Skin_Find ((*ent)->scoreboard);
		if ((*ent)->scoreboard && (*ent)->scoreboard->skin) {
			(*ent)->skin = Skin_NewTempSkin ();
			if ((*ent)->skin) {
				i = s1->colormap - 1;
				CL_NewTranslation (i, (*ent)->skin);
			}
		} else {
			(*ent)->skin = NULL;
		}

		// LordHavoc: cleaned up Endy's coding style, and fixed Endy's bugs
		// Ender: Extend (Colormod) [QSG - Begin]
		// N.B: All messy code below is the sole fault of LordHavoc and
		// his futile attempts to save bandwidth. :)
		(*ent)->glow_size =	s1->glow_size < 128 ? s1->glow_size * 8.0 :
			(s1->glow_size - 256) * 8.0;
		(*ent)->glow_color = s1->glow_color;
		(*ent)->alpha = s1->alpha / 255.0;
		(*ent)->scale = s1->scale / 16.0;

		if (s1->colormod == 255) {
			(*ent)->colormod[0] = (*ent)->colormod[1] =
				(*ent)->colormod[2] = 1;
		} else {
			(*ent)->colormod[0] = (float) ((s1->colormod >> 5) & 7) * (1.0 /
																	   7.0);
			(*ent)->colormod[1] = (float) ((s1->colormod >> 2) & 7) * (1.0 /
																	   7.0);
			(*ent)->colormod[2] = (float) (s1->colormod & 3) * (1.0 / 3.0);
		}
		// Ender: Extend (Colormod) [QSG - End]

		// set skin
		(*ent)->skinnum = s1->skinnum;

		// set frame
		(*ent)->frame = s1->frame;
		if ((*ent)->visframe != r_framecount - 1) {
			(*ent)->pose1 = (*ent)->pose2 = -1;
		}
		(*ent)->visframe = r_framecount;

		if (model->flags & EF_ROTATE) { // rotate binary objects locally
			(*ent)->angles[0] = 0;
			(*ent)->angles[1] = anglemod (100 * cl.time);
			(*ent)->angles[2] = 0;
		} else {
			VectorCopy(s1->angles, (*ent)->angles);
		}

		VectorCopy ((*ent)->origin, (*ent)->old_origin);
		VectorCopy (s1->origin, (*ent)->origin);

		// add automatic particle trails
		if (!model->flags)
			continue;

		if (model->flags & EF_ROCKET) {
			dl = R_AllocDlight (-(*ent)->keynum);
			VectorCopy ((*ent)->origin, dl->origin);
			VectorCopy (r_firecolor->vec, dl->color);
			dl->radius = 200;
			dl->die = cl.time + 0.1;
		}

		// No trail if too far.
		if (VectorDistance_fast((*ent)->old_origin, (*ent)->origin) >
				(256*256)) {
			VectorCopy ((*ent)->origin, (*ent)->old_origin);
			continue;
		}

		if (model->flags & EF_ROCKET)
			R_RocketTrail (*ent);
		else if (model->flags & EF_GRENADE)
			R_GrenadeTrail (*ent);
		else if (model->flags & EF_GIB)
			R_BloodTrail (*ent);
		else if (model->flags & EF_ZOMGIB)
			R_SlightBloodTrail (*ent);
		else if (model->flags & EF_TRACER)
			R_GreenTrail (*ent);
		else if (model->flags & EF_TRACER2)
			R_FlameTrail (*ent);
		else if (model->flags & EF_TRACER3)
			R_VoorTrail (*ent);
	}
}

/* PROJECTILE PARSING / LINKING */

typedef struct {
	int         modelindex;
	entity_t	ent;
} projectile_t;

projectile_t cl_projectiles[MAX_PROJECTILES];
int          cl_num_projectiles;
extern int   cl_spikeindex;

void
CL_ClearProjectiles (void)
{
	cl_num_projectiles = 0;
}

/*
	CL_ParseProjectiles

	Nails are passed as efficient temporary entities
*/
void
CL_ParseProjectiles (void)
{
	int			i;
	projectile_t *pr;
	net_svc_nails_t block;

	if (NET_SVC_Nails_Parse (&block, net_message)) {
		Host_NetError ("CL_ParseProjectiles: Bad Read\n");
		return;
	}

	for (i = 0; i < block.numnails; i++) {
		if (cl_num_projectiles == MAX_PROJECTILES)
			break;

		pr = &cl_projectiles[cl_num_projectiles];
		cl_num_projectiles++;

		pr->modelindex = cl_spikeindex;
		VectorCopy (block.nails[i].origin, pr->ent.origin);
		VectorCopy (block.nails[i].angles, pr->ent.angles);
	}
}

void
CL_LinkProjectiles (void)
{
	int			i;
	entity_t  **ent;
	projectile_t *pr;

	for (i = 0, pr = cl_projectiles; i < cl_num_projectiles; i++, pr++) {
		if (pr->modelindex < 1)
			continue;

		// grab an entity to fill in
		ent = R_NewEntity ();
		if (!ent)
			break;						// object list is full
		*ent = &pr->ent;
		(*ent)->model = cl.model_precache[pr->modelindex];
		(*ent)->skinnum = 0;
		(*ent)->frame = 0;
		(*ent)->colormap = vid.colormap8;
		(*ent)->scoreboard = NULL;
		(*ent)->skin = NULL;
		// LordHavoc: Endy had neglected to do this as part of the QSG
		// VERSION 2 stuff
		(*ent)->glow_size = 0;
		(*ent)->glow_color = 254;
		(*ent)->alpha = 1;
		(*ent)->scale = 1;
		(*ent)->colormod[0] = (*ent)->colormod[1] = (*ent)->colormod[2] = 1;
	}
}

extern double parsecounttime;
extern int    cl_spikeindex, cl_playerindex, cl_flagindex, parsecountmod;

void
CL_ParsePlayerinfo (void)
{
	player_state_t *state;
	net_svc_playerinfo_t block;

	if (NET_SVC_Playerinfo_Parse (&block, net_message)) {
		Host_NetError ("CL_ParsePlayerinfo: Bad Read\n");
		return;
	}

	if (block.playernum >= MAX_CLIENTS) {
		Host_NetError ("CL_ParsePlayerinfo: playernum %i >= MAX_CLIENTS",
					   block.playernum);
		return;
	}
	if (block.modelindex >= MAX_MODELS) {
		Host_NetError ("CL_ParsePlayerinfo: modelindex %i >= MAX_MODELS",
					   block.modelindex);
		return;
	}

	state = &cl.frames[parsecountmod].playerstate[block.playernum];

	state->number = block.playernum;
	state->flags = block.flags;

	state->messagenum = cl.parsecount;
	VectorCopy (block.origin, state->origin);

	state->frame = block.frame;

	// the other player's last move was likely some time
	// before the packet was sent out, so accurately track
	// the exact time it was valid at
	state->state_time = parsecounttime - block.msec * 0.001;

	if (block.flags & PF_COMMAND)
		memcpy (&state->command, &block.usercmd, sizeof (state->command));

	VectorCopy (block.velocity, state->velocity);

	if (block.flags & PF_MODEL)
		state->modelindex = block.modelindex;
	else
		state->modelindex = cl_playerindex;

	state->skinnum = block.skinnum;
	state->effects = block.effects;
	state->weaponframe = block.weaponframe;

	VectorCopy (state->command.angles, state->viewangles);
}

/*
	CL_AddFlagModels

	Called when the CTF flags are set
*/
void
CL_AddFlagModels (entity_t *ent, int team)
{
	float		f;
	int			i;
	entity_t  **newent;
	vec3_t		v_forward, v_right, v_up;

	if (cl_flagindex == -1)
		return;

	f = 14;
	if (ent->frame >= 29 && ent->frame <= 40) {
		if (ent->frame >= 29 && ent->frame <= 34) {	// axpain
			if (ent->frame == 29)
				f = f + 2;
			else if (ent->frame == 30)
				f = f + 8;
			else if (ent->frame == 31)
				f = f + 12;
			else if (ent->frame == 32)
				f = f + 11;
			else if (ent->frame == 33)
				f = f + 10;
			else if (ent->frame == 34)
				f = f + 4;
		} else if (ent->frame >= 35 && ent->frame <= 40) {	// pain
			if (ent->frame == 35)
				f = f + 2;
			else if (ent->frame == 36)
				f = f + 10;
			else if (ent->frame == 37)
				f = f + 10;
			else if (ent->frame == 38)
				f = f + 8;
			else if (ent->frame == 39)
				f = f + 4;
			else if (ent->frame == 40)
				f = f + 2;
		}
	} else if (ent->frame >= 103 && ent->frame <= 118) {
		if (ent->frame >= 103 && ent->frame <= 104)
			f = f + 6;					// nailattack
		else if (ent->frame >= 105 && ent->frame <= 106)
			f = f + 6;					// light 
		else if (ent->frame >= 107 && ent->frame <= 112)
			f = f + 7;					// rocketattack
		else if (ent->frame >= 112 && ent->frame <= 118)
			f = f + 7;					// shotattack
	}

	newent = R_NewEntity ();
	if (!newent)
		return;
	*newent = &cl_flag_ents[ent->keynum];
	(*newent)->model = cl.model_precache[cl_flagindex];
	(*newent)->skinnum = team;

	AngleVectors (ent->angles, v_forward, v_right, v_up);
	v_forward[2] = -v_forward[2];		// reverse z component
	for (i = 0; i < 3; i++)
		(*newent)->origin[i] = ent->origin[i] - f * v_forward[i] + 22 *
			v_right[i];
	(*newent)->origin[2] -= 16;

	VectorCopy (ent->angles, (*newent)->angles)
		(*newent)->angles[2] -= 45;
}

/*
	CL_LinkPlayers

	Create visible entities in the correct position
	for all current players
*/
void
CL_LinkPlayers (void)
{
	double			playertime;
	int				msec, oldphysent, i, j;
	entity_t	  **ent;
	frame_t		   *frame;
	player_info_t  *info;
	player_state_t	exact;
	player_state_t *state;
	vec3_t			org;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0, info = cl.players, state = frame->playerstate; j < MAX_CLIENTS;
		 j++, info++, state++) {
		if (state->messagenum != cl.parsecount)
			continue;					// not present this frame

		// spawn light flashes, even ones coming from invisible objects
		if (j == cl.playernum) {
			VectorCopy (cl.simorg, org);
			r_player_entity = &cl_player_ents[state - frame->playerstate];
		} else
			VectorCopy (state->origin, org);

		CL_NewDlight (j, org, state->effects); //FIXME:
		// appearently, this should be j+1, but that seems nasty for some
		// things (due to lack of lights?), so I'm leaving this as is for now.

		// the player object never gets added
		if (j == cl.playernum) {
			if (!Cam_DrawPlayer (-1))
				continue;
		} else {
			if (!Cam_DrawPlayer (j))
				continue;
		}

		if (!state->modelindex)
			continue;

		// Hack hack hack
		if (cl_deadbodyfilter->int_val && state->modelindex == cl_playerindex
			&& ((i = state->frame) == 49 || i == 60 || i == 69 || i == 84
				|| i == 93 || i == 102))
			continue;

		// grab an entity to fill in
		ent = R_NewEntity ();
		if (!ent)						// object list is full
			break;
		*ent = &cl_player_ents[state - frame->playerstate];

		(*ent)->frame = state->frame;
		(*ent)->keynum = state - frame->playerstate;
		(*ent)->model = cl.model_precache[state->modelindex];
		(*ent)->skinnum = state->skinnum;
		(*ent)->colormap = info->translations;
		if (state->modelindex == cl_playerindex)
			(*ent)->scoreboard = info;		// use custom skin
		else
			(*ent)->scoreboard = NULL;

		if ((*ent)->scoreboard && !(*ent)->scoreboard->skin)
			Skin_Find ((*ent)->scoreboard);
		if ((*ent)->scoreboard && (*ent)->scoreboard->skin) {
			(*ent)->skin = Skin_NewTempSkin ();
			if ((*ent)->skin) {
				CL_NewTranslation (j, (*ent)->skin);
			}
		} else {
			(*ent)->skin = NULL;
		}

		// LordHavoc: more QSG VERSION 2 stuff, FIXME: players don't have
		// extend stuff
		(*ent)->glow_size = 0;
		(*ent)->glow_color = 254;
		(*ent)->alpha = 1;
		(*ent)->scale = 1;
		(*ent)->colormod[0] = (*ent)->colormod[1] = (*ent)->colormod[2] = 1;

		// angles
		if (j == cl.playernum)
		{
			(*ent)->angles[PITCH] = -cl.viewangles[PITCH] / 3;
			(*ent)->angles[YAW] = cl.viewangles[YAW];
		}
		else
		{
			(*ent)->angles[PITCH] = -state->viewangles[PITCH] / 3;
			(*ent)->angles[YAW] = state->viewangles[YAW];
		}
		(*ent)->angles[ROLL] = 0;
		(*ent)->angles[ROLL] = V_CalcRoll ((*ent)->angles, state->velocity) * 4;

		// only predict half the move to minimize overruns
		msec = 500 * (playertime - state->state_time);
		if (msec <= 0 || (!cl_predict_players->int_val)) {
			VectorCopy (state->origin, (*ent)->origin);
		} else {	// predict players movement
			state->command.msec = msec = min (msec, 255);

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers (j);
			CL_PredictUsercmd (state, &exact, &state->command, false);
			pmove.numphysent = oldphysent;
			VectorCopy (exact.origin, (*ent)->origin);
		}

		if (state->effects & EF_FLAG1)
			CL_AddFlagModels ((*ent), 0);
		else if (state->effects & EF_FLAG2)
			CL_AddFlagModels ((*ent), 1);
	}
}

/*
	CL_SetSolid

	Builds all the pmove physents for the current frame
*/
void
CL_SetSolidEntities (void)
{
	int					i;
	entity_state_t	   *state;
	frame_t			   *frame;
	packet_entities_t  *pak;

	pmove.physents[0].model = cl.worldmodel;
	VectorCopy (vec3_origin, pmove.physents[0].origin);
	pmove.physents[0].info = 0;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i = 0; i < pak->num_entities; i++) {
		state = &pak->entities[i];

		if (!state->modelindex)
			continue;
		if (!cl.model_precache[state->modelindex])
			continue;
		if (cl.model_precache[state->modelindex]->hulls[1].firstclipnode
			|| cl.model_precache[state->modelindex]->clipbox) {
			pmove.physents[pmove.numphysent].model =
				cl.model_precache[state->modelindex];
			VectorCopy (state->origin,
						pmove.physents[pmove.numphysent].origin);
			pmove.numphysent++;
		}
	}

}

/*
	Calculate the new position of players, without other player clipping

	We do this to set up real player prediction.
	Players are predicted twice, first without clipping other players,
	then with clipping against them.
	This sets up the first phase.
*/
void
CL_SetUpPlayerPrediction (qboolean dopred)
{
	double			playertime;
	frame_t		   *frame;
	int				msec, j;
	player_state_t	exact;
	player_state_t *state;
	struct predicted_player *pplayer;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0, pplayer = predicted_players, state = frame->playerstate;
		 j < MAX_CLIENTS; j++, pplayer++, state++) {

		pplayer->active = false;

		if (state->messagenum != cl.parsecount)
			continue;					// not present this frame

		if (!state->modelindex)
			continue;

		pplayer->active = true;
		pplayer->flags = state->flags;

		// note that the local player is special, since he moves locally
		// we use his last predicted postition
		if (j == cl.playernum) {
			VectorCopy (cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].
						playerstate[cl.playernum].origin, pplayer->origin);
		} else {
			// only predict half the move to minimize overruns
			msec = 500 * (playertime - state->state_time);
			if (msec <= 0 ||
				(!cl_predict_players->int_val)
				|| !dopred) {
				VectorCopy (state->origin, pplayer->origin);
				// Con_DPrintf ("nopredict\n");
			} else {
				// predict players movement
				state->command.msec = msec = min (msec, 255);
				// Con_DPrintf ("predict: %i\n", msec);

				CL_PredictUsercmd (state, &exact, &state->command, false);
				VectorCopy (exact.origin, pplayer->origin);
			}
		}
	}
}

/*
	CL_SetSolid

	Builds all the pmove physents for the current frame
	Note that CL_SetUpPlayerPrediction() must be called first!
	pmove must be setup with world and solid entity hulls before calling
	(via CL_PredictMove)
*/
void
CL_SetSolidPlayers (int playernum)
{
	int			j;
	physent_t  *pent;
	struct predicted_player *pplayer;

	extern vec3_t player_maxs, player_mins;

	if (!cl_solid_players->int_val)
		return;

	pent = pmove.physents + pmove.numphysent;

	for (j = 0, pplayer = predicted_players; j < MAX_CLIENTS; j++, pplayer++) {

		if (!pplayer->active)
			continue;					// not present this frame

		// the player object never gets added
		if (j == playernum)
			continue;

		if (pplayer->flags & PF_DEAD)
			continue;					// dead players aren't solid

		pent->model = 0;
		VectorCopy (pplayer->origin, pent->origin);
		VectorCopy (player_mins, pent->mins);
		VectorCopy (player_maxs, pent->maxs);
		pmove.numphysent++;
		pent++;
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

	R_ClearEnts ();
	Skin_ClearTempSkins ();

	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_LinkProjectiles ();
	CL_UpdateTEnts ();
}

void
CL_Ents_Init (void)
{
}
