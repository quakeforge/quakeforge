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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/sys.h"

#include "qw/msg_ucmd.h"

#include "compat.h"
#include "server.h"
#include "sv_demo.h"
#include "sv_progs.h"


/*
	The PVS must include a small area around the client to allow head
	bobbing or other small motion on the client side.  Otherwise, a bob
	might cause an entity that should be visible to not show up, especially
	when the bob crosses a waterline.
*/

byte		fatpvs[MAX_MAP_LEAFS / 8];
int			fatbytes;


static void
SV_AddToFatPVS (vec3_t org, mnode_t *node)
{
	byte	   *pvs;
	int			i;
	float		d;
	mplane_t   *plane;

	while (1) {
		// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0) {
			if (node->contents != CONTENTS_SOLID) {
				pvs = Mod_LeafPVS ((mleaf_t *) node, sv.worldmodel);
				for (i = 0; i < fatbytes; i++)
					fatpvs[i] |= pvs[i];
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
static byte *
SV_FatPVS (vec3_t org)
{
	fatbytes = (sv.worldmodel->numleafs + 31) >> 3;
	memset (fatpvs, 0, fatbytes);
	SV_AddToFatPVS (org, sv.worldmodel->nodes);
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

		x = ((int) (SVvector (ent, origin)[0] + 4096 + 1) >> 1) & 4095;
		y = ((int) (SVvector (ent, origin)[1] + 4096 + 1) >> 1) & 4095;
		z = ((int) (SVvector (ent, origin)[2] + 4096 + 1) >> 1) & 4095;
		p = (int) (SVvector (ent, angles)[0] * (16.0 / 360.0)) & 15;
		yaw = (int) (SVvector (ent, angles)[1] * (256.0 / 360.0)) & 255;

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

	if (to->effects != from->effects)
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

		if (to->effects > 255)
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
SV_EmitPacketEntities (client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	int			newindex, oldindex, newnum, oldnum, oldmax;
	edict_t	   *ent;
	client_frame_t *fromframe;
	packet_entities_t *from;

	// this is the frame that we are going to delta update from
	if (client->delta_sequence != -1) {
		fromframe = &client->frames[client->delta_sequence & UPDATE_MASK];
		from = &fromframe->entities;
		oldmax = from->num_entities;

		MSG_WriteByte (msg, svc_deltapacketentities);
		MSG_WriteByte (msg, client->delta_sequence);
	} else {
		oldmax = 0;						// no delta update
		from = NULL;

		MSG_WriteByte (msg, svc_packetentities);
	}

	newindex = 0;
	oldindex = 0;
//	SV_Printf ("---%i to %i ----\n", client->delta_sequence & UPDATE_MASK,
//			   client->netchan.outgoing_sequence & UPDATE_MASK);
	while (newindex < to->num_entities || oldindex < oldmax) {
		newnum = newindex >= to->num_entities ? 9999 :
			to->entities[newindex].number;
		oldnum = oldindex >= oldmax ? 9999 : from->entities[oldindex].number;

		if (newnum == oldnum) {			// delta update from old position
//			SV_Printf ("delta %i\n", newnum);
			SV_WriteDelta (&from->entities[oldindex], &to->entities[newindex],
						   msg, false, client->stdver);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum) {
			// this is a new entity, send it from the baseline
			if (newnum == 9999) {
				Sys_Printf ("LOL, %d, %d, %d, %d %d %d\n", newnum, oldnum,
							to->num_entities, oldmax,
							client->netchan.incoming_sequence & UPDATE_MASK,
							client->delta_sequence & UPDATE_MASK);
				if (!client->edict)
					Sys_Printf ("demo\n");
			}
			ent = EDICT_NUM (&sv_pr_state, newnum);
//			SV_Printf ("baseline %i\n", newnum);
			SV_WriteDelta (ent->data, &to->entities[newindex], msg, true,
						   client->stdver);
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
SV_WritePlayersToClient (client_t *client, edict_t *clent, byte *pvs,
						 sizebuf_t *msg)
{
	int			msec, pflags, qf_bits, i, j;
	client_t   *cl;
	edict_t	   *ent;
	usercmd_t	cmd;
	demo_frame_t *demo_frame;
	demo_client_t *dcl;

	demo_frame = &demo.frames[demo.parsecount & DEMO_FRAMES_MASK];

	for (j = 0, cl = svs.clients, dcl = demo_frame->clients; j < MAX_CLIENTS;
		 j++, cl++, dcl++) {

		if (cl->state != cs_spawned && cl->state != cs_server)
			continue;

		ent = cl->edict;

		if (!clent) {
			if (cl->spectator)
				continue;

			dcl->parsecount = demo.parsecount;
			VectorCopy (SVvector (ent, origin), dcl->info.origin);
			VectorCopy (SVvector (ent, angles), dcl->info.angles);
			dcl->info.angles[0] *= -3;
			dcl->info.angles[2] = 0; // no roll angle

			if (SVfloat (ent, health) <= 0) {
				// don't show the corpse looking around...
				dcl->info.angles[0] = 0;
				dcl->info.angles[1] = SVvector (ent, angles)[1];
				dcl->info.angles[2] = 0;
			}

			dcl->info.skinnum = SVfloat (ent, skin);
			dcl->info.effects = SVfloat (ent, effects);
			dcl->info.weaponframe = SVfloat (ent, weaponframe);
			dcl->info.model = SVfloat (ent, modelindex);
			dcl->sec = sv.time - cl->localtime;
			dcl->frame = SVfloat (ent, frame);
			dcl->flags = 0;
			dcl->cmdtime = cl->localtime;
			dcl->fixangle = demo.fixangle[j];
			demo.fixangle[j] = 0;

			if (SVfloat (ent, health) <= 0)
				dcl->flags |= DF_DEAD;
			if (SVvector (ent, mins)[2] != -24)
				dcl->flags |= DF_GIB;
			continue;
		}

		// ZOID visibility tracking
		if (ent != clent &&
			!(client->spec_track && client->spec_track - 1 == j)) {
			if (cl->spectator)
				continue;

			if (pvs) {
				// ignore if not touching a PV leaf
				for (i = 0; i < ent->num_leafs; i++)
					if (pvs[ent->leafnums[i] >> 3]
						& (1 << (ent->leafnums[i] & 7)))
						break;
				if (i == ent->num_leafs)
					continue;				// not visible
			}
		}

		pflags = PF_MSEC | PF_COMMAND;

		if (SVfloat (ent, modelindex) != sv_playermodel)
			pflags |= PF_MODEL;
		for (i = 0; i < 3; i++)
			if (SVvector (ent, velocity)[i])
				pflags |= PF_VELOCITY1 << i;
		if (SVfloat (ent, effects))
			pflags |= PF_EFFECTS;
		if (SVfloat (ent, skin))
			pflags |= PF_SKINNUM;
		if (SVfloat (ent, health) <= 0)
			pflags |= PF_DEAD;
		if (SVvector (ent, mins)[2] != -24)
			pflags |= PF_GIB;

		qf_bits = 0;
		if (client->stdver > 1) {
			if (sv_fields.alpha != -1 && SVfloat (ent, alpha))
				qf_bits |= PF_ALPHA;
			if (sv_fields.scale != -1 && SVfloat (ent, scale))
				qf_bits |= PF_SCALE;
			if ((int) SVfloat (ent, effects) > 255)
				qf_bits |= PF_EFFECTS2;
			if (sv_fields.glow_size != -1 && SVfloat (ent, glow_size))
				qf_bits |= PF_GLOWSIZE;
			if (sv_fields.glow_color != -1 && SVfloat (ent, glow_color))
				qf_bits |= PF_GLOWCOLOR;
			if (sv_fields.colormod != -1
				&& !VectorIsZero (SVvector (ent, colormod)))
				qf_bits |= PF_COLORMOD;
			if ((int) SVfloat (ent, frame) > 255)
				qf_bits |= PF_EFFECTS2;
			if (qf_bits)
				pflags |= PF_QF;
		}

		if (cl->spectator) {
			// only sent origin and velocity to spectators
			pflags &= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3;
		} else if (ent == clent) {
			// don't send a lot of data on personal entity
			pflags &= ~(PF_MSEC | PF_COMMAND);
			if (SVfloat (ent, weaponframe))
				pflags |= PF_WEAPONFRAME;
		}

		if (client->spec_track && client->spec_track - 1 == j
			&& SVfloat (ent, weaponframe))
			pflags |= PF_WEAPONFRAME;

		MSG_WriteByte (msg, svc_playerinfo);
		MSG_WriteByte (msg, j);
		MSG_WriteShort (msg, pflags);

		MSG_WriteCoordV (msg, SVvector (ent, origin));

		MSG_WriteByte (msg, SVfloat (ent, frame));

		if (pflags & PF_MSEC) {
			msec = 1000 * (sv.time - cl->localtime);
			if (msec > 255)
				msec = 255;
			MSG_WriteByte (msg, msec);
		}

		if (pflags & PF_COMMAND) {
			cmd = cl->lastcmd;

			if (SVfloat (ent, health) <= 0) {	// don't show the corpse
												// looking around...
				cmd.angles[0] = 0;
				cmd.angles[1] = SVvector (ent, angles)[1];
				cmd.angles[0] = 0;
			}

			cmd.buttons = 0;			// never send buttons
			cmd.impulse = 0;			// never send impulses

			MSG_WriteDeltaUsercmd (msg, &nullcmd, &cmd);
		}

		for (i = 0; i < 3; i++)
			if (pflags & (PF_VELOCITY1 << i))
				MSG_WriteShort (msg, SVvector (ent, velocity)[i]);

		if (pflags & PF_MODEL)
			MSG_WriteByte (msg, SVfloat (ent, modelindex));

		if (pflags & PF_SKINNUM)
			MSG_WriteByte (msg, SVfloat (ent, skin));

		if (pflags & PF_EFFECTS)
			MSG_WriteByte (msg, SVfloat (ent, effects));

		if (pflags & PF_WEAPONFRAME)
			MSG_WriteByte (msg, SVfloat (ent, weaponframe));

		if (pflags & PF_QF) {
			MSG_WriteByte (msg, qf_bits);
			if (qf_bits & PF_ALPHA) {
				float       alpha = SVfloat (ent, alpha);
				MSG_WriteByte (msg, bound (0, alpha, 1) * 255);
			}
			if (qf_bits & PF_SCALE) {
				float       scale = SVfloat (ent, scale);
				MSG_WriteByte (msg, bound (0, scale, 15.9375) * 16);
			}
			if (qf_bits & PF_EFFECTS2) {
				MSG_WriteByte (msg, (int) (SVfloat (ent, effects)) >> 8);
			}
			if (qf_bits & PF_GLOWSIZE) {
				int         glow_size = SVfloat (ent, scale);
				MSG_WriteByte (msg, bound (-1024, glow_size, 1016) >> 3);
			}
			if (qf_bits & PF_GLOWCOLOR)
				MSG_WriteByte (msg, SVfloat (ent, glow_color));
			if (qf_bits & PF_COLORMOD) {
				float      *colormod= SVvector (ent, colormod);
				MSG_WriteByte (msg,
					 ((int) (bound (0, colormod[0], 1) * 7.0) << 5) |
					 ((int) (bound (0, colormod[1], 1) * 7.0) << 2) |
					 (int) (bound (0, colormod[2], 1) * 3.0));
			}
			if (qf_bits & PF_FRAME2)
				MSG_WriteByte (msg, (int) (SVfloat (ent, frame)) >> 8);
		}
	}
}

/*
	SV_WriteEntitiesToClient

	Encodes the current state of the world as
	a svc_packetentities messages and possibly
	a svc_nails message and
	svc_playerinfo messages
*/
void
SV_WriteEntitiesToClient (client_t *client, sizebuf_t *msg, qboolean recorder)
{
	byte	   *pvs = 0;
	int			e, i, num_edicts, mpe_moaned = 0;
	int         max_packet_entities = MAX_PACKET_ENTITIES;
	vec3_t		org;
	client_frame_t *frame;
	edict_t	   *clent, *ent;
	entity_state_t *state;
	packet_entities_t *pack;

	// this is the frame we are creating
	frame = &client->frames[client->netchan.incoming_sequence & UPDATE_MASK];

	// find the client's PVS
	clent = client->edict;
	if (!recorder) {
		VectorAdd (SVvector (clent, origin), SVvector (clent, view_ofs), org);
		pvs = SV_FatPVS (org);
	} else if (!sv_demoNoVis->int_val) {
		client_t   *cl;

		max_packet_entities = MAX_DEMO_PACKET_ENTITIES;

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
				SV_AddToFatPVS (org, sv.worldmodel->nodes);
			}
		}
	}

	// send over the players in the PVS
	SV_WritePlayersToClient (client, clent, pvs, msg);

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
			for (i = 0; i < ent->num_leafs; i++)
				if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i] & 7)))
					break;

			if (i == ent->num_leafs)
				continue;					// not visible
		}

		if (SV_AddNailUpdate (ent))
			continue;					// added to the special update list

		// add to the packetentities
		if (pack->num_entities == max_packet_entities) {
			if (!mpe_moaned) {
				Con_DPrintf ("hit max_packet_entities for client %d\n",
							 client->userid);
				mpe_moaned = 1;
			}
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
	SV_EmitPacketEntities (client, pack, msg);

	// now add the specialized nail update
	SV_EmitNailUpdate (msg, recorder);
}
