/*
	sv_send.c

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

#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "QF/cvar.h"
#include "QF/console.h"
#include "QF/dstring.h"
#include "QF/msg.h"
#include "QF/set.h"
#include "QF/sys.h"

#include "compat.h"

#include "qw/bothdefs.h"
#include "qw/include/server.h"
#include "qw/include/sv_progs.h"
#include "qw/include/sv_recorder.h"

#define CHAN_AUTO   0
#define CHAN_WEAPON 1
#define CHAN_VOICE  2
#define CHAN_ITEM   3
#define CHAN_BODY   4

/* SV_Printf redirection */

dstring_t   outputbuf = {&dstring_default_mem};
int         con_printf_no_log;
redirect_t  sv_redirected;


static void
SV_FlushRedirect (void)
{
	char        send[8000 + 6];
	size_t      count;
	int         bytes;
	const char *p;

	if (!outputbuf.size)
		return;

	count = strlen (outputbuf.str);
	if (sv_redirected == RD_PACKET) {

		send[0] = 0xff;
		send[1] = 0xff;
		send[2] = 0xff;
		send[3] = 0xff;
		send[4] = A2C_PRINT;

		p = outputbuf.str;
		while (count) {
			bytes = min (count, sizeof (send) - 5);
			memcpy (send + 5, p, bytes);
			Netchan_SendPacket (bytes + 5, send, net_from);
			p += bytes;
			count -= bytes;
		}
	} else if (sv_redirected == RD_CLIENT || sv_redirected > RD_MOD) {
		client_t   *cl;

		if (sv_redirected > RD_MOD) {
			cl = svs.clients + sv_redirected - RD_MOD - 1;
			if (cl->state != cs_spawned) //FIXME record to mvd?
				count = 0;
		} else {
			cl = host_client;
		}
		p = outputbuf.str;
		while (count) {
			// +/- 3 for svc_print, PRINT_HIGH and nul byte
			// min of 4 because we don't want to send an effectively empty
			// message
			bytes = MSG_ReliableCheckSize (&cl->backbuf, count + 3, 4) - 3;
			// if writing another packet would overflow the client, just drop
			// the rest of the data. getting rudely disconnected would be much
			// more annoying than losing the tail end of the output
			if (bytes <= 0)
				break;
			MSG_ReliableWrite_Begin (&cl->backbuf, svc_print, bytes + 3);
			MSG_ReliableWrite_Byte (&cl->backbuf, PRINT_HIGH);
			MSG_ReliableWrite_SZ (&cl->backbuf, p, bytes);
			MSG_ReliableWrite_Byte (&cl->backbuf, 0);
			p += bytes;
			count -= bytes;
		}
	}
	// RD_MOD doesn't do anything :)
	// clear it
	dstring_clear (&outputbuf);
}

/*
	SV_BeginRedirect

	Send SV_Printf data to the remote client
	instead of the console
*/
void
SV_BeginRedirect (redirect_t rd)
{
	sv_redirected = rd;
	dstring_clear (&outputbuf);
}

void
SV_EndRedirect (void)
{
	SV_FlushRedirect ();
	sv_redirected = RD_NONE;
}

#define	MAXPRINTMSG	4096
static int
find_userid (const char *name)
{
	int         i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!svs.clients[i].state)
			continue;
		if (!strcmp (svs.clients[i].name, name)) {
			return svs.clients[i].userid;
		}
	}
	return 0;
}

#define hstrftime ((size_t (*)(char *s, size_t, const char *, \
					const struct tm*))strftime)

/*
	SV_Printf

	Handles cursor positioning, line wrapping, etc
*/
// FIXME: the msg variables need to be renamed/cleaned up
void
SV_Print (const char *fmt, va_list args)
{
	static dstring_t *premsg;
	static dstring_t *msg;
	static dstring_t *msg2;
	static int  pending = 0;			// partial line being printed

	char        msg3[MAXPRINTMSG];

	time_t      mytime = 0;
	struct tm  *local = NULL;
	bool        timestamps = false;

	char        *in;

	if (!premsg) {
		premsg = dstring_newstr ();
		msg = dstring_new ();
		msg2 = dstring_new ();
	}
	dstring_clearstr (msg);

	dvsprintf (premsg, fmt, args);
	in = premsg->str;

	if (!*premsg->str)
		return;
	// expand FFnickFF to nick <userid>
	do {
		char *beg = strchr (in, 0xFF);
		if (beg) {
			char *name = beg + 1;
			char *end = strchr (name, 0xFF);
			if (!end) {
				end = beg + strlen (name);
			}
			*end = 0;
			dstring_appendsubstr (msg, in, beg - in);
			dasprintf (msg, "%s <%d>", name, find_userid (name));
			in = end + 1;
		} else {
			dstring_appendstr (msg, in);
			break;
		}
	} while (*in);

	if (sv_redirected) {				// Add to redirected message
		dstring_appendstr (&outputbuf, msg->str);
	}
	if (*msg->str && !con_printf_no_log) {
		// We want to output to console and maybe logfile
		if (sv_timefmt && sv_timestamps && !pending)
			timestamps = true;

		if (timestamps) {
			mytime = time (NULL);
			local = localtime (&mytime);
			hstrftime (msg3, sizeof (msg3), sv_timefmt, local);

			dsprintf (msg2, "%s%s", msg3, msg->str);
		} else {
			dsprintf (msg2, "%s", msg->str);
		}
		if (msg2->str[strlen (msg2->str) - 1] != '\n') {
			pending = 1;
		} else {
			pending = 0;
		}

		Con_Printf ("%s", msg2->str);		// also echo to debugging console
	}
}

/* EVENT MESSAGES */

static void
SV_PrintToClient (client_t *cl, int level, const char *string)
{
	static char *buffer;
	const char *a;
	unsigned char *b;
	int size;
	static int buffer_size;

	size = strlen (string) + 1;
	if (size > buffer_size) {
		buffer_size = (size + 1023) & ~1023; // 1k multiples
		if (buffer)
			free (buffer);
		buffer = malloc (buffer_size);
		if (!buffer)
			Sys_Error ("SV_PrintToClient: could not allocate %d bytes",
					   buffer_size);
	}

	a = string;
	b = (byte *) buffer;
	// strip 0xFFs
	while ((*b = *a++))
		if (*b != 0xFF)
			b++;

	MSG_ReliableWrite_Begin (&cl->backbuf, svc_print, strlen (buffer) + 3);
	MSG_ReliableWrite_Byte (&cl->backbuf, level);
	MSG_ReliableWrite_String (&cl->backbuf, buffer);
}

/*
	SV_Multicast

	Sends the contents of sv.multicast to a subset of the clients,
	then clears sv.multicast.

	MULTICAST_ALL	same as broadcast
	MULTICAST_PVS	send to clients potentially visible from org
	MULTICAST_PHS	send to clients potentially hearable from org
*/
void
SV_Multicast (const vec3_t origin, int to)
{
	set_t      *mask;
	client_t   *client;
	int         leafnum, j;
	mleaf_t    *leaf;
	bool        reliable;
	mod_brush_t *brush = &sv.worldmodel->brush;

	vec4f_t     org = { VectorExpand (origin), 1 };
	leaf = Mod_PointInLeaf (org, &sv.worldmodel->brush);
	if (!leaf)
		leafnum = 0;
	else
		leafnum = leaf - sv.worldmodel->brush.leafs;

	reliable = false;

	switch (to) {
	case MULTICAST_ALL_R:
		reliable = true;			// intentional fallthrough
	case MULTICAST_ALL:
		mask = &sv.pvs[0];			// leaf 0 is everything;
		break;

	case MULTICAST_PHS_R:
		reliable = true;			// intentional fallthrough
	case MULTICAST_PHS:
		mask = &sv.phs[leafnum];
		break;

	case MULTICAST_PVS_R:
		reliable = true;			// intentional fallthrough
	case MULTICAST_PVS:
		mask = &sv.pvs[leafnum];
		break;

	default:
		Sys_Error ("SV_Multicast: bad to:%i", to);
	}

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned)
			continue;

		if (to == MULTICAST_PHS_R || to == MULTICAST_PHS) {
			vec3_t      delta;

			VectorSubtract (origin, SVvector (client->edict, origin), delta);
			if (VectorLength (delta) <= 1024)
				goto inrange;
		}

		org = (vec4f_t) {VectorExpand (SVvector (client->edict, origin)), 1};
		leaf = Mod_PointInLeaf (org, &sv.worldmodel->brush);
		if (leaf) {
			// -1 is because pvs rows are 1 based, not 0 based like leafs
			leafnum = leaf - brush->leafs - 1;
			if (!set_is_member (mask, leafnum)) {
//				SV_Printf ("supressed multicast\n");
				continue;
			}
		}

	  inrange:
		if (reliable) {
			MSG_ReliableCheckBlock (&client->backbuf, sv.multicast.cursize);
			MSG_ReliableWrite_SZ (&client->backbuf, sv.multicast.data,
								  sv.multicast.cursize);
		} else
			SZ_Write (&client->datagram, sv.multicast.data,
					  sv.multicast.cursize);
	}

	if (sv.recorders) {
		if (reliable) {
			sizebuf_t *dbuf = SVR_WriteBegin (dem_all, 0,
											  sv.multicast.cursize);
			SZ_Write (dbuf, sv.multicast.data, sv.multicast.cursize);
		} else
			SZ_Write (SVR_Datagram (), sv.multicast.data, sv.multicast.cursize);
	}

	SZ_Clear (&sv.multicast);
}

/*
	SV_StartSound

	Each entity can have eight independant sound sources, like voice,
	weapon, feet, etc.

	Channel 0 is an auto-allocate channel, the others override anything
	already running on that entity/channel pair.

	An attenuation of 0 will play full volume everywhere in the level.
	Larger attenuations will drop off.  (max 4 attenuation)
*/
void
SV_StartSound (edict_t *entity, int channel, const char *sample, int volume,
			   float attenuation)
{
	int         ent, sound_num, i;
	bool        use_phs;
	bool        reliable = false;
	vec3_t      origin;

	if (volume < 0 || volume > 255)
		Sys_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 15)
		Sys_Error ("SV_StartSound: channel = %i", channel);

	// find precache number for sound
	for (sound_num = 1; sound_num < MAX_SOUNDS
		 && sv.sound_precache[sound_num]; sound_num++)
		if (!strcmp (sample, sv.sound_precache[sound_num]))
			break;

	if (sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num]) {
		SV_Printf ("SV_StartSound: %s not precacheed\n", sample);
		return;
	}

	ent = NUM_FOR_EDICT (&sv_pr_state, entity);

	if ((channel & 8) || !sv_phs)	// no PHS flag
	{
		if (channel & 8)
			reliable = true;			// sounds that break the phs are
										// reliable
		use_phs = false;
		channel &= 7;
	} else
		use_phs = true;

//	if (channel == CHAN_BODY || channel == CHAN_VOICE)
//		reliable = true;

	channel = (ent << 3) | channel;

	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		channel |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		channel |= SND_ATTENUATION;

	// use the entity origin unless it is a bmodel
	if (SVfloat (entity, solid) == SOLID_BSP) {
		for (i = 0; i < 3; i++)
			origin[i] = SVvector (entity, origin)[i] + 0.5 *
				(SVvector (entity, mins)[i] + SVvector (entity, maxs)[i]);
	} else {
		VectorCopy (SVvector (entity, origin), origin);
	}

	MSG_WriteByte (&sv.multicast, svc_sound);
	MSG_WriteShort (&sv.multicast, channel);
	if (channel & SND_VOLUME)
		MSG_WriteByte (&sv.multicast, volume);
	if (channel & SND_ATTENUATION)
		MSG_WriteByte (&sv.multicast, attenuation * 64);
	MSG_WriteByte (&sv.multicast, sound_num);
	MSG_WriteCoordV (&sv.multicast, origin);

	if (use_phs)
		SV_Multicast (origin, reliable ? MULTICAST_PHS_R : MULTICAST_PHS);
	else
		SV_Multicast (origin, reliable ? MULTICAST_ALL_R : MULTICAST_ALL);
}

/* FRAME UPDATES */

int         sv_nailmodel, sv_supernailmodel, sv_playermodel;

void
SV_FindModelNumbers (void)
{
	int         i;

	sv_nailmodel = -1;
	sv_supernailmodel = -1;
	sv_playermodel = -1;

	for (i = 0; i < MAX_MODELS; i++) {
		if (!sv.model_precache[i])
			break;
		if (!strcmp (sv.model_precache[i], "progs/spike.mdl"))
			sv_nailmodel = i;
		if (!strcmp (sv.model_precache[i], "progs/s_spike.mdl"))
			sv_supernailmodel = i;
		if (!strcmp (sv.model_precache[i], "progs/player.mdl"))
			sv_playermodel = i;
	}
}

void
SV_WriteClientdataToMessage (client_t *client, sizebuf_t *msg)
{
	edict_t    *ent, *other;
	int         i, clnum;
	sizebuf_t  *dbuf;

	ent = client->edict;

	clnum = NUM_FOR_EDICT (&sv_pr_state, ent) - 1;

	// send the chokecount for r_netgraph
	if (client->chokecount) {
		MSG_WriteByte (msg, svc_chokecount);
		MSG_WriteByte (msg, client->chokecount);
		client->chokecount = 0;
	}
	// send a damage message if the player got hit this frame
	if (SVfloat (ent, dmg_take) || SVfloat (ent, dmg_save)) {
		other = PROG_TO_EDICT (&sv_pr_state, SVentity (ent, dmg_inflictor));
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, SVfloat (ent, dmg_save));
		MSG_WriteByte (msg, SVfloat (ent, dmg_take));
		for (i = 0; i < 3; i++)
			MSG_WriteCoord (msg, SVvector (other, origin)[i] + 0.5 *
							(SVvector (other, mins)[i] +
							 SVvector (other, maxs)[i]));
		SVfloat (ent, dmg_take) = 0;
		SVfloat (ent, dmg_save) = 0;
	}

	// add this to server demo
	if (sv.recorders && msg->cursize) {
		dbuf = SVR_WriteBegin (dem_single, clnum, msg->cursize);
		SZ_Write (dbuf, msg->data, msg->cursize);
	}

	// a fixangle might get lost in a dropped packet.  Oh well.
	if (SVfloat (ent, fixangle)) {
		vec_t      *angles = SVvector (ent, angles);
		MSG_WriteByte (msg, svc_setangle);
		MSG_WriteAngleV (msg, angles);
		SVfloat (ent, fixangle) = 0;

		if (sv.recorders) {
			dbuf = SVR_Datagram ();
			MSG_WriteByte (dbuf, svc_setangle);
			MSG_WriteByte (dbuf, clnum);
			MSG_WriteAngleV (dbuf, angles);
		}
	}
}

void
SV_GetStats (edict_t *ent, int spectator, int stats[])
{
	memset (stats, 0, sizeof (int) * MAX_CL_STATS);

	stats[STAT_HEALTH] = SVfloat (ent, health);
	stats[STAT_WEAPON] = SV_ModelIndex (PR_GetString (&sv_pr_state,
										  SVstring (ent, weaponmodel)));
	stats[STAT_AMMO] = SVfloat (ent, currentammo);
	stats[STAT_ARMOR] = SVfloat (ent, armorvalue);
	stats[STAT_SHELLS] = SVfloat (ent, ammo_shells);
	stats[STAT_NAILS] = SVfloat (ent, ammo_nails);
	stats[STAT_ROCKETS] = SVfloat (ent, ammo_rockets);
	stats[STAT_CELLS] = SVfloat (ent, ammo_cells);
	if (!spectator)
		stats[STAT_ACTIVEWEAPON] = SVfloat (ent, weapon);
	// stuff the sigil bits into the high bits of items for sbar
	stats[STAT_ITEMS] = ((int) SVfloat (ent, items)
						 | ((int) *sv_globals.serverflags << 28));

	// Extensions to the QW 2.40 protocol for Mega2k  --KB
	stats[STAT_VIEWHEIGHT] = (int) SVvector (ent, view_ofs)[2];

	// FIXME: this should become a * key!  --KB
	if (SVfloat (ent, movetype) == MOVETYPE_FLY
		&& !atoi (Info_ValueForKey (svs.info, "playerfly")))
		SVfloat (ent, movetype) = MOVETYPE_WALK;

	stats[STAT_FLYMODE] = (SVfloat (ent, movetype) == MOVETYPE_FLY);
}

/*
	SV_UpdateClientStats

	Performs a delta update of the stats array.  This should be performed only
	when a reliable message can be delivered this frame.
*/
static void
SV_UpdateClientStats (client_t *client)
{
	edict_t    *ent;
	int         i;
	int         stats[MAX_CL_STATS];

	ent = client->edict;

	// if we are a spectator and we are tracking a player, we get his stats
	// so our status bar reflects his
	if (client->spectator && client->spec_track > 0)
		ent = svs.clients[client->spec_track - 1].edict;

	SV_GetStats (ent, client->spectator, stats);

	for (i = 0; i < MAX_CL_STATS; i++)
		if (stats[i] != client->stats[i]) {
			client->stats[i] = stats[i];
			if (stats[i] >= 0 && stats[i] <= 255) {
				MSG_ReliableWrite_Begin (&client->backbuf, svc_updatestat, 3);
				MSG_ReliableWrite_Byte (&client->backbuf, i);
				MSG_ReliableWrite_Byte (&client->backbuf, stats[i]);
			} else {
				MSG_ReliableWrite_Begin (&client->backbuf,
										 svc_updatestatlong, 6);
				MSG_ReliableWrite_Byte (&client->backbuf, i);
				MSG_ReliableWrite_Long (&client->backbuf, stats[i]);
			}
		}
}

static bool
SV_SendClientDatagram (client_t *client)
{
	byte        buf[MAX_DATAGRAM];
	sizebuf_t   msg;

	msg.data = buf;
	msg.maxsize = sizeof (buf);
	msg.cursize = 0;
	msg.allowoverflow = true;
	msg.overflowed = false;

	// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client, &msg);

	if (client->state == cs_server)
		return true;

	// send over all the objects that are in the PVS
	// this will include clients, a packetentities, and
	// possibly a nails update
	SV_WriteEntitiesToClient (&client->delta, &msg);

	// copy the accumulated multicast datagram
	// for this client out to the message
	if (client->datagram.overflowed)
		SV_Printf ("WARNING: datagram overflowed for %s\n", client->name);
	else
		SZ_Write (&msg, client->datagram.data, client->datagram.cursize);
	SZ_Clear (&client->datagram);

	// send deltas over reliable stream
	if (Netchan_CanReliable (&client->netchan))
		SV_UpdateClientStats (client);

	if (msg.overflowed) {
		SV_Printf ("WARNING: msg overflowed for %s\n", client->name);
		SZ_Clear (&msg);
	}
	// send the datagram
	Netchan_Transmit (&client->netchan, msg.cursize, msg.data);

	return true;
}

static void
SV_UpdateToReliableMessages (void)
{
	client_t   *client;
	edict_t    *ent;
	int         i, j;
	sizebuf_t  *dbuf;

	// check for changes to be sent over the reliable streams to all clients
	for (i = 0, host_client = svs.clients; i < MAX_CLIENTS; i++, host_client++) {
		if (host_client->state != cs_spawned && host_client->state != cs_server)
			continue;
		if (host_client->sendinfo) {
			host_client->sendinfo = false;
			SV_FullClientUpdate (host_client, &sv.reliable_datagram);
		}
		if (host_client->old_frags != (int) SVfloat (host_client->edict, frags)) {
			for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
				if (client->state < cs_connected)
					continue;
				MSG_ReliableWrite_Begin (&client->backbuf, svc_updatefrags, 4);
				MSG_ReliableWrite_Byte (&client->backbuf, i);
				MSG_ReliableWrite_Short (&client->backbuf,
										 SVfloat (host_client->edict, frags));
			}

			if (sv.recorders) {
				dbuf = SVR_WriteBegin (dem_all, 0, 4);
				MSG_WriteByte (dbuf, svc_updatefrags);
				MSG_WriteByte (dbuf, i);
				MSG_WriteShort (dbuf, SVfloat (host_client->edict, frags));
			}

			host_client->old_frags = SVfloat (host_client->edict, frags);
		}
		// maxspeed/entgravity changes
		ent = host_client->edict;

		if (sv_fields.gravity != -1
			&& host_client->entgravity != SVfloat (ent, gravity)) {
			host_client->entgravity = SVfloat (ent, gravity);
			if (host_client->state != cs_server) {
				MSG_ReliableWrite_Begin (&host_client->backbuf,
										 svc_entgravity, 5);
				MSG_ReliableWrite_Float (&host_client->backbuf,
										 host_client->entgravity);
			}
			if (sv.recorders) {
				dbuf = SVR_WriteBegin (dem_single, i, 5);
				MSG_WriteByte (dbuf, svc_entgravity);
				MSG_WriteFloat (dbuf, host_client->entgravity);
			}
		}
		if (sv_fields.maxspeed != -1
			&& host_client->maxspeed != SVfloat (ent, maxspeed)) {
			host_client->maxspeed = SVfloat (ent, maxspeed);
			if (host_client->state != cs_server) {
				MSG_ReliableWrite_Begin (&host_client->backbuf,
										 svc_maxspeed, 5);
				MSG_ReliableWrite_Float (&host_client->backbuf,
										 host_client->maxspeed);
			}
			if (sv.recorders) {
				dbuf = SVR_WriteBegin (dem_single, i, 5);
				MSG_WriteByte (dbuf, svc_maxspeed);
				MSG_WriteFloat (dbuf, host_client->maxspeed);
			}
		}
	}

	if (sv.datagram.overflowed)
		SZ_Clear (&sv.datagram);

	// append the broadcast messages to each client messages
	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state < cs_connected)
			continue;				// reliables go to all connected or spawned

		MSG_ReliableCheckBlock (&client->backbuf,
								sv.reliable_datagram.cursize);
		MSG_ReliableWrite_SZ (&client->backbuf, sv.reliable_datagram.data,
								sv.reliable_datagram.cursize);

		if (client->state != cs_spawned)
			continue;					// datagrams go to only spawned
		SZ_Write (&client->datagram, sv.datagram.data, sv.datagram.cursize);
	}

	if (sv.recorders && sv.reliable_datagram.cursize) {
		dbuf = SVR_WriteBegin (dem_all, 0, sv.reliable_datagram.cursize);
		SZ_Write (dbuf, sv.reliable_datagram.data,
				  sv.reliable_datagram.cursize);
	}
	if (sv.recorders)
		SZ_Write (SVR_Datagram (), sv.datagram.data, sv.datagram.cursize);

	SZ_Clear (&sv.reliable_datagram);
	SZ_Clear (&sv.datagram);
}

#if defined(_WIN32) && !defined(__GNUC__)
# pragma optimize( "", off )
#endif

void
SV_SendClientMessages (void)
{
	client_t   *c;
	int         i;

	// update frags, names, etc
	SV_UpdateToReliableMessages ();

	// build individual updates
	for (i = 0, c = svs.clients; i < MAX_CLIENTS; i++, c++) {
		if (c->state != cs_server) {
			if (c->state < cs_zombie)
				continue;

			if (c->drop) {
				SV_DropClient (c);
				c->drop = false;
				continue;
			}

			// check to see if we have a backbuf to stick in the reliable
			if (c->backbuf.num_backbuf)
				MSG_Reliable_Send (&c->backbuf);

			// if the reliable message overflowed, drop the client
			if (c->netchan.message.overflowed) {
				SZ_Clear (&c->netchan.message);
				SZ_Clear (&c->datagram);
				SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", c->name);
				SV_Printf ("WARNING: reliable overflow for %s\n", c->name);
				SV_DropClient (c);
				c->send_message = true;
				c->netchan.cleartime = 0;	// don't choke this message
			}
			// send messages only if the client has sent one
			// and the bandwidth is not choked
			if (!c->send_message)
				continue;
			c->send_message = false;		// try putting this after choke?
			if (!sv.paused && !Netchan_CanPacket (&c->netchan)) {
				c->chokecount++;
				continue;					// bandwidth choke
			}
		}

		if (c->state == cs_spawned || c->state == cs_server)
			SV_SendClientDatagram (c);
		else
			Netchan_Transmit (&c->netchan, 0, NULL);	// just update
														// reliable
	}
}

#if defined(_WIN32) && !defined(__GNUC__)
# pragma optimize( "", on )
#endif

/*
	SV_SendMessagesToAll

	FIXME: does this sequence right?
*/
void
SV_SendMessagesToAll (void)
{
	client_t   *c;
	int         i;

	for (i = 0, c = svs.clients; i < MAX_CLIENTS; i++, c++)
		if (c->state < cs_zombie)	// FIXME: should this send to only active?
			c->send_message = true;

	SV_SendClientMessages ();
}

void
SV_Printf (const char *fmt, ...)
{
	va_list     argptr;

	va_start (argptr, fmt);
	SV_Print (fmt, argptr);
	va_end (argptr);
}

/*
	SV_ClientPrintf

	Sends text across to be displayed if the level passes
*/
void
SV_ClientPrintf (int recorder, client_t *cl, int level, const char *fmt, ...)
{
	char        string[1024];
	va_list     argptr;

	if (level < cl->messagelevel)
		return;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	if (recorder && sv.recorders) {
		sizebuf_t *dbuf = SVR_WriteBegin (dem_single, cl - svs.clients,
										  strlen (string) + 3);
		MSG_WriteByte (dbuf, svc_print);
		MSG_WriteByte (dbuf, level);
		MSG_WriteString (dbuf, string);
	}

	SV_PrintToClient (cl, level, string);
}

/*
	SV_BroadcastPrintf

	Sends text to all active clients
*/
void
SV_BroadcastPrintf (int level, const char *fmt, ...)
{
	char        string[1024];
	client_t   *cl;
	int         i;
	va_list     argptr;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	SV_Printf ("%s", string);			// print to the console

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (level < cl->messagelevel)
			continue;
		if (cl->state < cs_zombie)	//FIXME record to mvd
			continue;

		SV_PrintToClient (cl, level, string);
	}

	if (sv.recorders) {
		sizebuf_t *dbuf = SVR_WriteBegin (dem_all, cl - svs.clients,
										  strlen (string) + 3);
		MSG_WriteByte (dbuf, svc_print);
		MSG_WriteByte (dbuf, level);
		MSG_WriteString (dbuf, string);
	}
}

/*
	SV_BroadcastCommand

	Sends text to all active clients
*/
void
SV_BroadcastCommand (const char *fmt, ...)
{
	char        string[1024];
	va_list     argptr;

	if (!sv.state)
		return;
	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&sv.reliable_datagram, svc_stufftext);
	MSG_WriteString (&sv.reliable_datagram, string);
}
