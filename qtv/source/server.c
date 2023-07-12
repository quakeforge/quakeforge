/*
	server.c

	quakeworld server processing

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2004/02/20

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

#include <stdio.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/idparse.h"
#include "QF/info.h"
#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "qw/protocol.h"

#include "qtv/include/client.h"
#include "qtv/include/connection.h"
#include "qtv/include/qtv.h"
#include "qtv/include/server.h"

int server_count;
static hashtab_t *server_hash;
static server_t *servers;

static const char *
server_get_key (const void *sv, void *unused)
{
	return ((server_t *) sv)->name;
}

static void
server_free (void *_sv, void *unused)
{
	server_t   *sv = (server_t *) _sv;
	server_t  **s;
	int         i;
	client_t   *cl;

	static byte final[] = {qtv_stringcmd, 'd', 'r', 'o', 'p', 0};

	if (sv->connected)
		Netchan_Transmit (&sv->netchan, sizeof (final), final);
	Connection_Del (sv->con);
	Info_Destroy (sv->info);
	free ((char *) sv->name);
	free ((char *) sv->address);
	if (sv->gamedir)
		free ((char *) sv->gamedir);
	if (sv->message)
		free ((char *) sv->message);

	for (s = &servers; *s; s = &(*s)->next) {
		if (*s == sv) {
			*s = sv->next;
			break;
		}
	}
	for (i = 0; i < 256; i++) {
		if (sv->soundlist[i])
			free (sv->soundlist[i]);
		if (sv->modellist[i])
			free (sv->modellist[i]);
	}
	for (cl = sv->clients; cl; cl = cl->next) {
		cl->server = 0;
		cl->connected = 0;
	}
	server_count--;
	free (sv);
}

static void
server_drop (server_t *sv)
{
	Hash_Del (server_hash, sv->name);
	Hash_Free (server_hash, sv);
}

static int
server_compare (const void *a, const void *b)
{
	return strcmp ((*(server_t **) a)->name, (*(server_t **) b)->name);
}

static void
setup_sub_message (qmsg_t *msg, qmsg_t *sub, sizebuf_t *buf, unsigned len)
{
	memset (sub, 0, sizeof (qmsg_t));
	memset (buf, 0, sizeof (sizebuf_t));

	if (len > msg->message->cursize - msg->readcount) {
		len = msg->message->cursize - msg->readcount;
		msg->badread = 1;
	}
	sub->message = buf;
	buf->cursize = buf->maxsize = len;
	buf->data = msg->message->data + msg->readcount;
	msg->readcount += len;
}

static void
setup_player_list (server_t *sv, unsigned mask)
{
	int         i;
	player_t  **p, *cl;

	if (sv->player_mask == mask)
		return;
	sv->player_mask = mask;
	p = &sv->player_list;
	*p = 0;
	for (i = 0, cl = sv->players; i < MAX_SV_PLAYERS; i++, cl++) {
		if (mask & (1 << i)) {
			*p = cl;
			p = &(*p)->next;
		}
	}
	*p = 0;
}

static void
qtv_parse_updates (server_t *sv, qmsg_t *msg, int reliable)
{
	byte        type;
	int         msec;
	sizebuf_t   sub_message_buf;
	qmsg_t      sub_message;

	while (1) {
		msec = MSG_ReadByte (msg);
		//qtv_printf ("msec = %d\n", msec);
		if (msec == -1)
			break;
		type = MSG_ReadByte (msg);
		switch (type & 7) {
			case dem_cmd:
			case dem_set:
				qtv_printf ("qtv_parse_updates: unexpected dem: %d\n",
							type & 7);
				return;
			case dem_read:
				break;
			case dem_multiple:
				setup_player_list (sv, MSG_ReadLong (msg));
				break;
			case dem_stats:
			case dem_single:
				setup_player_list (sv, 1 << (type >> 3));
				break;
			case dem_all:
				setup_player_list (sv, ~0);
				break;
		}

		setup_sub_message (msg, &sub_message, &sub_message_buf,
						   MSG_ReadLong (msg));
		sv_parse (sv, &sub_message, reliable);
	}
}

static void
save_signon (server_t *sv, qmsg_t *msg, int start)
{
	int         size = msg->readcount - start;
	byte       *buf = msg->message->data + start;

	if (!size)
		return;

	if (sv->num_signon_buffers >= MAX_SIGNON_BUFFERS)
		Sys_Error ("too many signon buffers: %d\n", sv->num_signon_buffers);
	sv->signon_buffer_size[sv->num_signon_buffers] = size;
	memcpy (sv->signon_buffers[sv->num_signon_buffers], buf, size);
	sv->num_signon_buffers++;
}

static void
qtv_packet_f (server_t *sv)
{
	int         len, type;
	int         reliable = 0;
	sizebuf_t   sub_message_buf;
	qmsg_t      sub_message;
	int         start = -1;

	len = MSG_ReadShort (net_message);
	type = len & 0xf000;
	len &= 0x0fff;
	setup_sub_message (net_message, &sub_message, &sub_message_buf, len);
	if (0) {
		static const char *qtv_types[16] = {
			"qtv_p_signon",
			"qtv_p_print",
			"qtv_p_reliable",
			"qtv_p_unreliable",
			"[4]", "[5]", "[6]", "[7]",
			"[8]", "[9]", "[a]", "[b]",
			"[c]", "[d]", "[e]", "[f]",
		};
		qtv_printf ("qtv_packet: %s %d\n", qtv_types[type >> 12], len);
	}
	switch (type) {
		case qtv_p_signon:
			if (sv->signon)
				start = sub_message.readcount;
			setup_player_list (sv, ~0);
			sv_parse (sv, &sub_message, 1);
			if (start >= 0)
				save_signon (sv, &sub_message, start);
			break;
		case qtv_p_print:
			MSG_ReadByte (&sub_message);
			qtv_printf ("%s\n", MSG_ReadString (&sub_message));
			break;
		case qtv_p_reliable:
			reliable = 1;
		case qtv_p_unreliable:
			qtv_parse_updates (sv, &sub_message, reliable);
			break;
		default:
			qtv_printf ("unknown packet type %x (%d bytes)\n", type, len);
			break;
	}
}

static void
server_handler (connection_t *con, void *object)
{
	server_t   *sv = (server_t *) object;

	if (*(int *) net_message->message->data == -1)
		return;
	if (!Netchan_Process (&sv->netchan))
		return;
	if (0) {
		unsigned    i;

		for (i = 0; i < net_message->message->cursize; i++)
			qtv_printf ("%c%02x", (i % 16) ? ' ' : '\n',
						net_message->message->data[i]);
		qtv_printf ("\n");
	}
	while (1) {
		int         cmd;
		if (net_message->badread) {
			qtv_printf ("badread\n");
			break;
		}
		cmd = MSG_ReadByte (net_message);
		if (cmd == -1) {
			//qtv_printf ("cmd = -1\n");
			net_message->readcount++;
			break;
		}
		//qtv_printf ("cmd: %d\n", cmd);
		switch (cmd) {
			default:
				qtv_printf ("Illegible server message: %d [%d]\n", cmd,
							net_message->readcount);
				while ((cmd = MSG_ReadByte (net_message)) != -1) {
					qtv_printf ("%02x (%c) ", cmd, cmd ? sys_char_map[cmd]
													   : '#');
				}
				qtv_printf ("\n");
				goto bail;
			case qtv_disconnect:
				qtv_printf ("%s: disconnected\n", sv->name);
				server_drop (sv);
				break;
			case qtv_stringcmd:
				sv_stringcmd (sv, net_message);
				break;
			case qtv_packet:
				qtv_packet_f (sv);
				break;
		}
	}
bail:
	return;
}

static inline const char *
expect_packet (qmsg_t *msg, int type)
{
	const char *str;
	int         seq;

	MSG_BeginReading (net_message);
	seq = MSG_ReadLong (net_message);
	if (seq != -1) {
		qtv_printf ("unexpected connected packet\n");
		return 0;
	}
	str = MSG_ReadString (net_message);
	if (str[0] == A2C_PRINT) {
		qtv_printf ("%s", str + 1);
		return 0;
	}
	if (str[0] != type) {
		qtv_printf ("unexpected connectionless packet type: %s\n", str);
		return 0;
	}
	return str;
}

static void
server_connect (connection_t *con, void *object)
{
	server_t   *sv = (server_t *) object;
	sizebuf_t  *msg = &sv->netchan.message;
	const char *str;

	if (!(str = expect_packet (net_message, S2C_CONNECTION)))
		return;

	qtv_printf ("connection from %s\n", sv->name);
	Netchan_Setup (&sv->netchan, con->address, sv->qport, NC_QPORT_SEND);
	sv->netchan.outgoing_sequence = 1;
	sv->connected = 1;
	sv->playermodel = -1;
	MSG_WriteByte (msg, qtv_stringcmd);
	MSG_WriteString (msg, "new");
	sv->next_run = realtime;
	con->handler = server_handler;
}

static void
server_challenge (connection_t *con, void *object)
{
	server_t   *sv = (server_t *) object;
	const char *str, *qtv = 0;
	int         challenge, i;
	dstring_t  *data;

	if (!(str = expect_packet (net_message, S2C_CHALLENGE)))
		return;

	COM_TokenizeString (str + 1, qtv_args);
	cmd_args = qtv_args;
	challenge = atoi (Cmd_Argv (0));

	for (i = 1; i < Cmd_Argc (); i++) {
		str = Cmd_Argv (i);
		if (!strcmp ("QF", str)) {
			qtv_printf ("QuakeForge server detected\n");
		} else if (!strcmp ("qtv", str)) {
			qtv_printf ("QTV capable server\n");
			qtv = str;
		} else {
			qtv_printf ("%s\n", str);
		}
	}

	if (!qtv) {
		qtv_printf ("%s can't handle qtv.\n", sv->name);
		server_drop (sv);
		return;
	}


	data = dstring_new ();
	dsprintf (data, "%c%c%c%cconnect %s %i %i \"%s\"\n", 255, 255, 255, 255,
			  qtv, sv->qport, challenge,
			  Info_MakeString (sv->info, 0));
	Netchan_SendPacket (strlen (data->str), data->str, net_from);
	dstring_delete (data);
	con->handler = server_connect;
}

static void
server_getchallenge (connection_t *con, server_t *sv)
{
	static const char *getchallenge = "\377\377\377\377getchallenge\n";

	Netchan_SendPacket (strlen (getchallenge), getchallenge, sv->adr);
}

static void
sv_new_f (void)
{
	const char *name;
	const char *address;
	server_t   *sv;
	netadr_t    adr;

	if (Cmd_Argc () < 2 || Cmd_Argc () > 3) {
		qtv_printf ("Usage: sv_new <name> <address>\n");
		return;
	}
	address = name = Cmd_Argv (1);
	if (Hash_Find (server_hash, name)) {
		qtv_printf ("sv_new: %s already exists\n", name);
		return;
	}
	if (Cmd_Argc () == 3)
		address = Cmd_Argv (2);
	if (!NET_StringToAdr (address, &adr)) {
		qtv_printf ("Bad server address\n");
		return;
	}
	if (!adr.port)
		adr.port = BigShort (27500);

	sv = calloc (1, sizeof (server_t));
	server_count++;
	sv->next = servers;
	servers = sv;
	sv->name = strdup (name);
	sv->address = strdup (address);
	sv->adr = adr;
	sv->qport = qport;
	sv->info = Info_ParseString ("", MAX_INFO_STRING, 0);
	Info_SetValueForStarKey (sv->info, "*ver",
							 va (0, "%s QTV %s", QW_VERSION, PACKAGE_VERSION),
							 0);
	Info_SetValueForStarKey (sv->info, "*qsg_version", QW_QSG_VERSION, 0);
	Info_SetValueForKey (sv->info, "name", "QTV Proxy", 0);
	Hash_Add (server_hash, sv);

	sv->con = Connection_Add (&adr, sv, server_challenge);
	Netchan_Setup (&sv->netchan, sv->con->address, sv->qport, NC_QPORT_SEND);

	server_getchallenge (sv->con, sv);
}

static void
sv_del_f (void)
{
	const char *name;
	server_t   *sv;

	if (Cmd_Argc () != 2) {
		qtv_printf ("Usage: sv_del <name>\n");
		return;
	}
	name = Cmd_Argv (1);
	if (!(sv = Hash_Del (server_hash, name))) {
		qtv_printf ("sv_del: %s unknown\n", name);
		return;
	}
	Hash_Free (server_hash, sv);
}

static void
sv_list_f (void)
{
	server_t  **list, **l, *sv;
	int         count;

	for (l = &servers, count = 0; *l; l = &(*l)->next)
		count++;
	if (!count) {
		qtv_printf ("no servers\n");
		return;
	}
	list = malloc (count * sizeof (server_t *));
	for (l = &servers, count = 0; *l; l = &(*l)->next, count++)
		list[count] = *l;
	qsort (list, count, sizeof (*list), server_compare);
	qtv_printf ("Name                 Address\n");
	qtv_printf ("-------------------- --------------------\n");
	for (l = list; count--; l++) {
		sv = *l;
		qtv_printf ("%-20s %s(%s)\n", sv->name, sv->address,
					NET_AdrToString (sv->adr));
	}
	free (list);
}

static void
server_shutdown (void *data)
{
	Hash_FlushTable (server_hash);
	Hash_DelTable (server_hash);
}

static void
server_run (server_t *sv)
{
	static byte delta_msg[2] = {qtv_delta};
	static byte nop_msg[1] = {qtv_nop};

	sv->next_run = realtime + 0.03;
	if (sv->connected > 1) {
		int         frame = (sv->netchan.outgoing_sequence) & UPDATE_MASK;
		sv->frames[frame].delta_sequence = sv->delta;
		if (sv->validsequence && sv->delta != -1) {
			delta_msg[1] = sv->delta;
			Netchan_Transmit (&sv->netchan, sizeof (delta_msg), delta_msg);
			return;
		} else if (!sv->netchan.message.cursize) {
			Netchan_Transmit (&sv->netchan, sizeof (nop_msg), nop_msg);
			return;
		}
	}
	if (sv->netchan.message.cursize)
		Netchan_Transmit (&sv->netchan, 0, 0);
}

void
Server_Init (void)
{
	Sys_RegisterShutdown (server_shutdown, 0);
	server_hash = Hash_NewTable (61, server_get_key, server_free, 0, 0);
	Cmd_AddCommand ("sv_new", sv_new_f, "Add a new server");
	Cmd_AddCommand ("sv_del", sv_del_f, "Remove an existing server");
	Cmd_AddCommand ("sv_list", sv_list_f, "List available servers");
}

void
Server_Frame (void)
{
	server_t   *sv;

	for (sv = servers; sv; sv = sv->next) {
		if (realtime - sv->netchan.last_received > sv_timeout) {
			qtv_printf ("Server %s timed out\n", sv->name);
			server_drop (sv);
			return; // chain has changed, avoid segfaulting
		}
		if (sv->next_run && sv->next_run <= realtime) {
			sv->next_run = 0;
			server_run (sv);
		}
	}
}

void
Server_List (void)
{
	sv_list_f ();
}

void
Server_Connect (const char *name, struct client_s *client)
{
	server_t   *sv;

	if (!(sv = Hash_Find (server_hash, name))) {
		qtv_printf ("server not found: %s\n", name);
		return;
	}
	client->server = sv;

	client->prev = &sv->clients;
	client->next = sv->clients;
	if (sv->clients)
		sv->clients->prev = &client->next;
	sv->clients = client;

	Client_New (client);
}

void
Server_Disconnect (struct client_s *client)
{
	client->server = 0;
	client->connected = 0;
	if (client->next)
		client->next->prev = client->prev;
	*client->prev = client->next;
}

void
Server_Broadcast (server_t *sv, int reliable, int all, const byte *msg,
				  int len)
{
	client_t   *cl;
	byte        svc;

	if (len < 1)
		return;
	svc = *msg++;
	len--;
	for (cl = sv->clients; cl; cl = cl->next) {
		if (!all && (sv->player_mask != ~0u)
			&& !(sv->player_mask & cl->spec_track))
			continue;
		if (reliable) {
			MSG_ReliableWrite_Begin (&cl->backbuf, svc, len + 1);
			MSG_ReliableWrite_SZ (&cl->backbuf, msg, len);
		} else {
			SZ_Write (&cl->datagram, msg - 1, len + 1);
		}
	}
}

void
Server_BroadcastCommand (server_t *sv, const char *cmd)
{
	const char *msg = va (0, "%c%s", svc_stufftext, cmd);
	int         len = strlen (msg) + 1;
	Server_Broadcast (sv, 1, 1, (const byte *) msg, len);
}
