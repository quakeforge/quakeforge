/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2004 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include <stdio.h>
#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/idparse.h"
#include "QF/info.h"

#include "qw/msg_ucmd.h"
#include "qw/protocol.h"

#include "client.h"
#include "connection.h"
#include "qtv.h"

typedef struct ucmd_s {
	const char *name;
	void        (*func) (client_t *cl, void *userdata);
	unsigned    no_redirect:1;
	unsigned    overridable:1;
	unsigned    freeable:1;
	void        *userdata;
	void        (*on_free) (void *userdata);
} ucmd_t;

static void
client_drop (client_t *cl)
{
	MSG_WriteByte (&cl->netchan.message, svc_disconnect);
	Con_Printf ("Client %s removed\n", Info_ValueForKey (cl->userinfo, "name"));
	cl->drop = 1;
}

static void
cl_new_f (client_t *cl, void *unused)
{
	qtv_printf ("\"cmd list\" for a list of servers\n");
	qtv_printf ("\"cmd connect <servername>\" to connect to a server\n");
}

static void
cl_modellist_f (client_t *cl, void *unused)
{
}

static void
cl_soundlist_f (client_t *cl, void *unused)
{
}

static void
cl_prespawn_f (client_t *cl, void *unused)
{
}

static void
cl_spawn_f (client_t *cl, void *unused)
{
}

static void
cl_begin_f (client_t *cl, void *unused)
{
}

static void
cl_drop_f (client_t *cl, void *unused)
{
	client_drop (cl);
}

static void
cl_pings_f (client_t *cl, void *unused)
{
}

static void
cl_rate_f (client_t *cl, void *unused)
{
}

static void
cl_say_f (client_t *cl, void *unused)
{
}

static void
cl_setinfo_f (client_t *cl, void *unused)
{
}

static void
cl_serverinfo_f (client_t *cl, void *unused)
{
}

static void
cl_download_f (client_t *cl, void *unused)
{
}

static void
cl_nextdl_f (client_t *cl, void *unused)
{
}

static void
cl_ptrack_f (client_t *cl, void *unused)
{
}

static ucmd_t ucmds[] = {
	{"new",			cl_new_f,			0, 0},
	{"modellist",	cl_modellist_f,		0, 0},
	{"soundlist",	cl_soundlist_f,		0, 0},
	{"prespawn",	cl_prespawn_f,		0, 0},
	{"spawn",		cl_spawn_f,			0, 0},
	{"begin",		cl_begin_f,			1, 0},

	{"drop",		cl_drop_f,			0, 0},
	{"pings",		cl_pings_f,			0, 0},

// issued by hand at client consoles    
	{"rate",		cl_rate_f,			0, 0},
	{"kill",		0,					1, 1},
	{"pause",		0,					1, 0},
	{"msg",			0,					0, 0},

	{"say",			cl_say_f,			1, 1},
	{"say_team",	cl_say_f,			1, 1},

	{"setinfo",		cl_setinfo_f,		1, 0},

	{"serverinfo",	cl_serverinfo_f,	0, 0},

	{"download",	cl_download_f,		1, 0},
	{"nextdl",		cl_nextdl_f,		0, 0},

	{"ptrack",		cl_ptrack_f,		0, 1},		// ZOID - used with autocam

	{"snap",		0,					0, 0},
};

static hashtab_t *ucmd_table;
int (*ucmd_unknown)(void);

static void
client_exec_command (client_t *cl, const char *s)
{
	ucmd_t     *u;

	COM_TokenizeString (s, qtv_args);
	cmd_args = qtv_args;

	u = (ucmd_t*) Hash_Find (ucmd_table, qtv_args->argv[0]->str);

	if (!u) {
		if (ucmd_unknown && !ucmd_unknown ()) {
			qtv_begin_redirect (RD_CLIENT, cl);
			qtv_printf ("Bad user command: %s\n", qtv_args->argv[0]->str);
			qtv_end_redirect ();
		}
	} else {
		if (u->func) {
			if (!u->no_redirect)
				qtv_begin_redirect (RD_CLIENT, cl);
			u->func (cl, u->userdata);
			if (!u->no_redirect)
				qtv_end_redirect ();
		}
	}
}

static void
client_parse_message (client_t *cl)
{
	int         c, size;
	vec3_t      o;
	const char *s;
	usercmd_t   oldest, oldcmd, newcmd;

	while (1) {
		if (net_message->badread) {
			Con_Printf ("SV_ReadClientMessage: badread\n");
			client_drop (cl);
			return;
		}

		c = MSG_ReadByte (net_message);
		if (c == -1)
			return;

		switch (c) {
			default:
				Con_Printf ("SV_ReadClientMessage: unknown command char\n");
				client_drop (cl);
				return;
			case clc_nop:
				break;
			case clc_delta:
				/*cl->delta_sequence = */MSG_ReadByte (net_message);
				break;
			case clc_move:
				/*if (move_issued)
					return;				// someone is trying to cheat...
				move_issued = true;*/
				/*checksumIndex = */MSG_GetReadCount (net_message);
				/*checksum = (byte) */MSG_ReadByte (net_message);
				// read loss percentage
				/*cl->lossage = */MSG_ReadByte (net_message);
				MSG_ReadDeltaUsercmd (&nullcmd, &oldest);
				MSG_ReadDeltaUsercmd (&oldest, &oldcmd);
				MSG_ReadDeltaUsercmd (&oldcmd, &newcmd);
#if 0
				if (cl->state != cs_spawned)
					break;
				// if the checksum fails, ignore the rest of the packet
				calculatedChecksum =
					COM_BlockSequenceCRCByte (net_message->message->data +
											  checksumIndex + 1,
											  MSG_GetReadCount (net_message) -
											  checksumIndex - 1, seq_hash);
				if (calculatedChecksum != checksum) {
					Con_DPrintf
						("Failed command checksum for %s(%d) (%d != %d)\n",
						 cl->name, cl->netchan.incoming_sequence, checksum,
						 calculatedChecksum);
					return;
				}
				if (!sv.paused) {
					SV_PreRunCmd ();
					if (net_drop < 20) {
						while (net_drop > 2) {
							SV_RunCmd (&cl->lastcmd, 0);
							net_drop--;
						}
						if (net_drop > 1)
							SV_RunCmd (&oldest, 0);
						if (net_drop > 0)
							SV_RunCmd (&oldcmd, 0);
					}
					SV_RunCmd (&newcmd, 0);
					SV_PostRunCmd ();
				}
				cl->lastcmd = newcmd;
				cl->lastcmd.buttons = 0;	// avoid multiple fires on lag
#endif
				break;
			case clc_stringcmd:
				s = MSG_ReadString (net_message);
				client_exec_command (cl, s);
				break;
			case clc_tmove:
				MSG_ReadCoordV (net_message, o);
#if 0
				// only allowed by spectators
				if (host_client->spectator) {
					VectorCopy (o, SVvector (sv_player, origin));
					SV_LinkEdict (sv_player, false);
				}
#endif
				break;
			case clc_upload:
				size = MSG_ReadShort (net_message);
				MSG_ReadByte (net_message);
				net_message->readcount += size;
				break;
		}
	}
}

static void
client_handler (connection_t *con, void *object)
{
	client_t   *cl = (client_t *) object;

	if (net_message->message->cursize < 11) {
		Con_Printf ("%s: Runt packet\n", NET_AdrToString (net_from));
		return;
	}
#if 0
	// read the qport out of the message so we can fix up
	// stupid address translating routers
	MSG_ReadLong (net_message);				// sequence number
	MSG_ReadLong (net_message);				// sequence number
	qport = MSG_ReadShort (net_message) & 0xffff;
	if (cl->netchan.qport != qport)
		return;
#endif
	if (Netchan_Process (&cl->netchan)) {
		// this is a valid, sequenced packet, so process it
		//svs.stats.packets++;
		//cl->send_message = true;
		//if (cl->state != cs_zombie)
			client_parse_message (cl);
		if (cl->backbuf.num_backbuf)
			MSG_Reliable_Send (&cl->backbuf);
		Netchan_Transmit (&cl->netchan, 0, NULL);
		if (cl->drop) {
			Connection_Del (cl->con);
			Info_Destroy (cl->userinfo);
			free (cl);
		}
	}
}

static void
client_connect (connection_t *con, void *object)
{
	challenge_t *ch = (challenge_t *) object;
	client_t   *cl;
	const char *str;
	info_t     *userinfo;
	int         version, qport, challenge, seq;

	MSG_BeginReading (net_message);
	seq = MSG_ReadLong (net_message);
	if (seq != -1) {
		Con_Printf ("unexpected connected packet\n");
		return;
	}
	str = MSG_ReadString (net_message);
	COM_TokenizeString (str, qtv_args);
	cmd_args = qtv_args;
	if (strcmp (Cmd_Argv (0), "connect")) {
		Con_Printf ("unexpected connected packet\n");
		return;
	}
	version = atoi (Cmd_Argv (1));
	if (version != PROTOCOL_VERSION) {
		Netchan_OutOfBandPrint (net_from, "%c\nServer is version %s.\n",
								A2C_PRINT, QW_VERSION);
		Con_Printf ("* rejected connect from version %i\n", version);
		return;
	}
	qport = atoi (Cmd_Argv (2));
	challenge = atoi (Cmd_Argv (3));
	if (!(con = Connection_Find (&net_from))
		|| (ch = con->object)->challenge != challenge) {
		Netchan_OutOfBandPrint (net_from, "%c\nBad challenge.\n",
								A2C_PRINT);
		return;
	}
	free (con->object);
	userinfo = Info_ParseString (Cmd_Argv (4), 0, 0);
	if (!userinfo) {
		Netchan_OutOfBandPrint (net_from, "%c\nInvalid userinfo string.\n",
								A2C_PRINT);
		return;
	}

	cl = calloc (1, sizeof (client_t));
	Netchan_Setup (&cl->netchan, con->address, qport, NC_READ_QPORT);
	cl->backbuf.netchan = &cl->netchan;
	cl->backbuf.name = "FIXME";
	cl->userinfo = userinfo;
	cl->con = con;
	con->object = cl;
	con->handler = client_handler;

	Con_Printf ("client %s (%s) connected\n",
				Info_ValueForKey (userinfo, "name"),
				NET_AdrToString (con->address));

	Netchan_OutOfBandPrint (net_from, "%c", S2C_CONNECTION);
}

void
Client_NewConnection (void)
{
	challenge_t *ch;
	connection_t *con;

	if ((con = Connection_Find (&net_from))) {
		if (con->handler == client_handler)
			return;
		ch = con->object;
	} else {
		ch = malloc (sizeof (challenge_t));
	}
	ch->challenge = (rand () << 16) ^ rand ();
	ch->time = realtime;
	if (!con)
		con = Connection_Add (&net_from, ch, 0);
	Netchan_OutOfBandPrint (net_from, "%c%i QF", S2C_CHALLENGE, ch->challenge);
	con->handler = client_connect;
}

static const char *
ucmds_getkey (void *_a, void *unused)
{
	ucmd_t *a = (ucmd_t*)_a;
	return a->name;
}

void
Client_Init (void)
{
	size_t      i;

	ucmd_table = Hash_NewTable (251, ucmds_getkey, 0, 0);
	for (i = 0; i < sizeof (ucmds) / sizeof (ucmds[0]); i++)
		Hash_Add (ucmd_table, &ucmds[i]);
}
