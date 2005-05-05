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

#include "connection.h"
#include "qtv.h"
#include "server.h"

static hashtab_t *server_hash;
static server_t *servers;

static const char *
server_get_key (void *sv, void *unused)
{
	return ((server_t *) sv)->name;
}

static void
server_free (void *_sv, void *unused)
{
	server_t   *sv = (server_t *) _sv;
	server_t  **s;

	static byte final[] = {qtv_stringcmd, 'd', 'r', 'o', 'p', 0};

	if (sv->connected)
		Netchan_Transmit (&sv->netchan, sizeof (final), final);
	Connection_Del (sv->con);

	for (s = &servers; *s; s = &(*s)->next) {
		if (*s == sv) {
			*s = sv->next;
			break;
		}
	}
}

static int
server_compare (const void *a, const void *b)
{
	return strcmp ((*(server_t **) a)->name, (*(server_t **) b)->name);
}

static void
qtv_serverdata (server_t *sv)
{
	const char *str;

	sv->ver = MSG_ReadLong (net_message);
	sv->spawncount = MSG_ReadLong (net_message);
	sv->gamedir = strdup (MSG_ReadString (net_message));

	sv->message = strdup (MSG_ReadString (net_message));
	sv->movevars.gravity = MSG_ReadFloat (net_message);
	sv->movevars.stopspeed = MSG_ReadFloat (net_message);
	sv->movevars.maxspeed = MSG_ReadFloat (net_message);
	sv->movevars.spectatormaxspeed = MSG_ReadFloat (net_message);
	sv->movevars.accelerate = MSG_ReadFloat (net_message);
	sv->movevars.airaccelerate = MSG_ReadFloat (net_message);
	sv->movevars.wateraccelerate = MSG_ReadFloat (net_message);
	sv->movevars.friction = MSG_ReadFloat (net_message);
	sv->movevars.waterfriction = MSG_ReadFloat (net_message);
	sv->movevars.entgravity = MSG_ReadFloat (net_message);

	sv->cdtrack = MSG_ReadByte (net_message);
	sv->sounds = MSG_ReadByte (net_message);

	COM_TokenizeString (MSG_ReadString (net_message), qtv_args);
	cmd_args = qtv_args;
	sv->info = Info_ParseString (Cmd_Argv (1), MAX_SERVERINFO_STRING, 0);

	str = Info_ValueForKey (sv->info, "hostname");
	if (strcmp (str, "unnamed"))
		qtv_printf ("%s: %s\n", sv->name, str);
	str = Info_ValueForKey (sv->info, "*version");
	qtv_printf ("%s: QW %s\n", sv->name, str);
	str = Info_ValueForKey (sv->info, "*qf_version");
	if (str[0])
		qtv_printf ("%s: QuakeForge %s\n", sv->name, str);
	qtv_printf ("%s: gamedir: %s\n", sv->name, sv->gamedir);
	str = Info_ValueForKey (sv->info, "map");
	qtv_printf ("%s: (%s) %s\n", sv->name, str, sv->message);

	MSG_WriteByte (&sv->netchan.message, qtv_stringcmd);
	MSG_WriteString (&sv->netchan.message,
					 va ("soundlist %i %i", sv->spawncount, 0));
	sv->next_run = realtime;
}

static void
qtv_soundlist (server_t *sv)
{
	int         numsounds = MSG_ReadByte (net_message);
	int         n;
	const char *str;

	for (;;) {
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		//qtv_printf ("%s\n", str);
		numsounds++;
		if (numsounds == MAX_SOUNDS) {
			while (str[0])
				str = MSG_ReadString (net_message);
			MSG_ReadByte (net_message);
			return;
		}
		// save sound name
	}
	n = MSG_ReadByte (net_message);
	if (n) {
		MSG_WriteByte (&sv->netchan.message, qtv_stringcmd);
		MSG_WriteString (&sv->netchan.message,
						 va ("soundlist %d %d", sv->spawncount, n));
	} else {
		MSG_WriteByte (&sv->netchan.message, qtv_stringcmd);
		MSG_WriteString (&sv->netchan.message,
						 va ("modellist %d %d", sv->spawncount, 0));
	}
	sv->next_run = realtime;
}

static void
qtv_modellist (server_t *sv)
{
	int         nummodels = MSG_ReadByte (net_message);
	int         n;
	const char *str;

	for (;;) {
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		//qtv_printf ("%s\n", str);
		nummodels++;
		if (nummodels == MAX_SOUNDS) {
			while (str[0])
				str = MSG_ReadString (net_message);
			MSG_ReadByte (net_message);
			return;
		}
		// save sound name
	}
	n = MSG_ReadByte (net_message);
	if (n) {
		MSG_WriteByte (&sv->netchan.message, qtv_stringcmd);
		MSG_WriteString (&sv->netchan.message,
						 va ("modellist %d %d", sv->spawncount, n));
	} else {
		MSG_WriteByte (&sv->netchan.message, qtv_stringcmd);
		MSG_WriteString (&sv->netchan.message,
						 va ("prespawn %d 0 0", sv->spawncount));
	}
	sv->next_run = realtime;
}

static void
qtv_cmd_f (server_t *sv)
{
	if (Cmd_Argc () > 1) {
		MSG_WriteByte (&sv->netchan.message, qtv_stringcmd);
		SZ_Print (&sv->netchan.message, Cmd_Args (1));
	}
	sv->next_run = realtime;
}

static void
qtv_skins_f (server_t *sv)
{
	// we don't actually bother checking skins here, but this is a good way
	// to get everything ready at the last miniute before we start getting
	// actual in-game update messages
	MSG_WriteByte (&sv->netchan.message, qtv_stringcmd);
	MSG_WriteString (&sv->netchan.message, va ("begin %d", sv->spawncount));
	sv->next_run = realtime;
	sv->connected = 2;
	sv->delta = -1;
}

typedef struct {
	const char *name;
	void      (*func) (server_t *sv);
} svcmd_t;

svcmd_t svcmds[] = {
	{"cmd",			qtv_cmd_f},
	{"skins",		qtv_skins_f},

	{0,				0},
};

static void
qtv_stringcmd_f (server_t *sv)
{
	svcmd_t    *c;
	const char *name;

	COM_TokenizeString (MSG_ReadString (net_message), qtv_args);
	cmd_args = qtv_args;
	name = Cmd_Argv (0);

	for (c = svcmds; c->name; c++)
		if (strcmp (c->name, name) == 0)
			break;
	if (!c->name) {
		qtv_printf ("Bad QTV command: %s\n", name);
		return;
	}
	c->func (sv);
}

static void
qtv_p_signon_f (server_t *sv, int len)
{
	int         c, start;
	vec3_t      v, a;

	start = net_message->readcount;
	while (net_message->readcount - start < len) {
		c = MSG_ReadByte (net_message);
		if (c == -1)
			break;
		//qtv_printf ("svc: %d\n", c);
		switch (c) {
			case svc_serverdata:
				qtv_serverdata (sv);
				break;
			case svc_stufftext:
				qtv_stringcmd_f (sv);
				break;
			case svc_soundlist:
				qtv_soundlist (sv);
				break;
			case svc_modellist:
				qtv_modellist (sv);
				break;
			case svc_spawnstaticsound:
				MSG_ReadCoordV (net_message, v);
				MSG_ReadByte (net_message);
				MSG_ReadByte (net_message);
				MSG_ReadByte (net_message);
				break;
			case svc_spawnbaseline:
				MSG_ReadShort (net_message);
			case svc_spawnstatic:
				MSG_ReadByte (net_message);
				MSG_ReadByte (net_message);
				MSG_ReadByte (net_message);
				MSG_ReadByte (net_message);
				MSG_ReadCoordAngleV (net_message, v, a);
				break;
			case svc_lightstyle:
				MSG_ReadByte (net_message);
				MSG_ReadString (net_message);
				break;
			case svc_updatestatlong:
				MSG_ReadByte (net_message);
				MSG_ReadLong (net_message);
				break;
			case svc_updatestat:
				MSG_ReadByte (net_message);
				MSG_ReadByte (net_message);
				break;
			default:
				qtv_printf ("unknown svc: %d\n", c);
				return;
		}
	}
}

static void
qtv_parse_delta (server_t *sv, qmsg_t *msg, int bits)
{
	bits &= ~511;

	if (bits & U_MOREBITS)
		bits |= MSG_ReadByte (msg);
	if (bits & U_EXTEND1) {
		bits |= MSG_ReadByte (msg) << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte (msg) << 24;
	}
	if (bits & U_MODEL)
		MSG_ReadByte (msg);
	if (bits & U_FRAME)
		MSG_ReadByte (msg);
	if (bits & U_COLORMAP)
		MSG_ReadByte (msg);
	if (bits & U_SKIN)
		MSG_ReadByte (msg);
	if (bits & U_EFFECTS)
		MSG_ReadByte (msg);
	if (bits & U_ORIGIN1)
		MSG_ReadCoord (msg);
	if (bits & U_ANGLE1)
		MSG_ReadAngle (msg);
	if (bits & U_ORIGIN2)
		MSG_ReadCoord (msg);
	if (bits & U_ANGLE2)
		MSG_ReadAngle (msg);
	if (bits & U_ORIGIN3)
		MSG_ReadCoord (msg);
	if (bits & U_ANGLE3)
		MSG_ReadAngle (msg);
	if (bits & U_SOLID) {
		// FIXME
	}
	if (!(bits & U_EXTEND1))
		return;
	if (bits & U_ALPHA)
		MSG_ReadByte (msg);
	if (bits & U_SCALE)
		MSG_ReadByte (msg);
	if (bits & U_EFFECTS2)
		MSG_ReadByte (msg);
	if (bits & U_GLOWSIZE)
		MSG_ReadByte (msg);
	if (bits & U_GLOWCOLOR)
		MSG_ReadByte (msg);
	if (bits & U_COLORMOD)
		MSG_ReadByte (msg);
	if (!(bits & U_EXTEND1))
		return;
	if (bits & U_FRAME2)
		MSG_ReadByte (msg);
}

static void
qtv_packetentities (server_t *sv, qmsg_t *msg, int delta)
{
	unsigned short word;
	int         newnum, oldnum;

	newnum = oldnum = 0;
	if (delta) {
		/*from =*/ MSG_ReadByte (msg);
	} else {
	}
	sv->delta = sv->netchan.incoming_sequence;
	while (1) {
		word = (unsigned short) MSG_ReadShort (msg);
		if (msg->badread) {     // something didn't parse right...
			qtv_printf ("msg_badread in packetentities\n");
			return;
		}
		if (!word) {
			// copy rest of ents from old packet
			break;
		}
		if (newnum < oldnum) {
			if (word & U_REMOVE) {
				continue;
			}
			qtv_parse_delta (sv, msg, word);
			continue;
		}
		if (newnum == oldnum) {
			if (word & U_REMOVE) {
				continue;
			}
			qtv_parse_delta (sv, msg, word);
			continue;
		}
	}
}

static void
qtv_parse_updates (server_t *sv, int reliable, int len)
{
	int         c, msec, type, to, start;
	sizebuf_t   sub_message_buf;
	qmsg_t      sub_message;
	vec3_t      v;

	memset (&sub_message_buf, 0, sizeof (sub_message_buf));
	memset (&sub_message, 0, sizeof (sub_message));
	sub_message.message = &sub_message_buf;

	start = net_message->readcount;
	while (net_message->readcount - start < len) {
		msec = MSG_ReadByte (net_message);
		type = MSG_ReadByte (net_message);
		if (type == dem_multiple) {
			to = MSG_ReadLong (net_message);
		} else if (type == dem_single) {
			to = MSG_ReadByte (net_message);
		}

		sub_message_buf.cursize = MSG_ReadLong (net_message);
		sub_message_buf.maxsize = sub_message_buf.cursize;
		sub_message_buf.data = net_message->message->data;
		sub_message_buf.data += net_message->readcount;
		sub_message.readcount = 0;
		sub_message.badread = 0;
		net_message->readcount += sub_message_buf.cursize;

		while (1) {
			c = MSG_ReadByte (&sub_message);
			if (c == -1)
				break;
			//qtv_printf ("qtv_parse_updates: svc: %d\n", c);
			switch (c) {
				default:
					qtv_printf ("qtv_parse_updates: unknown svc: %d\n", c);
					return;
				case svc_nop:
					break;
				case svc_updatestat:
					MSG_ReadByte (&sub_message);
					MSG_ReadByte (&sub_message);
					break;
				case svc_setview:
					break;
				case svc_sound:
					c = MSG_ReadShort (&sub_message);
					if (c & SND_VOLUME)
						MSG_ReadByte (&sub_message);
					if (c & SND_ATTENUATION)
						MSG_ReadByte (&sub_message);
					MSG_ReadByte (&sub_message);
					MSG_ReadCoordV (&sub_message, v);
					break;
				case svc_print:
					MSG_ReadByte (&sub_message);
					MSG_ReadString (&sub_message);
					break;
				case svc_stufftext:
					MSG_ReadString (&sub_message);
					break;
				case svc_setangle:
					MSG_ReadByte (&sub_message);
					MSG_ReadAngleV(&sub_message, v);
					break;
				case svc_lightstyle:
					MSG_ReadByte (&sub_message);
					MSG_ReadString (&sub_message);
					break;
				case svc_updatefrags:
					MSG_ReadByte (&sub_message);
					MSG_ReadShort (&sub_message);
					break;
				case svc_stopsound:
					MSG_ReadShort (&sub_message);
					break;
				case svc_damage:
					MSG_ReadByte (&sub_message);
					MSG_ReadByte (&sub_message);
					MSG_ReadCoordV (&sub_message, v);
					break;
				case svc_temp_entity:
					//XXX
					break;
				case svc_setpause:
					MSG_ReadByte (&sub_message);
					break;
				case svc_centerprint:
					MSG_ReadString (&sub_message);
					break;
				case svc_killedmonster:
					//XXX
					break;
				case svc_foundsecret:
					//XXX
					break;
				case svc_intermission:
					MSG_ReadCoordV (&sub_message, v);
					MSG_ReadAngleV (&sub_message, v);
					break;
				case svc_finale:
					MSG_ReadString (&sub_message);
					break;
				case svc_cdtrack:
					MSG_ReadByte (&sub_message);
					break;
				case svc_sellscreen:
					//XXX
					break;
				case svc_smallkick:
					//XXX
					break;
				case svc_bigkick:
					//XXX
					break;
				case svc_updateping:
					MSG_ReadByte (&sub_message);
					MSG_ReadShort (&sub_message);
					break;
				case svc_updateentertime:
					MSG_ReadByte (&sub_message);
					MSG_ReadFloat (&sub_message);
					break;
				case svc_updatestatlong:
					MSG_ReadByte (&sub_message);
					MSG_ReadLong (&sub_message);
					break;
				case svc_muzzleflash:
					MSG_ReadShort (&sub_message);
					break;
				case svc_updateuserinfo:
					MSG_ReadByte (&sub_message);
					MSG_ReadLong (&sub_message);
					MSG_ReadString (&sub_message);
					break;
				case svc_playerinfo:
					//XXX
					break;
				case svc_nails:
					//XXX
					break;
				case svc_packetentities:
					qtv_packetentities (sv, &sub_message, 0);
					break;
				case svc_deltapacketentities:
					qtv_packetentities (sv, &sub_message, 1);
					break;
				case svc_maxspeed:
					MSG_ReadFloat (&sub_message);
					break;
				case svc_entgravity:
					MSG_ReadFloat (&sub_message);
					break;
				case svc_setinfo:
					MSG_ReadByte (&sub_message);
					MSG_ReadString (&sub_message);
					MSG_ReadString (&sub_message);
					break;
				case svc_serverinfo:
					MSG_ReadString (&sub_message);
					MSG_ReadString (&sub_message);
					break;
				case svc_updatepl:
					MSG_ReadByte (&sub_message);
					MSG_ReadByte (&sub_message);
					break;
				case svc_nails2:
					//XXX
					break;
			}
		}
	}
}

static void
qtv_packet_f (server_t *sv)
{
	int         len_type, len, type, pos;
	byte       *buf;
	int         reliable = 0;

	len_type = MSG_ReadShort (net_message);
	len = len_type & 0x0fff;
	type = len_type & 0xf000;
	pos = net_message->readcount;
	qtv_printf ("qtv_packet: %x %d\n", type, len);
	switch (type) {
		case qtv_p_signon:
			qtv_p_signon_f (sv, len);
			break;
		case qtv_p_print:
			qtv_printf ("%s\n", MSG_ReadString (net_message));
			break;
		case qtv_p_reliable:
			reliable = 1;
		case qtv_p_unreliable:
			qtv_parse_updates (sv, reliable, len);
			break;
		default:
			//absorb unhandled packet types
			qtv_printf ("unknown packet type %x (%d bytes)\n", type, len);
			break;
	}
	if (net_message->readcount - pos != len) {
		qtv_printf ("packet not completely read\n");

		len -= net_message->readcount - pos;
		if (len > 0) {
			buf = malloc (len);
			MSG_ReadBytes (net_message, buf, len);
			free (buf);	//XXX
		}
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
	while (1) {
		int         cmd;
		if (net_message->badread) {
			break;
		}
		cmd = MSG_ReadByte (net_message);
		if (cmd == -1) {
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
				break;
			case qtv_stringcmd:
				qtv_stringcmd_f (sv);
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
	Netchan_Setup (&sv->netchan, con->address, sv->qport, NC_SEND_QPORT);
	sv->netchan.outgoing_sequence = 1;
	sv->connected = 1;
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
		Hash_Del (server_hash, sv->name);
		Hash_Free (server_hash, sv);
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

	Netchan_SendPacket (strlen (getchallenge), (void *) getchallenge, sv->adr);
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
	sv->next = servers;
	servers = sv;
	sv->name = strdup (name);
	sv->address = strdup (address);
	sv->adr = adr;
	sv->qport = qport->int_val;
	sv->info = Info_ParseString ("", MAX_INFO_STRING, 0);
	Info_SetValueForStarKey (sv->info, "*ver",
							 va ("%s QTV %s", QW_VERSION, VERSION), 0);
	Info_SetValueForStarKey (sv->info, "*qsg_version", QW_QSG_VERSION, 0);
	Info_SetValueForKey (sv->info, "name", "QTV Proxy", 0);
	Hash_Add (server_hash, sv);

	sv->con = Connection_Add (&adr, sv, server_challenge);

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
		qtv_printf ("sv_new: %s unknown\n", name);
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
	list = malloc (count * sizeof (server_t **));
	for (l = &servers, count = 0; *l; l = &(*l)->next)
		list[count] = *l;
	qsort (list, count, sizeof (*list), server_compare);
	for (l = list; *l; l++) {
		sv = *l;
		qtv_printf ("%-20s %s(%s)\n", sv->name, sv->address,
					NET_AdrToString (sv->adr));
	}
	free (list);
}

static void
server_shutdown (void)
{
	Hash_FlushTable (server_hash);
}

static void
server_run (server_t *sv)
{
	static byte delta_msg[2] = {qtv_delta};
	static byte nop_msg[1] = {qtv_nop};
	if (sv->connected > 1) {
		sv->next_run = realtime + 0.03;
		if (sv->delta != -1) {
			delta_msg[1] = sv->delta;
			Netchan_Transmit (&sv->netchan, sizeof (delta_msg), delta_msg);
			return;
		} else if (!sv->netchan.message.cursize) {
			Netchan_Transmit (&sv->netchan, sizeof (nop_msg), nop_msg);
			return;
		}
	}
	Netchan_Transmit (&sv->netchan, 0, 0);
}

void
Server_Init (void)
{
	Sys_RegisterShutdown (server_shutdown);
	server_hash = Hash_NewTable (61, server_get_key, server_free, 0);
	Cmd_AddCommand ("sv_new", sv_new_f, "Add a new server");
	Cmd_AddCommand ("sv_del", sv_del_f, "Remove an existing server");
	Cmd_AddCommand ("sv_list", sv_list_f, "List available servers");
}

void
Server_Frame (void)
{
	server_t   *sv;

	for (sv = servers; sv; sv = sv->next) {
		if (sv->next_run && sv->next_run <= realtime) {
			sv->next_run = 0;
			server_run (sv);
		}
	}
}
