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
static const char rcsid[] = 
	"$Id$";

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
#include "QF/draw.h"
#include "QF/progs.h"
#include "QF/sys.h"
#include "QF/zone.h"

typedef struct {
	inputline_t **lines;
	int         max_lines;
} il_resources_t;

//FIXME need to robustify the interface to avoid segfaults caused by errant
//progs

static inputline_t *
get_inputline (progs_t *pr, int arg, const char *func)
{
	pr_type_t  *handle;
	inputline_t *line;

	if (arg <= ((pr_type_t *) pr->zone - pr->pr_globals)
		|| arg >= (pr->zone_size / sizeof (pr_type_t)))
		PR_RunError (pr, "%s: Invalid inputline_t", func);

	handle = pr->pr_globals + arg;

	line = *(inputline_t **)handle;
	if (!line)
		PR_RunError (pr, "Invalid inputline_t");

	return line;
}

static void
bi_InputLine_Create (progs_t *pr)
{
	il_resources_t *res = PR_Resources_Find (pr, "InputLine");
	inputline_t **line = 0;
	int         i;
	int         lines = G_INT (pr, OFS_PARM0);
	int         size = G_INT (pr, OFS_PARM1);
	int         prompt = G_INT (pr, OFS_PARM2);
	pr_type_t  *handle;

	for (i = 0; i < res->max_lines; i++)
		if (!res->lines[i]) {
			line = &res->lines[i];
			break;
		}
	if (!line) {
		Sys_Printf ("out of resources\n");
		G_INT (pr, OFS_RETURN) = 0;
		return;
	}
	*line = Con_CreateInputLine (lines, size, prompt);
	if (!*line) {
		Sys_Printf ("failed to create inputline\n");
		G_INT (pr, OFS_RETURN) = 0;
		return;
	}
	handle = PR_Zone_Malloc (pr, sizeof (inputline_t *));
	*(inputline_t**)handle = *line;
	G_INT (pr, OFS_RETURN) = handle - pr->pr_globals;
}

static void
bi_InputLine_SetWidth (progs_t *pr)
{
	inputline_t *line = get_inputline (pr, G_INT (pr, OFS_PARM0),
									   "InputLine_SetWidth");
	int         width = G_INT (pr, OFS_PARM1);

	line->width = width;
}

static void
bi_InputLine_Destroy (progs_t *pr)
{
	il_resources_t *res = PR_Resources_Find (pr, "InputLine");
	pr_type_t  *handle;
	int         arg = G_INT (pr, OFS_PARM0);
	int         i;
	inputline_t *line;

	if (arg <= ((pr_type_t *) pr->zone - pr->pr_globals)
		|| arg >= (pr->zone_size / sizeof (pr_type_t)))
		PR_RunError (pr, "InputLine_Destroy: Invalid inputline_t");

	handle = pr->pr_globals + arg;

	line = *(inputline_t **)handle;
	if (!line)
		PR_RunError (pr, "InputLine_Destroy: Invalid inputline_t");

	for (i = 0; i < res->max_lines; i++)
		if (res->lines[i] == line) {
			Con_DestroyInputLine (line);
			res->lines = 0;
			PR_Zone_Free (pr, handle);
		}
}

static void
bi_InputLine_Clear (progs_t *pr)
{
	inputline_t *line = get_inputline (pr, G_INT (pr, OFS_PARM0),
									   "InputLine_Clear");

	Con_ClearTyping (line);
}

static void
bi_InputLine_Process (progs_t *pr)
{
	inputline_t *line = get_inputline (pr, G_INT (pr, OFS_PARM0),
									   "InputLine_Process");
	int         ch = G_INT (pr, OFS_PARM1);

	Con_ProcessInputLine (line, ch);
}

/*
	bi_InputLine_SetText

	Sets the inputline to a specified text
*/
static void
bi_InputLine_SetText (progs_t *pr)
{
	inputline_t *il = get_inputline (pr, G_INT (pr, OFS_PARM0),
									 "InputLine_SetText");
	const char  *str = G_STRING (pr, OFS_PARM1);

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
	inputline_t *il = get_inputline (pr, G_INT (pr, OFS_PARM0),
									 "InputLine_GetText");

	RETURN_STRING(pr, il->lines[il->edit_line]+1);
}

static void
bi_InputLine_Draw (progs_t *pr)
{
	inputline_t *il = get_inputline (pr, G_INT (pr, OFS_PARM0),
									 "InputLine_Draw");
	int         x = G_INT (pr, OFS_PARM1);
	int         y = G_INT (pr, OFS_PARM2);
	int         cursor = G_INT (pr, OFS_PARM3);
	const char *s = il->lines[il->edit_line] + il->scroll;

	if (il->scroll) {
		Draw_Character (x, y, '<' | 0x80);
		Draw_nString (x + 8, y, s + 1, il->width - 2);
	} else {
		Draw_nString (x, y, s, il->width - 1);
	}
	if (cursor)
		Draw_Character (x + ((il->linepos - il->scroll) << 3), y,
						10 + ((int) (*pr->time * 4) & 1));
	if (strlen (s) >= il->width)
		Draw_Character (x + ((il->width - 1) << 3), y, '>' | 0x80);
}

static void
bi_il_clear (progs_t *pr, void *data)
{
	il_resources_t *res = (il_resources_t *)data;
	int         i;

	for (i = 0; i < res->max_lines; i++)
		if (res->lines[i]) {
			Con_DestroyInputLine (res->lines[i]);
			res->lines[i] = 0;
		}
}

void
InputLine_Progs_Init (progs_t *pr)
{
	il_resources_t *res = malloc (sizeof (il_resources_t));
	res->max_lines = 64;
	res->lines = calloc (sizeof (inputline_t *), res->max_lines);

	PR_Resources_Register (pr, "InputLine", res, bi_il_clear);
	PR_AddBuiltin (pr, "InputLine_Create", bi_InputLine_Create, -1);
	PR_AddBuiltin (pr, "InputLine_SetWidth", bi_InputLine_SetWidth, -1);
	PR_AddBuiltin (pr, "InputLine_SetText", bi_InputLine_SetText, -1);
	PR_AddBuiltin (pr, "InputLine_GetText", bi_InputLine_GetText, -1);
	PR_AddBuiltin (pr, "InputLine_Destroy", bi_InputLine_Destroy, -1);
	PR_AddBuiltin (pr, "InputLine_Clear", bi_InputLine_Clear, -1);
	PR_AddBuiltin (pr, "InputLine_Process", bi_InputLine_Process, -1);
	PR_AddBuiltin (pr, "InputLine_Draw", bi_InputLine_Draw, -1);
}
