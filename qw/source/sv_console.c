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

#include "server.h"

#ifdef HAVE_CURSES_H
static WINDOW *output;
static WINDOW *status;
static WINDOW *input;
static int screen_x, screen_y;

#define     MAXCMDLINE  256
char        key_lines[32][MAXCMDLINE];
int         edit_line;
int         history_line;
int         key_linepos;

static chtype attr_table[4] = {
	A_NORMAL,
	COLOR_PAIR(1),
	COLOR_PAIR(2),
	COLOR_PAIR(3),
};

static const byte attr_map[256] = {
	3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 3, 3, 0, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 3, 3, 0, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
#endif

void
Con_Init (const char *plugin_name)
{
#ifdef HAVE_CURSES_H
	int i;

	for (i = 0; i < 32; i++) {
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	initscr ();
	start_color ();
	cbreak ();
	noecho ();

	nonl ();
	intrflush (stdscr, FALSE);

	getmaxyx (stdscr, screen_y, screen_x);
	output = subwin (stdscr, screen_y - 2, screen_x, 0, 0);
	status = subwin (stdscr, 1, screen_x, screen_y - 2, 0);
	input  = subwin (stdscr, 1, screen_x, screen_y - 1, 0);

	init_pair (1, COLOR_YELLOW, COLOR_BLACK);
	init_pair (2, COLOR_GREEN, COLOR_BLACK);
	init_pair (3, COLOR_RED, COLOR_BLACK);
	init_pair (4, COLOR_YELLOW, COLOR_BLUE);

	scrollok (output, TRUE);
	leaveok (output, TRUE);

	scrollok (status, FALSE);
	leaveok (status, TRUE);

	scrollok (input, FALSE);
	nodelay (input, TRUE);
	keypad (input, TRUE);

	wclear (output);
	wbkgdset (status, COLOR_PAIR(4));
	wclear (status);
	wclear (input);
	touchwin (stdscr);
	wrefresh (output);
	wrefresh (status);
	wrefresh (input);
#else
	if (setvbuf (stdout, 0, _IONBF, BUFSIZ))
		Con_Printf ("warning: couldn't set stdout to linebuffered.\n");
#endif
}

void
Con_Shutdown (void)
{
#ifdef HAVE_CURSES_H
	endwin ();
#endif
}

void
Con_Print (const char *txt)
{
#ifdef HAVE_CURSES_H
	if (output) {
		chtype      ch;

		while ((ch = (byte)*txt++)) {
			ch = sys_char_map[ch] | attr_table[attr_map[ch]];
			waddch (output, ch);
		}
		touchwin (stdscr);
		wrefresh (output);
	} else {
#endif
		int ch;
		while ((ch = (byte)*txt++)) {
			ch = sys_char_map[ch];
			putchar (ch);
		}
#ifdef HAVE_CURSES_H
	}
#endif
}

void
Con_ProcessInput (inputline_t *il, int ch)
{
#ifdef HAVE_CURSES_H
	int         i;
	int         curs_x;
	char       *text = 0;

	static int  scroll;

	ch = wgetch (input);
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
			SV_Printf ("%s\n", key_lines[edit_line]);
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
		case '\b':
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
#else
	const char *cmd;

	while (1) {
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd);
	}
#endif
}

#ifdef HAVE_CURSES_H
/*
	Con_CompleteCommandLine

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
void
Con_CompleteCommandLine (void)
{
	const char	*cmd = "";
	char	*s;
	int	c, v, a, i;
	int		cmd_len;
	const char	**list[3] = {0, 0, 0};

	s = key_lines[edit_line] + 1;
	if (*s == '\\' || *s == '/')
		s++;

	// Count number of possible matches
	c = Cmd_CompleteCountPossible(s);
	v = Cvar_CompleteCountPossible(s);
	a = Cmd_CompleteAliasCountPossible(s);
	
	if (!(c + v + a))	// No possible matches
		return;
	
	if (c + v + a == 1) {
		if (c)
			list[0] = Cmd_CompleteBuildList(s);
		else if (v)
			list[0] = Cvar_CompleteBuildList(s);
		else
			list[0] = Cmd_CompleteAliasBuildList(s);
		cmd = *list[0];
		cmd_len = strlen (cmd);
	} else {
		if (c)
			cmd = *(list[0] = Cmd_CompleteBuildList(s));
		if (v)
			cmd = *(list[1] = Cvar_CompleteBuildList(s));
		if (a)
			cmd = *(list[2] = Cmd_CompleteAliasBuildList(s));
		
		cmd_len = strlen (s);
		do {
			for (i = 0; i < 3; i++) {
				char ch = cmd[cmd_len];
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
		SV_Printf("\n\35");
		for (i = 0; i < screen_x - 4; i++)
			SV_Printf("\36");
		SV_Printf("\37\n");

		// Print Possible Commands
		if (c) {
			SV_Printf("%i possible command%s\n", c, (c > 1) ? "s: " : ":");
			Con_DisplayList(list[0], screen_x);
		}
		
		if (v) {
			SV_Printf("%i possible variable%s\n", v, (v > 1) ? "s: " : ":");
			Con_DisplayList(list[1], screen_x);
		}
		
		if (a) {
			SV_Printf("%i possible aliases%s\n", a, (a > 1) ? "s: " : ":");
			Con_DisplayList(list[2], screen_x);
		}
	}
	
	if (cmd) {
		key_lines[edit_line][1] = '/';
		strncpy(key_lines[edit_line] + 2, cmd, cmd_len);
		key_linepos = cmd_len + 2;
		if (c + v + a == 1) {
			key_lines[edit_line][key_linepos] = ' ';
			key_linepos++;
		}
		key_lines[edit_line][key_linepos] = 0;
	}
	for (i = 0; i < 3; i++)
		if (list[i])
			free (list[i]);
}
#endif
