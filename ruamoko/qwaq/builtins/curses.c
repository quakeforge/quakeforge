/*
	curses.c

	Ruamoko ncurses wrapper using threading

	Copyright (C) 2020 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/03/01

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

#include <sys/time.h>

#include <curses.h>
#include <errno.h>
#include <panel.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/ringbuffer.h"
#include "QF/sys.h"

#include "rua_internal.h"

#include "ruamoko/qwaq/qwaq.h"
#include "ruamoko/qwaq/ui/curses.h"
#include "ruamoko/qwaq/ui/rect.h"
#include "ruamoko/qwaq/ui/textcontext.h"

#define always_inline inline __attribute__((__always_inline__))
#define CMD_SIZE(x) sizeof(x)/sizeof(x[0])

typedef enum qwaq_commands_e {
	qwaq_cmd_syncprint,
	qwaq_cmd_newwin,
	qwaq_cmd_delwin,
	qwaq_cmd_getwrect,
	qwaq_cmd_new_panel,
	qwaq_cmd_del_panel,
	qwaq_cmd_hide_panel,
	qwaq_cmd_show_panel,
	qwaq_cmd_top_panel,
	qwaq_cmd_bottom_panel,
	qwaq_cmd_move_panel,
	qwaq_cmd_panel_window,
	qwaq_cmd_replace_panel,
	qwaq_cmd_update_panels,
	qwaq_cmd_doupdate,
	qwaq_cmd_mvwaddstr,
	qwaq_cmd_waddstr,
	qwaq_cmd_mvwaddch,
	qwaq_cmd_waddch,
	qwaq_cmd_wrefresh,
	qwaq_cmd_init_pair,
	qwaq_cmd_wbkgd,
	qwaq_cmd_werase,
	qwaq_cmd_scrollok,
	qwaq_cmd_wmove,
	qwaq_cmd_move,
	qwaq_cmd_curs_set,
	qwaq_cmd_wborder,
	qwaq_cmd_mvwblit_line,
	qwaq_cmd_wresize,
	qwaq_cmd_resizeterm,
	qwaq_cmd_mvwhline,
	qwaq_cmd_mvwvline,
} qwaq_commands;

static const char *qwaq_command_names[]= {
	"syncprint",
	"newwin",
	"delwin",
	"getwrect",
	"new_panel",
	"del_panel",
	"hide_panel",
	"show_panel",
	"top_panel",
	"bottom_panel",
	"move_panel",
	"panel_window",
	"replace_panel",
	"update_panels",
	"doupdate",
	"mvwaddstr",
	"waddstr",
	"mvwaddch",
	"waddch",
	"wrefresh",
	"init_pair",
	"wbkgd",
	"werase",
	"scrollok",
	"wmove",
	"move",
	"curs_set",
	"wborder",
	"mvwblit_line",
	"wresize",
	"resizeterm",
	"mvwhline",
	"mvwvline",

	"send_connected_devices",
	"get_device_info",
};

static window_t *
window_new (qwaq_resources_t *res)
{
	return PR_RESNEW (res->window_map);
}

static void
window_free (qwaq_resources_t *res, window_t *win)
{
	PR_RESFREE (res->window_map, win);
}

static void
window_reset (qwaq_resources_t *res)
{
	PR_RESRESET (res->window_map);
}

static inline window_t *
window_get (qwaq_resources_t *res, unsigned index)
{
	return PR_RESGET(res->window_map, index);
}

static inline int __attribute__((pure))
window_index (qwaq_resources_t *res, window_t *win)
{
	return PR_RESINDEX (res->window_map, win);
}

static always_inline window_t * __attribute__((pure))
get_window (qwaq_resources_t *res, const char *name, int handle)
{
	if (handle == 1) {
		return &res->stdscr;
	}

	window_t   *window = window_get (res, handle);

	if (!window || !window->win) {
		PR_RunError (res->pr, "invalid window %d passed to %s",
					 handle, name + 5);
	}
	return window;
}

static panel_t *
panel_new (qwaq_resources_t *res)
{
	return PR_RESNEW (res->panel_map);
}

static void
panel_free (qwaq_resources_t *res, panel_t *win)
{
	PR_RESFREE (res->panel_map, win);
}

static void
panel_reset (qwaq_resources_t *res)
{
	PR_RESRESET (res->panel_map);
}

static inline panel_t *
panel_get (qwaq_resources_t *res, unsigned index)
{
	return PR_RESGET(res->panel_map, index);
}

static inline int __attribute__((pure))
panel_index (qwaq_resources_t *res, panel_t *win)
{
	return PR_RESINDEX (res->panel_map, win);
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
qwaq_cmd_peek (qwaq_resources_t *res, int ahead)
{
	return *RB_PEEK_DATA (res->commands.pipe, ahead);
}

static dstring_t *
qwaq_cmd_string (qwaq_resources_t *res, int string_id)
{
	return res->commands.strings + string_id;
}

static void
cmd_syncprint (qwaq_resources_t *res)
{
	int         string_id = qwaq_cmd_peek (res, 2);

	Sys_Printf ("%s\n", qwaq_cmd_string (res, string_id)->str);
	qwaq_pipe_release_string (&res->commands, string_id);
}

static void
cmd_newwin (qwaq_resources_t *res)
{
	int         xpos = qwaq_cmd_peek (res, 2);
	int         ypos = qwaq_cmd_peek (res, 3);
	int         xlen = qwaq_cmd_peek (res, 4);
	int         ylen = qwaq_cmd_peek (res, 5);

	window_t   *window = window_new (res);
	window->win = newwin (ylen, xlen, ypos, xpos);
	keypad (window->win, FALSE);	// do it ourselves

	int         window_id = window_index (res, window);
	int         cmd_result[] = { qwaq_cmd_newwin, window_id };
	qwaq_pipe_submit (&res->results, cmd_result, CMD_SIZE (cmd_result));
}

static void
cmd_delwin (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	delwin (window->win);
	window_free (res, window);
}

static void
cmd_getwrect (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         xpos, ypos;
	int         xlen, ylen;

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	getparyx (window->win, ypos, xpos);
	if (xpos == -1 && ypos ==-1) {
		getbegyx (window->win, ypos, xpos);
	}
	getmaxyx (window->win, ylen, xlen);

	int         cmd_result[] = { qwaq_cmd_getwrect, xpos, ypos, xlen, ylen };
	qwaq_pipe_submit (&res->results, cmd_result, CMD_SIZE (cmd_result));
}

static void
cmd_new_panel (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	panel_t    *panel = panel_new (res);
	panel->panel = new_panel (window->win);
	panel->window_id = window_id;

	int         panel_id = panel_index (res, panel);
	int         cmd_result[] = { qwaq_cmd_new_panel, panel_id };
	qwaq_pipe_submit (&res->results, cmd_result, CMD_SIZE (cmd_result));
}

static void
cmd_del_panel (qwaq_resources_t *res)
{
	int         panel_id = qwaq_cmd_peek (res, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	del_panel (panel->panel);
	panel_free (res, panel);
}

static void
cmd_hide_panel (qwaq_resources_t *res)
{
	int         panel_id = qwaq_cmd_peek (res, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	hide_panel (panel->panel);
}

static void
cmd_show_panel (qwaq_resources_t *res)
{
	int         panel_id = qwaq_cmd_peek (res, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	show_panel (panel->panel);
}

static void
cmd_top_panel (qwaq_resources_t *res)
{
	int         panel_id = qwaq_cmd_peek (res, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	top_panel (panel->panel);
}

static void
cmd_bottom_panel (qwaq_resources_t *res)
{
	int         panel_id = qwaq_cmd_peek (res, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	bottom_panel (panel->panel);
}

static void
cmd_move_panel (qwaq_resources_t *res)
{
	int         panel_id = qwaq_cmd_peek (res, 2);
	int         x = qwaq_cmd_peek (res, 3);
	int         y = qwaq_cmd_peek (res, 4);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	move_panel (panel->panel, y, x);
}

static void
cmd_panel_window (qwaq_resources_t *res)
{
	int         panel_id = qwaq_cmd_peek (res, 2);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);

	int         window_id = panel->window_id;
	int         cmd_result[] = { qwaq_cmd_panel_window, window_id, };
	qwaq_pipe_submit (&res->results, cmd_result, CMD_SIZE (cmd_result));
}

static void
cmd_replace_panel (qwaq_resources_t *res)
{
	int         panel_id = qwaq_cmd_peek (res, 2);
	int         window_id = qwaq_cmd_peek (res, 3);

	panel_t   *panel = get_panel (res, __FUNCTION__, panel_id);
	window_t  *window = get_window (res, __FUNCTION__, window_id);

	replace_panel (panel->panel, window->win);
}

static void
cmd_update_panels (qwaq_resources_t *res)
{
	update_panels ();
}

static void
cmd_doupdate (qwaq_resources_t *res)
{
	doupdate ();
}

static void
cmd_mvwaddstr (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         x = qwaq_cmd_peek (res, 3);
	int         y = qwaq_cmd_peek (res, 4);
	int         string_id = qwaq_cmd_peek (res, 5);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	mvwaddstr (window->win, y, x, qwaq_cmd_string (res, string_id)->str);
	qwaq_pipe_release_string (&res->commands, string_id);
}

static void
cmd_waddstr (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         string_id = qwaq_cmd_peek (res, 3);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	waddstr (window->win, qwaq_cmd_string (res, string_id)->str);
	qwaq_pipe_release_string (&res->commands, string_id);
}

static void
cmd_mvwaddch (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         x = qwaq_cmd_peek (res, 3);
	int         y = qwaq_cmd_peek (res, 4);
	int         ch = qwaq_cmd_peek (res, 5);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	mvwaddch (window->win, y, x, ch);
}

static void
cmd_waddch (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         ch = qwaq_cmd_peek (res, 3);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	waddch (window->win, ch);
}

static void
cmd_wrefresh (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	wrefresh (window->win);
}

static void
cmd_init_pair (qwaq_resources_t *res)
{
	int         pair = qwaq_cmd_peek (res, 2);
	int         f = qwaq_cmd_peek (res, 3);
	int         b = qwaq_cmd_peek (res, 4);

	init_pair (pair, f, b);
}

static void
cmd_wbkgd (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         ch = qwaq_cmd_peek (res, 3);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	wbkgd (window->win, ch);
}

static void
cmd_werase (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	werase (window->win);
}

static void
cmd_scrollok (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         flag = qwaq_cmd_peek (res, 3);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	scrollok (window->win, flag);
}

static void
cmd_wmove (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         x = qwaq_cmd_peek (res, 3);
	int         y = qwaq_cmd_peek (res, 4);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	wmove (window->win, y, x);
}

static void
cmd_move (qwaq_resources_t *res)
{
	int         x = qwaq_cmd_peek (res, 2);
	int         y = qwaq_cmd_peek (res, 3);

	move (y, x);
}

static void
cmd_curs_set (qwaq_resources_t *res)
{
	int         visibility = qwaq_cmd_peek (res, 2);

	curs_set (visibility);
}

static void
cmd_wborder (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         ls = qwaq_cmd_peek (res, 3);
	int         rs = qwaq_cmd_peek (res, 4);
	int         ts = qwaq_cmd_peek (res, 5);
	int         bs = qwaq_cmd_peek (res, 6);
	int         tl = qwaq_cmd_peek (res, 7);
	int         tr = qwaq_cmd_peek (res, 8);
	int         bl = qwaq_cmd_peek (res, 9);
	int         br = qwaq_cmd_peek (res, 10);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	wborder (window->win, ls, rs, ts, bs, tl, tr, bl, br);
}

static void
cmd_mvwblit_line (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         x = qwaq_cmd_peek (res, 3);
	int         y = qwaq_cmd_peek (res, 4);
	int         chs_id = qwaq_cmd_peek (res, 5);
	int         len = qwaq_cmd_peek (res, 6);
	int        *chs = (int *) qwaq_cmd_string (res, chs_id)->str;
	int         save_x;
	int         save_y;

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	getyx (window->win, save_y, save_x);
	for (int i = 0; i < len; i++) {
		if (chs[i] & 0xff) {
			mvwaddch (window->win, y, x + i, chs[i]);
		}
	}
	wmove (window->win, save_y, save_x);
	qwaq_pipe_release_string (&res->commands, chs_id);
}

static void
cmd_wresize (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         width = qwaq_cmd_peek (res, 3);
	int         height = qwaq_cmd_peek (res, 4);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	wresize (window->win, height, width);
}

static void
cmd_resizeterm (qwaq_resources_t *res)
{
	int         width = qwaq_cmd_peek (res, 2);
	int         height = qwaq_cmd_peek (res, 3);

	resizeterm (height, width);
}

static void
cmd_mvwhline (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         x = qwaq_cmd_peek (res, 3);
	int         y = qwaq_cmd_peek (res, 4);
	int         ch = qwaq_cmd_peek (res, 5);
	int         n = qwaq_cmd_peek (res, 6);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	mvwhline (window->win, y, x, ch, n);
}

static void
cmd_mvwvline (qwaq_resources_t *res)
{
	int         window_id = qwaq_cmd_peek (res, 2);
	int         x = qwaq_cmd_peek (res, 3);
	int         y = qwaq_cmd_peek (res, 4);
	int         ch = qwaq_cmd_peek (res, 5);
	int         n = qwaq_cmd_peek (res, 6);

	window_t   *window = get_window (res, __FUNCTION__, window_id);
	mvwvline (window->win, y, x, ch, n);
}

static void
dump_command (qwaq_resources_t *res, int len)
{
	if (0) {
		qwaq_commands cmd = qwaq_cmd_peek (res, 0);
		Sys_Printf ("%s[%d]", qwaq_command_names[cmd], len);
		if (cmd == qwaq_cmd_syncprint) {
			Sys_Printf (" ");
		} else {
			for (int i = 2; i < len; i++) {
				Sys_Printf (" %d", qwaq_cmd_peek (res, i));
			}
			Sys_Printf ("\n");
		}
	}
}

static void
process_commands (qwaq_resources_t *res)
{
	struct timespec timeout;
	int         avail;
	int         len;
	int         ret = 0;

	pthread_mutex_lock (&res->commands.pipe_cond.mut);
	qwaq_init_timeout (&timeout, 20 * 1000000);
	while (RB_DATA_AVAILABLE (res->commands.pipe) < 2 && ret == 0) {
		ret = pthread_cond_timedwait (&res->commands.pipe_cond.rcond,
									  &res->commands.pipe_cond.mut, &timeout);
	}
	// The mutex is unlocked at the top of the loop and locked again
	// (for broadcast) at the bottom, then finally unlocked after the loop.
	// It should be only the data availability check that's not threadsafe
	// as the mutex is not released until after the data has been written to
	// the buffer.
	while ((avail = RB_DATA_AVAILABLE (res->commands.pipe)) >= 2
		   && avail >= (len = qwaq_cmd_peek (res, 1))) {
		pthread_mutex_unlock (&res->commands.pipe_cond.mut);
		dump_command (res, len);
		qwaq_commands cmd = qwaq_cmd_peek (res, 0);
		switch (cmd) {
			case qwaq_cmd_syncprint:
				cmd_syncprint (res);
				break;
			case qwaq_cmd_newwin:
				cmd_newwin (res);
				break;
			case qwaq_cmd_delwin:
				cmd_delwin (res);
				break;
			case qwaq_cmd_getwrect:
				cmd_getwrect (res);
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
			case qwaq_cmd_panel_window:
				cmd_panel_window (res);
				break;
			case qwaq_cmd_replace_panel:
				cmd_replace_panel (res);
				break;
			case qwaq_cmd_update_panels:
				cmd_update_panels (res);
				break;
			case qwaq_cmd_doupdate:
				cmd_doupdate (res);
				break;
			case qwaq_cmd_mvwaddstr:
				cmd_mvwaddstr (res);
				break;
			case qwaq_cmd_waddstr:
				cmd_waddstr (res);
				break;
			case qwaq_cmd_mvwaddch:
				cmd_mvwaddch (res);
				break;
			case qwaq_cmd_waddch:
				cmd_waddch (res);
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
			case qwaq_cmd_werase:
				cmd_werase (res);
				break;
			case qwaq_cmd_scrollok:
				cmd_scrollok (res);
				break;
			case qwaq_cmd_wmove:
				cmd_wmove (res);
				break;
			case qwaq_cmd_move:
				cmd_move (res);
				break;
			case qwaq_cmd_curs_set:
				cmd_curs_set (res);
				break;
			case qwaq_cmd_wborder:
				cmd_wborder (res);
				break;
			case qwaq_cmd_mvwblit_line:
				cmd_mvwblit_line (res);
				break;
			case qwaq_cmd_wresize:
				cmd_wresize (res);
				break;
			case qwaq_cmd_resizeterm:
				cmd_resizeterm (res);
				break;
			case qwaq_cmd_mvwhline:
				cmd_mvwhline (res);
				break;
			case qwaq_cmd_mvwvline:
				cmd_mvwvline (res);
				break;
		}
		pthread_mutex_lock (&res->commands.pipe_cond.mut);
		RB_RELEASE (res->commands.pipe, len);
		pthread_cond_broadcast (&res->commands.pipe_cond.wcond);
		// unlocked at top of top if there's more data, otherwise just after
		// the loop
	}
	pthread_mutex_unlock (&res->commands.pipe_cond.mut);
}

static int need_endwin;
static void
bi_shutdown (void *_pr)
{
	if (need_endwin) {
		qwaq_input_disable_mouse ();
		endwin ();
	}
}

static void
bi_syncprintf (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         string_id = qwaq_pipe_acquire_string (&res->commands);
	dstring_t  *print_buffer = qwaq_cmd_string (res, string_id);
	int         command[] = { qwaq_cmd_syncprint, 0, string_id };

	command[1] = CMD_SIZE(command);

	dstring_clearstr (print_buffer);
	RUA_Sprintf (pr, print_buffer, "syncprintf", 0);

	qwaq_pipe_submit (&res->commands, command, command[1]);
}

static void
bi_create_window (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         xpos = P_INT (pr, 0);
	int         ypos = P_INT (pr, 1);
	int         xlen = P_INT (pr, 2);
	int         ylen = P_INT (pr, 3);
	int         command[] = {
					qwaq_cmd_newwin, 0,
					xpos, ypos, xlen, ylen,
				};
	command[1] = CMD_SIZE(command);
	qwaq_pipe_submit (&res->commands, command, command[1]);

	int         cmd_result[2];
	qwaq_pipe_receive (&res->results, cmd_result, qwaq_cmd_newwin,
					   CMD_SIZE (cmd_result));
	R_INT (pr) = cmd_result[1];
}

static void
bi_destroy_window (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);

	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_delwin, 0, window_id, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}

static void
qwaq_getwrect (qwaq_resources_t *res, int window_id)
{
	progs_t    *pr = res->pr;

	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_getwrect, 0, window_id, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);

		int         cmd_result[5];
		qwaq_pipe_receive (&res->results, cmd_result, qwaq_cmd_getwrect,
						   CMD_SIZE (cmd_result));
		// return xpos, ypos, xlen, ylen
		(&R_INT (pr))[0] = cmd_result[1];
		(&R_INT (pr))[1] = cmd_result[2];
		(&R_INT (pr))[2] = cmd_result[3];
		(&R_INT (pr))[3] = cmd_result[4];
	}
}
static void
bi_getwrect (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	qwaq_getwrect (res, P_INT (pr, 0));
}

static void
bi_create_panel (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);

	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_new_panel, 0, window_id, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);

		int         cmd_result[2];
		qwaq_pipe_receive (&res->results, cmd_result, qwaq_cmd_new_panel,
						   CMD_SIZE (cmd_result));
		R_INT (pr) = cmd_result[1];
	}
}

static void
panel_command (progs_t *pr, qwaq_resources_t *res, qwaq_commands cmd)
{
	int         panel_id = P_INT (pr, 0);

	if (get_panel (res, __FUNCTION__, panel_id)) {
		int         command[] = { cmd, 0, panel_id, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}

static void
bi_destroy_panel (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	panel_command (pr, res, qwaq_cmd_del_panel);
}

static void
bi_hide_panel (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	panel_command (pr, res, qwaq_cmd_hide_panel);
}

static void
bi_show_panel (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	panel_command (pr, res, qwaq_cmd_show_panel);
}

static void
bi_top_panel (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	panel_command (pr, res, qwaq_cmd_top_panel);
}

static void
bi_bottom_panel (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	panel_command (pr, res, qwaq_cmd_bottom_panel);
}

static void
bi_move_panel (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         panel_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);

	if (get_panel (res, __FUNCTION__, panel_id)) {
		int         command[] = { qwaq_cmd_move_panel, 0, panel_id, x, y, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}

static void
bi_panel_window (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         panel_id = P_INT (pr, 0);

	if (get_panel (res, __FUNCTION__, panel_id)) {
		int         command[] = { qwaq_cmd_panel_window, 0, panel_id, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);

		int         cmd_result[2];
		qwaq_pipe_receive (&res->results, cmd_result, qwaq_cmd_panel_window,
						   CMD_SIZE (cmd_result));
		(&R_INT (pr))[0] = cmd_result[1];
	}
}

static void
bi_replace_panel (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         panel_id = P_INT (pr, 0);
	int         window_id = P_INT (pr, 1);

	if (get_panel (res, __FUNCTION__, panel_id)
		&& get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_replace_panel, 0,
								  panel_id, window_id};
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}

static void
qwaq_update_panels (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;

	int         command[] = { qwaq_cmd_update_panels, 0, };
	command[1] = CMD_SIZE(command);
	qwaq_pipe_submit (&res->commands, command, command[1]);
}
static void
bi_update_panels (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	qwaq_update_panels (pr, res);
}

static void
qwaq_doupdate (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;

	int         command[] = { qwaq_cmd_doupdate, 0, };
	command[1] = CMD_SIZE(command);
	qwaq_pipe_submit (&res->commands, command, command[1]);
}
static void
bi_doupdate (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	qwaq_doupdate (pr, res);
}

static void
qwaq_waddstr (qwaq_resources_t *res, int window_id, const char *str)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         string_id = qwaq_pipe_acquire_string (&res->commands);
		dstring_t  *print_buffer = qwaq_cmd_string (res, string_id);
		int         command[] = {
						qwaq_cmd_waddstr, 0,
						window_id, string_id
					};

		command[1] = CMD_SIZE(command);

		dstring_copystr (print_buffer, str);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_waddstr (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	const char *str = P_GSTRING (pr, 1);

	qwaq_waddstr (res, window_id, str);
}

static void
qwaq_wresize (qwaq_resources_t *res, int window_id, int width, int height)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = {
						qwaq_cmd_wresize, 0,
						window_id, width, height
					};

		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_wresize (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	int         width = P_INT (pr, 1);
	int         height = P_INT (pr, 2);

	qwaq_wresize (res, window_id, width, height);
}

static void
qwaq_resizeterm (qwaq_resources_t *res, int width, int height)
{
	int         command[] = { qwaq_cmd_resizeterm, 0, width, height };

	command[1] = CMD_SIZE(command);
	qwaq_pipe_submit (&res->commands, command, command[1]);
}
static void
bi_resizeterm (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         width = P_INT (pr, 0);
	int         height = P_INT (pr, 1);

	qwaq_resizeterm (res, width, height);
}

static void
qwaq_mvwhline (qwaq_resources_t *res, int window_id,
			   int x, int y, int ch, int n)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = {
						qwaq_cmd_mvwhline, 0,
						window_id, x, y, ch, n
					};

		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_mvwhline (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);
	int         ch = P_INT (pr, 3);
	int         n = P_INT (pr, 4);

	qwaq_mvwhline (res, window_id, x, y, ch, n);
}

static void
qwaq_mvwvline (qwaq_resources_t *res, int window_id,
			   int x, int y, int ch, int n)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = {
						qwaq_cmd_mvwvline, 0,
						window_id, x, y, ch, n
					};

		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_mvwvline (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);
	int         ch = P_INT (pr, 3);
	int         n = P_INT (pr, 4);

	qwaq_mvwvline (res, window_id, x, y, ch, n);
}

static void
qwaq_mvwaddstr (qwaq_resources_t *res, int window_id,
				int x, int y, const char *str)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         string_id = qwaq_pipe_acquire_string (&res->commands);
		dstring_t  *print_buffer = qwaq_cmd_string (res, string_id);
		int         command[] = {
						qwaq_cmd_mvwaddstr, 0,
						window_id, x, y, string_id
					};

		command[1] = CMD_SIZE(command);

		dstring_copystr (print_buffer, str);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_mvwaddstr (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);
	const char *str = P_GSTRING (pr, 3);

	qwaq_mvwaddstr (res, window_id, x, y, str);
}

static void
qwaq_mvwprintf (qwaq_resources_t *res, int window_id,
				int x, int y, int fmt_arg)
{
	progs_t    *pr = res->pr;

	if (get_window (res, __FUNCTION__, window_id)) {
		int         string_id = qwaq_pipe_acquire_string (&res->commands);
		dstring_t  *print_buffer = qwaq_cmd_string (res, string_id);
		int         command[] = {
						qwaq_cmd_mvwaddstr, 0,
						window_id, x, y, string_id
					};

		command[1] = CMD_SIZE(command);

		dstring_clearstr (print_buffer);
		RUA_Sprintf (pr, print_buffer, "mvwaddstr", fmt_arg);

		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_mvwprintf (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);

	qwaq_mvwprintf (res, window_id, x, y, 3);
}

static void
qwaq_wprintf (qwaq_resources_t *res, int window_id, int fmt_arg)
{
	progs_t    *pr = res->pr;

	if (get_window (res, __FUNCTION__, window_id)) {
		int         string_id = qwaq_pipe_acquire_string (&res->commands);
		dstring_t  *print_buffer = qwaq_cmd_string (res, string_id);
		int         command[] = {
						qwaq_cmd_waddstr, 0,
						window_id, string_id
					};

		command[1] = CMD_SIZE(command);

		dstring_clearstr (print_buffer);
		RUA_Sprintf (pr, print_buffer, "wprintf", fmt_arg);

		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_wprintf (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);

	qwaq_wprintf (res, window_id, 1);
}

static void
qwaq_wvprintf (qwaq_resources_t *res, int window_id,
			   const char *fmt, pr_va_list_t *args)
{
	progs_t    *pr = res->pr;
	pr_type_t  *list_start = PR_GetPointer (pr, args->list);
	pr_type_t **list = alloca (args->count * sizeof (*list));

	for (int i = 0; i < args->count; i++) {
		list[i] = list_start + i * pr->pr_param_size;
	}

	if (get_window (res, __FUNCTION__, window_id)) {
		int         string_id = qwaq_pipe_acquire_string (&res->commands);
		dstring_t  *print_buffer = qwaq_cmd_string (res, string_id);
		int         command[] = {
						qwaq_cmd_waddstr, 0,
						window_id, string_id
					};

		command[1] = CMD_SIZE(command);

		dstring_clearstr (print_buffer);
		PR_Sprintf (pr, print_buffer, "wvprintf", fmt, args->count, list);

		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_wvprintf (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	const char *fmt = P_GSTRING (pr, 1);
	__auto_type args = (pr_va_list_t *) &P_POINTER (pr, 2);

	qwaq_wvprintf (res, window_id, fmt, args);
}

static void
qwaq_waddch (qwaq_resources_t *res, int window_id, int ch)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_waddch, 0, window_id, ch };

		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_waddch (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         ch = P_INT (pr, 0);

	qwaq_waddch (res, window_id, ch);
}

static void
qwaq_mvwvprintf (qwaq_resources_t *res, int window_id, int x, int y,
				 const char *fmt, pr_va_list_t *args)
{
	progs_t    *pr = res->pr;
	pr_type_t  *list_start = PR_GetPointer (pr, args->list);
	pr_type_t **list = alloca (args->count * sizeof (*list));

	for (int i = 0; i < args->count; i++) {
		list[i] = list_start + i * pr->pr_param_size;
	}

	if (get_window (res, __FUNCTION__, window_id)) {
		int         string_id = qwaq_pipe_acquire_string (&res->commands);
		dstring_t  *print_buffer = qwaq_cmd_string (res, string_id);
		int         command[] = {
						qwaq_cmd_mvwaddstr, 0,
						window_id, x, y, string_id
					};

		command[1] = CMD_SIZE(command);

		dstring_clearstr (print_buffer);
		PR_Sprintf (pr, print_buffer, "mvwvprintf", fmt, args->count, list);

		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_mvwvprintf (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);
	const char *fmt = P_GSTRING (pr, 2);
	__auto_type args = (pr_va_list_t *) &P_POINTER (pr, 3);

	qwaq_mvwvprintf (res, window_id, x, y, fmt, args);
}

static void
qwaq_mvwaddch (qwaq_resources_t *res, int window_id, int x, int y, int ch)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = {
						qwaq_cmd_mvwaddch, 0,
						window_id, x, y, ch
					};
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_mvwaddch (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);
	int         ch = P_INT (pr, 3);

	qwaq_mvwaddch (res, window_id, x, y, ch);
}

static void
qwaq_wrefresh (qwaq_resources_t *res, int window_id)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_wrefresh, 0, window_id, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_wrefresh (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);

	qwaq_wrefresh (res, window_id);
}

static void
bi_max_colors (progs_t *pr, void *_res)
{
	R_INT (pr) = COLORS;
}

static void
bi_max_color_pairs (progs_t *pr, void *_res)
{
	R_INT (pr) = COLOR_PAIRS;
}

static void
qwaq_init_pair (qwaq_resources_t *res, int pair, int f, int b)
{

	int         command[] = { qwaq_cmd_init_pair, 0, pair, f, b, };
	command[1] = CMD_SIZE(command);
	qwaq_pipe_submit (&res->commands, command, command[1]);
}
static void
bi_init_pair (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         pair = P_INT (pr, 0);
	int         f = P_INT (pr, 1);
	int         b = P_INT (pr, 2);

	qwaq_init_pair (res, pair, f, b);
}

static void
qwaq_wbkgd (qwaq_resources_t *res, int window_id, int ch)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_wbkgd, 0, window_id, ch, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_wbkgd (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         ch = P_INT (pr, 1);

	qwaq_wbkgd (res, window_id, ch);
}

static void
qwaq_werase (qwaq_resources_t *res, int window_id, int ch)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_werase, 0, window_id, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_werase (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         ch = P_INT (pr, 1);

	qwaq_werase (res, window_id, ch);
}

static void
qwaq_scrollok (qwaq_resources_t *res, int window_id, int flag)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_scrollok, 0, window_id, flag, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_scrollok (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         flag = P_INT (pr, 1);

	qwaq_scrollok (res, window_id, flag);
}

static void
qwaq_wmove (qwaq_resources_t *res, int window_id, int x, int y)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_wmove, 0, window_id, x, y, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_wmove (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);

	qwaq_wmove (res, window_id, x, y);
}

static const char qwaq_acs_char_map[] = "lmkjtuvwqxnos`afg~,+.-hi0pryz{|}";
static void
qwaq_acs_char (progs_t *pr, unsigned acs)
{
	if (acs < 256) {
		R_INT (pr) = NCURSES_ACS(acs);
	} else if (acs - 256 < sizeof (qwaq_acs_char_map)) {
		R_INT (pr) = NCURSES_ACS(qwaq_acs_char_map[acs - 256]);
	} else {
		R_INT (pr) = 0;
	}
}
static void
bi_acs_char (progs_t *pr, void *_res)
{
	int         acs = P_INT (pr, 0);

	qwaq_acs_char (pr, acs);
}

static void
qwaq_move (qwaq_resources_t *res, int x, int y)
{
	int         command[] = { qwaq_cmd_move, 0, x, y, };
	command[1] = CMD_SIZE(command);
	qwaq_pipe_submit (&res->commands, command, command[1]);
}
static void
bi_move (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);

	qwaq_move (res, x, y);
}

static void
qwaq_curs_set (qwaq_resources_t *res, int visibility)
{
	int         command[] = { qwaq_cmd_curs_set, 0, visibility, };
	command[1] = CMD_SIZE(command);
	qwaq_pipe_submit (&res->commands, command, command[1]);
}
static void
bi_curs_set (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         visibility = P_INT (pr, 0);

	qwaq_curs_set (res, visibility);
}

static void
qwaq_wborder (qwaq_resources_t *res, int window_id,
			  box_sides_t sides, box_corners_t corns)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         command[] = { qwaq_cmd_wborder, 0, window_id,
								  sides.ls, sides.rs, sides.ts, sides.bs,
								  corns.tl, corns.tr, corns.bl, corns.br, };
		command[1] = CMD_SIZE(command);
		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_wborder (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	__auto_type sides = P_PACKED (pr, box_sides_t, 1);
	__auto_type corns = P_PACKED (pr, box_corners_t, 2);

	qwaq_wborder (res, window_id, sides, corns);
}

static void
qwaq__mvwblit_line (qwaq_resources_t *res, int window_id, int x, int y,
				    int *chs, int len)
{
	if (get_window (res, __FUNCTION__, window_id)) {
		int         chs_id = qwaq_pipe_acquire_string (&res->commands);
		dstring_t  *chs_buf = qwaq_cmd_string (res, chs_id);
		int         command[] = { qwaq_cmd_mvwblit_line, 0, window_id,
								  x, y, chs_id, len,};

		command[1] = CMD_SIZE(command);

		chs_buf->size = len * sizeof (int);
		dstring_adjust (chs_buf);
		memcpy (chs_buf->str, chs, len * sizeof (int));

		qwaq_pipe_submit (&res->commands, command, command[1]);
	}
}
static void
bi_mvwblit_line (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_INT (pr, 0);
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);
	int        *chs = (int *) P_GPOINTER (pr, 3);
	int         len = P_INT (pr, 4);
	qwaq__mvwblit_line (res, window_id, x, y, chs, len);
}

static void *
qwaq_curses_thread (qwaq_thread_t *thread)
{
	qwaq_resources_t *res = thread->data;

	while (1) {
		process_commands (res);
	}
	thread->return_code = 0;
	return thread;
}

static void
bi_initialize (progs_t *pr, void *data)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "curses");

	initscr ();
	need_endwin = 1;
	res->initialized = 1;
	start_color ();
	raw ();
	keypad (stdscr, FALSE);	// do it ourselves
	noecho ();
	nonl ();
	nodelay (stdscr, TRUE);
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	refresh();

	qwaq_input_enable_mouse ();

	res->stdscr.win = stdscr;

	create_thread (qwaq_curses_thread, res);
}

static void
bi__c_TextContext__is_initialized (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	R_INT (pr) = res->initialized;
}

static void
bi__c_TextContext__max_colors (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	bi_max_colors (pr, res);
}

static void
bi__c_TextContext__max_color_pairs (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	bi_max_color_pairs (pr, res);
}

static void
bi__c_TextContext__init_pair_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         pair = P_INT (pr, 2);
	int         f = P_INT (pr, 3);
	int         b = P_INT (pr, 4);

	qwaq_init_pair (res, pair, f, b);
}

static void
bi__c_TextContext__acs_char_ (progs_t *pr, void *_res)
{
	int         acs = P_INT (pr, 2);

	qwaq_acs_char (pr, acs);
}

static void
bi__c_TextContext__move_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	Point      *pos = &P_PACKED (pr, Point, 2);

	qwaq_move (res, pos->x, pos->y);
}

static void
bi__c_TextContext__curs_set_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         visibility = P_INT (pr, 2);

	qwaq_curs_set (res, visibility);
}

static void
bi__c_TextContext__doupdate (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	bi_doupdate (pr, res);
}

static void
bi__i_TextContext__mvprintf_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	Point      *pos = &P_PACKED (pr, Point, 2);

	qwaq_mvwprintf (res, window_id, pos->x, pos->y, 3);
}

static void
bi__i_TextContext__printf_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;

	qwaq_wprintf (res, window_id, 2);
}

static void
bi__i_TextContext__vprintf_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	const char *fmt = P_GSTRING (pr, 2);
	__auto_type args = (pr_va_list_t *) &P_POINTER (pr, 3);

	qwaq_wvprintf (res, window_id, fmt, args);
}

static void
bi__i_TextContext__addch_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	int         ch = P_INT (pr, 2);

	qwaq_waddch (res, window_id, ch);
}

static void
bi__i_TextContext__addstr_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	const char *str = P_GSTRING (pr, 2);

	qwaq_waddstr (res, window_id, str);
}

static void
bi__i_TextContext__mvvprintf_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	Point      *pos = &P_PACKED (pr, Point, 2);
	const char *fmt = P_GSTRING (pr, 3);
	__auto_type args = &P_PACKED (pr, pr_va_list_t, 4);

	qwaq_mvwvprintf (res, window_id, pos->x, pos->y, fmt, args);
}

static void
bi__i_TextContext__resizeTo_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	__auto_type self = &P_STRUCT (pr, qwaq_textcontext_t, 0);
	int         window_id = self->window;

	self->size = P_PACKED (pr, Extent, 2);
	qwaq_wresize (res, window_id, self->size.width, self->size.height);
	if (window_id == 1) {
		qwaq_wbkgd (res, window_id, self->background);
	}
}

static void
bi__c_TextContext__refresh (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	qwaq_update_panels (pr, res);
	qwaq_doupdate (pr, res);
}

static void
bi__i_TextContext__refresh (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;

	qwaq_update_panels (pr, res);
	if (window_id == 1) {
		qwaq_wrefresh (res, window_id);
		qwaq_doupdate (pr, res);
	}
}

static void
bi__i_TextContext__mvaddch_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	Point      *pos = &P_PACKED (pr, Point, 2);
	int         ch = P_INT (pr, 3);

	qwaq_mvwaddch (res, window_id, pos->x, pos->y, ch);
}

static void
bi__i_TextContext__mvaddstr_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	Point      *pos = &P_PACKED (pr, Point, 2);
	const char *str = P_GSTRING (pr, 3);

	qwaq_mvwaddstr (res, window_id, pos->x, pos->y, str);
}

static void
bi__i_TextContext__bkgd_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	__auto_type self = &P_STRUCT (pr, qwaq_textcontext_t, 0);
	int         window_id = self->window;
	int         ch = P_INT (pr, 2);

	self->background = ch;
	qwaq_wbkgd (res, window_id, ch);
}

static void
bi__i_TextContext__clear (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	__auto_type self = &P_STRUCT (pr, qwaq_textcontext_t, 0);
	int         window_id = self->window;
	int         ch = self->background;

	qwaq_werase (res, window_id, ch);
}

static void
bi__i_TextContext__scrollok_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	int         flag = P_INT (pr, 2);

	qwaq_scrollok (res, window_id, flag);
}

static void
bi__i_TextContext__border_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	int         window_id = P_STRUCT (pr, qwaq_textcontext_t, 0).window;
	__auto_type sides = P_PACKED (pr, box_sides_t, 2);
	__auto_type corns = P_PACKED (pr, box_corners_t, 3);

	qwaq_wborder (res, window_id, sides, corns);
}

static void
bi__i_TextContext__mvhline_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	__auto_type self = &P_STRUCT (pr, qwaq_textcontext_t, 0);
	int         window_id = self->window;
	__auto_type pos = &P_PACKED (pr, Point, 2);
	int         ch = P_INT (pr, 3);
	int         n = P_INT (pr, 4);

	qwaq_mvwhline (res, window_id, pos->x, pos->y, ch, n);
}

static void
bi__i_TextContext__mvvline_ (progs_t *pr, void *_res)
{
	qwaq_resources_t *res = _res;
	__auto_type self = &P_STRUCT (pr, qwaq_textcontext_t, 0);
	int         window_id = self->window;
	__auto_type pos = &P_PACKED (pr, Point, 2);
	int         ch = P_INT (pr, 3);
	int         n = P_INT (pr, 4);

	qwaq_mvwvline (res, window_id, pos->x, pos->y, ch, n);
}

static void
bi_curses_clear (progs_t *pr, void *_res)
{
	__auto_type res = (qwaq_resources_t *) _res;

	if (res->initialized) {
		qwaq_input_disable_mouse ();
		endwin ();
	}
	need_endwin = 0;
	window_reset (res);
	panel_reset (res);
}

static void
bi_curses_destroy (progs_t *pr, void *_res)
{
	free (_res);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(initialize,      0),
	bi(syncprintf,      -2, p(string)),
	bi(create_window,   4, p(int), p(int), p(int), p(int)),
	bi(destroy_window,  1, p(ptr)),
	bi(getwrect,        1, p(ptr)),
	bi(create_panel,    1, p(ptr)),
	bi(destroy_panel,   1, p(ptr)),
	bi(hide_panel,      1, p(ptr)),
	bi(show_panel,      1, p(ptr)),
	bi(top_panel,       1, p(ptr)),
	bi(bottom_panel,    1, p(ptr)),
	bi(move_panel,      3, p(ptr), p(int), p(int)),
	bi(panel_window,    1, p(ptr)),
	bi(replace_panel,   2, p(ptr), p(ptr)),
	bi(update_panels,   0),
	bi(doupdate,        0),
	bi(mvwprintf,       -5, p(ptr), p(int), p(int), p(string)),
	bi(wprintf,         -3, p(ptr), p(string)),
	bi(wvprintf,        3, p(ptr), p(string), P(1, 2)),
	bi(mvwvprintf,      5, p(ptr), p(int), p(int), p(string), P(1, 2)),
	bi(mvwaddch,        4, p(ptr), p(int), p(int), p(int)),
	bi(waddch,          2, p(ptr), p(int)),
	bi(mvwaddstr,       4, p(ptr), p(int), p(int), p(string)),
	bi(waddstr,         2, p(ptr), p(string)),
	bi(wrefresh,        1, p(ptr)),
	bi(max_colors,      0),
	bi(max_color_pairs, 0),
	bi(init_pair,       3, p(int), p(int), p(int)),
	bi(wbkgd,           2, p(ptr), p(int)),
	bi(werase,          1, p(ptr)),
	bi(scrollok,        2, p(ptr), p(int)),
	bi(wmove,           3, p(ptr), p(int), p(int), p(int)),
	bi(acs_char,        1, p(int)),
	bi(move,            2, p(int), p(int)),
	bi(curs_set,        1, p(int)),
	bi(wborder,         3, p(ptr), P(1, 4), P(1, 4)),
	bi(mvwblit_line,    5, p(ptr), p(int), p(int), p(ptr), p(int)),
	bi(wresize,         3, p(ptr), p(int), p(int)),
	bi(resizeterm,      2, p(int), p(int)),
	bi(mvwhline,        5, p(ptr), p(int), p(int), p(int), p(int)),
	bi(mvwvline,        5, p(ptr), p(int), p(int), p(int), p(int)),

	bi(_c_TextContext__is_initialized,  2, p(ptr), p(ptr)),
	bi(_c_TextContext__max_colors,      2, p(ptr), p(ptr)),
	bi(_c_TextContext__max_color_pairs, 2, p(ptr), p(ptr)),
	bi(_c_TextContext__init_pair_,      5, p(ptr), p(ptr),
										p(int), p(int), p(int)),
	bi(_c_TextContext__acs_char_,       3, p(ptr), p(ptr),
										p(int)),
	bi(_c_TextContext__move_,           3, p(ptr), p(ptr),
										P(1, 2)),
	bi(_c_TextContext__curs_set_,       3, p(ptr), p(ptr),
										p(int)),
	bi(_c_TextContext__doupdate,        2, p(ptr), p(ptr)),
	bi(_i_TextContext__mvprintf_,       -5, p(ptr), p(ptr),
										P(1, 2), p(string)),
	bi(_i_TextContext__printf_,         -4, p(ptr), p(ptr),
										p(string)),
	bi(_i_TextContext__vprintf_,        4, p(ptr), p(ptr),
										p(string), P(1, 2)),
	bi(_i_TextContext__addch_,          3, p(ptr), p(ptr),
										p(int)),
	bi(_i_TextContext__addstr_,         3, p(ptr), p(ptr),
										p(string)),
	bi(_i_TextContext__mvvprintf_,      5, p(ptr), p(ptr),
										P(1, 2), p(string), P(1, 2)),
	bi(_i_TextContext__resizeTo_,       3, p(ptr), p(ptr),
										P(1, 2)),
	bi(_c_TextContext__refresh,         2, p(ptr), p(ptr)),
	bi(_i_TextContext__refresh,         2, p(ptr), p(ptr)),
	bi(_i_TextContext__mvaddch_,        4, p(ptr), p(ptr),
										P(1, 2), p(int)),
	bi(_i_TextContext__mvaddstr_,       4, p(ptr), p(ptr),
										P(1, 2), p(string)),
	bi(_i_TextContext__bkgd_,           3, p(ptr), p(ptr),
										p(int)),
	bi(_i_TextContext__clear,           2, p(ptr), p(ptr)),
	bi(_i_TextContext__scrollok_,       3, p(ptr), p(ptr),
										p(int)),
	bi(_i_TextContext__border_,         4, p(ptr), p(ptr),
										P(1, 4), P(1, 4)),
	bi(_i_TextContext__mvhline_,        5, p(ptr), p(ptr),
										P(1, 2), p(int), p(int)),
	bi(_i_TextContext__mvvline_,        5, p(ptr), p(ptr),
										P(1, 2), p(int), p(int)),

	{0}
};

void
BI_Curses_Init (progs_t *pr)
{
	qwaq_resources_t *res = calloc (sizeof (*res), 1);
	res->pr = pr;

	qwaq_init_pipe (&res->commands);
	qwaq_init_pipe (&res->results);

	PR_Resources_Register (pr, "curses", res, bi_curses_clear,
						   bi_curses_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
	Sys_RegisterShutdown (bi_shutdown, pr);
}
