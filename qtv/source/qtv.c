/*
	qtv.c

	QTV main file

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2004/02/18

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
#include <time.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/idparse.h"
#include "QF/info.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "QF/plugin/console.h"

#include "compat.h"
#include "netchan.h"

#include "qw/protocol.h"

#include "qtv/include/client.h"
#include "qtv/include/connection.h"
#include "qtv/include/qtv.h"
#include "qtv/include/server.h"

#undef qtv_print

SERVER_PLUGIN_PROTOS
static plugin_list_t server_plugin_list[] = {
	SERVER_PLUGIN_LIST
};

double realtime;

cvar_t     *sv_timeout;

cbuf_t     *qtv_cbuf;
cbuf_args_t *qtv_args;

static cvar_t  *qtv_console_plugin;
static cvar_t  *qtv_port;
static cvar_t  *qtv_mem_size;

redirect_t  qtv_redirected;
client_t   *qtv_redirect_client;
dstring_t   outputbuf = {&dstring_default_mem};

static __attribute__((format(PRINTF, 1, 0))) void
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
#ifdef _WIN32
			strftime (stamp, sizeof (stamp), "[%b %d %X] ", local);
#else
			strftime (stamp, sizeof (stamp), "[%b %e %X] ", local);
#endif
			Con_Printf ("%s%s", stamp, msg->str);
		}
		if (msg->str[0] && msg->str[strlen (msg->str) - 1] != '\n') {
			pending = 1;
		} else {
			pending = 0;
		}
	}
}

void
qtv_printf (const char *fmt, ...)
{
	va_list     argptr;

	va_start (argptr, fmt);
	qtv_print (fmt, argptr);
	va_end (argptr);
}

static void
qtv_flush_redirect (void)
{
	char        send[8000 + 6];
	size_t      count;
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
	} else if (qtv_redirected == RD_CLIENT) {
		client_t   *cl = qtv_redirect_client;
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
	dstring_clear (&outputbuf);
}

void
qtv_begin_redirect (redirect_t rd, client_t *cl)
{
	qtv_redirected = rd;
	qtv_redirect_client = cl;
	dstring_clear (&outputbuf);
}

void
qtv_end_redirect (void)
{
	qtv_flush_redirect ();
	qtv_redirected = RD_NONE;
	qtv_redirect_client = 0;
}

static void
qtv_memory_init (void)
{
	int         mem_size;
	void       *mem_base;

	qtv_mem_size = Cvar_Get ("qtv_mem_size", "8", CVAR_NONE, NULL,
							 "Amount of memory (in MB) to allocate for the "
							 PACKAGE_NAME " heap");

	Cvar_SetFlags (qtv_mem_size, qtv_mem_size->flags | CVAR_ROM);
	mem_size = (int) (qtv_mem_size->value * 1024 * 1024);
	mem_base = Sys_Alloc (mem_size);
	if (!mem_base)
		Sys_Error ("Can't allocate %d", mem_size);
	Memory_Init (mem_base, mem_size);
}

static void
qtv_shutdown (void *data)
{
	Cbuf_Delete (qtv_cbuf);
	Cbuf_ArgsDelete (qtv_args);
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
	qtv_port = Cvar_Get ("qtv_port", va (0, "%d", PORT_QTV), 0, 0,
						 "udp port to use");
	sv_timeout = Cvar_Get ("sv_timeout", "60", 0, 0, "server timeout");
	NET_Init (qtv_port->int_val);
	Connection_Init ();
	net_realtime = &realtime;
	Netchan_Init ();
}

static void
qtv_init (void)
{
	qtv_cbuf = Cbuf_New (&id_interp);
	qtv_args = Cbuf_ArgsNew ();

	Sys_RegisterShutdown (qtv_shutdown, 0);

	Sys_Init ();
	COM_ParseConfig (qtv_cbuf);
	Cvar_Get ("cmd_warncmd", "1", CVAR_NONE, NULL, NULL);

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

	qtv_sbar_init ();

	Netchan_Init_Cvars ();

	Cmd_StuffCmds (qtv_cbuf);
	Cbuf_Execute_Sets (qtv_cbuf);

	qtv_net_init ();
	Server_Init ();
	Client_Init ();

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
	qtv_begin_redirect (RD_PACKET, 0);
	Sys_Printf ("\\*version\\%s qtv %s", QW_VERSION, PACKAGE_VERSION);
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
	if (!strcmp (cmd, "ping")) {
		qtv_ping ();
	} else if (!strcmp (cmd, "status")) {
		qtv_status ();
	} else if (!strcmp (cmd, "getchallenge")) {
		Client_NewConnection ();
	} else if (cmd[0]) {
		switch (cmd[0]) {
			default:
				goto bad_packet;
			case A2C_PRINT:
				Sys_Printf ("%s", str + 1);
				break;
			case A2A_PING:
				qtv_ping ();
				break;
		}
	} else {
bad_packet:
		Sys_Printf ("bad connectionless packet from %s:\n%s\n",
					NET_AdrToString (net_from), str);
	}
}

static void
qtv_read_packets (void)
{
	connection_t *con;

	while (NET_GetPacket ()) {
		if ((con = Connection_Find (&net_from))) {
			con->handler (con, con->object);
		} else if (*(int *) net_message->message->data == -1) {
			qtv_connectionless_packet ();
		}
	}
}

int
main (int argc, const char *argv[])
{
	int         frame = 0;

	COM_InitArgv (argc, argv);

	qtv_init ();

	Sys_Printf ("Ohayou gozaimasu\n");

	while (1) {
		Cbuf_Execute_Stack (qtv_cbuf);

		Sys_CheckInput (1, net_socket);
		realtime = Sys_DoubleTime () + 1;

		qtv_read_packets ();

		Con_ProcessInput ();

		Server_Frame ();
		Client_Frame ();

		if (++frame == 100) {
			frame = 0;
			Con_DrawConsole ();
		}
	}
	return 0;
}
