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
#include <time.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/idparse.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "compat.h"
#include "connection.h"
#include "netchan.h"
#include "server.h"

#define PORT_QTV 27501
#define A2A_PING	'k'
#define A2A_ACK		'l'
#define A2C_PRINT	'n'

typedef enum {
	RD_NONE,
	RD_CLIENT,
	RD_PACKET,
} redirect_t;

SERVER_PLUGIN_PROTOS
static plugin_list_t server_plugin_list[] = {
	SERVER_PLUGIN_LIST
};

cbuf_t     *qtv_cbuf;
cbuf_args_t *qtv_args;

cvar_t     *qtv_console_plugin;
cvar_t     *qtv_port;
cvar_t     *qtv_mem_size;
cvar_t     *fs_globalcfg;
cvar_t     *fs_usercfg;

redirect_t  qtv_redirected;
dstring_t   outputbuf = {&dstring_default_mem};

static void
qtv_print (const char *fmt, va_list args)
{
	static int pending;
	static dstring_t *msg;
	
	if (!msg)
		msg = dstring_new ();

	dvsprintf (msg, fmt, args);
	if (qtv_redirected)
		dstring_appendstr (&outputbuf, msg->str);
	else {
		time_t      mytime;
		struct tm  *local;
		char        stamp[123];

		if (pending) {
			Con_Printf ("%s", msg->str);
		} else {
			mytime = time (NULL);
			local = localtime (&mytime);
			strftime (stamp, sizeof (stamp), "[%b %e %X] ", local);
			Con_Printf ("%s%s", stamp, msg->str);
		}
		if (msg->str[0] && msg->str[strlen (msg->str) - 1] != '\n') {
			pending = 1;
		} else {
			pending = 0;
		}
	}
}

static void
qtv_flush_redirect (void)
{
	char        send[8000 + 6];
	int         count;
	int         bytes;
	const char *p;

	if (!outputbuf.size)
		return;

	count = strlen (outputbuf.str);
	if (qtv_redirected == RD_PACKET) {
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
	}
	dstring_clear (&outputbuf);
}

static void
qtv_begin_redirect (redirect_t rd)
{
	qtv_redirected = rd;
	dstring_clear (&outputbuf);
}

static void
qtv_end_redirect (void)
{
	qtv_flush_redirect ();
	qtv_redirected = RD_NONE;
}

static void
qtv_memory_init (void)
{
	int         mem_size;
	void       *mem_base;

	qtv_mem_size = Cvar_Get ("qtv_mem_size", "8", CVAR_NONE, NULL,
							 "Amount of memory (in MB) to allocate for the "
							 PROGRAM " heap");

	Cvar_SetFlags (qtv_mem_size, qtv_mem_size->flags | CVAR_ROM);
	mem_size = (int) (qtv_mem_size->value * 1024 * 1024);
	mem_base = malloc (mem_size);
	if (!mem_base)
		Sys_Error ("Can't allocate %d", mem_size);
	Memory_Init (mem_base, mem_size);
}

static void
qtv_shutdown (void)
{
	NET_Shutdown ();
	Con_Shutdown ();
}

static void
qtv_quit_f (void)
{
	Sys_Printf ("Shutting down.\n");
	Sys_Quit ();
}

static void
qtv_net_init (void)
{
	qtv_port = Cvar_Get ("qtv_port", va ("%d", PORT_QTV), 0, 0,
						 "udp port to use");
	NET_Init (qtv_port->int_val);
	Connection_Init ();
	Netchan_Init ();
}

static void
qtv_init (void)
{
	qtv_cbuf = Cbuf_New (&id_interp);
	qtv_args = Cbuf_ArgsNew ();

	Sys_RegisterShutdown (qtv_shutdown);

	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	Cvar_Init ();
	Sys_Init_Cvars ();
	Sys_Init ();
	Cvar_Get ("cmd_warncmd", "1", CVAR_NONE, NULL, NULL);
	Cmd_Init ();

	Cmd_StuffCmds (qtv_cbuf);
	Cbuf_Execute_Sets (qtv_cbuf);

	fs_globalcfg = Cvar_Get ("fs_globalcfg", FS_GLOBALCFG,
							 CVAR_ROM, 0, "global configuration file");
	Cmd_Exec_File (qtv_cbuf, fs_globalcfg->string, 0);
	Cbuf_Execute_Sets (qtv_cbuf);

	// execute +set again to override the config file
	Cmd_StuffCmds (qtv_cbuf);
	Cbuf_Execute_Sets (qtv_cbuf);
	fs_usercfg = Cvar_Get ("fs_usercfg", FS_USERCFG,
						   CVAR_ROM, 0, "user configuration file");
	Cmd_Exec_File (qtv_cbuf, fs_usercfg->string, 0);
	Cbuf_Execute_Sets (qtv_cbuf);

	// execute +set again to override the config file
	Cmd_StuffCmds (qtv_cbuf);
	Cbuf_Execute_Sets (qtv_cbuf);

	qtv_memory_init ();

	QFS_Init ("qw");
	PI_Init ();

	qtv_console_plugin = Cvar_Get ("qtv_console_plugin", "server",
								   CVAR_ROM, 0, "Plugin used for the console");
	PI_RegisterPlugins (server_plugin_list);
	Con_Init (qtv_console_plugin->string);
	if (con_module) 
		con_module->data->console->cbuf = qtv_cbuf;
	Sys_SetStdPrintf (qtv_print);

	Netchan_Init_Cvars ();

	Cmd_StuffCmds (qtv_cbuf);
	Cbuf_Execute_Sets (qtv_cbuf);

	qtv_net_init ();
	Server_Init ();

	Cmd_AddCommand ("quit", qtv_quit_f, "Shut down qtv");

	Cmd_StuffCmds (qtv_cbuf);
}

static void
qtv_ping (void)
{
	char        data = A2A_ACK;

	Netchan_SendPacket (1, &data, net_from);
}

static void
qtv_status (void)
{
	qtv_begin_redirect (RD_PACKET);
	Sys_Printf ("\\*version\\%s qtv %s", QW_VERSION, VERSION);
	qtv_end_redirect ();
}

static void
qtv_connectionless_packet (void)
{
	const char *cmd, *str;

	MSG_BeginReading (net_message);
	MSG_ReadLong (net_message);			// skip the -1 marker

	str = MSG_ReadString (net_message);
	COM_TokenizeString (str, qtv_args);
	cmd_args = qtv_args;

	cmd = qtv_args->argv[0]->str;
	if (!strcmp (cmd, "ping")
		|| (cmd[0] == A2A_PING && (cmd[1] == 0 || cmd[1] == '\n'))) {
		qtv_ping ();
	} else if (!strcmp (cmd, "status")) {
		qtv_status ();
	} else {
		Con_Printf ("bad connectionless packet from %s:\n%s\n",
					NET_AdrToString (net_from), str);
	}
}

static void
qtv_read_packets (void)
{
	connection_t *con;

	while (NET_GetPacket ()) {
		if (*(int *) net_message->message->data == -1) {
			qtv_connectionless_packet ();
			continue;
		}

		if ((con = Connection_Find (&net_from)))
			con->handler (con->object);
	}
}

int
main (int argc, const char *argv[])
{
	COM_InitArgv (argc, argv);

	qtv_init ();
	
	Con_Printf ("Ohayou gozaimasu\n");

	while (1) {
		Cbuf_Execute_Stack (qtv_cbuf);

		Sys_CheckInput (1, net_socket);

		qtv_read_packets ();

		Con_ProcessInput ();
	}
	return 0;
}
