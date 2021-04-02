/*
	input.c

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
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_SIGACTION	// no sigaction, no window resize
#include <sys/ioctl.h>
#include <termios.h>
#endif

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/keys.h"
#include "QF/sys.h"

#include "ruamoko/qwaq/qwaq.h"
#include "ruamoko/qwaq/ui/event.h"
#include "ruamoko/qwaq/ui/curses.h"

#define always_inline inline __attribute__((__always_inline__))

#define MOUSE_MOVES_ON "\033[?1003h"
#define MOUSE_MOVES_OFF "\033[?1003l"
#define SGR_ON "\033[?1006h"
#define SGR_OFF "\033[?1006l"
#define WHEEL_BUTTONS 0x78	// scroll up/down/left/right - always click

typedef struct qwaq_key_s {
	const char *sequence;
	knum_t      key;
	unsigned    shift;
} qwaq_key_t;

static qwaq_key_t default_keys[] = {
	{ "\033OP",		QFK_F1  },
	{ "\033OQ",		QFK_F2  },
	{ "\033OR",		QFK_F3  },
	{ "\033OS",		QFK_F4  },
	{ "\033[15~",	QFK_F5  },
	{ "\033[17~",	QFK_F6  },
	{ "\033[18~",	QFK_F7  },
	{ "\033[19~",	QFK_F8  },
	{ "\033[20~",	QFK_F9  },
	{ "\033[21~",	QFK_F10 },
	{ "\033[23~",	QFK_F11 },
	{ "\033[24~",	QFK_F12 },
	// shift F1-F12
	{ "\033[1;2P",	QFK_F13 },
	{ "\033[1;2Q",	QFK_F14 },
	{ "\033[1;2R",	QFK_F15 },
	{ "\033[1;2S",	QFK_F16 },
	{ "\033[15;2~",	QFK_F17 },
	{ "\033[17;2~",	QFK_F18 },
	{ "\033[18;2~",	QFK_F19 },
	{ "\033[19;2~",	QFK_F20 },
	{ "\033[20;2~",	QFK_F21 },
	{ "\033[21;2~",	QFK_F22 },
	{ "\033[23;2~",	QFK_F23 },
	{ "\033[24;2~",	QFK_F24 },
	// control F1-F12
	{ "\033[1;5P",	QFK_F25 },
	{ "\033[1;5Q",	QFK_F26 },
	{ "\033[1;5R",	QFK_F27 },
	{ "\033[1;5S",	QFK_F28 },
	{ "\033[15;5~",	QFK_F29 },
	{ "\033[17;5~",	QFK_F30 },
	{ "\033[18;5~",	QFK_F31 },
	{ "\033[19;5~",	QFK_F32 },
	{ "\033[20;5~",	QFK_F33 },
	{ "\033[21;5~",	QFK_F34 },
	{ "\033[23;5~",	QFK_F35 },
	{ "\033[24;5~",	QFK_F36 },
	// shift control F1-F12
	{ "\033[1;6P",	QFK_F37 },
	{ "\033[1;6Q",	QFK_F38 },
	{ "\033[1;6R",	QFK_F39 },
	{ "\033[1;6S",	QFK_F40 },
	{ "\033[15;6~",	QFK_F41 },
	{ "\033[17;6~",	QFK_F42 },
	{ "\033[18;6~",	QFK_F43 },
	{ "\033[19;6~",	QFK_F44 },
	{ "\033[20;6~",	QFK_F45 },
	{ "\033[21;6~",	QFK_F46 },
	{ "\033[23;6~",	QFK_F47 },
	{ "\033[24;6~",	QFK_F48 },

	{ "\033[2~",	QFK_INSERT,		0 },
	{ "\033[3~",	QFK_DELETE,		0 },
	{ "\033[H",		QFK_HOME,		0 },
	{ "\033[F",		QFK_END,		0 },
	{ "\033[5~",	QFK_PAGEUP,		0 },
	{ "\033[6~",	QFK_PAGEDOWN,	0 },
	{ "\033[A",		QFK_UP,			0 },
	{ "\033[B",		QFK_DOWN,		0 },
	{ "\033[C",		QFK_RIGHT,		0 },
	{ "\033[D",		QFK_LEFT,		0 },
	// shift
	// there may be a setting to tell xterm to NOT act on shift ins/pgup/pgdn
	{ "\033[2;2~",	QFK_INSERT,		1 },	// xterm gobbles (and pastes)
	{ "\033[3;2~",	QFK_DELETE,		1 },
	{ "\033[1;2H",	QFK_HOME,		1 },
	{ "\033[1;2F",	QFK_END,		1 },
	{ "\033[5;2~",	QFK_PAGEUP,		1 },	// xterm gobbles (scrolls term)
	{ "\033[6;2~",	QFK_PAGEDOWN,	1 },	// xterm gobbles (scrolls term)
	{ "\033[1;2A",	QFK_UP,			1 },
	{ "\033[1;2B",	QFK_DOWN,		1 },
	{ "\033[1;2C",	QFK_RIGHT,		1 },
	{ "\033[1;2D",	QFK_LEFT,		1 },
	{ "\033[Z",		QFK_TAB,		1 },
	// control
	{ "\033[2;5~",	QFK_INSERT,		4 },
	{ "\033[3;5~",	QFK_DELETE,		4 },
	{ "\033[1;5H",	QFK_HOME,		4 },
	{ "\033[1;5F",	QFK_END,		4 },
	{ "\033[5;5~",	QFK_PAGEUP,		4 },
	{ "\033[6;5~",	QFK_PAGEDOWN,	4 },
	{ "\033[1;5A",	QFK_UP,			4 },
	{ "\033[1;5B",	QFK_DOWN,		4 },
	{ "\033[1;5C",	QFK_RIGHT,		4 },
	{ "\033[1;5D",	QFK_LEFT,		4 },
	// shift control
	{ "\033[2;6~",	QFK_INSERT,		5 },
	{ "\033[3;6~",	QFK_DELETE,		5 },
	{ "\033[1;6H",	QFK_HOME,		5 },
	{ "\033[1;6F",	QFK_END,		5 },
	{ "\033[5;6~",	QFK_PAGEUP,		5 },
	{ "\033[6;6~",	QFK_PAGEDOWN,	5 },
	{ "\033[1;6A",	QFK_UP,			5 },
	{ "\033[1;6B",	QFK_DOWN,		5 },
	{ "\033[1;6C",	QFK_RIGHT,		5 },
	{ "\033[1;6D",	QFK_LEFT,		5 },
};

#ifdef HAVE_SIGACTION
static struct sigaction save_winch;
static sigset_t winch_mask;
static volatile sig_atomic_t winch_arrived;

static void
handle_winch (int sig)
{
	winch_arrived = 1;
}
#endif

int
qwaq_add_event (qwaq_resources_t *res, qwaq_event_t *event)
{
	struct timespec timeout;
	int         merged = 0;
	int         ret = 0;

	// merge motion events
	pthread_mutex_lock (&res->event_cond.mut);
	unsigned    last = RB_DATA_AVAILABLE (res->event_queue);
	if (event->what == qe_mousemove && last > 1
		&& RB_PEEK_DATA(res->event_queue, last - 1)->what == qe_mousemove) {
		RB_POKE_DATA(res->event_queue, last - 1, *event);
		merged = 1;
		pthread_cond_broadcast (&res->event_cond.rcond);
	}
	pthread_mutex_unlock (&res->event_cond.mut);
	if (merged) {
		return 0;
	}

	pthread_mutex_lock (&res->event_cond.mut);
	qwaq_init_timeout (&timeout, 5000 * (int64_t) 1000000);
	while (RB_SPACE_AVAILABLE (res->event_queue) < 1 && ret == 0) {
		ret = pthread_cond_timedwait (&res->event_cond.wcond,
									  &res->event_cond.mut, &timeout);
	}
	RB_WRITE_DATA (res->event_queue, event, 1);
	pthread_cond_broadcast (&res->event_cond.rcond);
	pthread_mutex_unlock (&res->event_cond.mut);
	return ret;
}

#ifdef HAVE_SIGACTION
static void
resize_event (qwaq_resources_t *res)
{
	qwaq_event_t event = {};
	struct winsize size;
	if (ioctl (fileno (stdout), TIOCGWINSZ, &size) != 0) {
		return;
	}
	event.what = qe_resize;
	event.resize.width = size.ws_col;
	event.resize.height = size.ws_row;
	qwaq_add_event (res, &event);
}
#endif

static void
key_event (qwaq_resources_t *res, int key, unsigned shift)
{
	qwaq_event_t event = {};
	event.what = qe_keydown;
	event.when = Sys_DoubleTime ();
	event.key.code = key;
	event.key.shift = shift;
	qwaq_add_event (res, &event);
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
	qwaq_add_event (res, &event);
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
	res->mouse_x = x;
	res->mouse_y = y;
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

static void
parse_key (qwaq_resources_t *res)
{
	qwaq_key_t *key = Hash_Find (res->key_sequences, res->escbuff.str);

	//Sys_Printf ("parse_key: %s\n", res->escbuff.str + 1);
	if (key) {
		//Sys_Printf ("    %d %03x %s\n", key->key, key->shift,
		//			Key_KeynumToString (key->key));
		key_event (res, key->key, key->shift);
	}
}

static void process_char (qwaq_resources_t *res, char ch)
{
	if (ch == 0x1b) {
		// always reset if escape is seen, may be a desync
		res->escstate = esc_escape;
	} else {
		switch (res->escstate) {
			case esc_ground:
				key_event (res, (byte) ch, 0);	// shift state unknown
				break;
			case esc_escape:
				if (ch == '[') {
					res->escstate = esc_csi;
				} else if (ch == 'O') {
					// will wind up accepting P;P... but meh
					res->escstate = esc_key;
					dstring_clear (&res->escbuff);
					// start the buffer with what got us hear: eases key lookup
					dstring_append (&res->escbuff, "\033O", 2);
				} else {
					res->escstate = esc_ground;
				}
				break;
			case esc_csi:
				if (ch == 'M') {
					res->escstate = esc_mouse;
					dstring_clear (&res->escbuff);
				} else if (ch == '<') {
					res->escstate = esc_sgr;
					dstring_clear (&res->escbuff);
				} else if (ch >= '0' && ch < 127) {
					res->escstate = esc_key;
					dstring_clear (&res->escbuff);
					// start the buffer with what got us hear: eases key lookup
					dstring_append (&res->escbuff, "\033[", 2);
					// the csi code might be short (eg, \e[H for home) so
					// need to check for end of string right away
					goto esc_key_jump;
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
				if (isdigit (ch) || ch == ';') {
					dstring_append (&res->escbuff, &ch, 1);
				} else {
					if (ch == 'm' || ch == 'M') {
						// terminate the string
						dstring_append (&res->escbuff, "", 1);
						parse_sgr (res, ch);
					}
					res->escstate = esc_ground;
				}
				break;
			case esc_key:
esc_key_jump:
				dstring_append (&res->escbuff, &ch, 1);
				if (!isdigit (ch) && ch != ';') {
					// terminate the string
					dstring_append (&res->escbuff, "", 1);
					// parse_key will sort out whether it was a valid sequence
					parse_key (res);
					res->escstate = esc_ground;
				}
				break;
		}
		//printf("res->escstate %d\n", res->escstate);
	}
}

static const char *
key_sequence_getkey (const void *_seq, void *unused)
{
	__auto_type seq = (const qwaq_key_t *) _seq;
	return seq->sequence;
}

void qwaq_input_init (qwaq_resources_t *res)
{
	if (res->key_sequences) {
		Hash_FlushTable (res->key_sequences);
	} else {
		res->key_sequences = Hash_NewTable (127, key_sequence_getkey, 0, 0,
											res->pr->hashlink_freelist);
	}
	for (size_t i = 0; i < sizeof (default_keys) / sizeof (default_keys[0]);
		 i++) {
		Hash_Add (res->key_sequences, &default_keys[i]);
	}

#ifdef HAVE_SIGACTION
	sigemptyset (&winch_mask);
	sigaddset (&winch_mask, SIGWINCH);
	struct sigaction action = {};
	action.sa_handler = handle_winch;
	sigaction (SIGWINCH, &action, &save_winch);
#endif

	// ncurses takes care of input mode for us, so need only tell xterm
	// what we need
	(void) !write(1, MOUSE_MOVES_ON, sizeof (MOUSE_MOVES_ON) - 1);
	(void) !write(1, SGR_ON, sizeof (SGR_ON) - 1);
}

void qwaq_input_shutdown (qwaq_resources_t *res)
{
	// ncurses takes care of input mode for us, so need only tell xterm
	// what we need
	(void) !write(1, SGR_OFF, sizeof (SGR_OFF) - 1);
	(void) !write(1, MOUSE_MOVES_OFF, sizeof (MOUSE_MOVES_OFF) - 1);

#ifdef HAVE_SIGACTION
	sigaction (SIGWINCH, &save_winch, 0);
#endif
}

void qwaq_process_input (qwaq_resources_t *res)
{
	char        buf[256];
	int         len;
#ifdef HAVE_SIGACTION
	sigset_t    save_set;
	int         saw_winch;

	pthread_sigmask (SIG_BLOCK, &winch_mask, &save_set);
	saw_winch = winch_arrived;
	winch_arrived = 0;
	pthread_sigmask (SIG_SETMASK, &save_set, 0);
	if (saw_winch) {
		resize_event (res);
	}
#endif
	while (Sys_CheckInput (1, -1)) {
		len = read(0, buf, sizeof (buf));
		for (int i = 0; i < len; i++) {
			process_char (res, buf[i]);
		}
	}
}
