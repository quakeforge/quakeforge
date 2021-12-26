/*
	complete.c

	command completion

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/console.h"

#include "QF/ui/inputline.h"

#include "compat.h"

/*
  Con_BasicCompleteCommandLine

  New function for tab-completion system
  Added by EvilTypeGuy
  Thanks to Fett erich@heintz.com
  Thanks to taniwha
*/
VISIBLE void
Con_BasicCompleteCommandLine (inputline_t *il)
{
	const char *s;
	const char *cmd = "", **list[3] = {0, 0, 0};
	int         cmd_len, c, i, v, o;
	cbuf_t     *cbuf;

	s = il->lines[il->edit_line] + 1;

	if (*s == '/')
		s++;

	cbuf = con_module->data->console->cbuf;

	if (cbuf->interpreter->complete) {
		const char **completions = cbuf->interpreter->complete (cbuf, s);
		const char **com;

		for (o = 0, com = completions; *com; com++, o++)
			;

		c = v = 0;

		list[2] = completions;
	} else {
		// Count number of possible matches
		c = Cmd_CompleteCountPossible (s);
		v = Cvar_CompleteCountPossible (s);
		o = 0;
	}

	if (!(c + v + o))	// No possible matches
		return;

	if (c + v + o == 1) {
		if (c) {
			list[0] = Cmd_CompleteBuildList (s);
			cmd = *list[0];
		} else if (v) {
			list[0] = Cvar_CompleteBuildList (s);
			cmd = *list[0];
		} else {
			cmd = *list[2];
		}
		cmd_len = strlen (cmd);
	} else {
		if (c)
			cmd = *(list[0] = Cmd_CompleteBuildList (s));
		if (v)
			cmd = *(list[1] = Cvar_CompleteBuildList (s));
		if (o)
			cmd = *(list[2]);

		cmd_len = 0;
		do {
			for (i = 0; i < 3; i++) {
				char        ch = cmd[cmd_len];
				const char **l = list[i];
				if (l) {
					while (*l && (*l)[cmd_len] == ch)
						l++;
					if (*l)
						break;
				}
			}
			if (i == 3)
				cmd_len++;
		} while (i == 3);
		// 'quakebar'
		Sys_Printf ("\n\35");
		for (i = 0; i < con_linewidth - 4; i++)
			Sys_Printf ("\36");
		Sys_Printf ("\37\n");

		// Print Possible Commands
		if (c) {
			Sys_Printf ("%i possible command%s\n", c, (c > 1) ? "s: " : ":");
			Con_DisplayList (list[0], con_linewidth);
		}

		if (v) {
			Sys_Printf ("%i possible variable%s\n", v, (v > 1) ? "s: " : ":");
			Con_DisplayList (list[1], con_linewidth);
		}
		if (o) {
			Sys_Printf ("%i possible matche%s\n", o, (o > 1) ? "s: " : ":");
			Con_DisplayList (list[2], con_linewidth);
		}
	}

	if (cmd) {
		unsigned    bound = max (0, (int) strlen (s) - (int) cmd_len);
		const char *overwrite;

		if (cmd_len > 0)
			while (bound < strlen (s)
				   && strncmp (s + bound, cmd, strlen (s + bound)))
				bound++;

		overwrite = va (0, "%.*s%.*s", bound, s, cmd_len, cmd);

		il->lines[il->edit_line][1] = '/';
		strncpy (il->lines[il->edit_line] + 2, overwrite, il->line_size - 3);
		il->lines[il->edit_line][il->line_size-1] = 0;
		il->linepos = strlen (overwrite) + 2;
		if (c + v == 1 && !o) {
			il->lines[il->edit_line][il->linepos] = ' ';
			il->linepos++;
		}
		il->lines[il->edit_line][il->linepos] = 0;
	}
	for (i = 0; i < 3; i++)
		if (list[i])
			free (list[i]);
}
