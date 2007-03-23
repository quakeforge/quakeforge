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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include <stdarg.h>
#include <stdio.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/plugin.h"
#include "QF/sys.h"

//FIXME probably shouldn't be visible
int         con_linewidth __attribute__ ((visibility ("default")));				// characters across screen

plugin_t *con_module __attribute__ ((visibility ("default")));

#define U __attribute__ ((used))
static U con_buffer_t *(*const buffer) (size_t, int) = Con_CreateBuffer;
static U void (*const complete)(inputline_t *) = Con_BasicCompleteCommandLine;
static U inputline_t *(*const create)(int, int, char) = Con_CreateInputLine;
static U void (*const display)(const char **, int) = Con_DisplayList;
#undef U

static cvar_t *con_interpreter;

__attribute__ ((visibility ("default")))
void
Con_Init (const char *plugin_name)
{
	con_module = PI_LoadPlugin ("console", plugin_name);
	if (con_module) {
		con_module->functions->general->p_Init ();
		Sys_SetStdPrintf (con_module->functions->console->pC_Print);
	} else {
		setvbuf (stdout, 0, _IOLBF, BUFSIZ);
	}
}

static void
Con_Interp_f (cvar_t* var)
{
	cbuf_interpreter_t* interp = Cmd_GetProvider(var->string);

	if (con_module && interp) {
		cbuf_t *new;

		Sys_Printf ("Switching to interpreter '%s'\n", var->string);

		new = Cbuf_New (interp);
	
		if (con_module->data->console->cbuf) {
			new->down = con_module->data->console->cbuf;
			new->state = CBUF_STATE_STACK;
			con_module->data->console->cbuf->up = new;
		}
		con_module->data->console->cbuf = new;
	} else {
		Sys_Printf ("Unknown interpreter '%s'\n", var->string);
	}
}

__attribute__ ((visibility ("default")))
void
Con_Init_Cvars (void)
{
	con_interpreter = 
		Cvar_Get("con_interpreter", "id", CVAR_NONE, Con_Interp_f,
				 "Interpreter for the interactive console");
}

__attribute__ ((visibility ("default")))
void
Con_Shutdown (void)
{
	if (con_module) {
		con_module->functions->general->p_Shutdown ();
		PI_UnloadPlugin (con_module);
	}
}

__attribute__ ((visibility ("default")))
void
Con_Printf (const char *fmt, ...)
{
	va_list		args;

	va_start (args, fmt);
	if (con_module)
		con_module->functions->console->pC_Print (fmt, args);
	else
		vfprintf (stdout, fmt, args);
	va_end (args);
}

__attribute__ ((visibility ("default")))
void
Con_Print (const char *fmt, va_list args)
{
	if (con_module)
		con_module->functions->console->pC_Print (fmt, args);
	else
		vfprintf (stdout, fmt, args);
}

__attribute__ ((visibility ("default")))
void
Con_DPrintf (const char *fmt, ...)
{
	if (developer && developer->int_val) {
		va_list     args;
		va_start (args, fmt);
		if (con_module)
			con_module->functions->console->pC_Print (fmt, args);
		else
			vfprintf (stdout, fmt, args);
		va_end (args);
	}
}

__attribute__ ((visibility ("default")))
void
Con_ProcessInput (void)
{
	if (con_module) {
		con_module->functions->console->pC_ProcessInput ();
	} else {
		static int  been_there_done_that = 0;

		if (!been_there_done_that) {
			been_there_done_that = 1;
			Con_Printf ("no input for you\n");
		}
	}
}

__attribute__ ((visibility ("default")))
void
Con_KeyEvent (knum_t key, short unicode, qboolean down)
{
	if (con_module)
		con_module->functions->console->pC_KeyEvent (key, unicode, down);
}

__attribute__ ((visibility ("default")))
void
Con_SetOrMask (int mask)
{
	if (con_module)
		con_module->data->console->ormask = mask;
}

__attribute__ ((visibility ("default")))
void
Con_DrawConsole (void)
{
	if (con_module)
		con_module->functions->console->pC_DrawConsole ();
}

__attribute__ ((visibility ("default")))
void
Con_CheckResize (void)
{
	if (con_module)
		con_module->functions->console->pC_CheckResize ();
}

__attribute__ ((visibility ("default")))
void
Con_NewMap (void)
{
	if (con_module)
		con_module->functions->console->pC_NewMap ();
}
