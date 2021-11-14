/*
	in_common.c

	general input driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
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

#include "QF/cbuf.h"
#include "QF/cvar.h"
#include "QF/darray.h"
#define IMPLEMENT_INPUT_Funcs
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/plist.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/input/event.h"

#include "qfselect.h"

typedef struct {
	in_driver_t driver;
	void       *data;
} in_regdriver_t;

static struct DARRAY_TYPE (in_regdriver_t) in_drivers = { .grow = 8 };
static struct DARRAY_TYPE (in_device_t) in_devices = { .grow = 8 };

VISIBLE viewdelta_t viewdelta;

cvar_t     *in_grab;
VISIBLE cvar_t     *in_amp;
VISIBLE cvar_t     *in_pre_amp;
cvar_t     *in_freelook;
cvar_t     *in_mouse_filter;
cvar_t     *in_mouse_amp;
cvar_t     *in_mouse_pre_amp;
cvar_t     *lookstrafe;

int64_t     in_timeout = 10000;//10ms default timeout

qboolean    in_mouse_avail;
float       in_mouse_x, in_mouse_y;
static float in_old_mouse_x, in_old_mouse_y;

int
IN_RegisterDriver (in_driver_t *driver, void *data)
{
	in_regdriver_t rdriver = { *driver, data };
	DARRAY_APPEND (&in_drivers, rdriver);
	return in_drivers.size - 1;
}

void
IN_DriverData (int handle, void *data)
{
	in_drivers.a[handle].data = data;
}

static int
in_add_device (int driver, void *device, const char *name,
			   const char *id)
{
	size_t      devid;
	in_device_t indev = {
		.driverid = driver,
		.device = device,
		.name = name,
		.id = id,
	};

	for (devid = 0; devid < in_devices.size; devid++) {
		if (in_devices.a[devid].driverid == -1) {
			in_devices.a[devid] = indev;
			return devid;
		}
	}
	DARRAY_APPEND (&in_devices, indev);
	return devid;
}

int
IN_AddDevice (int driver, void *device, const char *name, const char *id)
{
	if ((size_t) driver >= in_drivers.size) {
		Sys_Error ("IN_AddDevice: invalid driver: %d", driver);
	}

	int         devid = in_add_device (driver, device, name, id);

	IE_event_t  event = {
		.type = ie_add_device,
		.when = Sys_LongTime (),
		.device = {
			.devid = devid,
		},
	};
	IE_Send_Event (&event);
	return devid;
}

void
IN_RemoveDevice (int devid)
{
	if ((size_t) devid >= in_devices.size) {
		Sys_Error ("IN_RemoveDevice: invalid devid: %d", devid);
	}

	IE_event_t  event = {
		.type = ie_remove_device,
		.when = Sys_LongTime (),
		.device = {
			.devid = devid,
		},
	};
	IE_Send_Event (&event);

	in_devices.a[devid].device = 0;
}

void
IN_SendConnectedDevices (void)
{
	for (size_t devid = 0; devid < in_devices.size; devid++) {
		if (in_devices.a[devid].driverid >= 0
			&& in_devices.a[devid].device) {
			IE_event_t  event = {
				.type = ie_add_device,
				.when = Sys_LongTime (),//FIXME actual time?
				.device = {
					.devid = devid,
				},
			};
			IE_Send_Event (&event);
		}
	}
}

const char *
IN_GetDeviceName (int devid)
{
	if ((size_t) devid >= in_devices.size) {
		return 0;
	}
	if (!in_devices.a[devid].device || in_devices.a[devid].driverid == -1) {
		return 0;
	}
	return in_devices.a[devid].name;
}

const char *
IN_GetDeviceId (int devid)
{
	if ((size_t) devid >= in_devices.size) {
		return 0;
	}
	if (!in_devices.a[devid].device || in_devices.a[devid].driverid == -1) {
		return 0;
	}
	return in_devices.a[devid].id;
}

void
IN_SetDeviceEventData (int devid, void *data)
{
	if ((size_t) devid >= in_devices.size) {
		return;
	}
	if (!in_devices.a[devid].device || in_devices.a[devid].driverid == -1) {
		return;
	}
	in_regdriver_t *rd = &in_drivers.a[in_devices.a[devid].driverid];
	rd->driver.set_device_event_data (in_devices.a[devid].device, data,
									  rd->data);
}

void *
IN_GetDeviceEventData (int devid)
{
	if ((size_t) devid >= in_devices.size) {
		return 0;
	}
	if (!in_devices.a[devid].device || in_devices.a[devid].driverid == -1) {
		return 0;
	}
	in_regdriver_t *rd = &in_drivers.a[in_devices.a[devid].driverid];
	return rd->driver.get_device_event_data (in_devices.a[devid].device,
											 rd->data);
}

int
IN_AxisInfo (int devid, in_axisinfo_t *axes, int *numaxes)
{
	if ((size_t) devid >= in_devices.size) {
		return -1;
	}
	if (!in_devices.a[devid].device || in_devices.a[devid].driverid == -1) {
		return -1;
	}
	int         driver = in_devices.a[devid].driverid;
	in_regdriver_t *rd = &in_drivers.a[driver];
	if (!rd->driver.axis_info) {
		return -1;
	}
	rd->driver.axis_info (rd->data, in_devices.a[devid].device, axes, numaxes);
	if (axes) {
		for (int i = 0; i < *numaxes; i++) {
			axes[i].deviceid = devid;
		}
	}
	return 0;
}

int
IN_ButtonInfo (int devid, in_buttoninfo_t *buttons, int *numbuttons)
{
	if ((size_t) devid >= in_devices.size) {
		return -1;
	}
	if (!in_devices.a[devid].device || in_devices.a[devid].driverid == -1) {
		return -1;
	}
	int         driver = in_devices.a[devid].driverid;
	in_regdriver_t *rd = &in_drivers.a[driver];
	if (!rd->driver.button_info) {
		return -1;
	}
	rd->driver.button_info (rd->data, in_devices.a[devid].device,
							buttons, numbuttons);
	if (buttons) {
		for (int i = 0; i < *numbuttons; i++) {
			buttons[i].deviceid = devid;
		}
	}
	return 0;
}

void
IN_UpdateGrab (cvar_t *var)		// called from context_*.c
{
	if (var) {
		for (size_t i = 0; i < in_drivers.size; i++) {
			in_regdriver_t *rd = &in_drivers.a[i];
			if (rd->driver.grab_input) {
				rd->driver.grab_input (rd->data, var->int_val);
			}
		}
	}
}

void
IN_ProcessEvents (void)
{
	qf_fd_set   fdset;
	int         maxfd = -1;
	int64_t     timeout = in_timeout;

	QF_FD_ZERO (&fdset);
	for (size_t i = 0; i < in_drivers.size; i++) {
		in_regdriver_t *rd = &in_drivers.a[i];
		if (rd->driver.add_select) {
			rd->driver.add_select (&fdset, &maxfd, rd->data);
		}
		if (rd->driver.process_events) {
			rd->driver.process_events (rd->data);
			// if a driver can't use select, then we can't block in select
			timeout = 0;
		}
	}
	if (maxfd >= 0 && Sys_Select (maxfd, &fdset, timeout) > 0) {
		for (size_t i = 0; i < in_drivers.size; i++) {
			in_regdriver_t *rd = &in_drivers.a[i];
			if (rd->driver.check_select) {
				rd->driver.check_select (&fdset, rd->data);
			}
		}
	}
}

void
IN_SaveConfig (plitem_t *config)
{
	plitem_t   *input_config = PL_NewDictionary (0); //FIXME hashlinks
	PL_D_AddObject (config, "input", input_config);

	IMT_SaveConfig (input_config);
	IN_Binding_SaveConfig (input_config);
}

void
IN_LoadConfig (plitem_t *config)
{
	plitem_t   *input_config = PL_ObjectForKey (config, "input");

	if (input_config) {
		IMT_LoadConfig (input_config);
		IN_Binding_LoadConfig (input_config);
	}
}

void
IN_Move (void)
{
	//JOY_Move ();

	if (!in_mouse_avail)
		return;

	in_mouse_x *= in_mouse_pre_amp->value * in_pre_amp->value;
	in_mouse_y *= in_mouse_pre_amp->value * in_pre_amp->value;

	if (in_mouse_filter->int_val) {
		in_mouse_x = (in_mouse_x + in_old_mouse_x) * 0.5;
		in_mouse_y = (in_mouse_y + in_old_mouse_y) * 0.5;

		in_old_mouse_x = in_mouse_x;
		in_old_mouse_y = in_mouse_y;
	}

	in_mouse_x *= in_mouse_amp->value * in_amp->value;
	in_mouse_y *= in_mouse_amp->value * in_amp->value;
#if 0
	if ((in_strafe.state & 1) || (lookstrafe->int_val && freelook))
		viewdelta.position[0] += in_mouse_x;
	else
		viewdelta.angles[YAW] -= in_mouse_x;

	if (freelook && !(in_strafe.state & 1)) {
		viewdelta.angles[PITCH] += in_mouse_y;
	} else {
		viewdelta.position[2] -= in_mouse_y;
	}
#endif
	in_mouse_x = in_mouse_y = 0.0;
}

/* Called at shutdown */
static void
IN_shutdown (void *data)
{
	//JOY_Shutdown ();

	Sys_MaskPrintf (SYS_vid, "IN_Shutdown\n");
	for (size_t i = in_drivers.size; i-- > 0; ) {
		in_regdriver_t *rd = &in_drivers.a[i];
		if (rd->driver.shutdown) {
			rd->driver.shutdown (rd->data);
		}
	}
}

void
IN_Init (cbuf_t *cbuf)
{
	Sys_RegisterShutdown (IN_shutdown, 0);
	IMT_Init ();
	IN_Binding_Init ();

	for (size_t i = 0; i < in_drivers.size; i++) {
		in_regdriver_t *rd = &in_drivers.a[i];
		rd->driver.init (rd->data);
	}
	//Key_Init (cbuf);
	//JOY_Init ();

	in_mouse_x = in_mouse_y = 0.0;
}

void
IN_Init_Cvars (void)
{
	//Key_Init_Cvars ();
	//JOY_Init_Cvars ();
	in_grab = Cvar_Get ("in_grab", "0", CVAR_ARCHIVE, IN_UpdateGrab,
						"With this set to 1, quake will grab the mouse, "
						"preventing loss of input focus.");
	in_amp = Cvar_Get ("in_amp", "1", CVAR_ARCHIVE, NULL,
					   "global in_amp multiplier");
	in_pre_amp = Cvar_Get ("in_pre_amp", "1", CVAR_ARCHIVE, NULL,
						   "global in_pre_amp multiplier");
	in_freelook = Cvar_Get ("freelook", "0", CVAR_ARCHIVE, NULL,
							"force +mlook");
	in_mouse_filter = Cvar_Get ("in_mouse_filter", "0", CVAR_ARCHIVE, NULL,
								"Toggle mouse input filtering.");
	in_mouse_amp = Cvar_Get ("in_mouse_amp", "15", CVAR_ARCHIVE, NULL,
							 "mouse in_mouse_amp multiplier");
	in_mouse_pre_amp = Cvar_Get ("in_mouse_pre_amp", "1", CVAR_ARCHIVE, NULL,
								 "mouse in_mouse_pre_amp multiplier");
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE, NULL,
						   "when mlook/klook on player will strafe");
}

void
IN_ClearStates (void)
{
	for (size_t i = 0; i < in_drivers.size; i++) {
		in_regdriver_t *rd = &in_drivers.a[i];
		if (rd->driver.clear_states) {
			rd->driver.clear_states (rd->data);
		}
	}
	//Key_ClearStates ();
}

#ifdef HAVE_EVDEV
extern int in_evdev_force_link;
static __attribute__((used)) int *evdev_force_link = &in_evdev_force_link;
#endif
