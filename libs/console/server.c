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

static const component_t server_components[server_comp_count] = {
	[server_href] = {
		.size = sizeof (hierref_t),
		.name = "href",
		.destroy = Hierref_DestroyComponent,
	},
	[server_view] = {
		.size = sizeof (sv_view_t),
		.name = "sv_view",
	},
	[server_window] = {
		.size = sizeof (sv_view_t),
		.name = "sv_window",
	},
};

static console_data_t sv_con_data = {
	.components = server_components,
	.num_components = server_comp_count,
};
#define server_base sv_con_data.component_base
#define s_href (server_base + server_href)
#define s_view (server_base + server_view)
#define s_window (server_base + server_window)

static QFile  *log_file;
static char *sv_logfile;
static cvar_t sv_logfile_cvar = {
	.name = "sv_logfile",
	.description =
		"Control server console logging. \"none\" for off, or "
		"\"filename:gzflags\"",
	.default_value = "none",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &sv_logfile },
};
static exprenum_t sv_conmode_enum;
static exprtype_t sv_conmode_type = {
	.name = "sv_conmode",
	.size = sizeof (sv_con_data.exec_line),
	.data = &sv_conmode_enum,
	.get_string = cexpr_enum_get_string,
};
static int sv_exec_line_command (void *data, const char *line);
static int sv_exec_line_chat (void *data, const char *line);
static int (*sv_conmode_values[])(void *, const char *) = {
	sv_exec_line_command,
	sv_exec_line_chat,
};
static exprsym_t sv_conmode_symbols[] = {
	{"command", &sv_conmode_type, sv_conmode_values + 0},
	{"chat", &sv_conmode_type, sv_conmode_values + 1},
	{}
};
static exprtab_t sv_conmode_symtab = {
	sv_conmode_symbols,
};
static exprenum_t sv_conmode_enum = {
	&sv_conmode_type,
	&sv_conmode_symtab,
};
static cvar_t sv_conmode_cvar = {
	.name = "sv_conmode",
	.description =
		"Set the console input mode (command, chat)",
	.default_value = "command",
	.flags = CVAR_NONE,
	.value = { .type = &sv_conmode_type, .value = &sv_con_data.exec_line },
};
static int sv_use_curses;
static cvar_t sv_use_curses_cvar = {
	.name = "sv_use_curses",
	.description =
		"Set to 1 to enable curses server console.",
	.default_value = "0",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &sv_use_curses },
};

#ifdef HAVE_NCURSES

static void key_event (knum_t key, short unicode, bool down);

enum {
	sv_resize_x = 1,
	sv_resize_y = 2,
	sv_scroll	= 4,
	sv_cursor	= 8,
};

static int use_curses = 1;

static view_t sv_view;
static view_t output;
static view_t status;
static view_t input;
static int screen_x, screen_y;
static volatile sig_atomic_t interrupted;
static int batch_print;

static ecs_registry_t *server_reg;
static uint32_t view_base;

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
#define CP_WHITE_BLUE		(11)

static chtype attr_table[16] = {
	A_NORMAL,
	COLOR_PAIR (CP_GREEN_BLACK),
	COLOR_PAIR (CP_RED_BLACK),
	0,
	COLOR_PAIR (CP_YELLOW_BLACK),
	COLOR_PAIR (CP_CYAN_BLACK),
	COLOR_PAIR (CP_MAGENTA_BLACK),
	0,
	COLOR_PAIR(CP_WHITE_BLUE),
	A_BOLD | COLOR_PAIR (CP_GREEN_BLUE),
	A_BOLD | COLOR_PAIR (CP_RED_BLUE),
	0,
	A_BOLD | COLOR_PAIR (CP_YELLOW_BLUE),
	A_BOLD | COLOR_PAIR (CP_CYAN_BLUE),
	A_BOLD | COLOR_PAIR (CP_MAGENTA_BLUE),
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
sv_refresh_windows (void)
{
	sv_view_t  *window = server_reg->comp_pools[s_window].data;
	uint32_t    count = server_reg->comp_pools[s_window].count;
	while (count-- > 0) {
		wnoutrefresh ((WINDOW *) (window++)->win);
	}
	doupdate ();
}

static inline int
sv_getch (view_t view)
{
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	return wgetch ((WINDOW *) window->win);
}

static inline void
sv_draw (view_t view)
{
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	if (window->draw)
		window->draw (view);
	wnoutrefresh ((WINDOW *) window->win);
	doupdate ();
}

static void
sv_setgeometry (view_t view, view_pos_t foo)
{
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	WINDOW *win = window->win;

	view_pos_t  pos = View_GetAbs (view);
	view_pos_t  len = View_GetLen (view);
	wresize (win, len.y, len.x);
	mvwin (win, pos.y, pos.x);
	if (window->setgeometry)
		window->setgeometry (view);
}

static void
sv_complete (inputline_t *il)
{
	batch_print = 1;
	Con_BasicCompleteCommandLine (il);
	batch_print = 0;

	sv_refresh_windows ();
}

static void
draw_output (view_t view)
{
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	WINDOW     *win = window->win;
	con_buffer_t *output_buffer = window->obj;
	view_pos_t  len = View_GetLen (view);

	// this is not the most efficient way to update the screen, but oh well
	int         lines = len.y - 1; // leave a blank line
	int         width = len.x;
	int         cur_line = output_buffer->line_head - 1 + view_offset;
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
		byte       *text = output_buffer->buffer + l->text;
		int         len = l->len;

		if (y > 0) {
			text += y * width;
			len -= y * width;
			y = 0;
			if (len < 1) {
				len = 1;
				text = output_buffer->buffer + l->text + l->len - 1;
			}
		}
		while (len--)
			draw_fun_char (win, *text++, 0);
	} while (cur_line < (int) output_buffer->line_head - 1 + view_offset);
}

static void
draw_status (view_t view)
{
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	WINDOW     *win = window->win;
	sv_sbar_t  *sb = window->obj;
	char       *old = alloca (sb->width);

	memcpy (old, sb->text, sb->width);
	memset (sb->text, ' ', sb->width);

	ecs_pool_t *pool = &server_reg->comp_pools[s_view];
	sv_view_t  *sv_view = pool->data;
	for (uint32_t i = 0; i < pool->count; i++) {
		view_t      v = { .reg = view.reg, .id = pool->dense[i],
						  .comp = view.comp };
		(sv_view++)->draw (v);
	}

	if (memcmp (old, sb->text, sb->width)) {
		wbkgdset (win, COLOR_PAIR (CP_WHITE_BLUE));
		wmove (win, 0, 0);
		for (int i = 0; i < sb->width; i++)
			draw_fun_char (win, sb->text[i], 1);
	}
}

static void
draw_input_line (inputline_t *il)
{
	view_t      view = *(view_t *) il->user_data;
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	WINDOW     *win = window->win;
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
draw_input (view_t view)
{
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	draw_input_line (window->obj);
}

static void
setgeometry_input (view_t view)
{
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	view_pos_t  len = View_GetLen (view);
	inputline_t *il = window->obj;
	il->width = len.x;
}

static void
setgeometry_status (view_t view)
{
	sv_view_t  *window = Ent_GetComponent (view.id, s_window, view.reg);
	sv_sbar_t *sb = window->obj;
	view_pos_t  len = View_GetLen (view);
	sb->width = len.x;
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
		View_SetLen (sv_view, screen_x, screen_y);
		sv_refresh_windows ();
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
		key_event (ch, 0, 1);
	}
}

static void
key_event (knum_t key, short unicode, bool down)
{
	int         ovf = view_offset;
	sv_view_t  *window;
	con_buffer_t *buffer;
	int         num_lines;

	switch (key) {
		case QFK_PAGEUP:
			view_offset -= 10;
			window = Ent_GetComponent (output.id, s_window, output.reg);
			buffer = window->obj;
			num_lines = (buffer->line_head - buffer->line_tail
						 + buffer->max_lines) % buffer->max_lines;
			if (view_offset <= -(num_lines - (screen_y - 3)))
				view_offset = -(num_lines - (screen_y - 3)) + 1;
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
			window = Ent_GetComponent (input.id, s_window, input.reg);
			Con_ProcessInputLine (window->obj, key);
			sv_refresh_windows ();
			break;
	}
}

static void
print (char *txt)
{
	sv_view_t  *window = Ent_GetComponent (output.id, s_window,
										   output.reg);
	Con_BufferAddText (window->obj, txt);
	if (!view_offset) {
		while (*txt)
			draw_fun_char (window->win, (byte) *txt++, 0);
		if (!batch_print) {
			sv_refresh_windows ();
		}
	}
}

static view_t
create_window (view_t parent, int xpos, int ypos, int xlen, int ylen,
			   grav_t grav, void *obj, int opts, void (*draw) (view_t),
			   void (*setgeometry) (view_t))
{
	view_t      view = View_New ((ecs_system_t) { server_reg, view_base },
							     parent);
	View_SetPos (view, xpos, ypos);
	View_SetLen (view, xlen, ylen);
	View_SetGravity (view, grav);
	View_SetResize (view, !!(opts & sv_resize_x), !!(opts & sv_resize_y));
	View_SetOnResize (view, sv_setgeometry);
	View_SetOnMove (view, sv_setgeometry);

	sv_view_t   window = {
		.obj = obj,
		.win = newwin (ylen, xlen, 0, 0),	// will get moved when updated
		.draw = draw,
		.setgeometry = setgeometry,
	};
	Ent_SetComponent (view.id, s_window, view.reg, &window);

	scrollok (window.win, (opts & sv_scroll) ? TRUE : FALSE);
	leaveok (window.win, (opts & sv_cursor) ? FALSE : TRUE);
	nodelay (window.win, TRUE);
	keypad (window.win, TRUE);

	return view;
}

static void
exec_line (inputline_t *il)
{
	Con_ExecLine (il->line);
}

static inputline_t *
create_input_line (int width, view_t *view)
{
	inputline_t *input_line;

	input_line = Con_CreateInputLine (16, MAXCMDLINE, ']');
	input_line->complete = sv_complete;
	input_line->enter = exec_line;
	input_line->user_data = view;
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

	server_reg = ECS_NewRegistry ("sv con");
	server_base = ECS_RegisterComponents (server_reg, server_components,
										  server_comp_count);
	view_base = ECS_RegisterComponents (server_reg, view_components,
										view_comp_count);
	ECS_CreateComponentPools (server_reg);

	get_size (&screen_x, &screen_y);

	sv_view = View_New ((ecs_system_t) { server_reg, view_base }, nullview);
	View_SetPos (sv_view, 0, 0);
	View_SetLen (sv_view, screen_x, screen_y);
	View_SetGravity (sv_view, grav_northwest);

	output = create_window (sv_view,
							0, 0, screen_x, screen_y - 2, grav_northwest,
							Con_CreateBuffer (BUFFER_SIZE, MAX_LINES),
							sv_resize_x | sv_resize_y | sv_scroll,
							draw_output, 0);

	status = create_window (sv_view,
							0, 1, screen_x, 1, grav_southwest,
							calloc (1, sizeof (sv_sbar_t)),
							sv_resize_x,
							draw_status, setgeometry_status);
	sv_con_data.status_view = &status;

	input = create_window (sv_view,
						   0, 0, screen_x, 1, grav_southwest,
						   create_input_line (screen_x, &input),
						   sv_resize_x | sv_cursor,
						   draw_input, setgeometry_input);

	View_UpdateHierarchy (sv_view);

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
	init_pair (CP_WHITE_BLUE, COLOR_WHITE, COLOR_BLUE);

	con_linewidth = screen_x;

	wrefresh (curscr);
}
#endif

static void
sv_logfile_f (void *data, const cvar_t *cvar)
{
	if (!sv_logfile[0] || strequal (sv_logfile, "none")) {
		if (log_file)
			Qclose (log_file);
		log_file = 0;
	} else {
		char       *fname = strdup (sv_logfile);
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
C_InitCvars (void)
{
	Cvar_Register (&sv_use_curses_cvar, 0, 0);
	Cvar_Register (&sv_logfile_cvar, sv_logfile_f, 0);
	Cvar_Register (&sv_conmode_cvar, 0, 0);
}

static void
C_Init (void)
{
#ifdef HAVE_NCURSES
	use_curses = sv_use_curses;
	if (use_curses) {
		init ();
	} else
#endif
		setvbuf (stdout, 0, _IOLBF, BUFSIZ);
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
	char *s = buffer->str;
	if (s[0] > 0 && s[0] <= 3) {
		s++;
	}
#ifdef HAVE_NCURSES
	if (use_curses) {
		print (s);
	} else
#endif
	{
		unsigned char *txt = (unsigned char *) s;
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
C_DrawConsole (void)
{
#ifdef HAVE_NCURSES
	// only the status bar is drawn because the inputline and output views
	// take care of themselves
	if (use_curses) {
		draw_status (status);
		sv_refresh_windows ();
	}
#endif
}

static void
C_NewMap (void)
{
}

static general_funcs_t plugin_info_general_funcs = {
	.init = C_InitCvars,
	.shutdown = C_shutdown,
};
static general_data_t plugin_info_general_data;

static console_funcs_t plugin_info_console_funcs = {
	.init = C_Init,
	.print = C_Print,
	.process_input = C_ProcessInput,
	.draw_console = C_DrawConsole,
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
