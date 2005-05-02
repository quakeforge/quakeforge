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

#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/idparse.h"
#include "QF/info.h"

#include "compat.h"
#include "server.h"
#include "sv_qtv.h"

typedef struct {
	netchan_t   netchan;
	backbuf_t   backbuf;
	info_t     *info;
	info_key_t *name_key;
	int         packet;
} sv_qtv_t;

#define MAX_PROXIES 2

static sv_qtv_t proxies[MAX_PROXIES];

static sv_qtv_t *
alloc_proxy (void)
{
	int         i;

	for (i = 0; i < MAX_PROXIES; i++) {
		if (!proxies[i].info)
			return proxies;
	}
	return 0;
}

static void
drop_proxy (sv_qtv_t *proxy)
{
	SV_Printf ("dropped %s\n", proxy->name_key->value);
	MSG_WriteByte (&proxy->netchan.message, qtv_disconnect);
	Netchan_Transmit (&proxy->netchan, 0, 0);
	proxy->packet = 0;
	Info_Destroy (proxy->info);
	proxy->info = 0;
}

static void
qtv_new_f (sv_qtv_t *proxy)
{
	const char *gamedir;

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

	MSG_WriteByte (&proxy->netchan.message, qtv_serverdata);
	MSG_WriteLong (&proxy->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong (&proxy->netchan.message, svs.spawncount);
	MSG_WriteString (&proxy->netchan.message, gamedir);
	SV_WriteWorldVars (&proxy->netchan);
}

typedef struct {
	const char *name;
	void      (*func) (sv_qtv_t *proxy);
	unsigned    no_redirect:1;
} qcmd_t;

qcmd_t qcmds[] = {
	{"drop",	drop_proxy},
	{"new",		qtv_new_f},

	{0,			0},
};

static void
qtv_command (sv_qtv_t *proxy, const char *s)
{
	qcmd_t     *c;
	const char *name;

	COM_TokenizeString (s, sv_args);
	cmd_args = sv_args;
	name = Cmd_Argv (0);

	for (c = qcmds; c->name; c++)
		if (strcmp (c->name, name) == 0)
			break;
	if (!c->name) {
		SV_Printf ("Bad QTV command: %s\n", sv_args->argv[0]->str);
		return;
	}
	c->func (proxy);
}

static void
qtv_parse (sv_qtv_t *proxy)
{
	int         c;

	proxy->packet = 1;
	while (1) {
		if (net_message->badread) {
			SV_Printf ("SV_ReadClientMessage: badread\n");
			drop_proxy (proxy);
			return;
		}

		c = MSG_ReadByte (net_message);
		if (c == -1)
			return;

		switch (c) {
			default:
				SV_Printf ("SV_ReadClientMessage: unknown command char\n");
				drop_proxy (proxy);
				return;
			case qtv_nop:
				break;

			case qtv_stringcmd:
				qtv_command (proxy, MSG_ReadString (net_message));
				break;

			case qtv_delta:
				MSG_ReadByte (net_message);
				break;
		}
	}
}

void
SV_qtvConnect (int qport, info_t *info)
{
	sv_qtv_t   *proxy;

	SV_Printf ("QTV proxy connection: %d %s\n", Cmd_Argc (), Cmd_Args (1));
	SV_Printf ("qport: %d\n", qport);

	if (!(proxy = alloc_proxy ())) {
		SV_Printf ("%s:full proxy connect\n", NET_AdrToString (net_from));
		Netchan_OutOfBandPrint (net_from, "%c\nserver is full\n\n", A2C_PRINT);
		return;
	}
	proxy->info = info;
	while (!(proxy->name_key = Hash_Find (proxy->info->tab, "name")))
		Info_SetValueForKey (proxy->info, "name", "\"unnamed proxy\"", 1);

	Netchan_Setup (&proxy->netchan, net_from, qport, NC_READ_QPORT);
	proxy->backbuf.netchan = &proxy->netchan;
	proxy->backbuf.name = proxy->name_key->value;

	Netchan_OutOfBandPrint (net_from, "%c", S2C_CONNECTION);
}

int
SV_qtvPacket (int qport)
{
	int         i;

	for (i = 0; i < MAX_PROXIES; i++) {
		if (!proxies[i].info)
			continue;
		if (!NET_CompareBaseAdr (net_from, proxies[i].netchan.remote_address))
			continue;
		if (proxies[i].netchan.qport != qport)
			continue;
		if (proxies[i].netchan.remote_address.port != net_from.port) {
			Con_DPrintf ("SV_ReadPackets: fixing up a translated port\n");
			proxies[i].netchan.remote_address.port = net_from.port;
		}
		if (Netchan_Process (&proxies[i].netchan)) {
			// this is a valid, sequenced packet, so process it
			svs.stats.packets++;
		}
		qtv_parse (&proxies[i]);
		return 1;
	}
	return 0;
}

void
SV_qtvCheckTimeouts (void)
{
	int         i;
	float       droptime;
	sv_qtv_t   *proxy;

	droptime = realtime - timeout->value;

	for (i = 0; i < MAX_PROXIES; i++) {
		proxy = proxies + i;
		if (proxy->info) {
			if (proxy->netchan.last_received < droptime) {
				SV_Printf ("%s timed out\n", proxy->name_key->value);
				drop_proxy (proxy);
			}
		}
	}
}

void
SV_qtvSendMessages (void)
{
	int         i;
	sv_qtv_t   *proxy;

	for (i = 0; i < MAX_PROXIES; i++) {
		proxy = proxies + i;
		if (!proxy->info)
			continue;
		if (!proxy->packet)
			continue;
		proxy->packet = 0;
		Netchan_Transmit (&proxy->netchan, 0, 0);
	}
}
