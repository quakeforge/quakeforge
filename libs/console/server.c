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

*/
static const char rcsid[] = 
	"$Id$";

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

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif
#include <signal.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/keys.h"
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
static int interrupted;

#define     MAXCMDLINE  256
static inputline_t *input_line;

#define BUFFER_SIZE 32768
#define MAX_LINES 1024
static con_buffer_t *output_buffer;

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

static inline void
draw_fun_char (WINDOW *win, byte c)
{
	chtype      ch = c;
	ch = sys_char_map[ch] | attr_table[attr_map[ch]];
	waddch (win, ch);
}

static void
C_DrawOutput (void)
{
	// this is not the most efficient way to update the screen, but oh well
	int         lines = screen_y - 3; // leave a blank line
	int         width = screen_x;
	int         cur_line = output_buffer->cur_line;
	int         i, y;

	if (lines < 1)
		return;

	for (y = i = 0; y < lines; i++, y++) {
		con_line_t *l = Con_BufferLine (output_buffer, cur_line - i);
		if (!l->text) {
			i--;
			break;
		}
		y += l->len / width; // really dumb line wrap algo :)
	}
	cur_line -= i;
	y -= lines;
	wclear (output);
	wmove (output, 0, 0);
	do {
		con_line_t *l = Con_BufferLine (output_buffer, cur_line++);
		byte       *text = l->text;
		int         len = l->len;

		if (y > 0) {
			text += y * width;
			len -= y * width;
			y = 0; 
			if (len < 1) {
				len = 1;
				text = l->text + l->len - 1;
			}
		}
		while (len--)
			draw_fun_char (output, *text++);
	} while (cur_line < output_buffer->cur_line);
	//wrefresh (stdscr);
	wrefresh (output);
}

static void
C_DrawStatus (void)
{
	wbkgdset (status, COLOR_PAIR(4));
	wclear (status);
	wrefresh (status);
}

static void
C_DrawInput (inputline_t *il)
{
	WINDOW     *win = (WINDOW *) il->user_data;
	int         i;
	const char *text;

	text = il->lines[il->edit_line] + il->scroll;
	wmove (win, 0, 0);
	if (il->scroll) {
		waddch (win, '<' | COLOR_PAIR (5));
		text++;
	} else {
		waddch (win, *text++);
	}
	for (i = 0; i < il->width - 2 && *text; i++) {
		chtype      ch = (byte)*text++;
		ch = sys_char_map[ch] | attr_table[attr_map[ch]];
		waddch (win, ch);
	}
	while (i++ < il->width - 2) {
		waddch (win, ' ');
	}
	if (*text) {
		waddch (win, '>' | COLOR_PAIR (5));
	} else {
		waddch (win, ' ');
	}
	wmove (win, 0, il->linepos - il->scroll);
	wrefresh (win);
}

static void
sigwinch (int sig)
{
	interrupted = 1;
	signal (SIGWINCH, sigwinch);
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
		signal (SIGWINCH, sigwinch);

		initscr ();
		start_color ();
		cbreak ();
		noecho ();

		nonl ();
		intrflush (stdscr, FALSE);

		getmaxyx (stdscr, screen_y, screen_x);
		output = newwin (screen_y - 2, screen_x, 0, 0);
		status = newwin (1, screen_x, screen_y - 2, 0);
		input  = newwin (1, screen_x, screen_y - 1, 0);

		init_pair (1, COLOR_YELLOW, COLOR_BLACK);
		init_pair (2, COLOR_GREEN, COLOR_BLACK);
		init_pair (3, COLOR_RED, COLOR_BLACK);
		init_pair (4, COLOR_YELLOW, COLOR_BLUE);
		init_pair (5, COLOR_CYAN, COLOR_BLACK);

		scrollok (output, TRUE);
		leaveok (output, TRUE);

		scrollok (status, FALSE);
		leaveok (status, TRUE);

		scrollok (input, FALSE);
		leaveok (input, FALSE);
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

		output_buffer = Con_CreateBuffer (BUFFER_SIZE, MAX_LINES);

		input_line = Con_CreateInputLine (16, MAXCMDLINE, ']');
		input_line->complete = Con_BasicCompleteCommandLine;
		input_line->enter = C_ExecLine;
		input_line->width = screen_x;
		input_line->user_data = input;
		input_line->draw = C_DrawInput;

		con_linewidth = screen_x;

		C_DrawOutput ();
		C_DrawStatus ();
		C_DrawInput (input_line);
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
	//printf ("size = %d\n", size);
	while (size <= 0 || size > buffer_size) {
		if (size > 0)
			buffer_size = (size + 1023) & ~1023; // 1k multiples
		else
			buffer_size += 1024;
		buffer = realloc (buffer, buffer_size);
		if (!buffer)
			Sys_Error ("console: could not allocate %d bytes\n",
					   buffer_size);
		size = vsnprintf (buffer, buffer_size, fmt, args) + 1;
		//printf ("size = %d\n", size);
	}

	txt = buffer;
#ifdef HAVE_CURSES_H
	if (use_curses) {
		Con_BufferAddText (output_buffer, buffer);
		while (*txt)
			draw_fun_char (output, *txt++);
		wrefresh (output);
		wrefresh (input);	// move the screen cursor back to the input line
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
		int         ch;

		if (interrupted) {
			struct winsize size;

			interrupted = 0;
			if (ioctl (fileno (stdout), TIOCGWINSZ, &size) == 0) {
				resizeterm (size.ws_row, size.ws_col);
				getmaxyx (stdscr, screen_y, screen_x);
				con_linewidth = screen_x;
				input_line->width = screen_x;
				/* while a little ugly, this is needed because ncurses auto
				 * resizing doesn't always do the right thing
				 */
				wresize (input, 1, screen_x);
				mvwin (input, screen_y - 1, 0);
				wresize (status, 1, screen_x);
				mvwin (status, screen_y - 2, 0);
				wresize (output, screen_y - 2, screen_x);
				mvwin (output, 0, 0);
				wrefresh (curscr);

				C_DrawOutput ();
				C_DrawStatus ();
				// input gets drawn below in Con_ProcessInputLine
			}
		}

		ch = wgetch (input);
		switch (ch) {
			case KEY_ENTER:
			case '\n':
			case '\r':
				ch = QFK_RETURN;
				break;
			case '\t':
				ch = QFK_TAB;
				break;
			case KEY_BACKSPACE:
			case '\b':
				ch = QFK_BACKSPACE;
				break;
			case KEY_DC:
				ch = QFK_DELETE;
				break;
			case KEY_RIGHT:
				ch = QFK_RIGHT;
				break;
			case KEY_LEFT:
				ch = QFK_LEFT;
				break;
			case KEY_UP:
				ch = QFK_UP;
				break;
			case KEY_DOWN:
				ch = QFK_DOWN;
				break;
			case KEY_HOME:
				ch = QFK_HOME;
				break;
			case KEY_END:
				ch = QFK_END;
				break;
			case KEY_NPAGE:
				ch = QFK_PAGEDOWN;
				break;
			case KEY_PPAGE:
				ch = QFK_PAGEUP;
				break;
			default:
				if (ch < 0 || ch >= 256)
					ch = 0;
		}
		Con_ProcessInputLine (input_line, ch);
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

QFPLUGIN plugin_t *
console_server_PluginInfo (void)
{
	return &plugin_info;
}
