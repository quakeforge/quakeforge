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
#ifdef HAVE_CURSES_H
# include <curses.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/plugin.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "compat.h"

extern int  con_linewidth;

#ifdef HAVE_CURSES_H
static int use_curses = 1;

static WINDOW *output;
static WINDOW *status;
static WINDOW *input;
static int screen_x, screen_y;

#define     MAXCMDLINE  256
static inputline_t *input_line;

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


static void
C_ExecLine (const char *line)
{
	if (line[0] == '/')
		line++;
	Cbuf_AddText (line);
}
#endif


static void
C_Init (void)
{
#ifdef HAVE_CURSES_H
	cvar_t     *curses = Cvar_Get ("sv_use_curses", "1", CVAR_ROM, NULL,
								   "set to 0 to disable curses server console");
	use_curses = curses->int_val;
	if (use_curses) {
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

		input_line = Con_CreateInputLine (16, MAXCMDLINE, ']');
		input_line->complete = Con_BasicCompleteCommandLine;
		input_line->enter = C_ExecLine;

		con_linewidth = screen_x;
	} else
#endif
		setvbuf (stdout, 0, _IOLBF, BUFSIZ);
}

static void
C_Shutdown (void)
{
#ifdef HAVE_CURSES_H
	if (use_curses)
		endwin ();
#endif
}

static void
C_Print (const char *fmt, va_list args)
{
	static unsigned char *buffer;
	unsigned char *txt;
	int         size;
	static int  buffer_size;

	size = vsnprintf (buffer, buffer_size, fmt, args) + 1;	// +1 for nul
	if (size > buffer_size) {
		buffer_size = (size + 1023) & ~1023; // 1k multiples
		buffer = realloc (buffer, buffer_size);
		if (!buffer)
			Sys_Error ("console: could not allocate %d bytes\n",
					   buffer_size);
		vsnprintf (buffer, buffer_size, fmt, args);
	}

	txt = buffer;
#ifdef HAVE_CURSES_H
	if (use_curses) {
		chtype			ch;

		txt = buffer;

		while ((ch = (byte)*txt++)) {
			ch = sys_char_map[ch] | attr_table[attr_map[ch]];
			waddch (output, ch);
		}
		touchwin (stdscr);
		wrefresh (output);
	} else
#endif
	{
		while (*txt)
			putc (sys_char_map[*txt++], stdout);
		fflush (stdout);
	}
}

static void
C_ProcessInput (void)
{
#ifdef HAVE_CURSES_H
	if (use_curses) {
		int         ch = wgetch (input), i;
		const char *text;

		switch (ch) {
			case KEY_ENTER:
			case '\n':
			case '\r':
				ch = K_RETURN;
				break;
			case '\t':
				ch = K_TAB;
				break;
			case KEY_BACKSPACE:
			case '\b':
				ch = K_BACKSPACE;
				break;
			case KEY_DC:
				ch = K_DELETE;
				break;
			case KEY_RIGHT:
				ch = K_RIGHT;
				break;
			case KEY_LEFT:
				ch = K_LEFT;
				break;
			case KEY_UP:
				ch = K_UP;
				break;
			case KEY_DOWN:
				ch = K_DOWN;
				break;
			case KEY_HOME:
				ch = K_HOME;
				break;
			case KEY_END:
				ch = K_END;
				break;
			case KEY_NPAGE:
				ch = K_PAGEDOWN;
				break;
			case KEY_PPAGE:
				ch = K_PAGEUP;
				break;
			default:
				if (ch < 0 || ch >= 256)
					ch = 0;
		}
		if (ch)
			Con_ProcessInputLine (input_line, ch);
		i = input_line->linepos - 1;
		if (input_line->scroll > i)
			input_line->scroll = i;
		if (input_line->scroll < i - (screen_x - 2) + 1)
			input_line->scroll = i - (screen_x - 2) + 1;
		text = input_line->lines[input_line->edit_line] + input_line->scroll;
		if ((int)strlen (text + 1) < screen_x - 2) {
			text = input_line->lines[input_line->edit_line];
			input_line->scroll = strlen (text + 1) - (screen_x - 2);
			input_line->scroll = max (input_line->scroll, 0);
			text += input_line->scroll;
		}
		wmove (input, 0, 0);
		if (input_line->scroll) {
			waddch (input, '<');
		} else {
			waddch (input, *text++);
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
		wmove (input, 0, input_line->linepos - input_line->scroll);
		touchline (stdscr, screen_y - 1, 1);
		wrefresh (input);
	} else
#endif
		while (1) {
			const char *cmd = Sys_ConsoleInput ();
			if (!cmd)
				break;
			Cbuf_AddText (cmd);
		}
}

static general_funcs_t plugin_info_general_funcs = {
	C_Init,
	C_Shutdown,
};
static general_data_t plugin_info_general_data;

static console_funcs_t plugin_info_console_funcs = {
	C_Print,
	C_ProcessInput,
};
static console_data_t plugin_info_console_data;

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	0,
	&plugin_info_console_funcs,
};

static plugin_data_t plugin_info_data = {
	&plugin_info_general_data,
	0,
	0,
	&plugin_info_console_data,
};

static plugin_t plugin_info = {
	qfp_console,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"server console driver",
	"Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge"
		" project\n"
		"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

plugin_t *
console_server_PluginInfo (void)
{
	return &plugin_info;
}
