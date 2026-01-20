/*
	in_wl.c

	general wl input driver

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>
	Copyright (C) 2026 Peter Nilsson <peter8nilsson@live.se>
	Please see the file "AUTHORS" for a list of contributors

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define _BSD
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/input/event.h"

#include "compat.h"
#include "context_wl.h"
#include "in_wl.h"
#include "qfselect.h"
#include "vid_internal.h"

#include "libs/video/targets/relative-pointer-client-protocol.hinc"
#include "libs/video/targets/pointer-constraints-client-protocol.hinc"
#include "libs/video/targets/cursor-shape-client-protocol.hinc"

// TODO: Reduce global state by having a wayland input state struct

static int wl_driver_handle = -1;
static struct zwp_locked_pointer_v1 *zwp_locked_pointer_v1;
uint32_t wl_last_pointer_serial;
static struct xkb_context *xkb_context;
static struct xkb_keymap *xkb_keymap;
static xkb_mod_index_t xkb_mod_shift_index;
static xkb_mod_index_t xkb_mod_control_index;
static xkb_mod_index_t xkb_mod_alt_index;
static struct xkb_state *xkb_state;

typedef struct wl_idevice_s {
	const char *name;
	int32_t num_axes;
	int32_t num_buttons;
	in_axisinfo_t *axes;
	in_buttoninfo_t *buttons;
	void *event_data;
	int32_t devid;
} wl_idevice_t;

// Mouse wheel isn't an axis?
static constexpr size_t WL_MAX_MOUSE_AXES = 2;
static in_axisinfo_t wl_mouse_axes[WL_MAX_MOUSE_AXES];
//static const char *wl_mouse_axis_names[] = {"M_X", "M_Y"};

// linux/input-event-codes.h defines 8 named buttons (+2 for mouse wheel, blame X11)
static constexpr size_t WL_MAX_MOUSE_BUTTONS = 8 + 2;
static in_buttoninfo_t wl_mouse_buttons[WL_MAX_MOUSE_BUTTONS];
static int32_t wl_mouse_accumulated_scroll[2] = { 0 };
static const char *wl_mouse_button_names[] = {
	"M_BUTTON1",    "M_BUTTON2",  "M_BUTTON3",  "M_WHEEL_UP",
	"M_WHEEL_DOWN", "M_BUTTON6",  "M_BUTTON7",  "M_BUTTON8",
	"M_BUTTON9",
};
static IE_mouse_event_t wl_mouse;

static wl_idevice_t wl_mouse_device = {
	"core:mouse",
	countof (wl_mouse_axes), countof (wl_mouse_buttons),
	wl_mouse_axes, wl_mouse_buttons
};

// Wayland doesn't impose a maxium number of keys, so just limit
// ourselves to Linux's max evdev codes
static constexpr size_t WL_MAX_KEY_BUTTONS = KEY_MAX;
static in_buttoninfo_t wl_keyboard_buttons[WL_MAX_KEY_BUTTONS];
static IE_key_event_t wl_key_event;

static wl_idevice_t wl_keyboard_device = {
	"core:keyboard",
	0, countof (wl_keyboard_buttons),
	nullptr, wl_keyboard_buttons
};

static dstring_t *wl_utf8;

static void
wl_seat_name (void *data, struct wl_seat *seat, const char *name)
{
	Sys_MaskPrintf (SYS_wayland, "Wayland: Got seat '%s'\n", name);
}

/*
	Adjusts the given mouse button idx
	to compensate for mouse wheel up / down
	being considered a button. This is behavior
	imposed by X11, and unfortunately we have
	to compensate for that at the moment.
*/
static int32_t
in_wl_adjust_mouse_button_idx (int32_t button)
{
	return button <= 2 ? button : button + 2;
}

static void
wl_pointer_enter (void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface,
		      wl_fixed_t surface_x,
		      wl_fixed_t surface_y)
{
	wl_last_pointer_serial = serial;
}

static void
wl_pointer_leave (void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface)
{
}

static void
wl_pointer_motion (void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t time,
		       wl_fixed_t surface_x,
		       wl_fixed_t surface_y)
{
	wl_mouse.type = ie_mousemove;
	wl_mouse.x = wl_fixed_to_int (surface_x);
	wl_mouse.y = wl_fixed_to_int (surface_y);
}

static int32_t wl_last_button_idx = -1;

static void
wl_pointer_button (void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t serial,
		       uint32_t time,
		       uint32_t button,
		       uint32_t state)
{
	auto btn = button - BTN_MOUSE;
	btn = in_wl_adjust_mouse_button_idx (btn);
	auto pressed = state == WL_POINTER_BUTTON_STATE_PRESSED;
	wl_mouse_buttons[btn].state = pressed;

	if (pressed) {
		wl_mouse.buttons |= 1 << btn;
		wl_mouse.type = ie_mousedown;
	} else {
		wl_mouse.buttons &= ~(1 << btn);
		wl_mouse.type = ie_mouseup;
	}

	wl_last_button_idx = btn;
}

static void
wl_pointer_axis (void *data,
		     struct wl_pointer *wl_pointer,
		     uint32_t time,
		     uint32_t axis,
		     wl_fixed_t value)
{
	//Sys_MaskPrintf (SYS_wayland, "wl_pointer_axis\n");
}

static bool
in_wl_send_mouse_event (void)
{
	IE_event_t event = {
		.type = ie_mouse,
		.when = Sys_LongTime (),
		.mouse = wl_mouse
	};
	return IE_Send_Event (&event);
}

static void
in_wl_send_button_event (wl_idevice_t *dev, int32_t btn)
{
	auto btn_info = dev->buttons[btn];
	IE_event_t event = {
		.type = ie_button,
		.when = Sys_LongTime (),
		.button = {
			.data   = dev->event_data,
			.devid  = dev->devid,
			.button = btn_info.button,
			.state  = btn_info.state
		}
	};
	IE_Send_Event (&event);
}

static void
in_wl_send_scroll_button_event (IE_mouse_type type, int32_t btn)
{
	wl_mouse.buttons |= 1 << btn;
	wl_mouse_buttons[btn].state = type == ie_mousedown;
	wl_mouse.type = type;
	if (!in_wl_send_mouse_event ()) {
		in_wl_send_button_event (&wl_mouse_device, btn);
	}
}

static void
wl_pointer_frame (void *data,
		      struct wl_pointer *wl_pointer)
{
	if (!in_wl_send_mouse_event () && wl_last_button_idx != -1) {
		in_wl_send_button_event (&wl_mouse_device, wl_last_button_idx);
	}
	wl_last_button_idx = -1;

	for (size_t i = 0; i < countof (wl_mouse_accumulated_scroll); ++i) {
		auto value = wl_mouse_accumulated_scroll[i] / 120;
		if (value == 0) {
			continue;
		}

		auto btn = value > 0 ? 4 : 3;
		for (int32_t j = 0; j < abs (value); ++j) {
			in_wl_send_scroll_button_event(ie_mousedown, btn);
			in_wl_send_scroll_button_event(ie_mouseup, btn);
		}

		wl_mouse_accumulated_scroll[i] -= value * 120;
	}
}

static void
wl_pointer_axis_source (void *data,
			    struct wl_pointer *wl_pointer,
			    uint32_t axis_source)
{
}

static void
wl_pointer_axis_stop (void *data,
			  struct wl_pointer *wl_pointer,
			  uint32_t time,
			  uint32_t axis)
{
}

static void
wl_pointer_axis_value120 (void *data,
			      struct wl_pointer *wl_pointer,
			      uint32_t axis,
			      int32_t value120)
{
	wl_mouse_accumulated_scroll[axis] += value120;
}

static void
wl_pointer_axis_relative_direction (void *data,
					struct wl_pointer *wl_pointer,
					uint32_t axis,
					uint32_t direction)
{
	// TODO: Account for inverted direction
}

static const struct wl_pointer_listener wl_pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
	.frame = wl_pointer_frame,
	.axis_source = wl_pointer_axis_source,
	.axis_stop = wl_pointer_axis_stop,
	.axis_value120 = wl_pointer_axis_value120,
	.axis_relative_direction = wl_pointer_axis_relative_direction
};

static void
send_axis_event (int32_t axis, int32_t value)
{
	wl_mouse_axes[axis].value = value;

	IE_event_t event = {
		.type = ie_axis,
		.when = Sys_LongTime (),
		.axis = {
			.data = wl_mouse_device.event_data,
			.devid = wl_mouse_device.devid,
			.axis = axis,
			.value = value
		}
	};
	IE_Send_Event (&event);
}

static void
wl_relative_pointer_motion (void *data,
							struct zwp_relative_pointer_v1 *zwp_relative_pointer_v1,
							uint32_t utime_hi, uint32_t utime_lo,
							wl_fixed_t dx, wl_fixed_t dy,
							wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel)
{
	// TODO: CVar for using unaccelerated vs accelerated
	send_axis_event (0, wl_fixed_to_int (dx_unaccel));
	send_axis_event (1, wl_fixed_to_int (dy_unaccel));
}

static const struct zwp_relative_pointer_v1_listener wl_relative_pointer_listener = {
	.relative_motion = wl_relative_pointer_motion
};

static int32_t
in_wl_send_key_event (void)
{
	IE_event_t event = {
		.type = ie_key,
		.when = Sys_LongTime (),
		.key = wl_key_event
	};
	return IE_Send_Event (&event);
}

static void
in_wl_keyboard_keymap (void *data,
		   struct wl_keyboard *keyboard,
		   uint32_t format,
		   int32_t fd,
		   uint32_t size)
{
	if (format != XKB_KEYMAP_FORMAT_TEXT_V1) {
		Sys_Error ("in_wl_keyboard_keymap: Only supports XKB version 1 keymaps");
	}

	char *shm = mmap (nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (shm == MAP_FAILED) {
		Sys_Error ("in_wl_keyboard_keymap: Failed to map keymap file");
	}
	auto keymap = xkb_keymap_new_from_string (xkb_context,
			shm,
			XKB_KEYMAP_FORMAT_TEXT_V1,
			XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap (shm, size);
	close (fd);
	auto state = xkb_state_new (keymap);
	xkb_keymap_unref (xkb_keymap);
	xkb_state_unref (xkb_state);
	xkb_keymap = keymap;
	xkb_state = state;
	xkb_mod_shift_index = xkb_map_mod_get_index (xkb_keymap, XKB_MOD_NAME_SHIFT);
	xkb_mod_control_index = xkb_map_mod_get_index (xkb_keymap, XKB_MOD_NAME_CTRL);
	xkb_mod_alt_index = xkb_map_mod_get_index (xkb_keymap, XKB_MOD_NAME_ALT);
}

static void
in_wl_keyboard_enter (void *data,
		  struct wl_keyboard *keyboard,
		  uint32_t serial,
		  struct wl_surface *surface,
		  struct wl_array *keys)
{
}

static void
in_wl_keyboard_leave (void *data,
		  struct wl_keyboard *keyboard,
		  uint32_t serial,
		  struct wl_surface *surface)
{
}

static void
in_wl_process_key (uint32_t scancode, xkb_keysym_t keysym, int32_t *keycode, int32_t *unicode)
{
	int32_t key;

	*unicode = 0;

	auto utf8len = xkb_state_key_get_utf8 (xkb_state,
										   scancode, nullptr, 0);
	if (utf8len == -1) {
		Sys_Error ("in_wl_process_key: passed an invalid keysym: %u", keysym);
	} else if (utf8len > 0) {
		wl_utf8->size = utf8len + 1;
		dstring_adjust (wl_utf8);
		utf8len = xkb_state_key_get_utf8 (xkb_state, scancode,
										  wl_utf8->str, wl_utf8->size);

		if (utf8len == 1) {
			// FIXME "unicode" in IE_key_event_t is named incorrectly as it doesn't
			//		 *actually* represent a unicode code point
			*unicode = (byte)wl_utf8->str[0];
		}
	}

	switch (keysym) {
		case XKB_KEY_KP_Page_Up:
			key = QFK_KP9;
			break;
		case XKB_KEY_Page_Up:
			key = QFK_PAGEUP;
			break;

		case XKB_KEY_KP_Page_Down:
			key = QFK_KP3;
			break;
		case XKB_KEY_Page_Down:
			key = QFK_PAGEDOWN;
			break;

		case XKB_KEY_KP_Home:
			key = QFK_KP7;
			break;
		case XKB_KEY_Home:
			key = QFK_HOME;
			break;

		case XKB_KEY_KP_End:
			key = QFK_KP1;
			break;
		case XKB_KEY_End:
			key = QFK_END;
			break;

		case XKB_KEY_KP_Left:
			key = QFK_KP4;
			break;
		case XKB_KEY_Left:
			key = QFK_LEFT;
			break;

		case XKB_KEY_KP_Right:
			key = QFK_KP6;
			break;
		case XKB_KEY_Right:
			key = QFK_RIGHT;
			break;

		case XKB_KEY_KP_Down:
			key = QFK_KP2;
			break;
		case XKB_KEY_Down:
			key = QFK_DOWN;
			break;

		case XKB_KEY_KP_Up:
			key = QFK_KP8;
			break;
		case XKB_KEY_Up:
			key = QFK_UP;
			break;

		case XKB_KEY_Escape:
			key = QFK_ESCAPE;
			break;

		case XKB_KEY_KP_Enter:
			key = QFK_KP_ENTER;
			break;
		case XKB_KEY_Return:
			key = QFK_RETURN;
			break;

		case XKB_KEY_Tab:
			key = QFK_TAB;
			break;

		case XKB_KEY_F1:
			key = QFK_F1;
			break;
		case XKB_KEY_F2:
			key = QFK_F2;
			break;
		case XKB_KEY_F3:
			key = QFK_F3;
			break;
		case XKB_KEY_F4:
			key = QFK_F4;
			break;
		case XKB_KEY_F5:
			key = QFK_F5;
			break;
		case XKB_KEY_F6:
			key = QFK_F6;
			break;
		case XKB_KEY_F7:
			key = QFK_F7;
			break;
		case XKB_KEY_F8:
			key = QFK_F8;
			break;
		case XKB_KEY_F9:
			key = QFK_F9;
			break;
		case XKB_KEY_F10:
			key = QFK_F10;
			break;
		case XKB_KEY_F11:
			key = QFK_F11;
			break;
		case XKB_KEY_F12:
			key = QFK_F12;
			break;

		case XKB_KEY_BackSpace:
			key = QFK_BACKSPACE;
			break;

		case XKB_KEY_KP_Delete:
			key = QFK_KP_PERIOD;
			break;
		case XKB_KEY_Delete:
			key = QFK_DELETE;
			break;

		case XKB_KEY_Pause:
			key = QFK_PAUSE;
			break;

		case XKB_KEY_Shift_L:
			key = QFK_LSHIFT;
			break;
		case XKB_KEY_Shift_R:
			key = QFK_RSHIFT;
			break;

		case XKB_KEY_Execute:
		case XKB_KEY_Control_L:
			key = QFK_LCTRL;
			break;
		case XKB_KEY_Control_R:
			key = QFK_RCTRL;
			break;

		case XKB_KEY_Mode_switch:
		case XKB_KEY_Alt_L:
			key = QFK_LALT;
			break;
		case XKB_KEY_Meta_L:
			key = QFK_LMETA;
			break;
		case XKB_KEY_Alt_R:
			key = QFK_RALT;
			break;
		case XKB_KEY_Meta_R:
			key = QFK_RMETA;
			break;
		case XKB_KEY_Super_L:
			key = QFK_LSUPER;
			break;
		case XKB_KEY_Super_R:
			key = QFK_RSUPER;
			break;

		case XKB_KEY_Multi_key:
			key = QFK_COMPOSE;
			break;

		case XKB_KEY_Menu:
			key = QFK_MENU;
			break;

		case XKB_KEY_Caps_Lock:
			key = QFK_CAPSLOCK;
			break;

		case XKB_KEY_KP_Begin:
			key = QFK_KP5;
			break;

		case XKB_KEY_Print:
			key = QFK_PRINT;
			break;

		case XKB_KEY_Scroll_Lock:
			key = QFK_SCROLLOCK;
			break;

		case XKB_KEY_Num_Lock:
			key = QFK_NUMLOCK;
			break;

		case XKB_KEY_Insert:
			key = QFK_INSERT;
			break;
		case XKB_KEY_KP_Insert:
			key = QFK_KP0;
			break;

		case XKB_KEY_KP_Multiply:
			key = QFK_KP_MULTIPLY;
			break;
		case XKB_KEY_KP_Add:
			key = QFK_KP_PLUS;
			break;
		case XKB_KEY_KP_Subtract:
			key = QFK_KP_MINUS;
			break;
		case XKB_KEY_KP_Divide:
			key = QFK_KP_DIVIDE;
			break;

		// For Sun keyboards
		case XKB_KEY_F27:
			key = QFK_HOME;
			break;
		case XKB_KEY_F29:
			key = QFK_PAGEUP;
			break;
		case XKB_KEY_F33:
			key = QFK_END;
			break;
		case XKB_KEY_F35:
			key = QFK_PAGEDOWN;
			break;

		// Some high ASCII symbols, for azerty keymaps
		case XKB_KEY_twosuperior:
			key = QFK_WORLD_18;
			break;
		case XKB_KEY_eacute:
			key = QFK_WORLD_63;
			break;
		case XKB_KEY_section:
			key = QFK_WORLD_7;
			break;
		case XKB_KEY_egrave:
			key = QFK_WORLD_72;
			break;
		case XKB_KEY_ccedilla:
			key = QFK_WORLD_71;
			break;
		case XKB_KEY_agrave:
			key = QFK_WORLD_64;
			break;

		case XKB_KEY_Kanji:
			key = QFK_KANJI;
			break;
		case XKB_KEY_Muhenkan:
			key = QFK_MUHENKAN;
			break;
		case XKB_KEY_Henkan:
			key = QFK_HENKAN;
			break;
		case XKB_KEY_Romaji:
			key = QFK_ROMAJI;
			break;
		case XKB_KEY_Hiragana:
			key = QFK_HIRAGANA;
			break;
		case XKB_KEY_Katakana:
			key = QFK_KATAKANA;
			break;
		case XKB_KEY_Hiragana_Katakana:
			key = QFK_HIRAGANA_KATAKANA;
			break;
		case XKB_KEY_Zenkaku:
			key = QFK_ZENKAKU;
			break;
		case XKB_KEY_Hankaku:
			key = QFK_HANKAKU;
			break;
		case XKB_KEY_Zenkaku_Hankaku:
			key = QFK_ZENKAKU_HANKAKU;
			break;
		case XKB_KEY_Touroku:
			key = QFK_TOUROKU;
			break;
		case XKB_KEY_Massyo:
			key = QFK_MASSYO;
			break;
		case XKB_KEY_Kana_Lock:
			key = QFK_KANA_LOCK;
			break;
		case XKB_KEY_Kana_Shift:
			key = QFK_KANA_SHIFT;
			break;
		case XKB_KEY_Eisu_Shift:
			key = QFK_EISU_SHIFT;
			break;
		case XKB_KEY_Eisu_toggle:
			key = QFK_EISU_TOGGLE;
			break;
		case XKB_KEY_Kanji_Bangou:
			key = QFK_KANJI_BANGOU;
			break;
		case XKB_KEY_Zen_Koho:
			key = QFK_ZEN_KOHO;
			break;
		case XKB_KEY_Mae_Koho:
			key = QFK_MAE_KOHO;
			break;
		case XKB_KEY_XF86HomePage:
			key = QFK_HOMEPAGE;
			break;
		case XKB_KEY_XF86Search:
			key = QFK_SEARCH;
			break;
		case XKB_KEY_XF86Mail:
			key = QFK_MAIL;
			break;
		case XKB_KEY_XF86Favorites:
			key = QFK_FAVORITES;
			break;
		case XKB_KEY_XF86AudioMute:
			key = QFK_AUDIOMUTE;
			break;
		case XKB_KEY_XF86AudioLowerVolume:
			key = QFK_AUDIOLOWERVOLUME;
			break;
		case XKB_KEY_XF86AudioRaiseVolume:
			key = QFK_AUDIORAISEVOLUME;
			break;
		case XKB_KEY_XF86AudioPlay:
			key = QFK_AUDIOPLAY;
			break;
		case XKB_KEY_XF86Calculator:
			key = QFK_CALCULATOR;
			break;
		case XKB_KEY_Help:
			key = QFK_HELP;
			break;
		case XKB_KEY_Undo:
			key = QFK_UNDO;
			break;
		case XKB_KEY_Redo:
			key = QFK_REDO;
			break;
		case XKB_KEY_XF86New:
			key = QFK_NEW;
			break;
		case XKB_KEY_XF86Reload:	// eh? it's open (hiraku) on my kb
			key = QFK_RELOAD;
			break;
		case XKB_KEY_SunOpen:
			//FALL THROUGH
		case XKB_KEY_XF86Open:
			key = QFK_OPEN;
			break;
		case XKB_KEY_XF86Close:
			key = QFK_CLOSE;
			break;
		case XKB_KEY_XF86Reply:
			key = QFK_REPLY;
			break;
		case XKB_KEY_XF86MailForward:
			key = QFK_MAILFORWARD;
			break;
		case XKB_KEY_XF86Send:
			key = QFK_SEND;
			break;
		case XKB_KEY_XF86Save:
			key = QFK_SAVE;
			break;
		case XKB_KEY_KP_Equal:
			key = QFK_KP_EQUALS;
			break;
		case XKB_KEY_parenleft:
			key = QFK_LEFTPAREN;
			break;
		case XKB_KEY_parenright:
			key = QFK_RIGHTPAREN;
			break;
		case XKB_KEY_XF86Back:
			key = QFK_BACK;
			break;
		case XKB_KEY_XF86Forward:
			key = QFK_FORWARD;
			break;

		default:
			key = xkb_keysym_to_lower (keysym);
			break;
	}

	*keycode = key;
}

static bool
in_wl_mod_is_active (xkb_mod_index_t mod)
{
	return xkb_state_mod_index_is_active (xkb_state,
										  xkb_mod_control_index,
										  XKB_STATE_MODS_EFFECTIVE);
}

static void
in_wl_keyboard_key (void *data,
		struct wl_keyboard *keyboard,
		uint32_t serial,
		uint32_t time,
		uint32_t key,
		uint32_t state)
{
	auto scancode = key + 8;
	auto sym = xkb_state_key_get_one_sym (xkb_state, scancode);

	if (sym == XKB_KEY_NoSymbol) {
		Sys_Error ("in_wl_keyboard_key: Failed to get keysym for key %d, scancode %d\n",
					key, scancode);
	}

	auto pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED ||
				   state == WL_KEYBOARD_KEY_STATE_REPEATED;

	wl_keyboard_buttons[key].state = pressed;

	in_wl_process_key (scancode, sym,
					   &wl_key_event.code, &wl_key_event.unicode);

	// FIXME: Certain valid keys that need special handling
	// 		  triggers this error
	if ((size_t)wl_key_event.code >= WL_MAX_KEY_BUTTONS) {
		Sys_MaskPrintf (SYS_wayland,
						"Key %u (sc %u) not supported! %s\n",
						key, scancode, wl_utf8->str);
		return;
	}

	if (in_wl_mod_is_active(xkb_mod_shift_index)) {
		wl_key_event.shift |= ies_shift;
	}

	if (in_wl_mod_is_active(xkb_mod_control_index)) {
		wl_key_event.shift |= ies_control;
	}

	if (in_wl_mod_is_active(xkb_mod_alt_index)) {
		wl_key_event.shift |= ies_alt;
	}

	if (!(pressed && in_wl_send_key_event ())) {
		in_wl_send_button_event (&wl_keyboard_device, key);
	}
}

static void
in_wl_keyboard_modifiers (void *data,
		  struct wl_keyboard *keyboard,
		  uint32_t serial,
		  uint32_t mods_depressed,
		  uint32_t mods_latched,
		  uint32_t mods_locked,
		  uint32_t group)
{
	xkb_state_update_mask (xkb_state,
						   mods_depressed, mods_latched,
						   mods_locked, 0, 0, group);
}

static void
in_wl_keyboard_repeat_info (void *data,
			struct wl_keyboard *keyboard,
			int32_t rate,
			int32_t delay)
{
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
	.keymap = in_wl_keyboard_keymap,
	.enter = in_wl_keyboard_enter,
	.leave = in_wl_keyboard_leave,
	.key = in_wl_keyboard_key,
	.modifiers = in_wl_keyboard_modifiers,
	.repeat_info = in_wl_keyboard_repeat_info
};

static void
wl_seat_capabilities (void *data, struct wl_seat *seat, uint32_t caps)
{
	if ((caps & WL_SEAT_CAPABILITY_POINTER) > 0) {
		wl_pointer = wl_seat_get_pointer (seat);
		wl_pointer_add_listener (wl_pointer, &wl_pointer_listener, nullptr);
	}
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) > 0) {
		wl_keyboard = wl_seat_get_keyboard (seat);
		xkb_context = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
		wl_utf8 = dstring_newstr ();
		wl_key_event.utf8 = wl_utf8;
		wl_keyboard_add_listener (wl_keyboard, &wl_keyboard_listener, nullptr);
	}
}

static const struct wl_seat_listener wl_seat_listener = {
	.name = wl_seat_name,
	.capabilities = wl_seat_capabilities,
};

void
IN_WL_RegisterSeat (void)
{
	wl_seat_add_listener (wl_seat, &wl_seat_listener, nullptr);
}

static void
in_wl_process_events (void *data)
{
	WL_ProcessEvents ();
}

static void
wl_set_device_event_data (void *device, void *event_data, void *data)
{
	wl_idevice_t *dev = device;
	dev->event_data = event_data;
}

static void *
wl_get_device_event_data (void *device, void *data)
{
	wl_idevice_t *dev = device;
	return dev->event_data;
}

static void
in_wl_button_info (void *data, void *device, in_buttoninfo_t *buttons, int *numbuttons)
{
	wl_idevice_t *dev = device;
	if (!buttons) {
		*numbuttons = dev->num_buttons;
		return;
	}
	if (*numbuttons > dev->num_buttons) {
		*numbuttons = dev->num_buttons;
	}
	if (dev->num_buttons) {
		memcpy (buttons, dev->buttons, *numbuttons * sizeof (in_buttoninfo_t));
	}
}

static void
in_wl_axis_info (void *data, void *device, in_axisinfo_t *axes, int *numaxes)
{
	wl_idevice_t *dev = device;
	if (!axes) {
		*numaxes = dev->num_axes;
		return;
	}
	if (*numaxes > dev->num_axes) {
		*numaxes = dev->num_axes;
	}
	if (dev->num_axes) {
		memcpy (axes, dev->axes, *numaxes * sizeof (in_axisinfo_t));
	}
}

static const char *
in_wl_get_button_name (void *data, void *device, int button_num)
{
	wl_idevice_t *dev = device;
	const char *name = nullptr;
	if (button_num < 0 || button_num >= dev->num_buttons) {
		return name;
	}

	if (dev == &wl_keyboard_device) {
		auto keysym = xkb_state_key_get_one_sym (xkb_state, button_num + 8);
		wl_utf8->size = xkb_keysym_get_name (keysym, nullptr, 0) + 1;
		xkb_keysym_get_name (keysym, wl_utf8->str, wl_utf8->size);
		name = wl_utf8->str;
	} else if (dev == &wl_mouse_device) {
		name = wl_mouse_button_names[button_num];
	}

	return name;
}

static int
in_wl_get_button_info (void *data, void *device, int button_num,
					   in_buttoninfo_t *info)
{
	wl_idevice_t *dev = device;
	if (button_num < 0 || button_num >= dev->num_buttons) {
		return 0;
	}
	*info = dev->buttons[button_num];
	return 1;
}

static void
in_wl_grab_input (void *data, int grab)
{
	if (!zwp_pointer_constraints_v1) {
		// TODO: Queue up this event for when we get the global registered
		return;
	}

	if (grab && !zwp_locked_pointer_v1) {
		zwp_locked_pointer_v1 = zwp_pointer_constraints_v1_lock_pointer (
				zwp_pointer_constraints_v1,
				wl_surface, wl_pointer, nullptr,
				ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
	} else if (!grab && zwp_locked_pointer_v1) {
		zwp_locked_pointer_v1_destroy (zwp_locked_pointer_v1);
		zwp_locked_pointer_v1 = nullptr;
	}
}

static void
wl_add_device (wl_idevice_t *dev)
{
	for (int32_t i = 0; i < dev->num_axes; ++i) {
		dev->axes[i].axis = i;
	}

	for (int32_t i = 0; i < dev->num_buttons; ++i) {
		dev->buttons[i].button = i;
	}

	dev->devid = IN_AddDevice (wl_driver_handle, dev, dev->name, dev->name);
}

static void
in_wl_init (void *data)
{
	zwp_relative_pointer_v1 =
		zwp_relative_pointer_manager_v1_get_relative_pointer (
			zwp_relative_pointer_manager_v1, wl_pointer);
	zwp_relative_pointer_v1_add_listener (zwp_relative_pointer_v1,
			&wl_relative_pointer_listener, nullptr);

	wp_cursor_shape_device_v1 = wp_cursor_shape_manager_v1_get_pointer (
			wp_cursor_shape_manager_v1, wl_pointer);

	wl_add_device (&wl_mouse_device);
	wl_add_device (&wl_keyboard_device);
}

static in_driver_t in_wl_driver = {
	.init           = in_wl_init,
	.process_events = in_wl_process_events,

	.set_device_event_data = wl_set_device_event_data,
	.get_device_event_data = wl_get_device_event_data,

	.grab_input = in_wl_grab_input,

	.button_info = in_wl_button_info,
	.axis_info = in_wl_axis_info,

	.get_button_name = in_wl_get_button_name,

	.get_button_info = in_wl_get_button_info,
};

static void __attribute__((constructor))
in_wl_register_driver (void)
{
	wl_driver_handle = IN_RegisterDriver (&in_wl_driver, nullptr);
}

int wl_force_link;

