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

static void
qtv_serverdata (server_t *sv, qmsg_t *msg)
{
	const char *str;

	sv->ver = MSG_ReadLong (msg);
	sv->spawncount = MSG_ReadLong (msg);
	sv->gamedir = strdup (MSG_ReadString (msg));

	sv->message = strdup (MSG_ReadString (msg));
	sv->movevars.gravity = MSG_ReadFloat (msg);
	sv->movevars.stopspeed = MSG_ReadFloat (msg);
	sv->movevars.maxspeed = MSG_ReadFloat (msg);
	sv->movevars.spectatormaxspeed = MSG_ReadFloat (msg);
	sv->movevars.accelerate = MSG_ReadFloat (msg);
	sv->movevars.airaccelerate = MSG_ReadFloat (msg);
	sv->movevars.wateraccelerate = MSG_ReadFloat (msg);
	sv->movevars.friction = MSG_ReadFloat (msg);
	sv->movevars.waterfriction = MSG_ReadFloat (msg);
	sv->movevars.entgravity = MSG_ReadFloat (msg);

	sv->cdtrack = MSG_ReadByte (msg);
	sv->sounds = MSG_ReadByte (msg);

	COM_TokenizeString (MSG_ReadString (msg), qtv_args);
	cmd_args = qtv_args;
	Info_Destroy (sv->info);
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
qtv_soundlist (server_t *sv, qmsg_t *msg)
{
	int         numsounds = MSG_ReadByte (msg);
	int         n;
	const char *str;

	for (;;) {
		str = MSG_ReadString (msg);
		if (!str[0])
			break;
		//qtv_printf ("%s\n", str);
		numsounds++;
		if (numsounds == MAX_SOUNDS) {
			while (str[0])
				str = MSG_ReadString (msg);
			MSG_ReadByte (msg);
			return;
		}
		// save sound name
	}
	n = MSG_ReadByte (msg);
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
qtv_modellist (server_t *sv, qmsg_t *msg)
{
	int         nummodels = MSG_ReadByte (msg);
	int         n;
	const char *str;

	for (;;) {
		str = MSG_ReadString (msg);
		if (!str[0])
			break;
		//qtv_printf ("%s\n", str);
		nummodels++;
		if (nummodels == MAX_SOUNDS) {
			while (str[0])
				str = MSG_ReadString (msg);
			MSG_ReadByte (msg);
			return;
		}
		// save sound name
	}
	n = MSG_ReadByte (msg);
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

void
sv_stringcmd (server_t *sv, qmsg_t *msg)
{
	svcmd_t    *c;
	const char *name;

	COM_TokenizeString (MSG_ReadString (msg), qtv_args);
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

void
sv_parse (server_t *sv, qmsg_t *msg, int reliable)
{
	int         c;
	vec3_t      v, a;

	while (1) {
		c = MSG_ReadByte (msg);
		if (c == -1)
			break;
		//qtv_printf ("sv_parse: svc: %d\n", c);
		switch (c) {
			default:
				qtv_printf ("sv_parse: unknown svc: %d\n", c);
				return;
			case svc_nop:
				break;
			case svc_updatestat:
				MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				break;
			case svc_setview:
				break;
			case svc_sound:
				c = MSG_ReadShort (msg);
				if (c & SND_VOLUME)
					MSG_ReadByte (msg);
				if (c & SND_ATTENUATION)
					MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				MSG_ReadCoordV (msg, v);
				break;
			case svc_print:
				MSG_ReadByte (msg);
				MSG_ReadString (msg);
				break;
			case svc_setangle:
				MSG_ReadByte (msg);
				MSG_ReadAngleV(msg, v);
				break;
			case svc_updatefrags:
				MSG_ReadByte (msg);
				MSG_ReadShort (msg);
				break;
			case svc_stopsound:
				MSG_ReadShort (msg);
				break;
			case svc_damage:
				MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				MSG_ReadCoordV (msg, v);
				break;
			case svc_temp_entity:
				//XXX
				break;
			case svc_setpause:
				MSG_ReadByte (msg);
				break;
			case svc_centerprint:
				MSG_ReadString (msg);
				break;
			case svc_killedmonster:
				//XXX
				break;
			case svc_foundsecret:
				//XXX
				break;
			case svc_intermission:
				MSG_ReadCoordV (msg, v);
				MSG_ReadAngleV (msg, v);
				break;
			case svc_finale:
				MSG_ReadString (msg);
				break;
			case svc_cdtrack:
				MSG_ReadByte (msg);
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
				MSG_ReadByte (msg);
				MSG_ReadShort (msg);
				break;
			case svc_updateentertime:
				MSG_ReadByte (msg);
				MSG_ReadFloat (msg);
				break;
			case svc_updatestatlong:
				MSG_ReadByte (msg);
				MSG_ReadLong (msg);
				break;
			case svc_muzzleflash:
				MSG_ReadShort (msg);
				break;
			case svc_updateuserinfo:
				MSG_ReadByte (msg);
				MSG_ReadLong (msg);
				MSG_ReadString (msg);
				break;
			case svc_playerinfo:
				//XXX
				break;
			case svc_nails:
				//XXX
				break;
			case svc_packetentities:
				qtv_packetentities (sv, msg, 0);
				break;
			case svc_deltapacketentities:
				qtv_packetentities (sv, msg, 1);
				break;
			case svc_maxspeed:
				MSG_ReadFloat (msg);
				break;
			case svc_entgravity:
				MSG_ReadFloat (msg);
				break;
			case svc_setinfo:
				MSG_ReadByte (msg);
				MSG_ReadString (msg);
				MSG_ReadString (msg);
				break;
			case svc_serverinfo:
				MSG_ReadString (msg);
				MSG_ReadString (msg);
				break;
			case svc_updatepl:
				MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				break;
			case svc_nails2:
				//XXX
				break;
			case svc_serverdata:
				qtv_serverdata (sv, msg);
				break;
			case svc_stufftext:
				sv_stringcmd (sv, msg);
				break;
			case svc_soundlist:
				qtv_soundlist (sv, msg);
				break;
			case svc_modellist:
				qtv_modellist (sv, msg);
				break;
			case svc_spawnstaticsound:
				MSG_ReadCoordV (msg, v);
				MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				break;
			case svc_spawnbaseline:
				MSG_ReadShort (msg);
			case svc_spawnstatic:
				MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				MSG_ReadByte (msg);
				MSG_ReadCoordAngleV (msg, v, a);
				break;
			case svc_lightstyle:
				MSG_ReadByte (msg);
				MSG_ReadString (msg);
				break;
		}
	}
}
