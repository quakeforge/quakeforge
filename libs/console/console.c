/*
	console.c

	console api

	Copyright (C) 2001       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/16

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

#include <stdarg.h>
#include <stdio.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/sys.h"

#include "QF/plugin/general.h"
#include "QF/plugin/console.h"

#include "QF/ui/inputline.h"

//FIXME probably shouldn't be visible
VISIBLE int         con_linewidth;				// characters across screen

VISIBLE plugin_t *con_module;

#define U __attribute__ ((used))
static U con_buffer_t *(*const buffer) (size_t, int) = Con_CreateBuffer;
static U void (*const complete)(inputline_t *) = Con_BasicCompleteCommandLine;
static U inputline_t *(*const create)(int, int, char) = Con_CreateInputLine;
static U void (*const display)(const char **, int) = Con_DisplayList;
#undef U

static char *con_interpreter;
static cvar_t con_interpreter_cvar = {
	.name = "con_interpreter",
	.description =
		"Interpreter for the interactive console",
	.default_value = "id",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &con_interpreter },
};
static sys_printf_t saved_sys_printf;

static void
Con_Interp_f (void *data, const cvar_t *cvar)
{
	cbuf_interpreter_t *interp;

	if (!con_module)
		return;

	interp = Cmd_GetProvider(con_interpreter);

	if (interp) {
		cbuf_t *new;

		Sys_Printf ("Switching to interpreter '%s'\n", con_interpreter);

		new = Cbuf_New (interp);

		if (con_module->data->console->cbuf) {
			new->down = con_module->data->console->cbuf;
			new->state = CBUF_STATE_STACK;
			con_module->data->console->cbuf->up = new;
		}
		con_module->data->console->cbuf = new;
	} else {
		Sys_Printf ("Unknown interpreter '%s'\n", con_interpreter);
	}
}

static void
Con_shutdown (void *data)
{
	if (saved_sys_printf) {
		Sys_SetStdPrintf (saved_sys_printf);
	}
	if (con_module) {
		con_module->functions->general->shutdown ();
		PI_UnloadPlugin (con_module);
	}
}

VISIBLE void
Con_Init (const char *plugin_name)
{
	Sys_RegisterShutdown (Con_shutdown, 0);

	con_module = PI_LoadPlugin ("console", plugin_name);
	if (con_module) {
		__auto_type funcs = con_module->functions->console;
		saved_sys_printf = Sys_SetStdPrintf (funcs->print);
	} else {
		setvbuf (stdout, 0, _IOLBF, BUFSIZ);
	}
	Cvar_Register (&con_interpreter_cvar, Con_Interp_f, 0);
}

VISIBLE void
Con_ExecLine (const char *line)
{
	console_data_t *cd = con_module->data->console;
	int         echo = 1;
	if (line[0] == '/' && line [1] == '/') {
		goto no_lf;
	} else if (line[0] == '|') {
		Cbuf_AddText (cd->cbuf, line);
		Cbuf_AddText (cd->cbuf, "\n");
	} else if (line[0] == '/') {
		Cbuf_AddText (cd->cbuf, line + 1);
		Cbuf_AddText (cd->cbuf, "\n");
	} else if (cd->exec_line) {
		echo = cd->exec_line (cd->exec_data, line);
	} else {
		Cbuf_AddText (cd->cbuf, line);
		Cbuf_AddText (cd->cbuf, "\n");
	}
  no_lf:
	if (echo)
		Sys_Printf ("%s\n", line);
}

VISIBLE void
Con_Printf (const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	if (con_module)
		con_module->functions->console->print (fmt, args);
	else
		vfprintf (stdout, fmt, args);
	va_end (args);
}

VISIBLE void
Con_Print (const char *fmt, va_list args)
{
	if (con_module)
		con_module->functions->console->print (fmt, args);
	else
		vfprintf (stdout, fmt, args);
}

VISIBLE void
Con_SetState (con_state_t state)
{
	if (con_module) {
		con_module->functions->console->set_state (state);
	}
}

VISIBLE void
Con_ProcessInput (void)
{
	if (con_module) {
		if (con_module->functions->console->process_input) {
			con_module->functions->console->process_input ();
		}
	} else {
		static int  been_there_done_that = 0;

		if (!been_there_done_that) {
			been_there_done_that = 1;
			Sys_Printf ("no input for you\n");
		}
	}
}

VISIBLE void
Con_SetOrMask (int mask)
{
	if (con_module)
		con_module->data->console->ormask = mask;
}

VISIBLE void
Con_DrawConsole (void)
{
	if (con_module)
		con_module->functions->console->draw_console ();
}

VISIBLE void
Con_NewMap (void)
{
	if (con_module)
		con_module->functions->console->new_map ();
}
