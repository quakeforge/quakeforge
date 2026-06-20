/*
	in_evdev.c

	general evdev input driver

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>
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

#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/progs.h"   // for PR_RESMAP
#include "QF/sys.h"

#include "QF/input/event.h"

#include "compat.h"
#include "qfselect.h"
#include "evdev/inputlib.h"

typedef struct devmap_s {
	struct devmap_s *next;
	struct devmap_s **prev;
	device_t   *device;
	void       *event_data;
	int         devid;
} devmap_t;

static int evdev_driver_handle = -1;
static int evdev_have_focus;
static PR_RESMAP (devmap_t) devmap;
static devmap_t *devmap_list;

static const char *rel_axis_names[] = {
	"rel_x",
	"rel_y",
	"rel_z",
	"rel_rx",
	"rel_ry",
	"rel_rz",
	"rel_hwheel",
	"rel_dial",
	"rel_wheel",
	"rel_misc",
	nullptr,//rel_reserved
	"rel_wheel_hi_res",
	"rel_hwheel_hi_res",
	"rel_max",
};

static const char *abs_axis_names[] = {
	"abs_x",
	"abs_y",
	"abs_z",
	"abs_rx",
	"abs_ry",
	"abs_rz",
	"abs_throttle",
	"abs_rudder",
	"abs_wheel",
	"abs_gas",
	"abs_brake",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"abs_hat0x",
	"abs_hat0y",
	"abs_hat1x",
	"abs_hat1y",
	"abs_hat2x",
	"abs_hat2y",
	"abs_hat3x",
	"abs_hat3y",
	"abs_pressure",
	"abs_distance",
	"abs_tilt_x",
	"abs_tilt_y",
	"abs_tool_width",
	nullptr,
	nullptr,
	nullptr,
	"abs_volume",
	"abs_profile",
	"abs_snd_profile",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"abs_misc",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,//abs_reserved,
	"abs_mt_slot",
	"abs_mt_touch_major",
	"abs_mt_touch_minor",
	"abs_mt_width_major",
	"abs_mt_width_minor",
	"abs_mt_orientation",
	"abs_mt_position_x",
	"abs_mt_position_y",
	"abs_mt_tool_type",
	"abs_mt_blob_id",
	"abs_mt_tracking_id",
	"abs_mt_pressure",
	"abs_mt_distance",
	"abs_mt_tool_x",
	"abs_mt_tool_y",
};

static void
in_evdev_add_select (qf_fd_set *fdset, int *maxfd, void *data)
{
	inputlib_add_select (&fdset->fdset, maxfd);
}

static void
in_evdev_check_select (qf_fd_set *fdset, void *data)
{
	inputlib_check_select (&fdset->fdset);
}

static void
in_evdev_shutdown (void *data)
{
	Sys_MaskPrintf (SYS_input, "in_evdev_shutdown\n");
	inputlib_close ();

	for (unsigned i = 0; i < devmap._size; i++) {
		free (devmap._map[i]);
	}
	free (devmap._map);
}

static void
in_evdev_set_device_event_data (void *device, void *event_data, void *data)
{
	device_t   *dev = device;
	devmap_t   *dm = dev->data;
	dm->event_data = event_data;
}

static void *
in_evdev_get_device_event_data (void *device, void *data)
{
	device_t   *dev = device;
	devmap_t   *dm = dev->data;
	return dm->event_data;
}

static void
in_evdev_axis_event (axis_t *axis, void *_dm)
{
	if (!evdev_have_focus) {
		return;
	}

	devmap_t   *dm = _dm;
	//Sys_Printf ("in_evdev_axis_event: %d %d\n", axis->num, axis->value);

	IE_event_t  event = {
		.type = ie_axis,
		.when = Sys_LongTime (),
		.axis = {
			.data = dm->event_data,
			.devid = dm->devid,
			.axis = axis->num,
			.value = axis->value,
		},
	};
	IE_Send_Event (&event);
}

static void
in_evdev_button_event (button_t *button, void *_dm)
{
	if (!evdev_have_focus) {
		return;
	}

	devmap_t   *dm = _dm;
	//Sys_Printf ("in_evdev_button_event: %d %d\n", button->num, button->state);

	IE_event_t  event = {
		.type = ie_button,
		.when = Sys_LongTime (),
		.button = {
			.data = dm->event_data,
			.devid = dm->devid,
			.button = button->num,
			.state = button->state,
		},
	};
	IE_Send_Event (&event);
}

static void
device_add (device_t *dev)
{
	const char *name = dev->name;
	// prefer device unique string if available, otherwise fall back to
	// the physical path
	const char *id = dev->uniq;
	if (!id || !*id) {
		id = dev->phys;
	}

	devmap_t   *dm = PR_RESNEW (devmap);
	dm->next = devmap_list;
	dm->prev = &devmap_list;
	if (devmap_list) {
		devmap_list->prev = &dm->next;
	}
	devmap_list = dm;

	dev->data = dm;
	dev->axis_event = in_evdev_axis_event;
	dev->button_event = in_evdev_button_event;

	dm->device = dev;
	dm->devid = IN_AddDevice (evdev_driver_handle, dev, name, id);

#if 0
	Sys_Printf ("in_evdev: add %s\n", dev->path);
	Sys_Printf ("    %s\n", dev->name);
	Sys_Printf ("    %s\n", dev->phys);
	for (int i = 0; i < dev->num_axes; i++) {
		axis_t     *axis = dev->axes + i;
		Sys_Printf ("axis: %d %d\n", axis->num, axis->value);
	}
	for (int i = 0; i < dev->num_buttons; i++) {
		button_t     *button = dev->buttons + i;
		Sys_Printf ("button: %d %d\n", button->num, button->state);
	}
#endif
}

static void
device_remove (device_t *dev)
{
	for (devmap_t *dm = devmap_list; dm; dm = dm->next) {
		if (dm->device == dev) {
			IN_RemoveDevice (dm->devid);

			if (dm->next) {
				dm->next->prev = dm->prev;
			}
			*dm->prev = dm->next;

			PR_RESFREE (devmap, dm);
			break;
		}
	}
}

static void
in_evdev_init (void *data)
{
	inputlib_init (device_add, device_remove);
}

static void
in_evdev_clear_states (void *data)
{
}

static void
in_evdev_axis_info (void *data, void *device, in_axisinfo_t *axes,
					int *numaxes)
{
	device_t   *dev = device;
	if (!axes) {
		*numaxes = dev->num_axes;
		return;
	}
	if (*numaxes > dev->num_axes) {
		*numaxes = dev->num_axes;
	}
	for (int i = 0; i < *numaxes; i++) {
		axes[i].axis = dev->axes[i].num;
		axes[i].value = dev->axes[i].value;
		axes[i].min = dev->axes[i].min;
		axes[i].max = dev->axes[i].max;
	}
}

static void
in_evdev_button_info (void *data, void *device, in_buttoninfo_t *buttons,
					  int *numbuttons)
{
	device_t   *dev = device;
	if (!buttons) {
		*numbuttons = dev->num_buttons;
		return;
	}
	if (*numbuttons > dev->num_buttons) {
		*numbuttons = dev->num_buttons;
	}
	for (int i = 0; i < *numbuttons; i++) {
		buttons[i].button = dev->buttons[i].num;
		buttons[i].state = dev->buttons[i].state;
	}
}

static const char *
in_evdev_get_axis_name (void *data, void *device, int axis_num)
{
	device_t   *dev = device;
	if (axis_num < 0 || axis_num > dev->num_axes) {
		return nullptr;
	}
	auto axis = &dev->axes[axis_num];
	if (!axis->min && !axis->max) {
		if (axis->evnum >= (int) countof (rel_axis_names)) {
			return nullptr;
		}
		return rel_axis_names[axis->evnum];
	}
	if (axis->evnum >= (int) countof (abs_axis_names)) {
		return nullptr;
	}
	return abs_axis_names[axis->evnum];
}

static int
in_evdev_get_axis_num (void *data, void *device, const char *axis_name)
{
	device_t   *dev = device;
	int         count;
	int         map_count;
	int        *map;
	const char **names;
	if ((axis_name[0] | 0x20) == 'r') {
		count = countof (rel_axis_names);
		names = rel_axis_names;
		map = dev->rel_axis_map;
		map_count = dev->max_rel_axis + 1;
	} else {
		count = countof (abs_axis_names);
		names = abs_axis_names;
		map = dev->abs_axis_map;
		map_count = dev->max_abs_axis + 1;
	}
	for (int i = 0; i < count && i < map_count; i++) {
		if (names[i] && strcasecmp (axis_name, names[i]) == 0) {
			return map[i];
		}
	}
	return -1;
}

static int
in_evdev_get_axis_info (void *data, void *device, int axis_num,
						in_axisinfo_t *info)
{
	device_t   *dev = device;
	if (axis_num < 0 || axis_num > dev->num_axes) {
		return 0;
	}
	info->axis = dev->axes[axis_num].num;
	info->value = dev->axes[axis_num].value;
	info->min = dev->axes[axis_num].min;
	info->max = dev->axes[axis_num].max;
	return 1;
}

static int
in_evdev_get_button_info (void *data, void *device, int button_num,
						  in_buttoninfo_t *info)
{
	device_t   *dev = device;
	if (button_num < 0 || button_num > dev->num_buttons) {
		return 0;
	}
	info->button = dev->buttons[button_num].num;
	info->state = dev->buttons[button_num].state;
	return 1;
}

static in_driver_t in_evdev_driver = {
	.init = in_evdev_init,
	.shutdown = in_evdev_shutdown,
	.set_device_event_data = in_evdev_set_device_event_data,
	.get_device_event_data = in_evdev_get_device_event_data,
	.add_select = in_evdev_add_select,
	.check_select = in_evdev_check_select,
	.clear_states = in_evdev_clear_states,

	.axis_info = in_evdev_axis_info,
	.button_info = in_evdev_button_info,

	.get_axis_name = in_evdev_get_axis_name,
	.get_axis_num = in_evdev_get_axis_num,

	.get_axis_info = in_evdev_get_axis_info,
	.get_button_info = in_evdev_get_button_info,
};

static int
in_evdev_evend_handler (const IE_event_t *event, void *data)
{
	if (event->type == ie_app_gain_focus) {
		evdev_have_focus = 1;
		return 1;
	} else if (event->type == ie_app_lose_focus) {
		evdev_have_focus = 0;
		return 1;
	}
	return 0;
}

static void __attribute__((constructor))
in_evdev_register_driver (void)
{
	evdev_driver_handle = IN_RegisterDriver (&in_evdev_driver, 0);
	//FIXME probably shouldn't be here
	IE_Add_Handler (in_evdev_evend_handler, 0);
}

int in_evdev_force_link;
