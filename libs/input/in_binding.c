/*
	in_binding.c

	Input binding management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/11/2

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

#include "QF/cmd.h"
#include "QF/hash.h"
#include "QF/input.h"
#include "QF/sys.h"

#include "QF/input/imt.h"

#include "QF/input/binding.h"
#include "QF/input/event.h"
#include "QF/input/imt.h"

typedef struct DARRAY_TYPE (in_devbindings_t) in_devbindingset_t;

static in_devbindingset_t devbindings = DARRAY_STATIC_INIT (8);
static int in_binding_handler;
static int in_keyhelp_handler;
static int in_keyhelp_saved_handler;

static void
in_binding_add_device (const IE_event_t *ie_event)
{
	size_t      devid = ie_event->device.devid;
	if (devid >= devbindings.size) {
		DARRAY_RESIZE (&devbindings, devid + 1);
		memset (&devbindings.a[devid], 0, sizeof (in_devbindings_t));
	}
	in_devbindings_t *db = &devbindings.a[devid];
	if (db->id) {
		if (db->id != IN_GetDeviceId (devid)) {
			Sys_Error ("in_binding_add_device: devid conflict: %zd", devid);
		}
		Sys_Printf ("in_binding_add_device: readd %s\n", db->id);
		return;
	}
	db->id = IN_GetDeviceId (devid);
	db->devid = devid;
	IN_AxisInfo (devid, 0, &db->num_axes);
	IN_ButtonInfo (devid, 0, &db->num_buttons);
	db->axis_info = malloc (db->num_axes * sizeof (in_axisinfo_t)
							+ db->num_buttons * sizeof (in_buttoninfo_t));
	db->button_info = (in_buttoninfo_t *) &db->axis_info[db->num_axes];
	IN_AxisInfo (devid, db->axis_info, &db->num_axes);
	IN_ButtonInfo (devid, db->button_info, &db->num_buttons);
	Sys_Printf ("in_binding_add_device: %s %d %d\n", db->id, db->num_axes,
				db->num_buttons);
}

static void
in_binding_remove_device (const IE_event_t *ie_event)
{
	size_t      devid = ie_event->device.devid;
	in_devbindings_t *db = &devbindings.a[devid];

	if (devid >= devbindings.size) {
		Sys_Error ("in_binding_remove_device: invalid devid: %zd", devid);
	}
	if (!db->id) {
		return;
	}
	free (db->axis_info);	// axis and button info in same block
	memset (db, 0, sizeof (*db));
}

static void
in_binding_axis (const IE_event_t *ie_event)
{
	size_t      devid = ie_event->axis.devid;
	int         axis = ie_event->axis.axis;
	int         value = ie_event->axis.value;
	in_devbindings_t *db = &devbindings.a[devid];

	if (devid < devbindings.size && axis < db->num_axes) {
		db->axis_info[axis].value = value;
		if (db->axis_imt_id) {
			IMT_ProcessAxis (db->axis_imt_id + axis, value);
		}
	}
}

static void
in_binding_button (const IE_event_t *ie_event)
{
	size_t      devid = ie_event->button.devid;
	int         button = ie_event->button.button;
	int         state = ie_event->button.state;
	in_devbindings_t *db = &devbindings.a[devid];

	if (devid < devbindings.size && button < db->num_buttons) {
		db->button_info[button].state = state;
		if (db->button_imt_id) {
			IMT_ProcessButton (db->button_imt_id + button, state);
		}
	}
}

static int
in_binding_event_handler (const IE_event_t *ie_event, void *unused)
{
	static void (*handlers[ie_event_count]) (const IE_event_t *ie_event) = {
		[ie_add_device] = in_binding_add_device,
		[ie_remove_device] = in_binding_remove_device,
		[ie_axis] = in_binding_axis,
		[ie_button] = in_binding_button,
	};
	if (ie_event->type < 0 || ie_event->type >= ie_event_count
		|| !handlers[ie_event->type]) {
		return 0;
	}
	handlers[ie_event->type] (ie_event);
	return 1;
}

static int
in_keyhelp_event_handler (const IE_event_t *ie_event, void *unused)
{
	if (ie_event->type != ie_axis && ie_event->type != ie_button) {
		return 0;
	}

	size_t      devid = ie_event->button.devid;
	in_devbindings_t *db = &devbindings.a[devid];
	const char *name = db->name;
	const char *type = 0;
	int         num = -1;

	if (!name) {
		name = db->id;
	}

	if (ie_event->type == ie_axis) {
		int         axis = ie_event->axis.axis;
		int         value = ie_event->axis.value;
		in_axisinfo_t *ai = &db->axis_info[axis];
		if (!ai->min && !ai->max) {
			if (value > 2) {
				num = axis;
				type = "axis";
			}
		} else {
			int         diff = abs (value - ai->value);
			if (diff * 5 >= ai->max - ai->min) {
				num = axis;
				type = "axis";
			}
		}
	} else if (ie_event->type == ie_button) {
		if (ie_event->button.state) {
			num = ie_event->button.button;
			type = "button";
		}
	}
	if (!type) {
		return 0;
	}
	IE_Set_Focus (in_keyhelp_saved_handler);
	Sys_Printf ("%s %s %d\n", name, type, num);
	return 1;
}

static in_devbindings_t *
in_binding_find_device (const char *name)
{
	for (size_t i = 0; i < devbindings.size; i++) {
		in_devbindings_t *dev = &devbindings.a[i];
		if (strcmp (name, dev->id) == 0
			|| (dev->name && strcmp (name, dev->name) == 0)) {
			return dev;
		}
	}
	return 0;
}

static void
in_bind_f (void)
{
	if (Cmd_Argc () < 6) {
		Sys_Printf ("in_bind imt device type number binding...\n");
		Sys_Printf ("    imt: the name of the input mapping table in which the"
					" intput will be bound\n");
		Sys_Printf ("    device: the nickname or id of the devise owning"
					" the input to be bound\n");
		Sys_Printf ("    type: the type of input to be bound (axis or"
					" button)\n");
		// FIXME support names
		Sys_Printf ("    number: the numeric id of the input to be bound\n");
		Sys_Printf ("    binging...: the destination to which the input will"
					" be bound\n");
		Sys_Printf ("        for axis inputs, this can be an analog input or"
					" an axis-button\n");
		Sys_Printf ("        for button inputs, this can be a button or a"
					" command (spaces ok, but\n"
					"        quotes recommended)\n");
		return;
	}

	const char *imt_name = Cmd_Argv (1);
	const char *dev_name = Cmd_Argv (2);
	const char *type = Cmd_Argv (3);
	const char *number = Cmd_Argv (4);
	// the rest of the command line is the binding
	const char *binding = Cmd_Args (5);

	imt_t      *imt = IMT_FindIMT (imt_name);
	in_devbindings_t *dev = in_binding_find_device (dev_name);
	char       *end;
	int         num = strtol (number, &end, 0);
	if (!imt) {
		Sys_Printf ("unknown imt: %s\n", imt_name);
		return;
	}
	if (!dev) {
		Sys_Printf ("unknown device: %s\n", dev_name);
		return;
	}
	if (strcmp (type, "axis") != 0 && strcmp (type, "button") != 0) {
		Sys_Printf ("invalid input type: %s\n", type);
		return;
	}
	if (*type == 'a') {
		if (*end || num < 0 || num >= dev->num_axes) {
			Sys_Printf ("invalid axis number: %s\n", number);
			return;
		}
		IMT_BindAxis (imt, dev->axis_imt_id + num, binding);
	} else {
		if (*end || num < 0 || num >= dev->num_buttons) {
			Sys_Printf ("invalid button number: %s\n", number);
			return;
		}
		IMT_BindButton (imt, dev->button_imt_id + num, binding);
	}
}

static void
in_unbind_f (void)
{
	if (Cmd_Argc () < 6) {
		Sys_Printf ("in_unbind imt device type number\n");
		Sys_Printf ("    imt: the name of the input mapping table in which the"
					" intput will be unbound\n");
		Sys_Printf ("    device: the nickname or id of the devise owning"
					" the input to be unbound\n");
		Sys_Printf ("    type: the type of input to be unbound (axis or"
					" button)\n");
		// FIXME support names
		Sys_Printf ("    number: the numeric id of the input to be unbound\n");
		return;
	}

	const char *imt_name = Cmd_Argv (1);
	const char *dev_name = Cmd_Argv (2);
	const char *type = Cmd_Argv (3);
	const char *number = Cmd_Argv (4);

	imt_t      *imt = IMT_FindIMT (imt_name);
	in_devbindings_t *dev = in_binding_find_device (dev_name);
	char       *end;
	int         num = strtol (number, &end, 0);
	if (!imt) {
		Sys_Printf ("unknown imt: %s\n", imt_name);
		return;
	}
	if (!dev) {
		Sys_Printf ("unknown device: %s\n", dev_name);
		return;
	}
	if (strcmp (type, "axis") != 0 && strcmp (type, "button") != 0) {
		Sys_Printf ("invalid input type: %s\n", type);
		return;
	}
	if (*type == 'a') {
		if (*end || num < 0 || num >= dev->num_axes) {
			Sys_Printf ("invalid axis number: %s\n", number);
			return;
		}
		IMT_BindAxis (imt, dev->axis_imt_id + num, 0);
	} else {
		if (*end || num < 0 || num >= dev->num_buttons) {
			Sys_Printf ("invalid button number: %s\n", number);
			return;
		}
		IMT_BindButton (imt, dev->button_imt_id + num, 0);
	}
}

static void
in_clear_f (void)
{
	int         argc = Cmd_Argc ();
	if (argc < 2) {
		Sys_Printf ("in_clear imt [imt...]\n");
		return;
	}
	for (int i = 1; i < argc; i++) {
		const char *imt_name = Cmd_Argv (i);
		imt_t      *imt = IMT_FindIMT (imt_name);
		if (!imt) {
			Sys_Printf ("unknown imt: %s\n", imt_name);
			continue;
		}
		for (size_t ind = 0; ind < imt->axis_bindings.size; ind++) {
			IMT_BindAxis (imt, ind, 0);
		}
		for (size_t ind = 0; ind < imt->button_bindings.size; ind++) {
			IMT_BindButton (imt, ind, 0);
		}
	}
}

static void
in_devices_f (void)
{
	for (size_t i = 0; i < devbindings.size; i++) {
		in_devbindings_t *dev = &devbindings.a[i];
		Sys_Printf ("%s %s %s\n", dev->name, dev->id,
					dev->devid >= 0 ? "connected" : "disconnected");
		Sys_Printf ("    axes: %d\n", dev->num_axes);
		Sys_Printf ("    buttons: %d\n", dev->num_buttons);
	}
}

static void
in_devname_f (void)
{
	switch (Cmd_Argc ()) {
		case 2: {
			const char *name = Cmd_Argv (1);
			in_devbindings_t *dev = in_binding_find_device (name);
			if (dev) {
				if (strcmp (name, dev->id) == 0) {
					Sys_Printf ("nickname for %s is %s\n", dev->id, dev->name);
				} else {
					Sys_Printf ("device_id for %s is %s\n", dev->name, dev->id);
				}
			} else {
				Sys_Printf ("No device identified by %s\n", name);
			}
			break;
		}
		case 3: {
			const char *device_id = Cmd_Argv (1);
			const char *nickname = Cmd_Argv (2);
			in_devbindings_t *dev = in_binding_find_device (nickname);
			if (dev) {
				Sys_Printf ("%s already exists: %s %s\n",
							nickname, dev->name, dev->id);
				return;
			}
			dev = in_binding_find_device (device_id);
			if (!dev) {
				Sys_Printf ("%s does not exist\n", device_id);
				return;
			}
			if (dev->name) {
				free ((char *) dev->name);
			}
			dev->name = strdup (nickname);
			break;
		}
		default:
			Sys_Printf ("in_devname device_id nickname\n");
			Sys_Printf ("   Name a deviced identified by device_id as nickname\n");
			Sys_Printf ("in_devname device_id\n");
			Sys_Printf ("   Show the nickname given to the device identified by device_id\n");
			Sys_Printf ("in_devname nickname\n");
			Sys_Printf ("   Show the device_id of the device named by nickname\n");
			break;
	}
}

static void
keyhelp_f (void)
{
	in_keyhelp_saved_handler = IE_Get_Focus ();
	IE_Set_Focus (in_keyhelp_handler);
	Sys_Printf ("Press button or move axis to identify\n");
}

typedef struct {
	const char *name;
	xcommand_t  func;
	const char *desc;
} bindcmd_t;

static bindcmd_t in_binding_commands[] = {
	{	"in_bind", in_bind_f,
		"Assign a command or a set of commands to a key.\n"
		"Note: To bind multiple commands to a key, enclose the "
		"commands in quotes and separate with semi-colons."
	},
	{	"in_unbind", in_unbind_f,
		"Remove the bind from the the selected key"
	},
	{	"in_clear", in_clear_f,
		"Remove all binds from the specified imts"
	},
	{	"in_devices", in_devices_f,
		"List the known devices and their status."
	},
	{	"in_devname", in_devname_f,
		"Give a device a nickname."
	},
	{	"keyhelp", keyhelp_f,
		"Identify the next active input axis or button.\n"
		"The identification includes the device binding name, axis or button "
		"number, and (if known) the name of the axis or button. Axes and "
		"buttons can always be bound by number, so even those for which a "
		"name is not known, but" PACKAGE_NAME " sees, can be bound."
	},
	{ }
#if 0
	{	"bindlist", Key_Bindlist_f,
		"list all of the key bindings"
	},
	{	"unbindall", Key_Unbindall_f,
		"Remove all binds (USE CAUTIOUSLY!!!"
	},
	{	"unbind", Key_Unbind_f,
		"wrapper for in_unbind that uses in_bind_imt for the imt "
		"parameter"
	},
	{	"bind", Key_Bind_f,
		"wrapper for in_bind that uses "
		"in_bind_imt for the imt parameter"
	},
	{	"imt", Key_InputMappingTable_f,
		""
	},
	{	"imt_keydest", Key_IMT_Keydest_f,
		""
	},
	{	"imt_create", Key_IMT_Create_f,
		"create a new imt table:\n"
		"    imt_create <keydest> <imt_name> [chain_name]\n"
		"\n"
		"The new table will be attached to the specified keydest\n"
		"imt_name must not already exist.\n"
		"If given, chain_name must already exist and be on "
		"keydest.\n"
	},
	{	"imt_drop_all", Key_IMT_Drop_All_f,
		"delete all imt tables\n"
	},
	{	"in_type", Key_In_Type_f,
		"Send the given string as simulated key presses."
	},
#endif
};

void
IN_Binding_Init (void)
{
	in_binding_handler = IE_Add_Handler (in_binding_event_handler, 0);
	in_keyhelp_handler = IE_Add_Handler (in_keyhelp_event_handler, 0);

	for (bindcmd_t *cmd = in_binding_commands; cmd->name; cmd++) {
		Cmd_AddCommand (cmd->name, cmd->func, cmd->desc);
	}
}
