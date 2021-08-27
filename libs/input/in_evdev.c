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
#include "QF/sys.h"

#include "compat.h"
#include "evdev/inputlib.h"

static void
in_evdev_keydest_callback (keydest_t key_dest, void *data)
{
}

static void
in_evdev_process_events (void)
{
	if (inputlib_check_input ()) {
	}
}

static void
in_evdev_shutdown (void)
{
	inputlib_close ();
}

static void
device_add (device_t *dev)
{
}

static void
device_remove (device_t *dev)
{
}

static void
in_evdev_init (void)
{
	Key_KeydestCallback (in_evdev_keydest_callback, 0);

	inputlib_init (device_add, device_remove);
}

static void
in_evdev_clear_states (void)
{
}

static in_driver_t in_evdev_driver = {
	.init = in_evdev_init,
	.shutdown = in_evdev_shutdown,
	.process_events = in_evdev_process_events,
	.clear_states = in_evdev_clear_states,
};

static void __attribute__((constructor))
in_evdev_register_driver (void)
{
	IN_RegisterDriver (&in_evdev_driver);
}

int in_evdev_force_link;
