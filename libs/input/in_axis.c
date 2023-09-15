/*
	in_axis.c

	Logical axis support

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/10/1

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

static hashtab_t *axis_tab;

static const char *
axis_get_key (const void *a, void *data)
{
	__auto_type axis = (const in_axis_t *) a;
	return axis->name;
}

static void
axis_free (void *a, void *data)
{
}

VISIBLE int
IN_RegisterAxis (in_axis_t *axis)
{
	const char *name = axis->name;
	if (Hash_Find (axis_tab, name)) {
		return 0;
	}

	Hash_Add (axis_tab, axis);
	return 1;
}

VISIBLE int
IN_UnregisterAxis (in_axis_t *axis)
{
	const char *name = axis->name;
	if (!Hash_Find (axis_tab, name)) {
		return 0;
	}

	Hash_Free (axis_tab, Hash_Del (axis_tab, name));
	return 1;
}

VISIBLE in_axis_t *
IN_FindAxis (const char *name)
{
	return Hash_Find (axis_tab, name);
}

static void
axis_clear_state (void *_a, void *data)
{
	//FIXME what to do here?
}

void
IN_AxisClearStates (void)
{
	Hash_ForEach (axis_tab, axis_clear_state, 0);
}

void
IN_AxisAddListener (in_axis_t *axis, axis_listener_t listener, void *data)
{
	if (!axis->listeners) {
		axis->listeners = malloc (sizeof (*axis->listeners));
		LISTENER_SET_INIT (axis->listeners, 8);
	}
	LISTENER_ADD (axis->listeners, listener, data);
}

void
IN_AxisRemoveListener (in_axis_t *axis, axis_listener_t listener, void *data)
{
	if (axis->listeners) {
		LISTENER_REMOVE (axis->listeners, listener, data);
	}
}

static void
in_axis_shutdown (void *data)
{
	Hash_DelTable (axis_tab);
}

void
IN_AxisInit (void)
{
	axis_tab = Hash_NewTable (127, axis_get_key, axis_free, 0, 0);
	Sys_RegisterShutdown (in_axis_shutdown, 0);
}
