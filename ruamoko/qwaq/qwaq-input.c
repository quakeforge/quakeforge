/*
	qwaq-input.c

	Input handling

	Copyright (C) 2020 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/03/21

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

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/sys.h"

#include "qwaq.h"
#include "event.h"
#include "qwaq-curses.h"

#define always_inline inline __attribute__((__always_inline__))

#define MOUSE_MOVES_ON "\033[?1003h"
#define MOUSE_MOVES_OFF "\033[?1003l"
#define SGR_ON "\033[?1006h"
#define SGR_OFF "\033[?1006l"
#define WHEEL_BUTTONS 0x7c	// scroll up/down/left/right - always click

static void
add_event (qwaq_resources_t *res, qwaq_event_t *event)
{
	struct timespec timeout;
	int         merged = 0;
	int         ret = 0;

	// merge motion events
	pthread_mutex_lock (&res->event_cond.mut);
	unsigned    last = RB_DATA_AVAILABLE (res->event_queue);
	if (event->what == qe_mousemove && last > 1
		&& RB_PEEK_DATA(res->event_queue, last - 1).what == qe_mousemove) {
		RB_POKE_DATA(res->event_queue, last - 1, *event);
		merged = 1;
		pthread_cond_broadcast (&res->event_cond.rcond);
	}
	pthread_mutex_unlock (&res->event_cond.mut);
	if (merged) {
		return;
	}

	pthread_mutex_lock (&res->event_cond.mut);
	qwaq_init_timeout (&timeout, 5000 * 1000000L);
	while (RB_SPACE_AVAILABLE (res->event_queue) < 1 && ret == 0) {
		ret = pthread_cond_timedwait (&res->event_cond.wcond,
									  &res->event_cond.mut, &timeout);
	}
	RB_WRITE_DATA (res->event_queue, event, 1);
	pthread_cond_broadcast (&res->event_cond.rcond);
	pthread_mutex_unlock (&res->event_cond.mut);
}

static void
key_event (qwaq_resources_t *res, int key)
{
	qwaq_event_t event = {};
	event.what = qe_keydown;
	event.when = Sys_DoubleTime ();
	event.key = key;
	add_event (res, &event);
}

static void
mouse_event (qwaq_resources_t *res, int what, int x, int y)
{
	qwaq_event_t event = {};

	event.what = what;
	event.when = Sys_DoubleTime ();
	event.mouse.x = x;
	event.mouse.y = y;
	event.mouse.buttons = res->button_state;

	if (event.what == qe_mousedown) {
		if (event.when - res->lastClick.when <= 0.4
			&& res->lastClick.mouse.buttons == event.mouse.buttons
			&& res->lastClick.mouse.click < 3) {
			event.mouse.click = res->lastClick.mouse.click + 1;
			event.what = qe_mouseclick;
		}
		res->lastClick = event;
	} else if (event.what == qe_mouseclick) {
		// scroll button event, so always single click
		event.mouse.click = 1;
	}
	add_event (res, &event);
}

static void
parse_mouse (qwaq_resources_t *res, unsigned ctrl, int x, int y, int cmd)
{
	int         button = ctrl & 3;
	int         ext = (ctrl >> 6);
	int         what = qe_none;

	if (ctrl & 0x20) {
		// motion-only event
		button = -1;
		what = qe_mousemove;
	} else {
		// xterm doesn't send release events for buttons 4-7
		res->button_state &= ~(0x78);
		if (!ext && button == 3 && !cmd) {
			res->button_state = 0;	// unknown which button was released
			button = -1;
			what = qe_mouseup;
		} else {
			if (ext) {
				button += 4 * ext - 1;
			}
			if (cmd == 'm') {
				res->button_state &= ~(1 << button);
				what = qe_mouseup;
			} else {
				res->button_state |= 1 << button;
				if (res->button_state & WHEEL_BUTTONS) {
					what = qe_mouseclick;
				} else {
					what = qe_mousedown;
				}
			}
		}
	}
	mouse_event (res, what, x, y);
}

static void
parse_x10 (qwaq_resources_t *res)
{
	int         x = (byte) res->escbuff.str[1] - '!';	// want 0-based
	int         y = (byte) res->escbuff.str[2] - '!';	// want 0-based
	unsigned    ctrl = (byte) res->escbuff.str[0] - ' ';

	parse_mouse (res, ctrl, x, y, 0);
}

static void
parse_sgr (qwaq_resources_t *res, char cmd)
{
	unsigned    ctrl, x, y;

	sscanf (res->escbuff.str, "%u;%u;%u", &ctrl, &x, &y);
	// mouse coords are 1-based, but want 0-based
	parse_mouse (res, ctrl, x - 1, y - 1, cmd);
}

static void process_char (qwaq_resources_t *res, char ch)
{
	if (ch == 0x1b) {
		// always reset if escape is seen, may be a desync
		res->escstate = esc_escape;
	} else {
		switch (res->escstate) {
			case esc_ground:
				key_event (res, (byte) ch);
				break;
			case esc_escape:
				if (ch == '[') {
					res->escstate = esc_csi;
				}
				break;
			case esc_csi:
				if (ch == 'M') {
					res->escstate = esc_mouse;
					dstring_clear (&res->escbuff);
				} else if (ch == '<') {
					res->escstate = esc_sgr;
					dstring_clear (&res->escbuff);
				} else {
					res->escstate = esc_ground;
				}
				break;
			case esc_mouse:
				dstring_append (&res->escbuff, &ch, 1);
				if (res->escbuff.size == 3) {
					parse_x10 (res);
					res->escstate = esc_ground;
				}
				break;
			case esc_sgr:
				if (isdigit (ch) || ch ==';') {
					dstring_append (&res->escbuff, &ch, 1);
				} else {
					if (ch == 'm' || ch == 'M') {
						dstring_append (&res->escbuff, "", 1);
						parse_sgr (res, ch);
					}
					res->escstate = esc_ground;
				}
				break;
		}
		//printf("res->escstate %d\n", res->escstate);
	}
}

void qwaq_input_init (qwaq_resources_t *res)
{
	// ncurses takes care of input mode for us, so need only tell xterm
	// what we need
	write(1, MOUSE_MOVES_ON, sizeof (MOUSE_MOVES_ON) - 1);
	write(1, SGR_ON, sizeof (SGR_ON) - 1);
}

void qwaq_input_shutdown (qwaq_resources_t *res)
{
	// ncurses takes care of input mode for us, so need only tell xterm
	// what we need
	write(1, SGR_OFF, sizeof (SGR_OFF) - 1);
	write(1, MOUSE_MOVES_OFF, sizeof (MOUSE_MOVES_OFF) - 1);
}

void qwaq_process_input (qwaq_resources_t *res)
{
	char        buf[256];
	int         len;

	while (Sys_CheckInput (1, -1)) {
		len = read(0, buf, sizeof (buf));
		for (int i = 0; i < len; i++) {
			process_char (res, buf[i]);
		}
	}
}
