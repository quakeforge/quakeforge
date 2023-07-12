/*
	sv_gib.c

	GIB interface to qw-server

	Copyright (C) 2003 Brian Koropoff

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

#include "QF/cbuf.h"
#include "QF/dstring.h"
#include "QF/info.h"
#include "QF/gib.h"

#include "qw/include/server.h"
#include "qw/include/client.h"

gib_event_t *sv_chat_e;
gib_event_t *sv_client_connect_e;
gib_event_t *sv_client_disconnect_e;
gib_event_t *sv_client_spawn_e;
gib_event_t *sv_map_e;
gib_event_t *sv_frag_e;
gib_event_t *sv_setinfo_e;

static client_t *
SV_GIB_GetClient (int uid)
{
	client_t *cl;
	int i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		if (cl->state >= cs_connected && cl->userid == uid)
			return cl;
	return 0;
}

static void
SV_GIB_Client_GetList_f (void)
{
	client_t *cl;
	int i;

	if (GIB_Argc () != 1)
		GIB_USAGE ("");
	else if (GIB_CanReturn ())
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
			if (cl->state  >= cs_connected)
				dsprintf (GIB_Return (0), "%i", cl->userid);
}

static void
SV_GIB_Client_GetKeys_f (void)
{
	client_t *cl;

	if (GIB_Argc () != 2)
		GIB_USAGE ("");
	else if (!(cl = SV_GIB_GetClient (atoi (GIB_Argv (1)))))
		GIB_Error ("uid", "No user with id '%s' was found on the server.",
				   GIB_Argv (1));
	else if (GIB_CanReturn ()) {
		info_key_t **key, **keys = Info_KeyList (cl->userinfo);
		for (key = keys; *key; key++)
			dstring_appendstr (GIB_Return (0), (*key)->key);
		free (keys);
	}
}

static void
SV_GIB_Client_GetInfo_f (void)
{
	client_t *cl;
	const char *str;

	if (GIB_Argc () != 3)
		GIB_USAGE ("");
	else if (!(cl = SV_GIB_GetClient (atoi (GIB_Argv (1)))))
		GIB_Error ("uid", "No user with id '%s' was found on the server.",
				   GIB_Argv (1));
	else if ((str = Info_ValueForKey (cl->userinfo, GIB_Argv (2))))
		GIB_Return (str);
}

static void
SV_GIB_Client_Print_f (void)
{
	client_t *cl;

	if (GIB_Argc () != 3)
		GIB_USAGE ("uid message");
	else if (!(cl = SV_GIB_GetClient (atoi (GIB_Argv (1)))))
		GIB_Error ("uid", "No user with id '%s' was found on the server.",
				   GIB_Argv (1));
	else
		SV_ClientPrintf (0, cl, GIB_Argv (0)[13] ? PRINT_CHAT : PRINT_HIGH,
						 "%s", GIB_Argv (2));
}

static void
SV_GIB_Client_Print_All_f (void)
{
	client_t *cl;
	int i, level = GIB_Argv (0)[16] ? PRINT_CHAT : PRINT_HIGH;

	if (GIB_Argc () != 2)
		GIB_USAGE ("message");
	else for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		if (cl->state >= cs_connected)
			SV_ClientPrintf (0, cl, level, "%s", GIB_Argv (1));
}

static void
SV_GIB_Map_Get_Current_f (void)
{
	GIB_Return (SV_Current_Map());
}

static void
SV_GIB_Map_Time_Elapsed_f (void)
{
	if (GIB_CanReturn ())
		dsprintf (GIB_Return(0), "%f", sv.time);
}

void
SV_GIB_Init (void)
{
	// Builtins
	GIB_Builtin_Add ("client::getList", SV_GIB_Client_GetList_f);
	GIB_Builtin_Add ("client::getKeys", SV_GIB_Client_GetKeys_f);
	GIB_Builtin_Add ("client::getInfo", SV_GIB_Client_GetInfo_f);
	GIB_Builtin_Add ("client::print", SV_GIB_Client_Print_f);
	GIB_Builtin_Add ("client::printChat", SV_GIB_Client_Print_f);
	GIB_Builtin_Add ("client::printAll", SV_GIB_Client_Print_All_f);
	GIB_Builtin_Add ("client::printAllChat", SV_GIB_Client_Print_All_f);

	GIB_Builtin_Add ("map::getCurrent", SV_GIB_Map_Get_Current_f);
	GIB_Builtin_Add ("map::timeElapsed", SV_GIB_Map_Time_Elapsed_f);

	// Events
	sv_chat_e = GIB_Event_New ("chat");
	sv_client_connect_e = GIB_Event_New ("client.connect");
	sv_client_disconnect_e = GIB_Event_New ("client.disconnect");
	sv_client_spawn_e = GIB_Event_New ("client.spawn");
	sv_map_e = GIB_Event_New ("map");
	sv_frag_e = GIB_Event_New ("frag");
	sv_setinfo_e = GIB_Event_New ("setinfo");
}
