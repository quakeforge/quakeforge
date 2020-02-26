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
#include <stdlib.h>
#include <string.h>

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "qwaq.h"

#define always_inline inline __attribute__((__always_inline__))

typedef struct window_s {
	WINDOW     *win;
} window_t;

typedef struct qwaq_resources_s {
	progs_t    *pr;
	int         initialized;
	dstring_t  *print_buffer;
	PR_RESMAP (window_t) window_map;
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

static always_inline window_t *
get_window (qwaq_resources_t *res, const char *name, int handle)
{
	window_t   *window = window_get (res, handle);

	if (!window || !window->win) {
		PR_RunError (res->pr, "invalid window passed to %s", name + 3);
	}
	return window;
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
bi_initialize (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");

	initscr ();
	need_endwin = 1;
	res->initialized = 1;
	raw ();
	keypad (stdscr, TRUE);
	noecho ();
	nonl ();
}

static void
bi_create_window (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	int         xpos = P_INT (pr, 0);
	int         ypos = P_INT (pr, 1);
	int         xlen = P_INT (pr, 2);
	int         ylen = P_INT (pr, 3);
	window_t   *window = window_new (res);
	window->win = newwin (ylen, xlen, ypos, xpos);
	keypad (window->win, TRUE);
	R_INT (pr) = window_index (res, window);
}

static void
bi_destroy_window (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	window_t   *window = get_window (res, __FUNCTION__, P_INT (pr, 0));
	delwin (window->win);
	window->win = 0;
	window_free (res, window);
}

static void
bi_wprintf (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	window_t   *window = get_window (res, __FUNCTION__, P_INT (pr, 0));
	const char *fmt = P_GSTRING (pr, 1);
	int         count = pr->pr_argc - 2;
	pr_type_t **args = pr->pr_params + 2;

	PR_Sprintf (pr, res->print_buffer, "bi_wprintf", fmt, count, args);
	waddstr (window->win, res->print_buffer->str);
	wrefresh (window->win);
}

static void
bi_wgetch (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	window_t   *window = get_window (res, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = wgetch (window->win);
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
}

static builtin_t builtins[] = {
	{"initialize",		bi_initialize,		-1},
	{"create_window",	bi_create_window,	-1},
	{"destroy_window",	bi_destroy_window,	-1},
	{"wprintf",			bi_wprintf,			-1},
	{"wgetch",			bi_wgetch,			-1},
	{0}
};

void
BI_Init (progs_t *pr)
{
	qwaq_resources_t *res = calloc (sizeof (*res), 1);
	res->pr = pr;
	res->print_buffer = dstring_newstr ();

	PR_Resources_Register (pr, "qwaq", res, bi_qwaq_clear);
	PR_RegisterBuiltins (pr, builtins);
	Sys_RegisterShutdown (bi_shutdown);
}
