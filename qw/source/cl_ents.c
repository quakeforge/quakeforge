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
#include "pmove.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "view.h"

static struct predicted_player {
	int         flags;
	qboolean    active;
	vec3_t      origin;					// predicted origin
} predicted_players[MAX_CLIENTS];


#define	MAX_PROJECTILES	32
int          cl_num_projectiles;

entity_t    cl_player_ents[MAX_CLIENTS];
entity_t    cl_flag_ents[MAX_CLIENTS];
entity_t    cl_projectiles[MAX_PROJECTILES];
entity_t    cl_packet_ents[512];	// FIXME: magic number


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

/*
	CL_ParseDelta

	Can go from either a baseline or a previous packet_entity
*/
void
CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits)
{
	int		i;

	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = bits & 511;
	bits &= ~511;

	if (bits & U_MOREBITS) {			// read in the low order bits
		i = MSG_ReadByte (net_message);
		bits |= i;
	}
	// count the bits for net profiling
//	for (i=0 ; i<16 ; i++)
//		if (bits&(1<<i))
//			bitcounts[i]++;

	// LordHavoc: Endy neglected to mark this as being part of the QSG
	// version 2 stuff...
	if (bits & U_EXTEND1) {
		bits |= MSG_ReadByte (net_message) << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte (net_message) << 24;
	}

	to->flags = bits;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (net_message);

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte (net_message);

	if (bits & U_COLORMAP)
		to->colormap = MSG_ReadByte (net_message);

	if (bits & U_SKIN)
		to->skinnum = MSG_ReadByte (net_message);

	if (bits & U_EFFECTS)
		to->effects = MSG_ReadByte (net_message);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (net_message);

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle (net_message);

	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (net_message);

	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle (net_message);

	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (net_message);

	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle (net_message);

	if (bits & U_SOLID) {
		// FIXME
	}

	if (!bits & U_EXTEND1)
		return;

	// LordHavoc: Endy neglected to mark this as being part of the QSG
	// version 2 stuff... rearranged it and implemented missing effects
// Ender (QSG - Begin)
	if (bits & U_ALPHA)
		to->alpha = MSG_ReadByte (net_message);
	if (bits & U_SCALE)
		to->scale = MSG_ReadByte (net_message);
	if (bits & U_EFFECTS2)
		to->effects = (to->effects & 0xFF) | (MSG_ReadByte (net_message) << 8);
	if (bits & U_GLOWSIZE)
		to->glow_size = MSG_ReadByte (net_message);
	if (bits & U_GLOWCOLOR)
		to->glow_color = MSG_ReadByte (net_message);
	if (bits & U_COLORMOD)
		to->colormod = MSG_ReadByte (net_message);

	if (!bits & U_EXTEND2)
		return;

	if (bits & U_FRAME2)
		to->frame = (to->frame & 0xFF) | (MSG_ReadByte (net_message) << 8);
// Ender (QSG - End)
}

void
FlushEntityPacket (void)
{
	entity_state_t	olde, newe;
	int				word;

	Con_DPrintf ("FlushEntityPacket\n");

	memset (&olde, 0, sizeof (olde));

	cl.validsequence = 0;				// can't render a frame
	cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1) {
		word = (unsigned short) MSG_ReadShort (net_message);
		if (net_message->badread) {			// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word)
			break;						// done

		CL_ParseDelta (&olde, &newe, word);
	}
}

/*
	CL_ParsePacketEntities

	An svc_packetentities has just been parsed, deal with the
	rest of the data stream.
*/
void
CL_ParsePacketEntities (qboolean delta)
{
	byte		from;
	int			oldindex, newindex, newnum, oldnum, oldpacket, newpacket, word;
	packet_entities_t *oldp, *newp, dummy;
	qboolean	full;

	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	if (delta) {
		from = MSG_ReadByte (net_message);

		oldpacket = cl.frames[newpacket].delta_sequence;

		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK))
			Con_DPrintf ("WARNING: from mismatch\n");
	} else
		oldpacket = -1;

	full = false;
	if (oldpacket != -1) {
		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1) {
			// we can't use this, it is too old
			FlushEntityPacket ();
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

	while (1) {
		word = (unsigned short) MSG_ReadShort (net_message);
		if (net_message->badread) {			// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word) {	// copy rest of ents from old packet
			while (oldindex < oldp->num_entities) {	
//				Con_Printf ("copy %i\n", oldp->entities[oldindex].number);
				if (newindex >= MAX_PACKET_ENTITIES)
					Host_Error ("CL_ParsePacketEntities: newindex == "
								"MAX_PACKET_ENTITIES");
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
			if (full) {
				Con_Printf ("WARNING: oldcopy on full update");
				FlushEntityPacket ();
				return;
			}
//			Con_Printf ("copy %i\n", oldnum);
			// copy one of the old entities over to the new packet unchanged
			if (newindex >= MAX_PACKET_ENTITIES)
				Host_Error ("CL_ParsePacketEntities: newindex == "
							"MAX_PACKET_ENTITIES");
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
					Con_Printf ("WARNING: U_REMOVE on full update\n");
					FlushEntityPacket ();
					return;
				}
				continue;
			}

			if (newindex >= MAX_PACKET_ENTITIES)
				Host_Error ("CL_ParsePacketEntities: newindex == "
							"MAX_PACKET_ENTITIES");
			CL_ParseDelta (&cl_baselines[newnum], &newp->entities[newindex],
						   word);
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
			CL_ParseDelta (&oldp->entities[oldindex],
						   &newp->entities[newindex], word);
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
	player_info_t  *info;

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

		(*ent)->model = model = cl.model_precache[s1->modelindex];

		// set colormap
		if (s1->colormap && (s1->colormap <= MAX_CLIENTS)
			&& cl.players[s1->colormap - 1].name[0]
			&& !strcmp ((*ent)->model->name, "progs/player.mdl")) {
			(*ent)->colormap = cl.players[s1->colormap - 1].translations;
			info = &cl.players[s1->colormap - 1];
		} else {
			(*ent)->colormap = vid.colormap8;
			info = NULL;
		}

		if (info && !info->skin)
			Skin_Find (info);
		if (info && info->skin) {
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
			dl = R_AllocDlight (-s1->number);
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
	byte		bits[6];
	int			i, c, j;
	entity_t   *pr;

	c = MSG_ReadByte (net_message);
	for (i = 0; i < c; i++) {
		for (j = 0; j < 6; j++)
			bits[j] = MSG_ReadByte (net_message);

		if (cl_num_projectiles == MAX_PROJECTILES)
			continue;

		pr = &cl_projectiles[cl_num_projectiles];
		cl_num_projectiles++;

		pr->model = cl.model_precache[cl_spikeindex];
		pr->colormap = vid.colormap8;
		pr->origin[0] = ((bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		pr->origin[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		pr->origin[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		pr->angles[0] = 360 * (bits[4] >> 4) / 16;
		pr->angles[1] = 360 * bits[5] / 256;
	}
}

void
CL_LinkProjectiles (void)
{
	int			i;
	entity_t  **ent;
	entity_t   *pr;

	for (i = 0, pr = cl_projectiles; i < cl_num_projectiles; i++, pr++) {
		if (!pr->model)
			continue;
		// grab an entity to fill in
		ent = R_NewEntity ();
		if (!ent)
			break;						// object list is full
		*ent = pr;
	}
}

void
CL_ParsePlayerinfo (void)
{
	int				flags, msec, num, i;
	player_state_t *state;

	num = MSG_ReadByte (net_message);
	if (num > MAX_CLIENTS)
		Host_Error ("CL_ParsePlayerinfo: bad num");

	state = &cl.frames[parsecountmod].playerstate[num];

	state->number = num;
	flags = state->flags = MSG_ReadShort (net_message);

	state->messagenum = cl.parsecount;
	state->origin[0] = MSG_ReadCoord (net_message);
	state->origin[1] = MSG_ReadCoord (net_message);
	state->origin[2] = MSG_ReadCoord (net_message);

	state->frame = MSG_ReadByte (net_message);

	// the other player's last move was likely some time
	// before the packet was sent out, so accurately track
	// the exact time it was valid at
	if (flags & PF_MSEC) {
		msec = MSG_ReadByte (net_message);
		state->state_time = parsecounttime - msec * 0.001;
	} else
		state->state_time = parsecounttime;

	if (flags & PF_COMMAND)
		MSG_ReadDeltaUsercmd (&nullcmd, &state->command);

	for (i = 0; i < 3; i++) {
		if (flags & (PF_VELOCITY1 << i))
			state->velocity[i] = MSG_ReadShort (net_message);
		else
			state->velocity[i] = 0;
	}
	if (flags & PF_MODEL)
		i = MSG_ReadByte (net_message);
	else
		i = cl_playerindex;
	state->modelindex = i;

	if (flags & PF_SKINNUM)
		state->skinnum = MSG_ReadByte (net_message);
	else
		state->skinnum = 0;

	if (flags & PF_EFFECTS)
		state->effects = MSG_ReadByte (net_message);
	else
		state->effects = 0;

	if (flags & PF_WEAPONFRAME)
		state->weaponframe = MSG_ReadByte (net_message);
	else
		state->weaponframe = 0;

	VectorCopy (state->command.angles, state->viewangles);
}

/*
	CL_AddFlagModels

	Called when the CTF flags are set
*/
void
CL_AddFlagModels (entity_t *ent, int team, int key)
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
	*newent = &cl_flag_ents[key];
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
	entity_t	  **_ent;
	entity_t	   *ent;
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
			r_player_entity = &cl_player_ents[j];
		} else
			VectorCopy (state->origin, org);

		CL_NewDlight (j, org, state->effects); //FIXME:
		// appearently, this should be j+1, but that seems nasty for some
		// things (due to lack of lights?), so I'm leaving this as is for now.

		// the player object never gets added
		if (j == cl.playernum) {
			if (!Cam_DrawPlayer (-1))	// XXX
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
		_ent = R_NewEntity ();
		if (!_ent)						// object list is full
			break;
		ent = *_ent = &cl_player_ents[j];

		ent->frame = state->frame;
		ent->model = cl.model_precache[state->modelindex];
		ent->skinnum = state->skinnum;
		ent->colormap = info->translations;
		if (state->modelindex == cl_playerindex) { //XXX
			// use custom skin
			if (!info->skin)
				Skin_Find (info);
			if (info && info->skin) {
				ent->skin = Skin_NewTempSkin ();
				if (ent->skin) {
					CL_NewTranslation (j, ent->skin);
				}
			} else {
				ent->skin = NULL;
			}
		} else {
			ent->skin = NULL;
		}

		// LordHavoc: more QSG VERSION 2 stuff, FIXME: players don't have
		// extend stuff
		ent->glow_size = 0;
		ent->glow_color = 254;
		ent->alpha = 1;
		ent->scale = 1;
		ent->colormod[0] = ent->colormod[1] = ent->colormod[2] = 1;

		// angles
		if (j == cl.playernum)
		{
			ent->angles[PITCH] = -cl.viewangles[PITCH] / 3;
			ent->angles[YAW] = cl.viewangles[YAW];
		}
		else
		{
			ent->angles[PITCH] = -state->viewangles[PITCH] / 3;
			ent->angles[YAW] = state->viewangles[YAW];
		}
		ent->angles[ROLL] = 0;
		ent->angles[ROLL] = V_CalcRoll (ent->angles, state->velocity) * 4;

		// only predict half the move to minimize overruns
		msec = 500 * (playertime - state->state_time);
		if (msec <= 0 || (!cl_predict_players->int_val)) {
			VectorCopy (state->origin, ent->origin);
		} else {	// predict players movement
			state->command.msec = msec = min (msec, 255);

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers (j);
			CL_PredictUsercmd (state, &exact, &state->command, false);
			pmove.numphysent = oldphysent;
			VectorCopy (exact.origin, ent->origin);
		}

		if (state->effects & EF_FLAG1)
			CL_AddFlagModels (ent, 0, j);
		else if (state->effects & EF_FLAG2)
			CL_AddFlagModels (ent, 1, j);
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
		if (pmove.numphysent == MAX_PHYSENTS) {
			Con_Printf ("WARNING: entity physent overflow, email "
						"quake-devel@lists.sourceforge.net\n");
			break;
		}
		if (cl.model_precache[state->modelindex]->hulls[1].firstclipnode
			|| cl.model_precache[state->modelindex]->clipbox) {
			if (pmove.numphysent == MAX_PHYSENTS) {
				Con_Printf ("WARNING: entity physent overflow, email "
							"quake-devel@lists.sourceforge.net\n");
				break;
			}
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
			if (msec <= 0 || !dopred) {
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

		if (pmove.numphysent == MAX_PHYSENTS) {
			Con_Printf ("WARNING: player physent overflow, email "
						"quake-devel@lists.sourceforge.net\n");
			break;
		}

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
	int         i;

	for (i = 0; i < MAX_PROJECTILES; i++)
		CL_Init_Entity (&cl_projectiles[i]);
}
