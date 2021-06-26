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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_NCURSES
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
#if defined(_WIN32) && defined(HAVE_MALLOC_H)
# include <malloc.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif
#include <signal.h>
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/keys.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/general.h"
#include "QF/plugin/console.h"

#include "QF/ui/view.h"

#include "QF/ui/inputline.h"

#include "compat.h"
#include "sv_console.h"

static console_data_t sv_con_data;

static QFile  *log_file;
static cvar_t *sv_logfile;
static cvar_t *sv_conmode;

static void C_KeyEvent (knum_t key, short unicode, qboolean down);

#ifdef HAVE_NCURSES

enum {
	sv_resize_x = 1,
	sv_resize_y = 2,
	sv_scroll	= 4,
	sv_cursor	= 8,
};

static int use_curses = 1;

static view_t *output;
static view_t *status;
static view_t *input;
static int screen_x, screen_y;
static volatile sig_atomic_t interrupted;
static int batch_print;

#define     MAXCMDLINE  256

#define BUFFER_SIZE 32768
#define MAX_LINES 1024
static int view_offset;

#define CP_YELLOW_BLACK		(1)
#define CP_GREEN_BLACK		(2)
#define CP_RED_BLACK		(3)
#define CP_CYAN_BLACK		(4)
#define CP_MAGENTA_BLACK	(5)
#define CP_YELLOW_BLUE		(6)
#define CP_GREEN_BLUE		(7)
#define CP_RED_BLUE			(8)
#define CP_CYAN_BLUE		(9)
#define CP_MAGENTA_BLUE		(10)

static chtype attr_table[16] = {
	A_NORMAL,
	COLOR_PAIR (CP_GREEN_BLACK),
	COLOR_PAIR (CP_RED_BLACK),
	0,
	COLOR_PAIR (CP_YELLOW_BLACK),
	COLOR_PAIR (CP_CYAN_BLACK),
	COLOR_PAIR (CP_MAGENTA_BLACK),
	0,
	A_NORMAL,
	COLOR_PAIR (CP_GREEN_BLUE),
	COLOR_PAIR (CP_RED_BLUE),
	0,
	COLOR_PAIR (CP_YELLOW_BLUE),
	COLOR_PAIR (CP_CYAN_BLUE),
	COLOR_PAIR (CP_MAGENTA_BLUE),
	0,
};

static const byte attr_map[256] = {
	2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 2, 2, 0, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 0, 0, 6, 6, 0, 6, 6,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
};


static inline void
draw_fun_char (WINDOW *win, byte c, int blue)
{
	chtype      ch = c;
	int         offset = blue ? 8 : 0;
	ch = sys_char_map[ch] | attr_table[attr_map[ch] + offset];
	waddch (win, ch);
}

static inline void
sv_refresh (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	wnoutrefresh ((WINDOW *) sv_view->win);
}

static inline int
sv_getch (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	return wgetch ((WINDOW *) sv_view->win);
}

static inline void
sv_draw (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	if (sv_view->draw)
		sv_view->draw (view);
	wnoutrefresh ((WINDOW *) sv_view->win);
}

static inline void
sv_setgeometry (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	WINDOW *win = sv_view->win;
	wresize (win, view->ylen, view->xlen);
	mvwin (win, view->yabs, view->xabs);
	if (sv_view->setgeometry)
		sv_view->setgeometry (view);
}

static void
sv_complete (inputline_t *il)
{
	batch_print = 1;
	Con_BasicCompleteCommandLine (il);
	batch_print = 0;

	sv_refresh (output);
	sv_refresh (input);
	doupdate ();
}

static void
draw_output (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	WINDOW     *win = sv_view->win;
	con_buffer_t *output_buffer = sv_view->obj;

	// this is not the most efficient way to update the screen, but oh well
	int         lines = view->ylen - 1; // leave a blank line
	int         width = view->xlen;
	int         cur_line = output_buffer->cur_line + view_offset;
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
	wclear (win);
	wmove (win, 0, 0);
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
			draw_fun_char (win, *text++, 0);
	} while (cur_line < output_buffer->cur_line + view_offset);
}

static void
draw_status (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	WINDOW     *win = sv_view->win;
	sv_sbar_t  *sb = sv_view->obj;
	int         i;
	char       *old = alloca (sb->width);

	memcpy (old, sb->text, sb->width);
	memset (sb->text, ' ', sb->width);
	view_draw (view);
	if (memcmp (old, sb->text, sb->width)) {
		wbkgdset (win, COLOR_PAIR (CP_YELLOW_BLUE));
		wmove (win, 0, 0);
		for (i = 0; i < sb->width; i++)
			draw_fun_char (win, sb->text[i], 1);
	}
}

static void
draw_input_line (inputline_t *il)
{
	view_t     *view = il->user_data;
	sv_view_t  *sv_view = view->data;
	WINDOW     *win = sv_view->win;
	size_t      i;
	const char *text;

	text = il->lines[il->edit_line] + il->scroll;
	wmove (win, 0, 0);
	if (il->scroll) {
		waddch (win, '<' | COLOR_PAIR (CP_CYAN_BLACK));
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
		waddch (win, '>' | COLOR_PAIR (CP_CYAN_BLACK));
	} else {
		waddch (win, ' ');
	}
	wmove (win, 0, il->linepos - il->scroll);
}

static void
draw_input (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	draw_input_line (sv_view->obj);
}

static void
setgeometry_input (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	inputline_t *il = sv_view->obj;
	il->width = view->xlen;
}

static void
setgeometry_status (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	sv_sbar_t *sb = sv_view->obj;
	sb->width = view->xlen;
	sb->text = realloc (sb->text, sb->width);
	memset (sb->text, 0, sb->width);	// force an update
}
#ifdef SIGWINCH
static void
sigwinch (int sig)
{
	interrupted = 1;
}
#endif
static void
get_size (int *xlen, int *ylen)
{
#ifdef SIGWINCH
	struct winsize size;

	*xlen = *ylen = 0;
	if (ioctl (fileno (stdout), TIOCGWINSZ, &size) != 0)
		return;
	*xlen = size.ws_col;
	*ylen = size.ws_row;
#else
	getmaxyx (stdscr, *ylen, *xlen);
#endif
}

static void
process_input (void)
{
	int         ch;
	int         escape = 0;

	if (__builtin_expect (interrupted, 0)) {
		interrupted = 0;
#ifdef SIGWINCH
		get_size (&screen_x, &screen_y);
		Sys_MaskPrintf (SYS_dev, "resizing to %d x %d\n", screen_x, screen_y);
		resizeterm (screen_y, screen_x);
		con_linewidth = screen_x;
		view_resize (sv_con_data.view, screen_x, screen_y);
		sv_con_data.view->draw (sv_con_data.view);
#endif
	}

	for (ch = 1; ch; ) {
		ch = sv_getch (input);
		if (ch == ERR)
			escape = 0;
		if (escape) {
			switch (escape) {
				case 1:
					if (ch == '[') {
						escape = 2;
						continue;
					}
					break;
				case 2:
					switch (ch) {
						case '1':
							escape = 3;
							continue;
						case '4':
							escape = 4;
							continue;
						default:
							escape = 0;
							break;
					}
					break;
				case 3:
					if (ch == '~')
						ch = KEY_HOME;
					escape = 0;
					break;
				case 4:
					if (ch == '~')
						ch = KEY_END;
					escape = 0;
					break;
			}
		}
		switch (ch) {
			case '\033':
				escape = 1;
				continue;
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
		C_KeyEvent (ch, 0, 1);
	}
}

static void
key_event (knum_t key, short unicode, qboolean down)
{
	int         ovf = view_offset;
	sv_view_t  *sv_view;
	con_buffer_t *buffer;

	switch (key) {
		case QFK_PAGEUP:
			view_offset -= 10;
			sv_view = output->data;
			buffer = sv_view->obj;
			if (view_offset <= -(buffer->num_lines - (screen_y - 3)))
				view_offset = -(buffer->num_lines - (screen_y - 3)) + 1;
			if (ovf != view_offset)
				sv_draw (output);
			break;
		case QFK_PAGEDOWN:
			view_offset += 10;
			if (view_offset > 0)
				view_offset = 0;
			if (ovf != view_offset)
				sv_draw (output);
			break;
		case '\f':
			sv_draw (output);
			break;
		default:
			sv_view = input->data;
			Con_ProcessInputLine (sv_view->obj, key);
			sv_refresh (input);
			break;
	}
	doupdate ();
}

static void
print (char *txt)
{
	sv_view_t  *sv_view = output->data;
	Con_BufferAddText (sv_view->obj, txt);
	if (!view_offset) {
		while (*txt)
			draw_fun_char (sv_view->win, (byte) *txt++, 0);
		if (!batch_print) {
			sv_refresh (output);
			doupdate ();
		}
	}
}

static view_t *
create_window (view_t *parent, int xpos, int ypos, int xlen, int ylen,
			   grav_t grav, void *obj, int opts, void (*draw) (view_t *),
			   void (*setgeometry) (view_t *))
{
	view_t     *view;
	sv_view_t  *sv_view;

	sv_view = calloc (1, sizeof (sv_view_t));
	sv_view->obj = obj;
	sv_view->win = newwin (ylen, xlen, 0, 0);	// will get moved when added
	scrollok (sv_view->win, (opts & sv_scroll) ? TRUE : FALSE);
	leaveok (sv_view->win, (opts & sv_cursor) ? FALSE : TRUE);
	nodelay (sv_view->win, TRUE);
	keypad (sv_view->win, TRUE);
	sv_view->draw = draw;
	sv_view->setgeometry = setgeometry;

	view = view_new (xpos, ypos, xlen, ylen, grav);
	view->data = sv_view;
	view->draw = sv_draw;
	view->setgeometry = sv_setgeometry;
	view->resize_x = (opts & sv_resize_x) != 0;
	view->resize_y = (opts & sv_resize_y) != 0;
	view_add (parent, view);

	return view;
}

static void
exec_line (inputline_t *il)
{
	Con_ExecLine (il->line);
}

static inputline_t *
create_input_line (int width)
{
	inputline_t *input_line;

	input_line = Con_CreateInputLine (16, MAXCMDLINE, ']');
	input_line->complete = sv_complete;
	input_line->enter = exec_line;
	input_line->user_data = input;
	input_line->draw = draw_input_line;
	input_line->width = width;

	return input_line;
}

static void
init (void)
{
#ifdef SIGWINCH
	struct sigaction action = {};
	action.sa_handler = sigwinch;
	sigaction (SIGWINCH, &action, 0);
#endif

	initscr ();
	start_color ();
	cbreak ();
	noecho ();

	nonl ();

	get_size (&screen_x, &screen_y);
	sv_con_data.view = view_new (0, 0, screen_x, screen_y, grav_northwest);

	output = create_window (sv_con_data.view,
							0, 0, screen_x, screen_y - 2, grav_northwest,
							Con_CreateBuffer (BUFFER_SIZE, MAX_LINES),
							sv_resize_x | sv_resize_y | sv_scroll,
							draw_output, 0);

	status = create_window (sv_con_data.view,
							0, 1, screen_x, 1, grav_southwest,
							calloc (1, sizeof (sv_sbar_t)),
							sv_resize_x,
							draw_status, setgeometry_status);
	sv_con_data.status_view = status;

	input = create_window (sv_con_data.view,
						   0, 0, screen_x, 1, grav_southwest,
						   create_input_line (screen_x),
						   sv_resize_x | sv_cursor,
						   draw_input, setgeometry_input);
	((inputline_t *) ((sv_view_t *) input->data)->obj)->user_data = input;

	init_pair (CP_YELLOW_BLACK, COLOR_YELLOW, COLOR_BLACK);
	init_pair (CP_GREEN_BLACK, COLOR_GREEN, COLOR_BLACK);
	init_pair (CP_RED_BLACK, COLOR_RED, COLOR_BLACK);
	init_pair (CP_CYAN_BLACK, COLOR_CYAN, COLOR_BLACK);
	init_pair (CP_MAGENTA_BLACK, COLOR_MAGENTA, COLOR_BLACK);

	init_pair (CP_YELLOW_BLUE, COLOR_YELLOW, COLOR_BLUE);
	init_pair (CP_GREEN_BLUE, COLOR_GREEN, COLOR_BLUE);
	init_pair (CP_RED_BLUE, COLOR_RED, COLOR_BLUE);
	init_pair (CP_CYAN_BLUE, COLOR_CYAN, COLOR_BLUE);
	init_pair (CP_MAGENTA_BLUE, COLOR_MAGENTA, COLOR_BLUE);

	con_linewidth = screen_x;

	sv_con_data.view->draw (sv_con_data.view);
	wrefresh (curscr);
}
#endif

static void
sv_logfile_f (cvar_t *var)
{
	if (!var->string[0] || strequal (var->string, "none")) {
		if (log_file)
			Qclose (log_file);
		log_file = 0;
	} else {
		char       *fname = strdup (var->string);
		char       *flags = strrchr (fname, ':');

		if (flags) {
			*flags++ = 0;
			flags = nva ("a%s", flags);
		} else {
			flags = nva ("a");
		}
		log_file = QFS_Open (fname, flags);
		free (flags);
		free (fname);
	}
}

static int
sv_exec_line_command (void *data, const char *line)
{
	Cbuf_AddText (sv_con_data.cbuf, line);
	Cbuf_AddText (sv_con_data.cbuf, "\n");
	return 1;
}

static int
sv_exec_line_chat (void *data, const char *line)
{
	Cbuf_AddText (sv_con_data.cbuf, "say ");
	Cbuf_AddText (sv_con_data.cbuf, line);
	Cbuf_AddText (sv_con_data.cbuf, "\n");
	return 0;
}

static void
sv_conmode_f (cvar_t *var)
{
	if (!strcmp (var->string, "command")) {
		sv_con_data.exec_line = sv_exec_line_command;
	} else if (!strcmp (var->string, "chat")) {
		sv_con_data.exec_line = sv_exec_line_chat;
	} else {
		Sys_Printf ("mode must be one of \"command\" or \"chat\"\n");
		Sys_Printf ("    forcing \"command\"\n");
		Cvar_Set (var, "command");
	}
}

static void
C_Init (void)
{
#ifdef HAVE_NCURSES
	cvar_t	  *curses = Cvar_Get ("sv_use_curses", "0", CVAR_ROM, NULL,
								  "Set to 1 to enable curses server console.");
	use_curses = curses->int_val;
	if (use_curses) {
		init ();
	} else
#endif
		setvbuf (stdout, 0, _IOLBF, BUFSIZ);
	sv_logfile = Cvar_Get ("sv_logfile", "none", CVAR_NONE, sv_logfile_f,
						   "Control server console logging. \"none\" for off, "
						   "or \"filename:gzflags\"");
	sv_conmode = Cvar_Get ("sv_conmode", "command", CVAR_NONE, sv_conmode_f,
						   "Set the console input mode (command, chat)");
}

static void
C_shutdown (void)
{
	if (log_file) {
		Qclose (log_file);
		log_file = 0;
	}
#ifdef HAVE_NCURSES
	if (use_curses)
		endwin ();
#endif
}

static __attribute__((format(PRINTF, 1, 0))) void
C_Print (const char *fmt, va_list args)
{
	static dstring_t *buffer;

	if (!buffer)
		buffer = dstring_new ();

	dvsprintf (buffer, fmt, args);

	if (log_file) {
		Qputs (log_file, buffer->str);
		Qflush (log_file);
	}
#ifdef HAVE_NCURSES
	if (use_curses) {
		print (buffer->str);
	} else
#endif
	{
		unsigned char *txt = (unsigned char *) buffer->str;
		while (*txt)
			putc (sys_char_map[*txt++], stdout);
		fflush (stdout);
	}
}

static void
C_ProcessInput (void)
{
#ifdef HAVE_NCURSES
	if (use_curses) {
		process_input ();
	} else
#endif
		while (1) {
			const char *cmd = Sys_ConsoleInput ();
			if (!cmd)
				break;
			Con_ExecLine (cmd);
		}
}

static void
C_KeyEvent (knum_t key, short unicode, qboolean down)
{
#ifdef HAVE_NCURSES
	key_event (key, unicode, down);
#endif
}

static void
C_DrawConsole (void)
{
	// only the status bar is drawn because the inputline and output views
	// take care of themselves
	if (sv_con_data.status_view)
		sv_con_data.status_view->draw (sv_con_data.status_view);
}

static void
C_CheckResize (void)
{
}

static void
C_NewMap (void)
{
}

static general_funcs_t plugin_info_general_funcs = {
	.init = C_Init,
	.shutdown = C_shutdown,
};
static general_data_t plugin_info_general_data;

static console_funcs_t plugin_info_console_funcs = {
	.print = C_Print,
	.process_input = C_ProcessInput,
	.key_event = C_KeyEvent,
	.draw_console = C_DrawConsole,
	.check_resize = C_CheckResize,
	.new_map = C_NewMap,
};

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.console = &plugin_info_console_funcs,
};

static plugin_data_t plugin_info_data = {
	.general = &plugin_info_general_data,
	.console = &sv_con_data,
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

PLUGIN_INFO(console, server)
{
	return &plugin_info;
}
