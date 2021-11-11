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

#include "QF/cexpr.h"
#include "QF/cmd.h"
#include "QF/cmem.h"
#include "QF/hash.h"
#include "QF/heapsort.h"
#include "QF/input.h"
#include "QF/plist.h"
#include "QF/progs.h"   // for PR_RESMAP
#include "QF/sys.h"

#include "QF/input/imt.h"

#include "QF/input/binding.h"
#include "QF/input/event.h"
#include "QF/input/imt.h"

typedef struct DARRAY_TYPE (int) in_knowndevset_t;

static in_knowndevset_t known_devices = DARRAY_STATIC_INIT (8);
static int in_binding_handler;
static int in_keyhelp_handler;
static int in_keyhelp_saved_handler;

static PR_RESMAP (in_devbindings_t) devbindings;
static in_devbindings_t *devbindings_list;

static int
devid_cmp (const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

static int * __attribute__ ((pure))
in_find_devid (int devid)
{
	return bsearch (&devid, known_devices.a, known_devices.size,
					sizeof (int), devid_cmp);
}

static in_devbindings_t * __attribute__ ((pure))
in_binding_find_connection (const char *devname, const char *id)
{
	in_devbindings_t *db;

	//FIXME slow
	for (db = devbindings_list; db; db = db->next) {
		if (strcmp (devname, db->devname) != 0) {
			continue;
		}
		if (db->match_id && strcmp (id, db->id) != 0) {
			continue;
		}
		return db;
	}
	return 0;
}

static void
in_binding_add_device (const IE_event_t *ie_event)
{
	size_t      devid = ie_event->device.devid;

	if (in_find_devid (devid)) {
		// the device is already known. this is likely the result of a
		// broadcast of connected devices
		return;
	}
	DARRAY_APPEND (&known_devices, devid);
	// keep the known devices sorted by id
	heapsort (known_devices.a, known_devices.size, sizeof (int), devid_cmp);

	const char *devname = IN_GetDeviceName (devid);
	const char *id = IN_GetDeviceId (devid);
	in_devbindings_t *db = in_binding_find_connection (devname, id);

	if (db) {
		if (db->match_id) {
			Sys_Printf ("Reconnected %s to %s %s\n", db->name, devname, id);
		} else {
			Sys_Printf ("Reconnected %s to %s\n", db->name, devname);
		}
		db->devid = devid;
		IN_SetDeviceEventData (devid, db);
	} else {
		Sys_Printf ("Added device %s %s\n", devname, id);
	}
}

static void
in_binding_remove_device (const IE_event_t *ie_event)
{
	size_t      devid = ie_event->device.devid;
	in_devbindings_t *db = IN_GetDeviceEventData (devid);
	int        *kd;

	if (!(kd = in_find_devid (devid))) {
		Sys_Error ("in_binding_remove_device: invalid devid: %zd", devid);
	}
	DARRAY_REMOVE_AT (&known_devices, kd - known_devices.a);

	const char *devname = IN_GetDeviceName (devid);
	const char *id = IN_GetDeviceId (devid);
	if (db) {
		db->devid = -1;
		if (db->match_id) {
			Sys_Printf ("Disconnected %s from %s %s\n", db->name, devname, id);
		} else {
			Sys_Printf ("Disconnected %s from %s\n", db->name, devname);
		}
	}
	Sys_Printf ("Removed device %s %s\n", devname, id);
}

static void
in_binding_axis (const IE_event_t *ie_event)
{
	int         axis = ie_event->axis.axis;
	int         value = ie_event->axis.value;
	in_devbindings_t *db = ie_event->axis.data;;

	if (db && axis < db->num_axes) {
		db->axis_info[axis].value = value;
		if (db->axis_imt_id >= 0) {
			IMT_ProcessAxis (db->axis_imt_id + axis, value);
		}
	}
}

static void
in_binding_button (const IE_event_t *ie_event)
{
	int         button = ie_event->button.button;
	int         state = ie_event->button.state;
	in_devbindings_t *db = ie_event->button.data;

	if (db && button < db->num_buttons) {
		db->button_info[button].state = state;
		if (db->button_imt_id >= 0) {
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

static int keyhelp_axis_threshold;

static int
in_keyhelp_event_handler (const IE_event_t *ie_event, void *unused)
{
	if (ie_event->type != ie_axis && ie_event->type != ie_button) {
		return 0;
	}

	size_t      devid = ie_event->button.devid;
	in_devbindings_t *db = ie_event->button.data;
	const char *name = db ? db->name : 0;
	const char *type = 0;
	int         num = -1;
	const char *devname = IN_GetDeviceName (devid);
	const char *id = IN_GetDeviceId (devid);

	if (ie_event->type == ie_axis) {
		int         axis = ie_event->axis.axis;
		int         value = ie_event->axis.value;
		in_axisinfo_t *ai;
		if (db) {
			ai = &db->axis_info[axis];
		} else {
			//FIXME set single axis info entry
			int         num_axes;
			in_axisinfo_t *axis_info;
			IN_AxisInfo (devid, 0, &num_axes);
			axis_info = alloca (num_axes * sizeof (in_axisinfo_t));
			IN_AxisInfo (devid, axis_info, &num_axes);
			ai = &axis_info[axis];
		}
		if (!ai->min && !ai->max) {
			if (abs (value) > keyhelp_axis_threshold) {
				num = axis;
				type = "axis";
			}
		} else {
			//FIXME does not work if device has not been connected (db is null)
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
	Sys_Printf ("%s (%s %s) %s %d\n", name, devname, id, type, num);
	return 1;
}

static in_devbindings_t * __attribute__ ((pure))
in_binding_find_device (const char *name)
{
	in_devbindings_t *db;

	for (db = devbindings_list; db; db = db->next) {
		if (strcmp (name, db->name) == 0) {
			break;
		}
	}
	return db;
}

static void
in_bind_f (void)
{
	int         argc = Cmd_Argc ();
	if (argc < 6) {
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
		if (dev->axis_imt_id == -1) {
			dev->axis_imt_id = IMT_GetAxisBlock (dev->num_axes);
		}
		const char *axis_name = Cmd_Argv (5);
		in_axis_t  *axis = IN_FindAxis (axis_name);
		if (!axis) {
			Sys_Printf ("unknown axis: %s\n", axis_name);
			return;
		}
		in_axisinfo_t *axisinfo = &dev->axis_info[num];
		in_recipe_t recipe = {
			.min = axisinfo->min,
			.max = axisinfo->max,
			.curve = 1,
			.scale = 1,
		};
		double curve = recipe.curve;
		double scale = recipe.scale;
		exprsym_t   var_syms[] = {
			{"minzone", &cexpr_int, &recipe.minzone},
			{"maxzone", &cexpr_int, &recipe.maxzone},
			{"deadzone", &cexpr_int, &recipe.deadzone},
			{"curve", &cexpr_double, &curve},
			{"scale", &cexpr_double, &scale},
			{}
		};
		exprtab_t   vars_tab = { var_syms, 0 };
		exprctx_t   exprctx = {
			.symtab = &vars_tab,
			.memsuper = new_memsuper (),
			.messages = PL_NewArray (),
		};
		cexpr_init_symtab (&vars_tab, &exprctx);

		int     i;
		for (i = 6; i < argc; i++) {
			const char *arg = Cmd_Argv (i);
			if (cexpr_eval_string (arg, &exprctx)) {
				plitem_t   *messages = exprctx.messages;
				for (int j = 0; j < PL_A_NumObjects (messages); j++) {
					Sys_Printf ("%s\n",
								PL_String (PL_ObjectAtIndex (messages, j)));
				}
				break;
			}
		}
		if (i == argc) {
			recipe.curve = curve;
			recipe.scale = scale;
			IMT_BindAxis (imt, dev->axis_imt_id + num, axis, &recipe);
		}
		Hash_DelTable (vars_tab.tab);
		PL_Free (exprctx.messages);
		delete_memsuper (exprctx.memsuper);
	} else {
		// the rest of the command line is the binding
		const char *binding = Cmd_Args (5);

		if (*end || num < 0 || num >= dev->num_buttons) {
			Sys_Printf ("invalid button number: %s\n", number);
			return;
		}
		if (dev->button_imt_id == -1) {
			dev->button_imt_id = IMT_GetButtonBlock (dev->num_buttons);
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
		IMT_BindAxis (imt, dev->axis_imt_id + num, 0, 0);
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
			IMT_BindAxis (imt, ind, 0, 0);
		}
		for (size_t ind = 0; ind < imt->button_bindings.size; ind++) {
			IMT_BindButton (imt, ind, 0);
		}
	}
}

static void
in_devices_f (void)
{
	for (size_t i = 0; i < known_devices.size; i++) {
		int     devid = known_devices.a[i];
		in_devbindings_t *db = IN_GetDeviceEventData (devid);
		const char *name = IN_GetDeviceName (devid);
		const char *id = IN_GetDeviceId (devid);
		int         num_axes, num_buttons;
		IN_AxisInfo (devid, 0, &num_axes);
		IN_ButtonInfo (devid, 0, &num_buttons);

		Sys_Printf ("devid %d:\n", devid);
		if (db) {
			Sys_Printf ("    bind name: %s\n", db->name);
		} else {
			Sys_Printf ("    no bind name\n");
		}
		Sys_Printf ("    name: %s\n", name);
		Sys_Printf ("    id: %s\n", id);
		Sys_Printf ("    axes: %d\n", num_axes);
		Sys_Printf ("    buttons: %d\n", num_buttons);
	}
}

static void
in_connect_f (void)
{
	int         argc = Cmd_Argc ();
	const char *fullid = 0;

	if (argc == 4) {
		fullid = Cmd_Argv (3);
	}
	if (argc < 3 || argc > 4 || (fullid && strcmp (fullid, "fullid"))) {
		goto in_connect_usage;
	}
	const char *bindname = Cmd_Argv (1);
	const char *device_id = Cmd_Argv (2);
	int         devid = -1;

	for (in_devbindings_t *db = devbindings_list; db; db = db->next) {
		if (strcmp (bindname, db->name) == 0) {
			Sys_Printf ("%s already exists\n", bindname);
			return;
		}
	}

	if (device_id[0] == '#') {
		char       *end;
		devid = strtol (device_id + 1, &end, 0);
		if (*end || !in_find_devid (devid)) {
			Sys_Printf ("Not a valid device number: %s", device_id);
			return;
		}
	} else {
		int         len = strlen (device_id);

		for (size_t i = 0; i < known_devices.size; i++) {
			if (strcmp (device_id, IN_GetDeviceId (known_devices.a[i])) == 0) {
				devid = known_devices.a[i];
				break;
			}
			if (strncasecmp (device_id,
							 IN_GetDeviceName (known_devices.a[i]),
							 len) == 0) {
				if (devid > -1) {
					Sys_Printf ("'%s' is ambiguous\n", device_id);
					return;
				}
				devid = known_devices.a[i];
			}
		}
	}
	if (devid == -1) {
		Sys_Printf ("No such device: %s\n", device_id);
		return;
	}
	if (IN_GetDeviceEventData (devid)) {
		Sys_Printf ("%s already connected\n", device_id);
		return;
	}

	in_devbindings_t *db = PR_RESNEW (devbindings);
	db->next = devbindings_list;
	devbindings_list = db;

	db->name = strdup (bindname);
	db->devname = strdup (IN_GetDeviceName (devid));
	db->id = strdup (IN_GetDeviceId (devid));
	db->match_id = !!fullid;
	db->devid = devid;

	IN_AxisInfo (devid, 0, &db->num_axes);
	IN_ButtonInfo (devid, 0, &db->num_buttons);
	db->axis_info = malloc (db->num_axes * sizeof (in_axisinfo_t)
							+ db->num_buttons * sizeof (in_buttoninfo_t));
	db->button_info = (in_buttoninfo_t *) &db->axis_info[db->num_axes];
	IN_AxisInfo (devid, db->axis_info, &db->num_axes);
	IN_ButtonInfo (devid, db->button_info, &db->num_buttons);

	db->axis_imt_id = -1;
	db->button_imt_id = -1;

	IN_SetDeviceEventData (devid, db);

	return;
in_connect_usage:
	Sys_Printf ("in_connect bindname device_id [fullid]\n");
	Sys_Printf ("   Create a new device binding connection.\n");
	Sys_Printf ("   bindname: Connection name used for binding inputs\n.");
	Sys_Printf ("   device_id: Specify the device to be connected.\n");
	Sys_Printf ("      May be the numeric device number (#N), the device\n");
	Sys_Printf ("      name or device id as shown by in_devices.\n");
	Sys_Printf ("   fullid: if present, both device name and device id\n");
	Sys_Printf ("      will be used when automatically reconnecting the\n");
	Sys_Printf ("      device.\n");
}

static void
in_connections_f (void)
{
	for (in_devbindings_t *db = devbindings_list; db; db = db->next) {
		if (db->match_id) {
			Sys_Printf ("%s: %s %s\n", db->name, db->devname, db->id);
		} else {
			Sys_Printf ("%s: %s\n", db->name, db->devname);
		}
		if (db->devid > -1) {
			Sys_Printf ("    connected\n");
		} else {
			Sys_Printf ("    disconnected\n");
		}
	}
}

static void
keyhelp_f (void)
{
	keyhelp_axis_threshold = 3;
	if (Cmd_Argc () > 1) {
		char       *end;
		int         threshold = strtol (Cmd_Argv (1), &end, 0);
		if (!*end && threshold > 0) {
			keyhelp_axis_threshold = threshold;
		}
	}
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
	{	"in_connect", in_connect_f,
		"Create a device binding connection. Supports hot-plug in that the "
		"device will be automatically reconnected when plugged in or"
		PACKAGE_NAME " is restarted."
	},
	{	"in_connections", in_connections_f,
		"List device bindings and statuses."
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
IN_Binding_Activate (void)
{
	IE_Set_Focus (in_binding_handler);
}

void
IN_Binding_Init (void)
{
	in_binding_handler = IE_Add_Handler (in_binding_event_handler, 0);
	in_keyhelp_handler = IE_Add_Handler (in_keyhelp_event_handler, 0);

	for (bindcmd_t *cmd = in_binding_commands; cmd->name; cmd++) {
		Cmd_AddCommand (cmd->name, cmd->func, cmd->desc);
	}
}
