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

#include "QF/cmd.h"
#include "QF/hash.h"
#include "QF/info.h"

#include "server.h"
#include "sv_qtv.h"

typedef struct {
	netchan_t   netchan;
	backbuf_t   backbuf;
	info_t     *info;
	info_key_t *name_key;
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
		Con_Printf ("boo\n");
		if (Netchan_Process (&proxies[i].netchan)) {
			// this is a valid, sequenced packet, so process it
			svs.stats.packets++;
		}
		Netchan_Transmit (&proxies[i].netchan, 0, 0);
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
				MSG_WriteByte (&proxy->netchan.message, svc_disconnect);
				Netchan_Transmit (&proxy->netchan, 0, 0);
				Info_Destroy (proxy->info);
				proxy->info = 0;
			}
		}
	}
}
