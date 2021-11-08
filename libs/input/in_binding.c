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

static void
in_bind_f (void)
{
}

static void
in_unbind_f (void)
{
}

static void
in_clear_f (void)
{
}

static void
in_devices_f (void)
{
}

static void
keyhelp_f (void)
{
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

	for (bindcmd_t *cmd = in_binding_commands; cmd->name; cmd++) {
		Cmd_AddCommand (cmd->name, cmd->func, cmd->desc);
	}
}
