/*
	sv_console.c

	ncurses console for the server

	Copyright (C) 2001       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/10

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_CURSES_H
# include <curses.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

static WINDOW *output;
static WINDOW *status;
static WINDOW *input;
static int screen_x, screen_y;

#define     MAXCMDLINE  256
char        key_lines[32][MAXCMDLINE];
int         edit_line;
int         history_line;
int         key_linepos;

void
Con_ProcessInput (void)
{
	int         ch = wgetch (input);
	int         i;
	int         curs_x;
	char       *text = 0;

	static int  scroll;

	switch (ch) {
		case KEY_ENTER:
		case '\n':
		case '\r':
			if (key_lines[edit_line][1] == '/'
				&& key_lines[edit_line][2] == '/')
				goto no_lf;
			else if (key_lines[edit_line][1] == '\\'
					 || key_lines[edit_line][1] == '/')
				Cbuf_AddText (key_lines[edit_line] + 2);
			else
				Cbuf_AddText (key_lines[edit_line] + 1);
			Cbuf_AddText ("\n");
		  no_lf:
			Con_Printf ("%s\n", key_lines[edit_line]);
			edit_line = (edit_line + 1) & 31;
			history_line = edit_line;
			key_lines[edit_line][0] = ']';
			key_lines[edit_line][1] = 0;
			key_linepos = 1;
			break;
		case '\t':
			Con_CompleteCommandLine();
			break;
		case KEY_BACKSPACE:
			if (key_linepos > 1) {
				strcpy (key_lines[edit_line] + key_linepos - 1,
						key_lines[edit_line] + key_linepos);
				key_linepos--;
			}
			break;
		case KEY_DC:
			if (key_linepos < strlen (key_lines[edit_line]))
				strcpy (key_lines[edit_line] + key_linepos,
						key_lines[edit_line] + key_linepos + 1);
			break;
		case KEY_RIGHT:
			if (key_linepos < strlen (key_lines[edit_line]))
				key_linepos++;
			break;
		case KEY_LEFT:
			if (key_linepos > 1)
				key_linepos--;
			break;
		case KEY_UP:
			do {
				history_line = (history_line - 1) & 31;
			} while (history_line != edit_line && !key_lines[history_line][1]);
			if (history_line == edit_line)
				history_line = (edit_line + 1) & 31;
			strcpy (key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen (key_lines[edit_line]);
			break;
		case KEY_DOWN:
			if (history_line == edit_line)
				break;
			do {
				history_line = (history_line + 1) & 31;
			} while (history_line != edit_line && !key_lines[history_line][1]);
			if (history_line == edit_line) {
				key_lines[edit_line][0] = ']';
				key_lines[edit_line][1] = 0;
				key_linepos = 1;
			} else {
				strcpy (key_lines[edit_line], key_lines[history_line]);
				key_linepos = strlen (key_lines[edit_line]);
			}
			break;
		case KEY_PPAGE:
			break;
		case KEY_NPAGE:
			break;
		case KEY_HOME:
			key_linepos = 1;
			break;
		case KEY_END:
			key_linepos = strlen (key_lines[edit_line]);
			break;
		default:
			if (ch >= ' ' && ch < 127) {
				i = strlen (key_lines[edit_line]);
				if (i >= MAXCMDLINE - 1)
					break;
				// This also moves the ending \0
				memmove (key_lines[edit_line] + key_linepos + 1,
						key_lines[edit_line] + key_linepos,
						i - key_linepos + 1);
				key_lines[edit_line][key_linepos] = ch;
				key_linepos++;
			}
			break;
	}

	i = key_linepos - 1;
	if (scroll > i)
		scroll = i;
	if (scroll < i - (screen_x - 2) + 1)
		scroll = i - (screen_x - 2) + 1;
	text = key_lines[edit_line] + scroll + 1;
	if ((int)strlen (text) < screen_x - 2) {
		scroll = strlen (key_lines[edit_line] + 1) - (screen_x - 2);
		if (scroll < 0)
			scroll = 0;
		text = key_lines[edit_line] + scroll + 1;
	}

	curs_x = key_linepos - scroll;

	wmove (input, 0, 0);
	if (scroll) {
		waddch (input, '<');
	} else {
		waddch (input, key_lines[edit_line][0]);
	}
	for (i = 0; i < screen_x - 2 && *text; i++)
		waddch (input, *text++);
	while (i++ < screen_x - 2)
		waddch (input, ' ');
	if (*text) {
		waddch (input, '>');
	} else {
		waddch (input, ' ');
	}
	wmove (input, 0, curs_x);
	touchline (stdscr, screen_y - 1, 1);
	wrefresh (input);
}
