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
#include "QF/plugin.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "compat.h"

#ifdef HAVE_CURSES_H
static int use_curses = 1;

static WINDOW *output;
static WINDOW *status;
static WINDOW *input;
static int screen_x, screen_y;

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

static void
C_Init (void)
{
#ifdef HAVE_CURSES_H
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
#ifdef HAVE_CURSES_H
	if (use_curses) {
		static char *buffer;
		static int buffer_size;
		char      *txt;
		chtype     ch;
		int size;

		size = vsnprintf (buffer, buffer_size, fmt, args);
		if (size + 1 > buffer_size) {
			buffer_size = (size + 1 + 1024) % 1024;	// 1k multiples
			buffer = realloc (buffer, buffer_size);
			if (!buffer)
				Sys_Error ("console: could not allocate %d bytes\n",
						   buffer_size);
			vsnprintf (buffer, buffer_size, fmt, args);
		}

		txt = buffer;

		while ((ch = *txt++)) {
			ch = sys_char_map[ch] | attr_table[attr_map[ch]];
			waddch (output, ch);
		}
		touchwin (stdscr);
		wrefresh (output);
	} else
#endif
		vfprintf (stdout, fmt, args);
}

static general_funcs_t plugin_info_general_funcs = {
	C_Init,
	C_Shutdown,
};
static general_data_t plugin_info_general_data;

static console_funcs_t plugin_info_console_funcs = {
	C_Print,
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
