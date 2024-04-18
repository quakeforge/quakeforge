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

#include "compat.h"

#include "client/temp_entities.h"
#include "client/view.h"
#include "client/world.h"

#include "qw/msg_ucmd.h"
#include "qw/pmove.h"
#include "qw/bothdefs.h"

#include "qw/include/cl_cam.h"
#include "qw/include/cl_ents.h"
#include "qw/include/cl_main.h"
#include "qw/include/cl_parse.h"
#include "qw/include/cl_pred.h"
#include "qw/include/host.h"

static struct predicted_player {
	int         flags;
	bool        active;
	vec3_t      origin;					// predicted origin
} predicted_players[MAX_CLIENTS];


// PACKET ENTITY PARSING / LINKING ============================================

/*
	CL_ParseDelta

	Can go from either a baseline or a previous packet_entity
*/
static void
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

	to->origin[3] = 1;

	if (bits & U_SOLID) {
		// FIXME
	}

	if (!(bits & U_EXTEND1))
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

	if (!(bits & U_EXTEND2))
		return;

	if (bits & U_FRAME2)
		to->frame = (to->frame & 0xFF) | (MSG_ReadByte (net_message) << 8);
// Ender (QSG - End)
}

static void
FlushEntityPacket (void)
{
	entity_state_t	olde, newe;
	int				word;

	Sys_MaskPrintf (SYS_dev, "FlushEntityPacket\n");

	memset (&olde, 0, sizeof (olde));

	cl.validsequence = 0;				// can't render a frame
	cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1) {
		word = MSG_ReadShort (net_message);
		if (net_message->badread) {		// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word)
			break;						// done

		CL_ParseDelta (&olde, &newe, word);
	}
}

static void
copy_state (packet_entities_t *newp, packet_entities_t *oldp, int newindex,
			int oldindex, int num)
{
	newp->entities[num] = oldp->entities[num];
	newp->ent_nums[newindex] = num;
	cl_entity_valid[0][num] = 1;
}

void
CL_ParsePacketEntities (bool delta)
{
	byte        from;
	int         oldindex, newindex, newnum, oldnum, oldpacket, word;
	packet_entities_t *oldp, *newp, dummy;
	bool        full;

	cl.prev_sequence = cl.link_sequence;
	cl.link_sequence = cls.netchan.incoming_sequence;
	newp = &cl.frames[cl.link_sequence & UPDATE_MASK].packet_entities;
	cl.frames[cl.link_sequence & UPDATE_MASK].invalid = false;

	if (delta) {
		from = MSG_ReadByte (net_message);

		oldpacket = cl.frames[cl.link_sequence & UPDATE_MASK].delta_sequence;
		if (cls.demoplayback2)
			from = oldpacket = (cls.netchan.incoming_sequence - 1);
		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK))
			Sys_MaskPrintf (SYS_dev, "WARNING: from mismatch\n");
	} else
		oldpacket = -1;

	full = false;
	//memcpy (cl_entity_valid[1], cl_entity_valid[0],
	//		sizeof (cl_entity_valid[0]));
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
		memset (cl_entity_valid[0], 0, sizeof (cl_entity_valid[0]));
	}

	oldindex = 0;
	newindex = 0;
	newp->num_entities = 0;

	newnum = 0;
	while (1) {
		word = MSG_ReadShort (net_message);
		if (net_message->badread) {		// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word) {					// copy rest of ents from old packet
			while (oldindex < oldp->num_entities) {
				if (newindex >= MAX_DEMO_PACKET_ENTITIES)
					Host_Error ("CL_ParsePacketEntities: newindex == "
								"MAX_DEMO_PACKET_ENTITIES");
				oldnum = oldp->ent_nums[oldindex];
				copy_state (newp, oldp, newindex++, oldindex++, oldnum);
			}
			break;
		}
		newnum = word & 511;
		oldnum = oldindex >= oldp->num_entities ? 9999 :
			oldp->ent_nums[oldindex];

		while (newnum > oldnum) {
			if (full) {
				Sys_Printf ("WARNING: oldcopy on full update");
				FlushEntityPacket ();
				return;
			}
			// copy one of the old entities over to the new packet unchanged
			if (newindex >= MAX_DEMO_PACKET_ENTITIES)
				Host_Error ("CL_ParsePacketEntities: newindex == "
							"MAX_DEMO_PACKET_ENTITIES");
			copy_state (newp, oldp, newindex++, oldindex++, oldnum);
			oldnum = oldindex >= oldp->num_entities ? 9999 :
				oldp->ent_nums[oldindex];
		}

		if (newnum < oldnum) {						// new from baseline
			if (word & U_REMOVE) {
				cl_entity_valid[0][newnum] = 0;
				if (full) {
					cl.validsequence = 0;
					Sys_Printf ("WARNING: U_REMOVE on full update\n");
					FlushEntityPacket ();
					return;
				}
				continue;
			}

			if (newindex >= MAX_DEMO_PACKET_ENTITIES)
				Host_Error ("CL_ParsePacketEntities: newindex == "
							"MAX_DEMO_PACKET_ENTITIES");
			CL_ParseDelta (&qw_entstates.baseline[newnum],
						   &newp->entities[newnum], word);
			newp->ent_nums[newindex] = newnum;
			cl_entity_valid[0][newnum] = 1;
			newindex++;
			continue;
		}

		if (newnum == oldnum) {					// delta from previous
			if (full) {
				cl.validsequence = 0;
				Sys_Printf ("WARNING: delta on full update");
			}
			if (word & U_REMOVE) {				// Clear the entity
				cl_entity_valid[0][newnum] = 0;
				oldindex++;
				continue;
			}
			CL_ParseDelta (&oldp->entities[oldnum],
						   &newp->entities[newnum], word);
			newp->ent_nums[newindex] = newnum;
			cl_entity_valid[0][newnum] = 1;
			newindex++;
			oldindex++;
		}
	}

	cl.num_entities = max (cl.num_entities, newnum);
	newp->num_entities = newindex;
}

static int
TranslateFlags (int src)
{
	int         dst = 0;

	if (src & DF_EFFECTS)
		dst |= PF_EFFECTS;
	if (src & DF_SKINNUM)
		dst |= PF_SKINNUM;
	if (src & DF_DEAD)
		dst |= PF_DEAD;
	if (src & DF_GIB)
		dst |= PF_GIB;
	if (src & DF_WEAPONFRAME)
		dst |= PF_WEAPONFRAME;
	if (src & DF_MODEL)
		dst |= PF_MODEL;

	return dst;
}

static void
CL_ParseDemoPlayerinfo (int num)
{
	int            flags, i;
	player_info_t *info;
	player_state_t *state, *prevstate;
	static player_state_t dummy;

	info = &cl.players[num];
	state = &cl.frames[parsecountmod].playerstate[num];

	state->pls.es.number = num;

	if (info->prevcount > cl.parsecount || !cl.parsecount) {
		prevstate = &dummy;
	} else {
		if (cl.parsecount - info->prevcount >= UPDATE_BACKUP-1)
			prevstate = &dummy;
		else
			prevstate = &cl.frames[info->prevcount
								   & UPDATE_MASK].playerstate[num];
	}
	info->prevcount = cl.parsecount;

	if (cls.findtrack && info->stats[STAT_HEALTH] != 0) {
		autocam = CAM_TRACK;
		Cam_Lock (num);
		ideal_track = num;
		cls.findtrack = false;
	}

	memcpy (state, prevstate, sizeof (player_state_t));

	flags = MSG_ReadShort (net_message);
	state->pls.es.flags = TranslateFlags (flags);
	state->messagenum = cl.parsecount;
	state->pls.cmd.msec = 0;
	state->pls.es.frame = MSG_ReadByte (net_message);
	state->state_time = parsecounttime;
	for (i=0; i <3; i++)
		if (flags & (DF_ORIGIN << i))
			state->pls.es.origin[i] = MSG_ReadCoord (net_message);
	for (i=0; i <3; i++)
		if (flags & (DF_ANGLES << i))
			state->pls.cmd.angles[i] = MSG_ReadAngle16 (net_message);
	if (flags & DF_MODEL)
		state->pls.es.modelindex = MSG_ReadByte (net_message);
	if (flags & DF_SKINNUM)
		state->pls.es.skinnum = MSG_ReadByte (net_message);
	if (flags & DF_EFFECTS)
		state->pls.es.effects = MSG_ReadByte (net_message);
	if (flags & DF_WEAPONFRAME)
		state->pls.es.weaponframe = MSG_ReadByte (net_message);
	VectorCopy (state->pls.cmd.angles, state->viewangles);
}

void
CL_ParsePlayerinfo (void)
{
	int            flags, msec, num, i;
	player_state_t *state;

	num = MSG_ReadByte (net_message);
	if (num > MAX_CLIENTS)
		Host_Error ("CL_ParsePlayerinfo: bad num");

	if (cls.demoplayback2) {
		CL_ParseDemoPlayerinfo (num);
		return;
	}

	state = &cl.frames[parsecountmod].playerstate[num];

	state->pls.es.number = num;

	flags = state->pls.es.flags = MSG_ReadShort (net_message);

	state->messagenum = cl.parsecount;
	MSG_ReadCoordV (net_message, (vec_t*)&state->pls.es.origin);//FIXME
	state->pls.es.origin[3] = 1;

	state->pls.es.frame = MSG_ReadByte (net_message);

	// the other player's last move was likely some time
	// before the packet was sent out, so accurately track
	// the exact time it was valid at
	if (flags & PF_MSEC) {
		msec = MSG_ReadByte (net_message);
		state->state_time = parsecounttime - msec * 0.001;
	} else
		state->state_time = parsecounttime;

	if (flags & PF_COMMAND)
		MSG_ReadDeltaUsercmd (net_message, &nullcmd, &state->pls.cmd);

	for (i = 0; i < 3; i++) {
		if (flags & (PF_VELOCITY1 << i))
			state->pls.es.velocity[i] = (short) MSG_ReadShort (net_message);
		else
			state->pls.es.velocity[i] = 0;
	}
	if (flags & PF_MODEL)
		i = MSG_ReadByte (net_message);
	else
		i = cl_playerindex;
	state->pls.es.modelindex = i;

	if (flags & PF_SKINNUM)
		state->pls.es.skinnum = MSG_ReadByte (net_message);
	else
		state->pls.es.skinnum = 0;

	if (flags & PF_EFFECTS)
		state->pls.es.effects = MSG_ReadByte (net_message);
	else
		state->pls.es.effects = 0;

	if (flags & PF_WEAPONFRAME)
		state->pls.es.weaponframe = MSG_ReadByte (net_message);
	else
		state->pls.es.weaponframe = 0;

	VectorCopy (state->pls.cmd.angles, state->viewangles);

	if (cl.stdver >= 2.0 && (flags & PF_QF)) {
		// QSG2
		int         bits;
		byte        val;
		entity_t    ent;

		ent = CL_GetEntity (num + 1);
		auto renderer = Entity_GetRenderer (ent);
		bits = MSG_ReadByte (net_message);
		if (bits & PF_ALPHA) {
			val = MSG_ReadByte (net_message);
			renderer->colormod[3] = val / 255.0;
		}
		if (bits & PF_SCALE) {
			val = MSG_ReadByte (net_message);
			state->pls.es.scale = val;
		}
		if (bits & PF_EFFECTS2) {
			state->pls.es.effects |= MSG_ReadByte (net_message) << 8;
		}
		if (bits & PF_GLOWSIZE) {
			state->pls.es.glow_size = MSG_ReadByte (net_message);
		}
		if (bits & PF_GLOWCOLOR) {
			state->pls.es.glow_color = MSG_ReadByte (net_message);
		}
		if (bits & PF_COLORMOD) {
			float       r = 1.0, g = 1.0, b = 1.0;
			val = MSG_ReadByte (net_message);
			if (val != 255) {
				r = (float) ((val >> 5) & 7) * (1.0 / 7.0);
				g = (float) ((val >> 2) & 7) * (1.0 / 7.0);
				b = (float) (val & 3) * (1.0 / 3.0);
			}
			VectorSet (r, g, b, renderer->colormod);
		}
		if (bits & PF_FRAME2) {
			state->pls.es.frame |= MSG_ReadByte (net_message) << 8;
		}
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

	pmove.physents[0].model = cl_world.scene->worldmodel;
	VectorZero (pmove.physents[0].origin);
	VectorZero (pmove.physents[0].angles);
	pmove.physents[0].info = 0;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i = 0; i < pak->num_entities; i++) {
		state = &pak->entities[i];

		if (!state->modelindex)
			continue;
		if (!cl_world.models.a[state->modelindex])
			continue;
		if (cl_world.models.a[state->modelindex]->brush.hulls[1].firstclipnode){
			if (pmove.numphysent == MAX_PHYSENTS) {
				Sys_Printf ("WARNING: entity physent overflow, email "
							"quakeforge-devel@lists.quakeforge.net\n");
				break;
			}
			pmove.physents[pmove.numphysent].model =
				cl_world.models.a[state->modelindex];
			VectorCopy (state->origin,
						pmove.physents[pmove.numphysent].origin);
			VectorCopy (state->angles,
						pmove.physents[pmove.numphysent].angles);
			pmove.numphysent++;
		}
	}
}

void
CL_ClearPredict (void)
{
	memset (predicted_players, 0, sizeof (predicted_players));
	//fixangle = 0;
}

/*
	Calculate the new position of players, without other player clipping

	We do this to set up real player prediction.
	Players are predicted twice, first without clipping other players,
	then with clipping against them.
	This sets up the first phase.
*/
void
CL_SetUpPlayerPrediction (bool dopred)
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

		if (!state->pls.es.modelindex)
			continue;

		pplayer->active = true;
		pplayer->flags = state->pls.es.flags;

		// note that the local player is special, since he moves locally
		// we use his last predicted postition
		if (j == cl.playernum) {
			VectorCopy (cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].
						playerstate[cl.playernum].pls.es.origin,
						pplayer->origin);
		} else {
			// predict only half the move to minimize overruns
			msec = 500 * (playertime - state->state_time);
			if (msec <= 0 || !dopred) {
				VectorCopy (state->pls.es.origin, pplayer->origin);
//				Sys_MaskPrintf (SYS_dev, "nopredict\n");
			} else {
				// predict players movement
				state->pls.cmd.msec = msec = min (msec, 255);
//				Sys_MaskPrintf (SYS_dev, "predict: %i\n", msec);

				CL_PredictUsercmd (state, &exact, &state->pls.cmd, false);
				VectorCopy (exact.pls.es.origin, pplayer->origin);
			}
		}
	}
}

/*
	CL_SetSolid

	Builds all the pmove physents for the current frame
	Note that CL_SetUpPlayerPrediction () must be called first!
	pmove must be setup with world and solid entity hulls before calling
	(via CL_PredictMove)
*/
void
CL_SetSolidPlayers (int playernum)
{
	int			j;
	physent_t  *pent;
	struct predicted_player *pplayer;

	if (!cl_solid_players)
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
			Sys_Printf ("WARNING: player physent overflow, email "
						"quakeforge-devel@lists.quakeforge.net\n");
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
