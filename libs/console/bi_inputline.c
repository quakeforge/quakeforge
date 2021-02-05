/*
	bi_inputline.c

	CSQC inputline builtins

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/console.h"
#include "QF/csqc.h"
#include "QF/draw.h"
#include "QF/progs.h"
#include "QF/sys.h"
#include "QF/zone.h"

typedef struct il_data_s {
	struct il_data_s *next;
	struct il_data_s **prev;
	inputline_t *line;
	progs_t    *pr;
	func_t      enter;		// enter key callback
	pointer_t   data[2];	// allow two data params for the callback
	int         method;		// true if method rather than function
} il_data_t;

typedef struct {
	PR_RESMAP (il_data_t) line_map;
	il_data_t *lines;
	void      (*draw)(inputline_t *il);
} il_resources_t;

static il_data_t *
il_data_new (il_resources_t *res)
{
	PR_RESNEW (il_data_t, res->line_map);
}

static void
il_data_free (il_resources_t *res, il_data_t *line)
{
	PR_RESFREE (il_data_t, res->line_map, line);
}

static void
il_data_reset (il_resources_t *res)
{
	PR_RESRESET (il_data_t, res->line_map);
}

static inline il_data_t *
il_data_get (il_resources_t *res, unsigned index)
{
	PR_RESGET (res->line_map, index);
}

static inline int __attribute__((pure))
il_data_index (il_resources_t *res, il_data_t *line)
{
	PR_RESINDEX (res->line_map, line);
}

static void
bi_il_clear (progs_t *pr, void *data)
{
	il_resources_t *res = (il_resources_t *)data;
	il_data_t  *line;

	for (line = res->lines; line; line = line->next)
		Con_DestroyInputLine (line->line);
	res->lines = 0;
	il_data_reset (res);
}

static il_data_t *
get_inputline (progs_t *pr, int arg, const char *func)
{
	il_resources_t *res = PR_Resources_Find (pr, "InputLine");
	il_data_t  *line = il_data_get (res, arg);

	// line->prev will be null if the handle is unallocated
	if (!line || !line->prev)
		PR_RunError (pr, "invalid inputline: passed to %s", func);

	return line;
}

static void
bi_inputline_enter (inputline_t *il)
{
	il_data_t  *data = (il_data_t *) il->user_data;
	progs_t    *pr = data->pr;
	const char *line = il->line;

	if (!data->enter)
		return;			// no callback defined

	PR_PushFrame (pr);
	PR_RESET_PARAMS (pr);
	if (data->method) {
		P_POINTER (pr, 0) = data->data[0];
		P_POINTER (pr, 1) = data->data[1];
		P_STRING (pr, 2) = PR_SetTempString (pr, line);
		pr->pr_argc = 3;
	} else {
		P_STRING (pr, 0) = PR_SetTempString (pr, line);
		P_POINTER (pr, 1) = data->data[0];
		pr->pr_argc = 2;
	}
	PR_ExecuteProgram (pr, data->enter);
	PR_PopFrame (pr);
}

static void
bi_InputLine_Create (progs_t *pr)
{
	il_resources_t *res = PR_Resources_Find (pr, "InputLine");
	il_data_t  *data;
	inputline_t *line;
	int         lines = P_INT (pr, 0);
	int         size = P_INT (pr, 1);
	int         prompt = P_INT (pr, 2);

	line = Con_CreateInputLine (lines, size, prompt);
	if (!line) {
		Sys_Printf ("failed to create inputline\n");
		R_INT (pr) = 0;
		return;
	}

	data = il_data_new (res);
	if (!data) {
		Con_DestroyInputLine (line);
		Sys_Printf ("out of resources\n");
		R_INT (pr) = 0;
		return;
	}

	data->next = res->lines;
	data->prev = &res->lines;
	if (res->lines)
		res->lines->prev = &data->next;
	res->lines = data;
	data->line = line;
	data->pr = pr;

	line->draw = res->draw;
	line->enter = bi_inputline_enter;
	line->user_data = data;

	R_INT (pr) = il_data_index (res, data);
}

static void
bi_InputLine_SetPos (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0),
									  "InputLine_SetPos");
	line->line->x = P_INT (pr, 1);
	line->line->y = P_INT (pr, 2);
}

static void
bi_InputLine_SetCursor (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0),
									  "InputLine_SetCursor");
	line->line->cursor = P_INT (pr, 1);
}

static void
bi_InputLine_SetEnter (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0),
									  "InputLine_SetEnter");

	line->data[1] = 0;

	//FIXME look up implementation here?
	if (pr->pr_argc == 4) {
		line->enter = P_FUNCTION (pr, 1);	// implementation
		line->data[0] = P_POINTER (pr, 2);	// object
		line->data[1] = P_POINTER (pr, 3);	// selector
		line->method = 1;
	} else {
		line->enter = P_FUNCTION (pr, 1);	// function
		line->data[0] = P_POINTER (pr, 2);	// data
		line->data[1] = 0;
		line->method = 0;
	}
}

static void
bi_InputLine_SetWidth (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0),
									   "InputLine_SetWidth");
	int         width = P_INT (pr, 1);

	line->line->width = width;
}

static void
bi_InputLine_Destroy (progs_t *pr)
{
	il_resources_t *res = PR_Resources_Find (pr, "InputLine");
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0), "InputLine_Destroy");

	Con_DestroyInputLine (line->line);
	*line->prev = line->next;
	if (line->next)
		line->next->prev = line->prev;
	il_data_free (res, line);
}

static void
bi_InputLine_Clear (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0), "InputLine_Clear");
	int         save = P_INT (pr, 1);

	Con_ClearTyping (line->line, save);
}

static void
bi_InputLine_Process (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0), "InputLine_Process");
	int         ch = P_INT (pr, 1);

	Con_ProcessInputLine (line->line, ch);
}

/*
	bi_InputLine_SetText

	Sets the inputline to a specified text
*/
static void
bi_InputLine_SetText (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0), "InputLine_SetText");
	const char *str = P_GSTRING (pr, 1);
	inputline_t *il = line->line;

	/* this was segfault trap:
		 il->lines[il->edit_line][0] is promt character
	*/
	strncpy(il->lines[il->edit_line] + 1,str,il->line_size - 1);
	il->lines[il->edit_line][il->line_size-1] = '\0';
}

/*
	bi_InputLine_GetText

	Gets the text from a inputline
*/
static void
bi_InputLine_GetText (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0), "InputLine_GetText");
	inputline_t *il = line->line;

	RETURN_STRING(pr, il->lines[il->edit_line]+1);
}

static void
bi_InputLine_Draw (progs_t *pr)
{
	il_data_t  *line = get_inputline (pr, P_INT (pr, 0), "InputLine_Draw");

	line->line->draw (line->line);
}

static builtin_t builtins[] = {
	{"InputLine_Create",		bi_InputLine_Create,		-1},
	{"InputLine_SetPos",		bi_InputLine_SetPos,		-1},
	{"InputLine_SetCursor",		bi_InputLine_SetCursor,		-1},
	{"InputLine_SetEnter|^{tag _inputline_t=}(v*^v)^v",
								bi_InputLine_SetEnter,		-1},
	{"InputLine_SetEnter|^{tag _inputline_t=}(@@:.)@:",
								bi_InputLine_SetEnter,		-1},
	{"InputLine_SetWidth",		bi_InputLine_SetWidth,		-1},
	{"InputLine_SetText",		bi_InputLine_SetText,		-1},
	{"InputLine_GetText",		bi_InputLine_GetText,		-1},
	{"InputLine_Destroy",		bi_InputLine_Destroy,		-1},
	{"InputLine_Clear",			bi_InputLine_Clear,			-1},
	{"InputLine_Process",		bi_InputLine_Process,		-1},
	{"InputLine_Draw",			bi_InputLine_Draw,			-1},
	{0}
};

VISIBLE void
InputLine_Progs_Init (progs_t *pr)
{
	il_resources_t *res = calloc (1, sizeof (il_resources_t));

	PR_Resources_Register (pr, "InputLine", res, bi_il_clear);
	PR_RegisterBuiltins (pr, builtins);
}

VISIBLE void
InputLine_Progs_SetDraw (progs_t *pr, void (*draw)(inputline_t *))
{
	il_resources_t *res = PR_Resources_Find (pr, "InputLine");
	res->draw = draw;
}
