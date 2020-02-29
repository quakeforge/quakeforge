/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include <curses.h>
#include <panel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "qwaq.h"
#include "event.h"

#define always_inline inline __attribute__((__always_inline__))
#define QUEUE_SIZE 16
#define MOUSE_MOVES "\033[?1003h"	// Make the terminal report mouse movements
#define STRING_ID_QUEUE_SIZE 8		// must be > 1
#define COMMAND_QUEUE_SIZE 128
#define CMD_SIZE(x) sizeof(x)/sizeof(x[0])

typedef enum qwaq_commands_e {
	qwaq_cmd_newwin,
	qwaq_cmd_delwin,
	qwaq_cmd_new_panel,
	qwaq_cmd_del_panel,
	qwaq_cmd_hide_panel,
	qwaq_cmd_show_panel,
	qwaq_cmd_top_panel,
	qwaq_cmd_bottom_panel,
	qwaq_cmd_move_panel,
	qwaq_cmd_mvwaddstr,
	qwaq_cmd_wrefresh,
	qwaq_cmd_init_pair,
	qwaq_cmd_wbkgd,
} qwaq_commands;

#define RING_BUFFER(type, size) 	\
	struct {						\
		type        buffer[size];	\
		unsigned    head;			\
		unsigned    tail;			\
	}

#define RB_buffer_size(ring_buffer)						\
	({	__auto_type rb = (ring_buffer);					\
		sizeof (rb->buffer) / sizeof (rb->buffer[0]);	\
	})

#define RB_SPACE_AVAILABLE(ring_buffer)										\
	({	__auto_type rb = &(ring_buffer);									\
		(rb->tail + RB_buffer_size(rb) - rb->head - 1) % RB_buffer_size(rb);\
	})

#define RB_DATA_AVAILABLE(ring_buffer)										\
	({	__auto_type rb = &(ring_buffer);									\
		(rb->head + RB_buffer_size (rb) - rb->tail) % RB_buffer_size (rb);	\
	})

#define RB_WRITE_DATA(ring_buffer, data, count)								\
	({	__auto_type rb = &(ring_buffer);									\
		typeof (&rb->buffer[0]) d = (data);									\
		unsigned    c = (count);											\
		unsigned    h = rb->head;											\
		rb->head = (h + c) % RB_buffer_size (rb);							\
		if (c > RB_buffer_size (rb) - h) {									\
			memcpy (rb->buffer + h, d,										\
					(RB_buffer_size (rb) - h) * sizeof (rb->buffer[0]));	\
			c -= RB_buffer_size (rb) - h;									\
			d += RB_buffer_size (rb) - h;									\
			h = 0;															\
		}																	\
		memcpy (rb->buffer + h, d, c * sizeof (rb->buffer[0]));				\
	})

#define RB_READ_DATA(ring_buffer, data, count)								\
	({	__auto_type rb = &(ring_buffer);									\
		typeof (&rb->buffer[0]) d = (data);									\
		unsigned    c = (count);											\
		unsigned    oc = c;													\
		unsigned    t = rb->tail;											\
		if (c > RB_buffer_size (rb) - t) {									\
			memcpy (d, rb->buffer + t,										\
					(RB_buffer_size (rb) - t) * sizeof (rb->buffer[0]));	\
			c -= RB_buffer_size (rb) - t;									\
			d += RB_buffer_size (rb) - t;									\
			t = 0;															\
		}																	\
		memcpy (d, rb->buffer + t, c * sizeof (rb->buffer[0]));				\
		rb->tail = (t + oc) % RB_buffer_size (rb);							\
	})

#define RB_DROP_DATA(ring_buffer, count)									\
	({	__auto_type rb = &(ring_buffer);									\
		unsigned    c = (count);											\
		unsigned    t = rb->tail;											\
		rb->tail = (t + c) % RB_buffer_size (rb);							\
	})

#define RB_PEEK_DATA(ring_buffer, ahead)						\
	({	__auto_type rb = &(ring_buffer);						\
		rb->buffer[(rb->tail + ahead) % RB_buffer_size (rb)];	\
	})

typedef struct window_s {
	WINDOW     *win;
} window_t;

typedef struct panel_s {
	PANEL      *panel;
} panel_t;

typedef struct qwaq_resources_s {
	progs_t    *pr;
	int         initialized;
	window_t    stdscr;
	PR_RESMAP (window_t) window_map;
	PR_RESMAP (panel_t) panel_map;
	RING_BUFFER (qwaq_event_t, QUEUE_SIZE) event_queue;
	RING_BUFFER (int, COMMAND_QUEUE_SIZE) command_queue;
	RING_BUFFER (int, COMMAND_QUEUE_SIZE) results;
	RING_BUFFER (int, STRING_ID_QUEUE_SIZE) string_ids;
	dstring_t   strings[STRING_ID_QUEUE_SIZE - 1];
} qwaq_resources_t;

static window_t *
window_new (qwaq_resources_t *res)
{
	PR_RESNEW (window_t, res->window_map);
}

static void
window_free (qwaq_resources_t *res, window_t *win)
{
	PR_RESFREE (window_t, res->window_map, win);
}

static void
window_reset (qwaq_resources_t *res)
{
	PR_RESRESET (window_t, res->window_map);
}

static inline window_t *
window_get (qwaq_resources_t *res, unsigned index)
{
	PR_RESGET(res->window_map, index);
}

static inline int
window_index (qwaq_resources_t *res, window_t *win)
{
	PR_RESINDEX (res->window_map, win);
}

static always_inline window_t * __attribute__((pure))
get_window (qwaq_resources_t *res, const char *name, int handle)
{
	if (handle == 1) {
		return &res->stdscr;
	}

	window_t   *window = window_get (res, handle);

	if (!window || !window->win) {
		PR_RunError (res->pr, "invalid window passed to %s", name + 3);
	}
	return window;
}

static panel_t *
panel_new (qwaq_resources_t *res)
{
	PR_RESNEW (panel_t, res->panel_map);
}

static void
panel_free (qwaq_resources_t *res, panel_t *win)
{
	PR_RESFREE (panel_t, res->panel_map, win);
}

static void
panel_reset (qwaq_resources_t *res)
{
	PR_RESRESET (panel_t, res->panel_map);
}

static inline panel_t *
panel_get (qwaq_resources_t *res, unsigned index)
{
	PR_RESGET(res->panel_map, index);
}

static inline int
panel_index (qwaq_resources_t *res, panel_t *win)
{
	PR_RESINDEX (res->panel_map, win);
}

static always_inline panel_t * __attribute__((pure))
get_panel (qwaq_resources_t *res, const char *name, int handle)
{
	panel_t   *panel = panel_get (res, handle);

	if (!panel || !panel->panel) {
		PR_RunError (res->pr, "invalid panel passed to %s", name + 3);
	}
	return panel;
}

static int
acquire_string (qwaq_resources_t *res)
{
	int         string_id = -1;

	// XXX add locking and loop for available
	if (RB_DATA_AVAILABLE (res->string_ids)) {
		RB_READ_DATA (res->string_ids, &string_id, 1);
	}
	// XXX unlock and end of loop
	return string_id;
}

static void
release_string (qwaq_resources_t *res, int string_id)
{
	// locking shouldn't be necessary as there should be only one writer
	// but if it becomes such that there is more than one writer, locks as per
	// acquire
	if (RB_SPACE_AVAILABLE (res->string_ids)) {
		RB_WRITE_DATA (res->string_ids, &string_id, 1);
	}
}

static void
cmd_newwin (qwaq_resources_t *res)
{
	int         xpos = RB_PEEK_DATA (res->command_queue, 2);
	int         ypos = RB_PEEK_DATA (res->command_queue, 3);
	int         xlen = RB_PEEK_DATA (res->command_queue, 4);
	int         ylen = RB_PEEK_DATA (res->command_queue, 5);

	window_t   *window = window_new (res);
	window->win = newwin (ylen, xlen, ypos, xpos);
	keypad (window->win, TRUE);

	int         window_id = window_index (res, window);
	int         cmd_result[] = { qwaq_cmd_newwin, window_id };

	// loop
	if (RB_SPACE_AVAILABLE (res->results) >= CMD_SIZE (cmd_result)) {
		RB_WRITE_DATA (res->results, cmd_result, CMD_SIZE (cmd_result));
	}
}

static void
cmd_delwin (qwaq_resources_t *res)
{
	int         window_id = RB_PEEK_DATA (res->command_queue, 2);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	delwin (window->win);
	window_free (res, window);
}

static void
cmd_new_panel (qwaq_resources_t *res)
{
	int         window_id = RB_PEEK_DATA (res->command_queue, 2);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	panel_t    *panel = panel_new (res);
	panel->panel = new_panel (window->win);

	int         panel_id = panel_index (res, panel);
	int         cmd_result[] = { qwaq_cmd_new_panel, panel_id };

	// loop
	if (RB_SPACE_AVAILABLE (res->results) >= CMD_SIZE (cmd_result)) {
		RB_WRITE_DATA (res->results, cmd_result, CMD_SIZE (cmd_result));
	}
}

static void
cmd_del_panel (qwaq_resources_t *res)
{
	int         panel_id = RB_PEEK_DATA (res->command_queue, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	del_panel (panel->panel);
	panel_free (res, panel);
}

static void
cmd_hide_panel (qwaq_resources_t *res)
{
	int         panel_id = RB_PEEK_DATA (res->command_queue, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	hide_panel (panel->panel);
	panel_free (res, panel);
}

static void
cmd_show_panel (qwaq_resources_t *res)
{
	int         panel_id = RB_PEEK_DATA (res->command_queue, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	show_panel (panel->panel);
	panel_free (res, panel);
}

static void
cmd_top_panel (qwaq_resources_t *res)
{
	int         panel_id = RB_PEEK_DATA (res->command_queue, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	top_panel (panel->panel);
	panel_free (res, panel);
}

static void
cmd_bottom_panel (qwaq_resources_t *res)
{
	int         panel_id = RB_PEEK_DATA (res->command_queue, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	bottom_panel (panel->panel);
	panel_free (res, panel);
}

static void
cmd_move_panel (qwaq_resources_t *res)
{
	int         panel_id = RB_PEEK_DATA (res->command_queue, 2);
	int         x = RB_PEEK_DATA (res->command_queue, 3);
	int         y = RB_PEEK_DATA (res->command_queue, 4);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	move_panel (panel->panel, y, x);
	panel_free (res, panel);
}

static void
cmd_mvwaddstr (qwaq_resources_t *res)
{
	int         window_id = RB_PEEK_DATA (res->command_queue, 2);
	int         x = RB_PEEK_DATA (res->command_queue, 3);
	int         y = RB_PEEK_DATA (res->command_queue, 4);
	int         string_id = RB_PEEK_DATA (res->command_queue, 5);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	mvwaddstr (window->win, y, x, res->strings[string_id].str);
	release_string (res, string_id);
}

static void
cmd_wrefresh (qwaq_resources_t *res)
{
	int         window_id = RB_PEEK_DATA (res->command_queue, 2);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	wrefresh (window->win);
}

static void
cmd_init_pair (qwaq_resources_t *res)
{
	int         pair = RB_PEEK_DATA (res->command_queue, 2);
	int         f = RB_PEEK_DATA (res->command_queue, 3);
	int         b = RB_PEEK_DATA (res->command_queue, 4);

	init_pair (pair, f, b);
}

static void
cmd_wbkgd (qwaq_resources_t *res)
{
	int         window_id = RB_PEEK_DATA (res->command_queue, 2);
	int         ch = RB_PEEK_DATA (res->command_queue, 3);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	wbkgd (window->win, ch);
}

static void
process_commands (qwaq_resources_t *res)
{
	while (RB_DATA_AVAILABLE (res->command_queue) > 2) {
		switch ((qwaq_commands) RB_PEEK_DATA (res->command_queue, 0)) {
			case qwaq_cmd_newwin:
				cmd_newwin (res);
				break;
			case qwaq_cmd_delwin:
				cmd_delwin (res);
				break;
			case qwaq_cmd_new_panel:
				cmd_new_panel (res);
				break;
			case qwaq_cmd_del_panel:
				cmd_del_panel (res);
				break;
			case qwaq_cmd_hide_panel:
				cmd_hide_panel (res);
				break;
			case qwaq_cmd_show_panel:
				cmd_show_panel (res);
				break;
			case qwaq_cmd_top_panel:
				cmd_top_panel (res);
				break;
			case qwaq_cmd_bottom_panel:
				cmd_bottom_panel (res);
				break;
			case qwaq_cmd_move_panel:
				cmd_move_panel (res);
				break;
			case qwaq_cmd_mvwaddstr:
				cmd_mvwaddstr (res);
				break;
			case qwaq_cmd_wrefresh:
				cmd_wrefresh (res);
				break;
			case qwaq_cmd_init_pair:
				cmd_init_pair (res);
				break;
			case qwaq_cmd_wbkgd:
				cmd_wbkgd (res);
				break;
		}
		RB_DROP_DATA (res->command_queue, RB_PEEK_DATA (res->command_queue, 1));
	}
}

static void
add_event (qwaq_resources_t *res, qwaq_event_t *event)
{
	if (RB_SPACE_AVAILABLE (res->event_queue) >= 1) {
		RB_WRITE_DATA (res->event_queue, event, 1);
	}
}

static int
get_event (qwaq_resources_t *res, qwaq_event_t *event)
{
	if (RB_DATA_AVAILABLE (res->event_queue) >= 1) {
		if (event) {
			RB_READ_DATA (res->event_queue, event, 1);
		}
		return 1;
	}
	return 0;
}

static void
mouse_event (qwaq_resources_t *res, MEVENT *mevent)
{
	qwaq_event_t event = {};
	event.event_type = qe_mouse;
	event.e.mouse.x = mevent->x;
	event.e.mouse.y = mevent->y;
	event.e.mouse.buttons = mevent->bstate;
	add_event (res, &event);
}

static void
key_event (qwaq_resources_t *res, int key)
{
	qwaq_event_t event = {};
	event.event_type = qe_key;
	event.e.key = key;
	add_event (res, &event);
}

static void
process_input (qwaq_resources_t *res)
{
	if (Sys_CheckInput (1, -1)) {
		int         ch;
		while ((ch = getch ()) != ERR && ch) {
			fflush (stderr);
			if (ch == KEY_MOUSE) {
				MEVENT    mevent;
				getmouse (&mevent);
				mouse_event (res, &mevent);
			} else {
				key_event (res, ch);
			}
		}
	}
}

static int need_endwin;
static void
bi_shutdown (void)
{
	if (need_endwin) {
		endwin ();
	}
}

static void
bi_newwin (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         xpos = P_INT (pr, 0);
	int         ypos = P_INT (pr, 1);
	int         xlen = P_INT (pr, 2);
	int         ylen = P_INT (pr, 3);
	int         command[] = {
					qwaq_cmd_newwin, 0,
					xpos, ypos, xlen, ylen,
				};

	command[1] = CMD_SIZE(command);

	if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
		RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
	}
	// XXX should just wait on the mutex
	process_commands (res);
	process_input (res);
	// locking and loop until id is correct
	if (RB_DATA_AVAILABLE (res->results)
		&& RB_PEEK_DATA (res->results, 0) == qwaq_cmd_newwin) {
		int         cmd_result[2];	// should results have a size?
		RB_READ_DATA (res->results, cmd_result, CMD_SIZE (cmd_result));
		R_INT (pr) = cmd_result[1];
	}
}

static void
bi_delwin (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         window_id = P_INT (pr, 0);

	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_delwin, 0, window_id, };

		command[1] = CMD_SIZE(command);

		if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
			RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
		}
	}
}

static void
bi_new_panel (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         window_id = P_INT (pr, 0);

	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_new_panel, 0, window_id, };

		command[1] = CMD_SIZE(command);

		if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
			RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
		}

		// locking and loop until id is correct
		if (RB_DATA_AVAILABLE (res->results)
			&& RB_PEEK_DATA (res->results, 0) == qwaq_cmd_new_panel) {
			int         cmd_result[2];	// should results have a size?
			RB_READ_DATA (res->results, cmd_result, CMD_SIZE (cmd_result));
			R_INT (pr) = cmd_result[1];
		}
	}
}

static void
panel_command (progs_t *pr, qwaq_commands cmd)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         panel_id = P_INT (pr, 0);

	if (get_panel (res, __FUNCTION__, panel_id)) {
		int         command[] = { cmd, 0, panel_id, };
		command[1] = CMD_SIZE(command);

		if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
			RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
		}
	}
}

static void
bi_del_panel (progs_t *pr)
{
	panel_command (pr, qwaq_cmd_del_panel);
}

static void
bi_hide_panel (progs_t *pr)
{
	panel_command (pr, qwaq_cmd_hide_panel);
}

static void
bi_show_panel (progs_t *pr)
{
	panel_command (pr, qwaq_cmd_show_panel);
}

static void
bi_top_panel (progs_t *pr)
{
	panel_command (pr, qwaq_cmd_top_panel);
}

static void
bi_bottom_panel (progs_t *pr)
{
	panel_command (pr, qwaq_cmd_bottom_panel);
}

static void
bi_move_panel (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         panel_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);

	if (get_panel (res, __FUNCTION__, panel_id)) {
		int         command[] = { qwaq_cmd_move_panel, 0, panel_id, x, y, };
		command[1] = CMD_SIZE(command);

		if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
			RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
		}
	}
}

static void
bi_mvwprintf (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         window_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);
	const char *fmt = P_GSTRING (pr, 3);
	int         count = pr->pr_argc - 4;
	pr_type_t **args = pr->pr_params + 4;

	if (get_window (res, __FUNCTION__, window_id)) {
		int         string_id = acquire_string (res);
		dstring_t  *print_buffer = res->strings + string_id;
		int         command[] = {
						qwaq_cmd_mvwaddstr, 0,
						window_id, x, y, string_id
					};

		command[1] = CMD_SIZE(command);

		dstring_clearstr (print_buffer);
		PR_Sprintf (pr, print_buffer, "mvwaddstr", fmt, count, args);
		if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
			RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
		}
	}
}

static void
bi_wrefresh (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         window_id = P_INT (pr, 0);

	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_wrefresh, 0, window_id, };
		command[1] = CMD_SIZE(command);

		if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
			RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
		}
	}
}

static void
bi_get_event (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	qwaq_event_t *event = &G_STRUCT (pr, qwaq_event_t, P_INT (pr, 0));

	process_commands (res);
	process_input (res);
	R_INT (pr) = get_event (res, event);
}

static void
bi_max_colors (progs_t *pr)
{
	R_INT (pr) = COLORS;
}

static void
bi_max_color_pairs (progs_t *pr)
{
	R_INT (pr) = COLOR_PAIRS;
}

static void
bi_init_pair (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         pair = P_INT (pr, 0);
	int         f = P_INT (pr, 1);
	int         b = P_INT (pr, 2);

	int         command[] = { qwaq_cmd_init_pair, 0, pair, f, b, };
	command[1] = CMD_SIZE(command);

	if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
		RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
	}
}

static void
bi_wbkgd (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         window_id = P_INT (pr, 0);
	int         ch = P_INT (pr, 1);

	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_wbkgd, 0, window_id, ch, };

		command[1] = CMD_SIZE(command);

		if (RB_SPACE_AVAILABLE (res->command_queue) >= CMD_SIZE(command)) {
			RB_WRITE_DATA (res->command_queue, command, CMD_SIZE(command));
		}
	}
}

static void
bi_initialize (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");

	initscr ();
	need_endwin = 1;
	res->initialized = 1;
	start_color ();
	raw ();
	keypad (stdscr, TRUE);
	noecho ();
	nonl ();
	nodelay (stdscr, TRUE);
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	write(1, MOUSE_MOVES, sizeof (MOUSE_MOVES) - 1);
	refresh();

	res->stdscr.win = stdscr;
}

static void
bi_qwaq_clear (progs_t *pr, void *data)
{
	__auto_type res = (qwaq_resources_t *) data;

	if (res->initialized) {
		endwin ();
	}
	need_endwin = 0;
	window_reset (res);
	panel_reset (res);
}

static builtin_t builtins[] = {
	{"initialize",		bi_initialize,		-1},
	{"create_window",	bi_newwin,			-1},
	{"destroy_window",	bi_delwin,			-1},
	{"create_panel",	bi_new_panel,		-1},
	{"destroy_panel",	bi_del_panel,		-1},
	{"hide_panel",		bi_hide_panel,		-1},
	{"show_panel",		bi_show_panel,		-1},
	{"top_panel",		bi_top_panel,		-1},
	{"bottom_panel",	bi_bottom_panel,	-1},
	{"move_panel",		bi_move_panel,		-1},
	{"mvwprintf",		bi_mvwprintf,		-1},
	{"wrefresh",		bi_wrefresh,		-1},
	{"get_event",		bi_get_event,		-1},
	{"max_colors",		bi_max_colors,		-1},
	{"max_color_pairs",	bi_max_color_pairs,	-1},
	{"init_pair",		bi_init_pair,		-1},
	{"wbkgd",			bi_wbkgd,			-1},
	{0}
};

static __attribute__((format(printf, 1, 0))) void
qwaq_print (const char *fmt, va_list args)
{
	vfprintf (stderr, fmt, args);
	fflush (stderr);
}

void
BI_Init (progs_t *pr)
{
	qwaq_resources_t *res = calloc (sizeof (*res), 1);
	res->pr = pr;
	for (int i = 0; i < STRING_ID_QUEUE_SIZE - 1; i++) {
		RB_WRITE_DATA (res->string_ids, &i, 1);
		res->strings[i].mem = &dstring_default_mem;
	}

	PR_Resources_Register (pr, "qwaq", res, bi_qwaq_clear);
	PR_RegisterBuiltins (pr, builtins);
	Sys_RegisterShutdown (bi_shutdown);
	Sys_SetStdPrintf (qwaq_print);
}
