/*
	sv_ents.c

	(description)

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
#include "QF/set.h"
#include "QF/sys.h"

#include "compat.h"

#include "qw/msg_ucmd.h"

#include "qw/include/server.h"
#include "qw/include/sv_progs.h"


/*
	The PVS must include a small area around the client to allow head
	bobbing or other small motion on the client side.  Otherwise, a bob
	might cause an entity that should be visible to not show up, especially
	when the bob crosses a waterline.
*/

static set_t *fatpvs;


static void
SV_AddToFatPVS (vec3_t org, mnode_t *node)
{
	float       d;
	plane_t    *plane;

	while (1) {
		// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0) {
			if (node->contents != CONTENTS_SOLID) {
				set_union (fatpvs, Mod_LeafPVS ((mleaf_t *) node,
												sv.worldmodel));
			}
			return;
		}

		plane = node->plane;
		d = DotProduct (org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else {							// go down both
			SV_AddToFatPVS (org, node->children[0]);
			node = node->children[1];
		}
	}
}

/*
	SV_FatPVS

	Calculates a PVS that is the inclusive or of all leafs within 8 pixels
	of the given point.
*/
static set_t *
SV_FatPVS (vec3_t org)
{
	if (!fatpvs) {
		fatpvs = set_new_size (sv.worldmodel->brush.visleafs);
	}
	set_expand (fatpvs, sv.worldmodel->brush.visleafs);
	set_empty (fatpvs);
	SV_AddToFatPVS (org, sv.worldmodel->brush.nodes);
	return fatpvs;
}

// nails are plentiful, so there is a special network protocol for them
#define	MAX_NAILS	32
edict_t	   *nails[MAX_NAILS];
int			numnails;
int         nailcount;


static qboolean
SV_AddNailUpdate (edict_t *ent)
{
	if (SVfloat (ent, modelindex) != sv_nailmodel
		&& SVfloat (ent, modelindex) != sv_supernailmodel)
		return false;
	if (numnails == MAX_NAILS)
		return true;
	nails[numnails] = ent;
	numnails++;
	return true;
}

static void
SV_EmitNailUpdate (sizebuf_t *msg, qboolean recorder)
{
	byte	   *buf;				// [48 bits] xyzpy 12 12 12 4 8
	int			n, p, x, y, z, yaw;
	int         bpn = recorder ? 7 : 6;	// bytes per nail
	edict_t	   *ent;

	if (!numnails)
		return;

	buf =  SZ_GetSpace (msg, numnails * bpn + 2);
	*buf++ = recorder ? svc_nails2 : svc_nails;
	*buf++ = numnails;

	for (n = 0; n < numnails; n++) {
		ent = nails[n];
		if (recorder) {
			if (!SVfloat (ent, colormap)) {
				if (!((++nailcount) & 255))
					nailcount++;
				SVfloat (ent, colormap) = nailcount&255;
			}
			*buf++ = (byte)SVfloat (ent, colormap);
		}

		x   = ((int) (SVvector (ent, origin)[0] + 4096 + 1) >> 1) & 0xfff;
		y   = ((int) (SVvector (ent, origin)[1] + 4096 + 1) >> 1) & 0xfff;
		z   = ((int) (SVvector (ent, origin)[2] + 4096 + 1) >> 1) & 0xfff;
		p   =  (int) (SVvector (ent, angles)[0] * (16.0 / 360.0)) & 0x00f;
		yaw = (int) (SVvector (ent, angles)[1] * (256.0 / 360.0)) & 0x0ff;

		*buf++ = x;
		*buf++ = (x >> 8) | (y << 4);
		*buf++ = (y >> 4);
		*buf++ = z;
		*buf++ = (z >> 8) | (p << 4);
		*buf++ = yaw;
	}
}

/*
	SV_WriteDelta

	Writes part of a packetentities message.
	Can delta from either a baseline or a previous packet_entity
*/
static void
SV_WriteDelta (entity_state_t *from, entity_state_t *to, sizebuf_t *msg,
			   qboolean force, int stdver)
{
	int			bits, i;
	float		miss;

	// send an update
	bits = 0;

	for (i = 0; i < 3; i++) {
		miss = to->origin[i] - from->origin[i];
		if (miss < -0.1 || miss > 0.1)
			bits |= U_ORIGIN1 << i;
	}

	if (to->angles[0] != from->angles[0])
		bits |= U_ANGLE1;

	if (to->angles[1] != from->angles[1])
		bits |= U_ANGLE2;

	if (to->angles[2] != from->angles[2])
		bits |= U_ANGLE3;

	if (to->colormap != from->colormap)
		bits |= U_COLORMAP;

	if (to->skinnum != from->skinnum)
		bits |= U_SKIN;

	if (to->frame != from->frame)
		bits |= U_FRAME;

	if ((to->effects & 0xff) != (from->effects & 0xff))
		bits |= U_EFFECTS;

	if (to->modelindex != from->modelindex)
		bits |= U_MODEL;

	// LordHavoc: cleaned up Endy's coding style, and added missing effects
// Ender (QSG - Begin)
	if (stdver > 1) {
		if (to->alpha != from->alpha)
			bits |= U_ALPHA;

		if (to->scale != from->scale)
			bits |= U_SCALE;

		if ((to->effects & 0xff00) != (from->effects & 0xff00))
			bits |= U_EFFECTS2;

		if (to->glow_size != from->glow_size)
			bits |= U_GLOWSIZE;

		if (to->glow_color != from->glow_color)
			bits |= U_GLOWCOLOR;

		if (to->colormod != from->colormod)
			bits |= U_COLORMOD;

		if (to->frame > 255)
			bits |= U_EFFECTS2;
	}

	if (bits >= 16777216)
		bits |= U_EXTEND2;

	if (bits >= 65536)
		bits |= U_EXTEND1;
// Ender (QSG - End)
	if (bits & 511)
		bits |= U_MOREBITS;

	if (to->flags & U_SOLID)
		bits |= U_SOLID;

	// write the message
	if (!to->number)
		Sys_Error ("Unset entity number");
	if (to->number >= 512)
		Sys_Error ("Entity number >= 512");

	if (!bits && !force)
		return;							// nothing to send!
	i = to->number | (bits & ~511);
	if (i & U_REMOVE)
		Sys_Error ("U_REMOVE");
	MSG_WriteShort (msg, i);

	if (bits & U_MOREBITS)
		MSG_WriteByte (msg, bits & 255);

	// LordHavoc: cleaned up Endy's tabs
// Ender (QSG - Begin)
	if (bits & U_EXTEND1)
		MSG_WriteByte (msg, bits >> 16);
	if (bits & U_EXTEND2)
		MSG_WriteByte (msg, bits >> 24);
// Ender (QSG - End)

	if (bits & U_MODEL)
		MSG_WriteByte (msg, to->modelindex);
	if (bits & U_FRAME)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte (msg, to->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte (msg, to->skinnum);
	if (bits & U_EFFECTS)
		MSG_WriteByte (msg, to->effects);
	if (bits & U_ORIGIN1)
		MSG_WriteCoord (msg, to->origin[0]);
	if (bits & U_ANGLE1)
		MSG_WriteAngle (msg, to->angles[0]);
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (msg, to->origin[1]);
	if (bits & U_ANGLE2)
		MSG_WriteAngle (msg, to->angles[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (msg, to->origin[2]);
	if (bits & U_ANGLE3)
		MSG_WriteAngle (msg, to->angles[2]);

	// LordHavoc: cleaned up Endy's tabs, rearranged bytes, and implemented
	// missing effects
// Ender (QSG - Begin)
	if (bits & U_ALPHA)
		MSG_WriteByte (msg, to->alpha);
	if (bits & U_SCALE)
		MSG_WriteByte (msg, to->scale);
	if (bits & U_EFFECTS2)
		MSG_WriteByte (msg, (to->effects >> 8));
	if (bits & U_GLOWSIZE)
		MSG_WriteByte (msg, to->glow_size);
	if (bits & U_GLOWCOLOR)
		MSG_WriteByte (msg, to->glow_color);
	if (bits & U_COLORMOD)
		MSG_WriteByte (msg, to->colormod);
	if (bits & U_FRAME2)
		MSG_WriteByte (msg, (to->frame >> 8));
// Ender (QSG - End)
}

/*
	SV_EmitPacketEntities

	Writes a delta update of a packet_entities_t to the message.
*/
static void
SV_EmitPacketEntities (delta_t *delta, packet_entities_t *to, sizebuf_t *msg,
					   int stdver)
{
	int			newindex, oldindex, newnum, oldnum, oldmax;
	edict_t	   *ent;
	client_frame_t *fromframe;
	packet_entities_t *from;

	// this is the frame that we are going to delta update from
	if (delta->delta_sequence != -1) {
		fromframe = &delta->frames[delta->delta_sequence & UPDATE_MASK];
		from = &fromframe->entities;
		oldmax = from->num_entities;

		MSG_WriteByte (msg, svc_deltapacketentities);
		MSG_WriteByte (msg, delta->delta_sequence);
	} else {
		oldmax = 0;						// no delta update
		from = NULL;

		MSG_WriteByte (msg, svc_packetentities);
	}

	newindex = 0;
	oldindex = 0;
//	SV_Printf ("---%i to %i ----\n", delta->delta_sequence & UPDATE_MASK,
//			   delta->netchan.outgoing_sequence & UPDATE_MASK);
	while (newindex < to->num_entities || oldindex < oldmax) {
		newnum = newindex >= to->num_entities ? 9999 :
			to->entities[newindex].number;
		oldnum = oldindex >= oldmax ? 9999 : from->entities[oldindex].number;

		if (newnum == oldnum) {			// delta update from old position
//			SV_Printf ("delta %i\n", newnum);
			SV_WriteDelta (&from->entities[oldindex], &to->entities[newindex],
						   msg, false, stdver);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum) {
			// this is a new entity, send it from the baseline
			if (newnum == 9999) {
				Sys_Printf ("LOL, %d, %d, %d, %d %d %d\n", newnum, oldnum,
							to->num_entities, oldmax,
							delta->in_frame,
							delta->delta_sequence & UPDATE_MASK);
				if (!delta->client)
					Sys_Printf ("demo\n");
			}
			ent = EDICT_NUM (&sv_pr_state, newnum);
//			SV_Printf ("baseline %i\n", newnum);
			SV_WriteDelta (&SVdata (ent)->state, &to->entities[newindex], msg,
						   true, stdver);
			newindex++;
			continue;
		}

		if (newnum > oldnum) {			// the old entity isn't present in
										// the new message
//			SV_Printf ("remove %i\n", oldnum);
			MSG_WriteShort (msg, oldnum | U_REMOVE);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (msg, 0);			// end of packetentities
}

static void
write_demoplayer (delta_t *delta, plent_state_t *from, plent_state_t *to,
				  sizebuf_t *msg, int mask, int full)
{
	int         flags;
	int         j;

	flags = to->es.flags >> 1;	// convert PF_(GIB|DEAD) to DF_(GIB|DEAD)
	flags &= DF_GIB | DF_DEAD;	// PF_MSEC and PF_COMMAND aren't wanted
	if (full) {
		flags |= DF_ORIGIN | (DF_ORIGIN << 1) | (DF_ORIGIN << 2)
				| DF_ANGLES | (DF_ANGLES << 1) | (DF_ANGLES << 2)
				| DF_EFFECTS | DF_SKINNUM | DF_WEAPONFRAME | DF_MODEL;
	} else {
		for (j = 0; j < 3; j++)
			if (from->es.origin[j] != to->es.origin[j])
				flags |= DF_ORIGIN << j;

		for (j = 0; j < 3; j++)
			if (from->cmd.angles[j] != to->cmd.angles[j])
				flags |= DF_ANGLES << j;

		if (from->es.modelindex != to->es.modelindex)
			flags |= DF_MODEL;
		if ((from->es.effects & 0xff) != (to->es.effects & 0xff))
			flags |= DF_EFFECTS;
		if (from->es.skinnum != to->es.skinnum)
			flags |= DF_SKINNUM;
		if (from->es.weaponframe != to->es.weaponframe)
			flags |= DF_WEAPONFRAME;
	}

	MSG_WriteByte (msg, svc_playerinfo);
	MSG_WriteByte (msg, to->es.number);
	MSG_WriteShort (msg, flags);

	MSG_WriteByte (msg, to->es.frame);

	for (j = 0; j < 3; j++)
		if (flags & (DF_ORIGIN << j))
			MSG_WriteCoord (msg, to->es.origin[j]);

	for (j = 0; j < 3; j++)
		if (flags & (DF_ANGLES << j))
			MSG_WriteAngle16 (msg, to->cmd.angles[j]);

	if (flags & DF_MODEL)
		MSG_WriteByte (msg, to->es.modelindex);

	if (flags & DF_SKINNUM)
		MSG_WriteByte (msg, to->es.skinnum);

	if (flags & DF_EFFECTS)
		MSG_WriteByte (msg, to->es.effects);

	if (flags & DF_WEAPONFRAME)
		MSG_WriteByte (msg, to->es.weaponframe);
}

static void
write_player (delta_t *delta, plent_state_t *from, plent_state_t *to,
			  sizebuf_t *msg, int mask, int full)
{
	int         flags = to->es.flags;
	int         qf_bits = 0;
	int         i;
	int         ds = delta->delta_sequence & 0x7f;

	if (full) {
		flags |= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3 | PF_MODEL
				| PF_SKINNUM | PF_EFFECTS | PF_WEAPONFRAME;
		if (full > 1) {
			qf_bits = PF_ALPHA | PF_SCALE | PF_EFFECTS2 | PF_GLOWSIZE
					  | PF_GLOWCOLOR | PF_COLORMOD | PF_FRAME2;
			flags |= PF_QF;
		}
		ds = -1;
	} else {
		for (i = 0; i < 3; i++)
			if (from->es.velocity[i] != to->es.velocity[i])
				flags |= PF_VELOCITY1 << i;
		if (from->es.modelindex != to->es.modelindex)
			flags |= PF_MODEL;
		if (from->es.skinnum != to->es.skinnum)
			flags |= PF_SKINNUM;
		if ((from->es.effects & 0xff) != (to->es.effects &0xff))
			flags |= PF_EFFECTS;
		if (from->es.weaponframe != to->es.weaponframe)
			flags |= PF_WEAPONFRAME;

		if (from->es.alpha != to->es.alpha)
			qf_bits |= PF_ALPHA;
		if (from->es.scale != to->es.scale)
			qf_bits |= PF_SCALE;
		if ((from->es.effects & 0xff00) != (to->es.effects & 0xff00))
			qf_bits |= PF_EFFECTS2;
		if (from->es.glow_size != to->es.glow_size)
			qf_bits |= PF_GLOWSIZE;
		if (from->es.glow_color != to->es.glow_color)
			qf_bits |= PF_GLOWCOLOR;
		if (from->es.colormod != to->es.colormod)
			qf_bits |= PF_COLORMOD;
		if ((from->es.frame & 0xff00) != (to->es.frame & 0xff00))
			qf_bits |= PF_FRAME2;
		if (qf_bits)
			flags |= PF_QF;
	}

	flags &= mask;

	MSG_WriteByte (msg, svc_playerinfo);
	if (delta->type == dt_tp_qtv)
		MSG_WriteByte (msg, ds);
	MSG_WriteByte (msg, to->es.number);
	MSG_WriteShort (msg, flags);

	MSG_WriteCoordV (msg, &to->es.origin[0]);//FIXME

	MSG_WriteByte (msg, to->es.frame);

	if (flags & PF_MSEC)
		MSG_WriteByte (msg, to->msec);
	if (flags & PF_COMMAND)
		MSG_WriteDeltaUsercmd (msg, &from->cmd, &to->cmd);
	for (i = 0; i < 3; i++)
		if (flags & (PF_VELOCITY1 << i))
			MSG_WriteShort (msg, to->es.velocity[i]);
	if (flags & PF_MODEL)
		MSG_WriteByte (msg, to->es.modelindex);
	if (flags & PF_SKINNUM)
		MSG_WriteByte (msg, to->es.skinnum);
	if (flags & PF_EFFECTS)
		MSG_WriteByte (msg, to->es.effects);
	if (flags & PF_WEAPONFRAME)
		MSG_WriteByte (msg, to->es.weaponframe);

	if (flags & PF_QF) {
		MSG_WriteByte (msg, qf_bits);
		if (qf_bits & PF_ALPHA)
			MSG_WriteByte (msg, to->es.alpha);
		if (qf_bits & PF_SCALE)
			MSG_WriteByte (msg, to->es.scale);
		if (qf_bits & PF_EFFECTS2)
			MSG_WriteByte (msg, to->es.effects >> 8);
		if (qf_bits & PF_GLOWSIZE)
			MSG_WriteByte (msg, to->es.glow_size);
		if (qf_bits & PF_GLOWCOLOR)
			MSG_WriteByte (msg, to->es.glow_color);
		if (qf_bits & PF_COLORMOD)
			MSG_WriteByte (msg, to->es.colormod);
		if (qf_bits & PF_FRAME2)
			MSG_WriteByte (msg, to->es.frame);
	}
}

static void
SV_WritePlayersToClient (delta_t *delta, set_t *pvs, sizebuf_t *msg)
{
	int			j, k;
	client_t   *cl;
	edict_t    *clent = 0;
	int         spec_track = 0;
	int         stdver = 2, full = stdver;
	edict_t	   *ent;
	edict_leaf_t *el;
	packet_players_t *pack;
	client_frame_t *frame = &delta->frames[delta->in_frame];
	packet_players_t *from_pack = 0;
	plent_state_t dummy_player_state, *state = &dummy_player_state;
	static plent_state_t null_player_state;
	void (*write) (delta_t *, plent_state_t *, plent_state_t *, sizebuf_t *,
				   int, int);

	if (!null_player_state.es.alpha) {
		null_player_state.es.alpha = 255;
		null_player_state.es.scale = 16;
		null_player_state.es.glow_size = 0;
		null_player_state.es.glow_color = 254;
		null_player_state.es.colormod = 255;
	}
	null_player_state.es.modelindex = 0;

	if (delta->client) {
		clent = delta->client->edict;
		spec_track = delta->client->spec_track;
		null_player_state.es.modelindex = sv_playermodel;
		full = 0;		// normal qw clients don't get real deltas on players
	}

	write = write_player;
	if (!clent && delta->type == dt_tp_demo)
		write = write_demoplayer;

	if (delta->delta_sequence != -1) {
		client_frame_t *fromframe;
		fromframe = &delta->frames[delta->delta_sequence & UPDATE_MASK];
		from_pack = &fromframe->players;
		full = 0;
	}

	pack = &frame->players;
	pack->num_players = 0;

	for (j = k = 0, cl = svs.clients; j < MAX_CLIENTS; j++, cl++) {
		int         mask = ~PF_WEAPONFRAME;

		if (cl->state != cs_spawned && cl->state != cs_server)
			continue;

		ent = cl->edict;

		// ZOID visibility tracking
		// self or the the ent being tracked as a spectator always gets sent
		if (ent != clent && !(spec_track && spec_track - 1 == j)) {
			if (cl->spectator)
				continue;

			if (pvs) {
				// ignore if not touching a PV leaf
				for (el = SVdata (ent)->leafs; el; el = el->next) {
					if (set_is_member (pvs, el->leafnum))
						break;
				}
				if (!el)
					continue;				// not visible
			}
		}

		if (pack->players)
			state = &pack->players[pack->num_players];
		pack->num_players++;

		state->es.number = j;
		state->es.flags = 0;
		VectorCopy (SVvector (ent, origin), state->es.origin);

		state->msec = min (255, 1000 * (sv.time - cl->localtime));

		state->cmd = cl->lastcmd;
		if (SVfloat (ent, health) <= 0) {	// don't show the corpse
											// looking around...
			state->cmd.angles[0] = 0;
			state->cmd.angles[1] = SVvector (ent, angles)[1];
			state->cmd.angles[0] = 0;
		}

		VectorCopy (SVvector (ent, velocity), state->es.velocity);
		state->es.modelindex = SVfloat (ent, modelindex);
		state->es.frame = SVfloat (ent, frame);
		state->es.skinnum = SVfloat (ent, skin);
		state->es.effects = SVfloat (ent, effects);
		state->es.weaponframe = SVfloat (ent, weaponframe);

		if (SVfloat (ent, health) <= 0)
			state->es.flags |= PF_DEAD;
		if (SVvector (ent, mins)[2] != -24)
			state->es.flags |= PF_GIB;
		state->es.flags |= PF_MSEC | PF_COMMAND;

		state->es.alpha = 255;
		state->es.scale = 16;
		state->es.glow_size = 0;
		state->es.glow_color = 254;
		state->es.colormod = 255;
		if (sv_fields.alpha != -1 && SVfloat (ent, alpha)) {
			float       alpha = SVfloat (ent, alpha);
			state->es.alpha = bound (0, alpha, 1) * 255.0;
		}
		if (sv_fields.scale != -1 && SVfloat (ent, scale)) {
			float       scale = SVfloat (ent, scale);
			state->es.scale = bound (0, scale, 15.9375) * 16;
		}
		if (sv_fields.glow_size != -1 && SVfloat (ent, glow_size)) {
			int         glow_size = SVfloat (ent, glow_size);
			state->es.glow_size = bound (-1024, glow_size, 1016) >> 3;
		}
		if (sv_fields.glow_color != -1 && SVfloat (ent, glow_color))
			state->es.glow_color = SVfloat (ent, glow_color);
		if (sv_fields.colormod != -1
			&& !VectorIsZero (SVvector (ent, colormod))) {
			float      *colormod= SVvector (ent, colormod);
			state->es.colormod = ((int) (bound (0, colormod[0], 1) * 7) << 5) |
								 ((int) (bound (0, colormod[1], 1) * 7) << 2) |
								  (int) (bound (0, colormod[2], 1) * 3);
		}

		if (cl->spectator) {
			// send only origin and velocity of spectators
			mask &= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3;
		} else if (ent == clent) {
			// don't send a lot of data on personal entity
			mask &= ~(PF_MSEC | PF_COMMAND);
			mask |= PF_WEAPONFRAME;
		}

		if (spec_track && spec_track - 1 == j)
			mask |= PF_WEAPONFRAME;

		if (from_pack && from_pack->players) {
			while (k < from_pack->num_players
				   && from_pack->players[k].es.number < state->es.number)
				k++;
			if (k < from_pack->num_players
				&& from_pack->players[k].es.number == state->es.number) {
				write (delta, &from_pack->players[k], state, msg, mask, full);
				continue;
			}
			full = 1;
		}
		write (delta, &null_player_state, state, msg, mask, full);
	}
}

static inline set_t *
calc_pvs (delta_t *delta)
{
	set_t      *pvs = 0;
	vec3_t      org;

	// find the client's PVS
	if (delta->pvs == dt_pvs_normal) {
		edict_t *clent = delta->client->edict;
		VectorAdd (SVvector (clent, origin), SVvector (clent, view_ofs), org);
		pvs = SV_FatPVS (org);
	} else if (delta->pvs == dt_pvs_fat) {
		// when recording a demo, send only entities that can be seen. Can help
		// shrink the mvd at expense of effectively lossy compressio (ents that
		// can't be seen by a player either won't get updated or will disappear
		// for people watching the mvd from viewpoints with no players around
		// (not sure yet:).
		client_t   *cl;
		int         i;

		for (i=0, cl = svs.clients; i<MAX_CLIENTS; i++, cl++) {
			if (cl->state != cs_spawned && cl->state != cs_server)
				continue;

			if (cl->spectator)
				continue;

			VectorAdd (SVvector (cl->edict, origin),
					   SVvector (cl->edict, view_ofs), org);
			if (pvs == NULL) {
				pvs = SV_FatPVS (org);
			} else {
				SV_AddToFatPVS (org, sv.worldmodel->brush.nodes);
			}
		}
	}
	return pvs;
}

/*
	SV_WriteEntitiesToClient

	Encodes the current state of the world as
	a svc_packetentities messages and possibly
	a svc_nails message and
	svc_playerinfo messages
*/
void
SV_WriteEntitiesToClient (delta_t *delta, sizebuf_t *msg)
{
	set_t      *pvs = 0;
	int			e, num_edicts;
	int         max_packet_entities = MAX_DEMO_PACKET_ENTITIES;
	int         stdver = 1;
	client_frame_t *frame;
	edict_t	   *ent;
	edict_leaf_t *el;
	entity_state_t *state;
	packet_entities_t *pack;

	if (delta->client) {
		max_packet_entities = MAX_PACKET_ENTITIES;
		stdver = delta->client->stdver;
	}

	pvs = calc_pvs (delta);

	// send over the players in the PVS
	SV_WritePlayersToClient (delta, pvs, msg);

	// this is the frame we are creating
	frame = &delta->frames[delta->in_frame];

	// put other visible entities into either a packet_entities or a nails
	// message
	pack = &frame->entities;
	pack->num_entities = 0;

	numnails = 0;

	num_edicts = min (sv.num_edicts, 512);	//FIXME stupid protocol limit

	for (e = MAX_CLIENTS + 1, ent = EDICT_NUM (&sv_pr_state, e);
		 e < num_edicts; e++, ent = NEXT_EDICT (&sv_pr_state, ent)) {
		if (ent->free)
			continue;

		// ignore ents without visible models
		if (!SVfloat (ent, modelindex)
			|| !*PR_GetString (&sv_pr_state, SVstring (ent, model)))
			continue;

		if (pvs) {
			// ignore if not touching a PV leaf
			for (el = SVdata (ent)->leafs; el; el = el->next) {
				if (set_is_member (pvs, el->leafnum))
					break;
			}
			if (!el)
				continue;				// not visible
		}

		if (SV_AddNailUpdate (ent))
			continue;					// added to the special update list

		// add to the packetentities
		if (pack->num_entities == max_packet_entities) {
			continue;					// all full
		}

		state = &pack->entities[pack->num_entities];
		pack->num_entities++;

		state->number = e;
		state->flags = 0;
		VectorCopy (SVvector (ent, origin), state->origin);
		VectorCopy (SVvector (ent, angles), state->angles);
		state->modelindex = SVfloat (ent, modelindex);
		state->frame = SVfloat (ent, frame);
		state->colormap = SVfloat (ent, colormap);
		state->skinnum = SVfloat (ent, skin);
		state->effects = SVfloat (ent, effects);

		// LordHavoc: cleaned up Endy's coding style, shortened the code,
		// and implemented missing effects
// Ender: EXTEND (QSG - Begin)
		{
			state->alpha = 255;
			state->scale = 16;
			state->glow_size = 0;
			state->glow_color = 254;
			state->colormod = 255;

			if (sv_fields.alpha != -1 && SVfloat (ent, alpha))
				state->alpha = bound (0, SVfloat (ent, alpha), 1) * 255.0;

			if (sv_fields.scale != -1 && SVfloat (ent, scale))
				state->scale = bound (0, SVfloat (ent, scale), 15.9375) * 16.0;

			if (sv_fields.glow_size != -1 && SVfloat (ent, glow_size))
				state->glow_size =
					bound (-1024, (int) SVfloat (ent, glow_size), 1016) >> 3;

			if (sv_fields.glow_color != -1 && SVfloat (ent, glow_color))
				state->glow_color = (int) SVfloat (ent, glow_color);

			if (sv_fields.colormod != -1 && SVvector (ent, colormod)[0]
				&& SVvector (ent, colormod)[1] && SVvector (ent, colormod)[2])
				state->colormod =
					((int) (bound (0, SVvector (ent, colormod)[0], 1) * 7.0)
					 << 5) |
					((int) (bound (0, SVvector (ent, colormod)[1], 1) * 7.0)
					 << 2) |
					(int) (bound (0, SVvector (ent, colormod)[2], 1) * 3.0);
		}
// Ender: EXTEND (QSG - End)
	}

	// encode the packet entities as a delta from the
	// last packetentities acknowledged by the client
	SV_EmitPacketEntities (delta, pack, msg, stdver);

	// now add the specialized nail update
	SV_EmitNailUpdate (msg, !delta->client);
}
