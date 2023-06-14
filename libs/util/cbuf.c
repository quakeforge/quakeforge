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

VISIBLE cbuf_t *cbuf_active = NULL;

VISIBLE cbuf_args_t *
Cbuf_ArgsNew (void)
{
	return calloc (1, sizeof (cbuf_args_t));
}

VISIBLE void
Cbuf_ArgsDelete (cbuf_args_t *args)
{
	int		i;

	for (i = 0; i < args->argv_size; i++)
		dstring_delete (args->argv[i]);
	free (args->argv);
	free ((void*)args->args);
	free (args->argm);
	free (args);
}

VISIBLE void
Cbuf_ArgsAdd (cbuf_args_t *args, const char *arg)
{
	int		i;

	if (args->argc == args->argv_size) {
		args->argv_size += 4;
		args->argv = realloc (args->argv,
							  args->argv_size * sizeof (dstring_t *));
		args->args = realloc ((void*)args->args, args->argv_size * sizeof (char *));
		args->argm = realloc (args->argm, args->argv_size * sizeof (void *));
		for (i = args->argv_size - 4; i < args->argv_size; i++) {
			args->argv[i] = dstring_newstr ();
			args->args[i] = 0;
		}
	}
	dstring_copystr (args->argv[args->argc], arg);
	args->argc++;
}

VISIBLE cbuf_t *
_Cbuf_New (cbuf_interpreter_t *interp, void *data)
{
	cbuf_t		*cbuf = malloc (sizeof (cbuf_t));
	*cbuf = (cbuf_t) {
		.args = Cbuf_ArgsNew (),
		.interpreter = interp,
		.data = data,
	};

	if (interp->construct) {
		interp->construct (cbuf);
	}
	return cbuf;
}

VISIBLE cbuf_t *
Cbuf_New (cbuf_interpreter_t *interp)
{
	qfZoneScoped (true);
	return _Cbuf_New (interp, nullptr);
}

VISIBLE void
Cbuf_Delete (cbuf_t *cbuf)
{
	if (!cbuf)
		return;
	Cbuf_ArgsDelete (cbuf->args);
	if (cbuf->interpreter->destruct)
		cbuf->interpreter->destruct (cbuf);
	free (cbuf);
}

VISIBLE void
Cbuf_DeleteStack (cbuf_t *stack)
{
	qfZoneScoped (true);
	cbuf_t		*next;

	for (; stack; stack = next) {
		next = stack->down;
		Cbuf_Delete (stack);
	}
}

VISIBLE void
Cbuf_DeleteStackReverse (cbuf_t *stack)
{
	cbuf_t		*next;

	for (; stack; stack = next) {
		next = stack->up;
		Cbuf_Delete (stack);
	}
}

void
Cbuf_Reset (cbuf_t *cbuf)
{
	cbuf->resumetime = 0.0;
	cbuf->args->argc = 0;
	cbuf->state = CBUF_STATE_NORMAL;
	if (cbuf->interpreter->reset)
		cbuf->interpreter->reset (cbuf);
}

VISIBLE cbuf_t *
Cbuf_PushStack (cbuf_interpreter_t *interp)
{
	cbuf_t *new;
	if (cbuf_active->down) {
		new = cbuf_active->down;
		if (new->interpreter != interp) {
			new->interpreter->destruct (new);
			new->interpreter = interp;
			new->interpreter->construct (new);
		}
		Cbuf_Reset (new);
	} else
		new = Cbuf_New (interp);
	cbuf_active->down = new;
	new->up = cbuf_active;
	cbuf_active->state = CBUF_STATE_STACK;
	return new;
}

VISIBLE void
Cbuf_AddText (cbuf_t *cbuf, const char *text)
{
	if (cbuf->state == CBUF_STATE_JUNK)
		cbuf->state = CBUF_STATE_NORMAL;
	cbuf->interpreter->add (cbuf, text);
}

VISIBLE void
Cbuf_InsertText (cbuf_t *cbuf, const char *text)
{
	qfZoneScoped (true);
	if (cbuf->state == CBUF_STATE_JUNK)
		cbuf->state = CBUF_STATE_NORMAL;
	cbuf->interpreter->insert (cbuf, text);
}

VISIBLE void
Cbuf_Execute (cbuf_t *cbuf)
{
	cbuf_t *old = cbuf_active;
	cbuf_active = cbuf;
	cbuf->interpreter->execute (cbuf);
	cbuf_active = old;
}

VISIBLE void
Cbuf_Execute_Stack (cbuf_t *cbuf)
{
	cbuf_t		*sp;

	if (cbuf->resumetime) {
		if (cbuf->resumetime < Sys_DoubleTime())
				cbuf->resumetime = 0;
		else
				return;
	}
	for (sp = cbuf; sp->down && sp->down->state != CBUF_STATE_JUNK; sp = sp->down);
	if (sp->state == CBUF_STATE_BLOCKED)
		return;
	while (sp) {
		Cbuf_Execute (sp);
		if (sp->state) {
			if (sp->state == CBUF_STATE_STACK) {
				sp->state = CBUF_STATE_NORMAL;
				sp = sp->down;
				continue;
			} else if (sp->state == CBUF_STATE_ERROR)
				break;
			else if (sp->state == CBUF_STATE_BLOCKED)
				return;
			else {
				sp->state = CBUF_STATE_NORMAL;
				return;
			}
		}
		sp->state = CBUF_STATE_JUNK;
		sp = sp->up;
	}
	if (cbuf->down) {
		Cbuf_DeleteStack (cbuf->down);
		cbuf->down = 0;
	}
	if (sp)
		Cbuf_Reset (cbuf);
}

VISIBLE void
Cbuf_Execute_Sets (cbuf_t *cbuf)
{
	cbuf->interpreter->execute_sets (cbuf);
}

