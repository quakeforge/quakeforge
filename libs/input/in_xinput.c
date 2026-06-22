/*
	in_xinput.c

	general xinput input driver

	Copyright (C) 2026 Bill Currie <bill@taniwha.org>
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
#include "QF/input/gamepad.h"

#include "compat.h"
#include "qfselect.h"
#include "xinput/xi.h"

typedef struct devmap_s {
	struct devmap_s *next;
	struct devmap_s **prev;
	xi_device_t *device;
	in_gamepad_t *gamepad;
	void       *event_data;
	int         devid;
} devmap_t;

static int xinput_driver_handle = -1;
static int xinput_have_focus;
static PR_RESMAP (devmap_t) devmap;
static devmap_t *devmap_list;

static char *xinput_library_name;
static cvar_t xinput_library_name_cvar = {
	.name = "xinput_library",
	.description =
		"the name of the xinput shared library",
	.default_value = "XInput1_4.dll",
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &xinput_library_name },
};

static void
in_xinput_shutdown (void *data)
{
	Sys_MaskPrintf (SYS_input, "in_xinput_shutdown\n");
	xi_close ();

	for (unsigned i = 0; i < devmap._size; i++) {
		free (devmap._map[i]);
	}
	free (devmap._map);
}

static void
in_xinput_set_device_event_data (void *device, void *event_data, void *data)
{
	xi_device_t *dev = device;
	devmap_t   *dm = dev->data;
	dm->event_data = event_data;
}

static void *
in_xinput_get_device_event_data (void *device, void *data)
{
	xi_device_t *dev = device;
	devmap_t   *dm = dev->data;
	return dm->event_data;
}

static void
in_xinput_axis_event (axis_t *axis, void *_dm)
{
	if (!xinput_have_focus) {
		return;
	}

	devmap_t   *dm = _dm;
	//Sys_Printf ("in_xinput_axis_event: %d %d\n", axis->num, axis->value);

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
in_xinput_button_event (button_t *button, void *_dm)
{
	if (!xinput_have_focus) {
		return;
	}

	devmap_t   *dm = _dm;
	//Sys_Printf ("in_xinput_button_event: %d %d\n", button->num, button->state);

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
in_xinput_gamepad_axis_event (axis_t *axis, void *_dm)
{
	if (!xinput_have_focus) {
		return;
	}

	devmap_t   *dm = _dm;
	//Sys_Printf ("in_xinput_gamepad_axis_event: %d %d\n",
	//			axis->num, axis->value);

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
	IN_Gamepad_Event (dm->gamepad, &event);
	IE_Send_Event (&event);
}

static void
in_xinput_gamepad_button_event (button_t *button, void *_dm)
{
	if (!xinput_have_focus) {
		return;
	}

	devmap_t   *dm = _dm;
	//Sys_Printf ("in_xinput_gamepad_button_event: %d %d\n",
	//			button->num, button->state);

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
	IN_Gamepad_Event (dm->gamepad, &event);
	IE_Send_Event (&event);
}

static void
device_add (xi_device_t *dev)
{
	const char *name = "";//dev->name;
	// prefer device unique string if available, otherwise fall back to
	// the physical path
	const char *id = "";//dev->uniq;
	if (!id || !*id) {
		//id = dev->phys;
	}

	devmap_t   *dm = PR_RESNEW (devmap);
	dm->next = devmap_list;
	dm->prev = &devmap_list;
	if (devmap_list) {
		devmap_list->prev = &dm->next;
	}
	devmap_list = dm;

	dev->data = dm;

	dm->device = dev;
	dm->devid = IN_AddDevice (xinput_driver_handle, dev, name, id);

	dm->gamepad = IN_Gamepad_Add ((in_devid_t) {
			.bustype = dev->bustype,
			.vendor = dev->vendor,
			.product = dev->product,
			.version = dev->version,
		}, dm->devid);
	if (dm->gamepad) {
		dev->axis_event = in_xinput_gamepad_axis_event;
		dev->button_event = in_xinput_gamepad_button_event;
	} else {
		dev->axis_event = in_xinput_axis_event;
		dev->button_event = in_xinput_button_event;
	}

#if 0
	Sys_Printf ("in_xinput: add %s\n", dev->path);
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
device_remove (xi_device_t *dev)
{
	for (devmap_t *dm = devmap_list; dm; dm = dm->next) {
		if (dm->device == dev) {
			if (dm->gamepad) {
				IN_Gamepad_Remove (dm->gamepad);
			}
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
in_xinput_init (void *data)
{
	Cvar_Register (&xinput_library_name_cvar, 0, 0);
	xi_init (xinput_library_name, device_add, device_remove);
}

static void
in_xinput_process_events (void *data)
{
	xi_check_input ();
}

static void
in_xinput_clear_states (void *data)
{
}

static void
in_xinput_axis_info (void *data, void *device, in_axisinfo_t *axes,
					int *numaxes)
{
	xi_device_t *dev = device;
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
in_xinput_button_info (void *data, void *device, in_buttoninfo_t *buttons,
					  int *numbuttons)
{
	xi_device_t *dev = device;
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
in_xinput_get_axis_name (void *data, void *device, int axis_num)
{
	return nullptr;
}

static int
in_xinput_get_axis_num (void *data, void *device, const char *axis_name)
{
	return -1;
}

static int
in_xinput_get_axis_info (void *data, void *device, int axis_num,
						in_axisinfo_t *info)
{
	xi_device_t *dev = device;
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
in_xinput_get_button_info (void *data, void *device, int button_num,
						  in_buttoninfo_t *info)
{
	xi_device_t *dev = device;
	if (button_num < 0 || button_num > dev->num_buttons) {
		return 0;
	}
	info->button = dev->buttons[button_num].num;
	info->state = dev->buttons[button_num].state;
	return 1;
}

static in_mapping_t
in_xinput_gamepad_mapping (void *data, void *device, const char *name)
{
	xi_device_t *dev = device;
	in_mapping_t mapping = {};
	if (name[0] == '+' || name[0] == '-') {
		mapping.sign = name[0] == '-' ? -1 : 1;
		name++;
	}
	int         index = strtoul (name + 1, nullptr, 0);
	switch (name[0]) {
		case 'a':
			if (index < dev->num_axes) {
				mapping.type = inm_abs_axis;
				mapping.index = index;
			}
			break;
		case 'b':
			if (index < dev->num_buttons) {
				mapping.type = inm_button;
				mapping.index = index;
			}
			break;
		case 'h':
			// xinput supports only one hat (if that)
			if (index > 0) {
				break;
			}
			int         mask = strtoul (name + 3, nullptr, 0);
			int         button = -1;
			switch (mask) {
				case 1: button = 12; break;
				case 2: button = 15; break;
				case 4: button = 13; break;
				case 8: button = 14; break;
			}
			if (button < 0) {
				break;
			}
			mapping.type = inm_button;
			mapping.index = button;
			mapping.sign = 0;
			break;
		default:
			break;
	}
	return mapping;
}

static in_driver_t in_xinput_driver = {
	.init = in_xinput_init,
	.shutdown = in_xinput_shutdown,
	.set_device_event_data = in_xinput_set_device_event_data,
	.get_device_event_data = in_xinput_get_device_event_data,
	.process_events = in_xinput_process_events,
	.clear_states = in_xinput_clear_states,

	.axis_info = in_xinput_axis_info,
	.button_info = in_xinput_button_info,

	.get_axis_name = in_xinput_get_axis_name,
	.get_axis_num = in_xinput_get_axis_num,

	.get_axis_info = in_xinput_get_axis_info,
	.get_button_info = in_xinput_get_button_info,

	.gamepad_mapping = in_xinput_gamepad_mapping,
};

static int
in_xinput_event_handler (const IE_event_t *event, void *data)
{
	if (event->type == ie_app_gain_focus) {
		xinput_have_focus = 1;
		return 1;
	} else if (event->type == ie_app_lose_focus) {
		xinput_have_focus = 0;
		return 1;
	}
	return 0;
}

static void __attribute__((constructor))
in_xinput_register_driver (void)
{
	xinput_driver_handle = IN_RegisterDriver (&in_xinput_driver, 0);
	//FIXME probably shouldn't be here
	IE_Add_Handler (in_xinput_event_handler, 0);
}

int in_xinput_force_link;
