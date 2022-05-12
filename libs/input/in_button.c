/*
	in_button.c

	Logical button support

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/09/29

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

typedef struct regbutton_s {
	in_button_t *button;
	char       *press_cmd;
	char       *release_cmd;
} regbutton_t;

static hashtab_t *button_tab;

static const char *
button_get_key (const void *b, void *data)
{
	__auto_type regbutton = (const regbutton_t *) b;
	return regbutton->button->name;
}

static void
button_free (void *b, void *data)
{
	regbutton_t *rb = b;
	if (rb->button->listeners) {
		DARRAY_CLEAR (rb->button->listeners);
		free (rb->button->listeners);
	}
	free (rb);
}

static void
button_press (in_button_t *button, int id)
{
	if (id == button->down[0] || id == button->down[1]) {
		// repeating key
		return;
	}

	if (!button->down[0]) {
		button->down[0] = id;
	} else if (!button->down[1]) {
		button->down[1] = id;
	} else {
		Sys_Printf ("Three keys down for a button!\n");
		return;
	}

	if (button->state & inb_down) {
		// still down
		return;
	}
	button->state |= inb_down | inb_edge_down;
	if (button->listeners) {
		LISTENER_INVOKE (button->listeners, button);
	}
}

static void
button_release (in_button_t *button, int id)
{
	if (id == -1) {
		// typed manually at the console, assume for unsticking, so clear all
		button->down[0] = button->down[1] = 0;
		button->state = inb_edge_up;
		return;
	}

	if (button->down[0] == id) {
		button->down[0] = 0;
	} else if (button->down[1] == id) {
		button->down[1] = 0;
	} else {
		// key up without coresponding down (menu pass through)
		return;
	}
	if (button->down[0] || button->down[1]) {
		// some other key is still holding it down
		return;
	}

	if (!(button->state & inb_down)) {
		// still up (this should not happen)
		return;
	}
	button->state &= ~inb_down;			// now up
	button->state |= inb_edge_up;
	if (button->listeners) {
		LISTENER_INVOKE (button->listeners, button);
	}
}

void
IN_ButtonAction (in_button_t *button, int id, int pressed)
{
	if (pressed) {
		button_press (button, id);
	} else {
		button_release (button, id);
	}
}


static void
button_press_cmd (void *_b)
{
	in_button_t *button = _b;
	const char *idstr = Cmd_Argv (1);
	// assume typed manually at the console for continuous down
	int         id = -1;

	if (idstr[0]) {
		id = atoi (idstr);
	}
	button_press (button, id);
}

static void
button_release_cmd (void *_b)
{
	in_button_t *button = _b;
	const char *idstr = Cmd_Argv (1);
	// assume typed manually at the console, probably for unsticking
	int         id = -1;

	if (idstr[0]) {
		id = atoi (idstr);
	}
	button_release (button, id);
}

VISIBLE int
IN_RegisterButton (in_button_t *button)
{
	const char *name = button->name;
	if (Hash_Find (button_tab, name)) {
		return 0;
	}
	size_t      size = strlen (name) + 2;
	regbutton_t *regbutton = malloc (sizeof (regbutton_t) + 2 * size);
	regbutton->button = button;

	regbutton->press_cmd = (char *) (regbutton + 1);
	regbutton->release_cmd = regbutton->press_cmd + size;
	*regbutton->press_cmd = '+';
	*regbutton->release_cmd = '-';
	strcpy (regbutton->press_cmd + 1, name);
	strcpy (regbutton->release_cmd + 1, name);

	Cmd_AddDataCommand (regbutton->press_cmd, button_press_cmd, button,
						"Set the button's state to on/pressed.");
	Cmd_AddDataCommand (regbutton->release_cmd, button_release_cmd, button,
						"Set the button's state to off/released.");

	Hash_Add (button_tab, regbutton);
	return 1;
}

in_button_t *
IN_FindButton (const char *name)
{
	regbutton_t *regbutton = Hash_Find (button_tab, name);
	if (regbutton) {
		return regbutton->button;
	}
	return 0;
}

void
IN_ButtonAddListener (in_button_t *button, button_listener_t listener,
					  void *data)
{
	if (!button->listeners) {
		button->listeners = malloc (sizeof (*button->listeners));
		LISTENER_SET_INIT (button->listeners, 8);
	}
	LISTENER_ADD (button->listeners, listener, data);
}

void
IN_ButtonRemoveListener (in_button_t *button, button_listener_t listener,
						 void *data)
{
	if (button->listeners) {
		LISTENER_REMOVE (button->listeners, listener, data);
	}
}

static void
in_button_shutdown (void *data)
{
	Hash_DelTable (button_tab);
}

static void __attribute__((constructor))
in_button_init (void)
{
	button_tab = Hash_NewTable (127, button_get_key, button_free, 0, 0);
	Sys_RegisterShutdown (in_button_shutdown, 0);
}
