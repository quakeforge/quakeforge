/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

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
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/sys.h"
#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/dstring.h"
#include "QF/qtypes.h"

#include "compat.h"

cbuf_t     *cbuf_active;

cbuf_args_t *
Cbuf_ArgsNew (void)
{
	return calloc (1, sizeof (cbuf_args_t));
}

void
Cbuf_ArgsDelete (cbuf_args_t *args)
{
	int		i;

	for (i = 0; i < args->argv_size; i++)
		dstring_delete (args->argv[i]);
	free (args->argv);
	free (args->args);
	free (args);
}

void
Cbuf_ArgsAdd (cbuf_args_t *args, const char *arg)
{
	int		i;

	if (args->argc == args->argv_size) {
		args->argv_size += 4;
		args->argv = realloc (args->argv,
							  args->argv_size * sizeof (dstring_t *));
		args->args = realloc (args->args, args->argv_size * sizeof (char *));
		for (i = args->argv_size - 4; i < args->argv_size; i++) {
			args->argv[i] = dstring_newstr ();
			args->args[i] = 0;
		}
	}
	dstring_clearstr (args->argv[args->argc]);
	dstring_appendstr (args->argv[args->argc], arg);
	args->argc++;
}

cbuf_t *
Cbuf_New (cbuf_interpreter_t *interp)
{
	cbuf_t		*cbuf = calloc (1, sizeof (cbuf_t));

	cbuf->buf = dstring_newstr ();
	cbuf->line = dstring_newstr ();
	cbuf->args = Cbuf_ArgsNew ();
	cbuf->interpreter = interp;
	if (interp->construct)
			interp->construct (cbuf);
	return cbuf;
}

void
Cbuf_Delete (cbuf_t *cbuf)
{
	if (!cbuf)
		return;
	dstring_delete (cbuf->buf);
	dstring_delete (cbuf->line);
	Cbuf_ArgsDelete (cbuf->args);
	if (cbuf->interpreter->destruct)
		cbuf->interpreter->destruct (cbuf);
	free (cbuf);
}

void
Cbuf_DeleteStack (cbuf_t *stack)
{
	cbuf_t		*next;
	
	for (; stack; stack = next) {
		next = stack->down;
		Cbuf_Delete (stack);
	}
}

void
Cbuf_AddText (cbuf_t *cbuf, const char *text)
{
	dstring_appendstr (cbuf->buf, text);
}

void
Cbuf_InsertText (cbuf_t *cbuf, const char *text)
{
	dstring_insertstr (cbuf->buf, 0, "\n");
	dstring_insertstr (cbuf->buf, 0, text);
}

void
Cbuf_Execute (cbuf_t *cbuf)
{
	cbuf_active = cbuf;
	cbuf->state = CBUF_STATE_NORMAL;
	while (cbuf->buf->str[0] || cbuf->line->str[0]) {
		cbuf->interpreter->extract_line (cbuf);
		if (cbuf->state)
			break;
		cbuf->interpreter->parse_line (cbuf);
		if (cbuf->state) // Merging extract and parse
			break;       // will get rid of extra checks
		if (!cbuf->args->argc)
			continue;
		cbuf->interpreter->execute_line (cbuf);
		if (cbuf->state)
			break;
	}
}

void
Cbuf_Execute_Stack (cbuf_t *cbuf)
{
	cbuf_t		*sp;
	
	if (cbuf->resumetime) {
		if (cbuf->resumetime < Sys_DoubleTime())
				cbuf->resumetime = 0;
		else
				return;
	}
	for (sp = cbuf; sp->down; sp = sp->down);
	while (sp) {
		if (sp->down) {
			Cbuf_Delete (sp->down);
			sp->down = 0;
		}
		Cbuf_Execute (sp);
		if (sp->state) {
			if (sp->state == CBUF_STATE_STACK) {
				sp = sp->down;
				continue;
			} else if (sp->state == CBUF_STATE_ERROR)
				break;
			else
				return;
		}		
		sp = sp->up;
	}
	dstring_clearstr (cbuf->buf);
	dstring_clearstr (cbuf->line);
	if (cbuf->down) {
		Cbuf_DeleteStack (cbuf->down);
		cbuf->down = 0;
	}
	if (cbuf->interpreter->reset)
		cbuf->interpreter->reset (cbuf);
}

void
Cbuf_Execute_Sets (cbuf_t *cbuf)
{
	cbuf_args_t *args = cbuf->args;

	cbuf_active = cbuf;
	while (cbuf->buf->str[0] || cbuf->line->str[0]) {
		cbuf->interpreter->extract_line (cbuf);
		cbuf->interpreter->parse_line (cbuf);
		if (!args->argc)
			continue;
		if (strequal (args->argv[0]->str, "set")
			|| strequal (args->argv[0]->str, "setrom"))
			Cmd_Command (args);
	}
}

void
Cbuf_Error (const char *class, const char *fmt, ...)
{
	dstring_t *message = dstring_newstr();
	char *n;
	va_list args;
	
	va_start (args, fmt);
	dvsprintf (message, fmt, args);
	va_end (args);
	if ((n = strchr (cbuf_active->line->str, '\n')))
		*n = 0;
	Sys_Printf ( 
				"-----------------------------------\n"
				"|Error in command buffer execution|\n"
				"-----------------------------------\n"
				"Type: %s\n\n"
				"%s\n\n"
				"Near/on line: %s\n",
				class,
				message->str,
				cbuf_active->line->str
				);
	cbuf_active->state = CBUF_STATE_ERROR;
	dstring_clearstr (cbuf_active->buf);
	dstring_delete (message);
}
