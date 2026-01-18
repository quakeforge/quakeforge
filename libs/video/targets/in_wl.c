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

#define SIZE(x) (sizeof (x) / sizeof (x[0]))

// TODO: Reduce global state by having a wayland input state struct

static int wl_driver_handle = -1;
static struct zwp_locked_pointer_v1 *zwp_locked_pointer_v1;
static uint32_t wl_last_pointer_serial;
static struct xkb_context *xkb_context;
static struct xkb_keymap *xkb_keymap;
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

// linux/input-event-codes.h defines 8 named buttons
static constexpr size_t WL_MAX_MOUSE_BUTTONS = 8;
static in_buttoninfo_t wl_mouse_buttons[WL_MAX_MOUSE_BUTTONS];
static IE_mouse_event_t wl_mouse;

static wl_idevice_t wl_mouse_device = {
	"core:mouse",
	SIZE (wl_mouse_axes), SIZE (wl_mouse_buttons),
	wl_mouse_axes, wl_mouse_buttons
};

// Wayland doesn't impose a maxium number of keys, so just limit
// ourselves to Linux's max evdev codes
static constexpr size_t WL_MAX_KEY_BUTTONS = KEY_MAX;
static in_buttoninfo_t wl_keyboard_buttons[WL_MAX_KEY_BUTTONS];
static IE_key_event_t wl_key_event;
static wl_idevice_t wl_keyboard_device = {
	"core:keyboard",
	0, SIZE (wl_keyboard_buttons),
	nullptr, wl_keyboard_buttons
};

static dstring_t *wl_utf8;

static void
wl_seat_name (void *data, struct wl_seat *seat, const char *name)
{
	Sys_MaskPrintf (SYS_wayland, "Wayland: Got seat '%s'\n", name);
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
	wl_mouse.x = wl_fixed_to_int (surface_x);
	wl_mouse.y = wl_fixed_to_int (surface_y);
}

static int32_t btn = -1;

static void
wl_pointer_button (void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t serial,
		       uint32_t time,
		       uint32_t button,
		       uint32_t state)
{
	auto bid = button - BTN_MOUSE;
	auto pressed = state == WL_POINTER_BUTTON_STATE_PRESSED;
	wl_mouse_buttons[bid].state = pressed;

	if (pressed) {
		wl_mouse.buttons |= 1 << bid;
		wl_mouse.type = ie_mousedown;
	} else {
		wl_mouse.buttons &= ~(1 << bid);
		wl_mouse.type = ie_mouseup;
	}

	btn = bid;
}

static void
wl_pointer_axis (void *data,
		     struct wl_pointer *wl_pointer,
		     uint32_t time,
		     uint32_t axis,
		     wl_fixed_t value)
{
}

static void
in_wl_send_button_event (in_buttoninfo_t *btn)
{
	IE_event_t event = {
		.type = ie_button,
		.when = Sys_LongTime (),
		.button = {
			.data = wl_mouse_device.event_data,
			.devid = wl_mouse_device.devid,
			.button = btn->button,
			.state = btn->state,
		},
	};
	IE_Send_Event (&event);
}

static void
wl_pointer_frame (void *data,
		      struct wl_pointer *wl_pointer)
{
	IE_event_t event = {
		.type = ie_mouse,
		.when = Sys_LongTime (),
		.mouse = wl_mouse
	};

	if (!IE_Send_Event (&event)) {
		in_wl_send_button_event (&wl_mouse_buttons[btn]);
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
wl_pointer_axis_discrete (void *data,
			      struct wl_pointer *wl_pointer,
			      uint32_t axis,
			      int32_t discrete)
{
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
	.axis_discrete = wl_pointer_axis_discrete,
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
in_wl_keysym_to_keycode (xkb_keysym_t keysym, int32_t *keycode)
{
	switch (keysym) {
		default:
			*keycode = xkb_keysym_to_lower (keysym);
			break;
	}
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
	auto utf8len = xkb_state_key_get_utf8 (xkb_state,
										   scancode, nullptr, 0);
	if (utf8len == -1) {
		Sys_Error ("xkb_state_key_get_utf8 passed an invalid keysym: %u", sym);
	}
	wl_utf8->size = utf8len + 1;
	dstring_adjust (wl_utf8);
	utf8len = xkb_state_key_get_utf8 (xkb_state, scancode,
									  wl_utf8->str, wl_utf8->size);
	if (utf8len == -1) {
		Sys_Error ("xkb_state_key_get_utf8 passed an invalid keysym: %u", sym);
	}

	if (utf8len == 1) {
		wl_key_event.unicode = (byte)wl_utf8->str[0];
	}

	in_wl_keysym_to_keycode (sym, &wl_key_event.code);

	auto pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED ||
				   state == WL_KEYBOARD_KEY_STATE_REPEATED;

	// TODO: For wl_seat >= 10 we need to check for REPEAT state as well
	wl_keyboard_buttons[wl_key_event.code].state = pressed;

	if (!(pressed && in_wl_send_key_event ())) {
		in_wl_send_button_event(&wl_keyboard_buttons[key]);
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
		wl_utf8 = dstring_new ();
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

static int
in_wl_get_button_info (void *data, void *device, int button_num,
						in_buttoninfo_t *info)
{
	wl_idevice_t *dev = device;
	if (button_num < 0 || button_num > dev->num_buttons) {
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
				wl_surf, wl_pointer, nullptr,
				ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
		wl_pointer_set_cursor (wl_pointer,
				wl_last_pointer_serial, nullptr, 0, 0);
	} else if (!grab && zwp_locked_pointer_v1) {
		wp_cursor_shape_device_v1_set_shape (
				wp_cursor_shape_device_v1,
				wl_last_pointer_serial,
				WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
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
	wl_relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer (
			wl_relative_pointer_manager, wl_pointer);
	zwp_relative_pointer_v1_add_listener (wl_relative_pointer,
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

	.get_button_info = in_wl_get_button_info
};

static void __attribute__((constructor))
in_wl_register_driver (void)
{
	wl_driver_handle = IN_RegisterDriver (&in_wl_driver, nullptr);
}

int wl_force_link;

