/*
	input.h

	External (non-keyboard) input devices

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef __QF_input_h
#define __QF_input_h

#include "QF/keys.h"

typedef struct in_axisinfo_s {
	int         deviceid;
	int         axis;
	int         value;
	int         min;
	int         max;
} in_axisinfo_t;

typedef struct in_buttoninfo_s {
	int         deviceid;
	int         button;
	int         state;
} in_buttoninfo_t;

#include "QF/input/binding.h"
#include "QF/input/imt.h"

#ifndef __QFCC__

typedef struct {
	vec3_t angles;
	vec3_t position;
} viewdelta_t;

struct qf_fd_set;

typedef struct in_driver_s {
	void (*init) (void *data);
	void (*shutdown) (void *data);

	// The driver must provide either both or none of add_select and
	// chec_select.
	void (*add_select) (struct qf_fd_set *fdset, int *maxfd, void *data);
	void (*check_select) (struct qf_fd_set *fdset, void *data);
	// Generally musually exclusive with add_select/check_select
	void (*process_events) (void *data);

	void (*clear_states) (void *data);
	void (*grab_input) (void *data, int grab);

	void (*axis_info) (void *data, void *device, in_axisinfo_t *axes,
					   int *numaxes);
	void (*button_info) (void *data, void *device, in_buttoninfo_t *buttons,
						 int *numbuttons);
} in_driver_t;

typedef struct in_device_s {
	int         driverid;
	void       *device;
	const char *name;
	const char *id;
} in_device_t;

/*** Connect a device and its axes and buttons to logical axes and buttons.

	\a name is used to specify the device when creating bindings, and
	identifying the device for hints etc (eg, "press joy1 1 to ...").

	\a id is the device name (eg, "6d spacemouse") or (preferred) the device
	id (eg "usb-0000:00:1d.1-2/input0"). The device name is useful for ease of
	user identification and allowing the device to be plugged into any USB
	socket. However, it doesn't allow multiple devices with the same name
	(eg, using twin joysticks of the same model). Thus the device id is
	preferred, but does require the device to be plugged into the same uSB
	path (ie, same socket on the same hub connected to the same port on the PC)

	\a devid is the actual device associated with the bindings. If -1, the
	device is not currently connected.

	\a axis_info holds the device/axis specific range info and the current
	raw value of the axis.

	\a button_info holds the current raw state of the button

	\a axis_imt_id is 0 if the device has no axis bindings, otherwise it is
    the index into the imt array for the imt group.

	\a button_imt_id is 0 if the device has no button bindings, otherwise it
    is the index into the imt array for the imt group.
*/
typedef struct in_devbindings_s {
	const char *name;		///< name used when binding inputs
	const char *id;			///< physical device name or id (preferred)
	int         devid;		///< id of device associated with these bindings
	int         num_axes;
	int         num_buttons;
	in_axisinfo_t *axis_info;		///< axis range info and raw state
	in_buttoninfo_t *button_info;	///< button raw state
    int         axis_imt_id;    ///< index into array of imt axis bindings
    int         button_imt_id;  ///< index into array of imt button bindings
} in_devbindings_t;

extern viewdelta_t viewdelta;

#define freelook (in_mlook.state & 1 || in_freelook->int_val)

struct cvar_s;

int IN_RegisterDriver (in_driver_t *driver, void *data);
void IN_DriverData (int handlle, void *data);
void IN_Init (struct cbuf_s *cbuf);
void IN_Init_Cvars (void);

int IN_AddDevice (int driver, void *device, const char *name, const char *id);
void IN_RemoveDevice (int devid);

void IN_SendConnectedDevices (void);
const char *IN_GetDeviceName (int devid) __attribute__((pure));
const char *IN_GetDeviceId (int devid) __attribute__((pure));
int IN_AxisInfo (int devid, in_axisinfo_t *axes, int *numaxes);
int IN_ButtonInfo (int devid, in_buttoninfo_t *button, int *numbuttons);

void IN_ProcessEvents (void);

void IN_UpdateGrab (struct cvar_s *);

void IN_ClearStates (void);

void IN_Move (void); // FIXME: was cmduser_t?
// add additional movement on top of the keyboard move cmd

extern struct cvar_s		*in_grab;
extern struct cvar_s		*in_amp;
extern struct cvar_s		*in_pre_amp;
extern struct cvar_s		*m_filter;
extern struct cvar_s		*in_mouse_accel;
extern struct cvar_s		*in_freelook;
extern struct cvar_s		*lookstrafe;

extern qboolean 	in_mouse_avail;
extern float		in_mouse_x, in_mouse_y;


#endif

#endif//__QF_input_h
