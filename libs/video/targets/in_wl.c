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

#define SIZE(x) (sizeof (x) / sizeof (x[0]))

static int wl_driver_handle = -1;
static struct wl_pointer  *wl_pointer;
static struct wl_keyboard *wl_keyboard;

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
//static IE_mouse_event_t wl_mouse;

static wl_idevice_t wl_mouse_device = {
	"core:mouse",
	SIZE (wl_mouse_axes), SIZE (wl_mouse_buttons),
	wl_mouse_axes, wl_mouse_buttons
};

static void
wl_seat_name (void *data, struct wl_seat *seat, const char *name)
{
	Sys_MaskPrintf(SYS_wayland, "Wayland: Got seat '%s'\n", name);
}

static void
wl_pointer_enter (void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface,
		      wl_fixed_t surface_x,
		      wl_fixed_t surface_y)
{
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
}

static void
wl_pointer_button (void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t serial,
		       uint32_t time,
		       uint32_t button,
		       uint32_t state)
{
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
wl_pointer_frame (void *data,
		      struct wl_pointer *wl_pointer)
{
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
wl_seat_capabilities (void *data, struct wl_seat *seat, uint32_t caps)
{
	if ((caps & WL_SEAT_CAPABILITY_POINTER) > 0) {
		wl_pointer = wl_seat_get_pointer (seat);
		wl_pointer_add_listener (wl_pointer, &wl_pointer_listener, nullptr);
	} else if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) > 0) {
		wl_keyboard = wl_seat_get_keyboard (seat);
		//wl_keyboard_add_listener (wl_keyboard, wl_keyboard_listener, nullptr);
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
	wl_add_device (&wl_mouse_device);
}

static in_driver_t in_wl_driver = {
	.init           = in_wl_init,
	.process_events = in_wl_process_events,
	.set_device_event_data = wl_set_device_event_data,
	.get_device_event_data = wl_get_device_event_data,
	.axis_info = in_wl_axis_info,
};

static void __attribute__((constructor))
in_wl_register_driver (void)
{
	wl_driver_handle = IN_RegisterDriver (&in_wl_driver, nullptr);
}

int wl_force_link;

