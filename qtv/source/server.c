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
#include "QF/info.h"
#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "qw/protocol.h"

#include "connection.h"
#include "server.h"

static hashtab_t *servers;

static const char *
server_get_key (void *sv, void *unused)
{
	return ((server_t *) sv)->name;
}

static void
server_free (void *_sv, void *unused)
{
	server_t   *sv = (server_t *) _sv;
	static byte final[6] = {clc_stringcmd, 'd', 'r', 'o', 'p', 0};

	Netchan_Transmit (&sv->netchan, 6, final);
	Connection_Del (sv->con);
}

static int
server_compare (const void *a, const void *b)
{
	return strcmp ((*(server_t **) a)->name, (*(server_t **) b)->name);
}

static void
server_handler (connection_t *con, void *object)
{
	server_t   *sv = (server_t *) object;
	byte        d[1] = { clc_nop };

	Con_Printf ("hi\n");
	Netchan_Transmit (&sv->netchan, 1, d);
}

static inline const char *
expect_packet (qmsg_t *msg, int type)
{
	const char *str;
	int         seq;

	MSG_BeginReading (net_message);
	seq = MSG_ReadLong (net_message);
	if (seq != -1) {
		Con_Printf ("unexpected connected packet\n");
		return 0;
	}
	str = MSG_ReadString (net_message);
	if (str[0] != type) {
		Con_Printf ("unexpected connectionless packet type: %s\n", str);
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

	Con_Printf ("connection from %s\n", sv->name);
	Netchan_Setup (&sv->netchan, con->address, sv->qport, NC_SEND_QPORT);
	sv->netchan.outgoing_sequence = 1;
	MSG_WriteByte (msg, clc_stringcmd);
	MSG_WriteString (msg, "new");
	Netchan_Transmit (&sv->netchan, 0, 0);
	con->handler = server_handler;
}

static void
server_challenge (connection_t *con, void *object)
{
	server_t   *sv = (server_t *) object;
	const char *str;
	int         challenge;
	dstring_t  *data;

	if (!(str = expect_packet (net_message, S2C_CHALLENGE)))
		return;

	challenge = atoi (str + 1);
	Con_Printf ("%d %s\n", challenge, Cmd_Argv (1));

	data = dstring_new ();
	dsprintf (data, "%c%c%c%cconnect %i %i %i \"%s\"\n", 255, 255, 255, 255,
			  PROTOCOL_VERSION, sv->qport, challenge,
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

	if (Cmd_Argc () != 3) {
		Con_Printf ("Usage: sv_new <name> <address>\n");
		return;
	}
	name = Cmd_Argv (1);
	if (Hash_Find (servers, name)) {
		Con_Printf ("sv_new: %s already exists\n", name);
		return;
	}
	address = Cmd_Argv (2);
	if (!NET_StringToAdr (address, &adr)) {
		Con_Printf ("Bad server address\n");
		return;
	}
	if (!adr.port)
		adr.port = BigShort (27500);

	sv = calloc (1, sizeof (server_t));
	sv->name = strdup (name);
	sv->address = strdup (address);
	sv->adr = adr;
	sv->qport = qport->int_val;
	sv->info = Info_ParseString ("", MAX_INFO_STRING, 0);
	Info_SetValueForStarKey (sv->info, "*ver",
							 va ("%s QTV %s", QW_VERSION, VERSION), 0);
	Info_SetValueForStarKey (sv->info, "*qsg_version", QW_QSG_VERSION, 0);
	Info_SetValueForKey (sv->info, "name", "QTV Proxy", 0);
	Hash_Add (servers, sv);

	sv->con = Connection_Add (&adr, sv, server_challenge);

	server_getchallenge (sv->con, sv);
}

static void
sv_del_f (void)
{
	const char *name;
	server_t   *sv;

	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: sv_del <name>\n");
		return;
	}
	name = Cmd_Argv (1);
	if (!(sv = Hash_Del (servers, name))) {
		Con_Printf ("sv_new: %s unkown\n", name);
		return;
	}
	Hash_Free (servers, sv);
}

static void
sv_list_f (void)
{
	server_t  **list, **l, *sv;
	int         count;

	list = (server_t **) Hash_GetList (servers);
	for (l = list, count = 0; *l; l++)
		count++;
	if (!count) {
		Con_Printf ("no servers\n");
		return;
	}
	qsort (list, count, sizeof (*list), server_compare);
	for (l = list; *l; l++) {
		sv = *l;
		Con_Printf ("%-20s %s(%s)\n", sv->name, sv->address,
					NET_AdrToString (sv->adr));
	}
}

static void
server_shutdown (void)
{
	Hash_FlushTable (servers);
}

void
Server_Init (void)
{
	Sys_RegisterShutdown (server_shutdown);
	servers = Hash_NewTable (61, server_get_key, server_free, 0);
	Cmd_AddCommand ("sv_new", sv_new_f, "Add a new server");
	Cmd_AddCommand ("sv_del", sv_del_f, "Remove an existing server");
	Cmd_AddCommand ("sv_list", sv_list_f, "List available servers");
}
