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

#include <stdio.h>
#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/idparse.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "netchan.h"

SERVER_PLUGIN_PROTOS
static plugin_list_t server_plugin_list[] = {
	SERVER_PLUGIN_LIST
};

cbuf_t     *qtv_cbuf;

cvar_t     *qtv_console_plugin;
cvar_t     *qtv_mem_size;
cvar_t     *fs_globalcfg;
cvar_t     *fs_usercfg;

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
qtv_quit_f (void)
{
	Sys_Printf ("Shutting down.\n");
	Sys_Quit ();
}

static void
qtv_init (void)
{
	qtv_cbuf = Cbuf_New (&id_interp);

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

	Netchan_Init_Cvars ();

	Cmd_StuffCmds (qtv_cbuf);
	Cbuf_Execute_Sets (qtv_cbuf);

	Cmd_AddCommand ("quit", qtv_quit_f, "Shut down qtv");

	Cmd_StuffCmds (qtv_cbuf);
}

int
main (int argc, const char *argv[])
{
	COM_InitArgv (argc, argv);

	qtv_init ();
	
	Con_Printf ("konnichi wa\n");

	while (1) {
		Cbuf_Execute_Stack (qtv_cbuf);

		Sys_CheckInput (1, net_socket);
		Con_ProcessInput ();
	}
	return 0;
}
