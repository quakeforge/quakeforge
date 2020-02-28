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
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "qwaq.h"
#include "event.h"

#define always_inline inline __attribute__((__always_inline__))
#define QUEUE_SIZE 16		// must be power of 2 greater than 1
#define QUEUE_MASK (QUEUE_SIZE - 1)
#define MOUSE_MOVES "\033[?1003h"	// Make the terminal report mouse movements

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

typedef struct window_s {
	WINDOW     *win;
} window_t;

typedef struct qwaq_resources_s {
	progs_t    *pr;
	int         initialized;
	dstring_t  *print_buffer;
	PR_RESMAP (window_t) window_map;
	RING_BUFFER (qwaq_event_t, QUEUE_SIZE) event_queue;
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
	nodelay (stdscr, TRUE);
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	write(1, MOUSE_MOVES, sizeof (MOUSE_MOVES) - 1);
	refresh();
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

	dstring_clearstr (res->print_buffer);
	PR_Sprintf (pr, res->print_buffer, "bi_wprintf", fmt, count, args);
	waddstr (window->win, res->print_buffer->str);
	wrefresh (window->win);
}

static void
bi_mvwprintf (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	window_t   *window = get_window (res, __FUNCTION__, P_INT (pr, 0));
	int         x = P_INT (pr, 1);
	int         y = P_INT (pr, 2);
	const char *fmt = P_GSTRING (pr, 3);
	int         count = pr->pr_argc - 4;
	pr_type_t **args = pr->pr_params + 4;

	dstring_clearstr (res->print_buffer);
	PR_Sprintf (pr, res->print_buffer, "bi_wprintf", fmt, count, args);
	mvwaddstr (window->win, y, x, res->print_buffer->str);
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
bi_process_input (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
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

static void
bi_get_event (progs_t *pr)
{
	qwaq_resources_t *res = PR_Resources_Find (pr, "qwaq");
	qwaq_event_t *event = &G_STRUCT (pr, qwaq_event_t, P_INT (pr, 0));

	R_INT (pr) = get_event (res, event);
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
	{"mvwprintf",		bi_mvwprintf,		-1},
	{"wgetch",			bi_wgetch,			-1},
	{"process_input",	bi_process_input,	-1},
	{"get_event",		bi_get_event,		-1},
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
