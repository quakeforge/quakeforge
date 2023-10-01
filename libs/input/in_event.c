/*
	in_event.c

	input event handling

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/8/9

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
#include <stdlib.h>

#include "QF/darray.h"

#include "QF/input/event.h"

#define IE_EVENT(event) #event,
const char *ie_event_names[] = {
#include "QF/input/event_names.h"
	0
};

typedef struct {
	ie_handler_t *handler;
	void       *data;
} ie_reghandler_t;

static struct DARRAY_TYPE (ie_reghandler_t) ie_handlers = { .grow = 8, };
static unsigned focus;

int
IE_Send_Event (const IE_event_t *event)
{
	if ((1 << event->type) & IE_broadcast_events) {
		for (size_t i = 0; i < ie_handlers.size; i++) {
			ie_reghandler_t *reg = &ie_handlers.a[i];
			if (reg->handler) {
				reg->handler (event, reg->data);
			}
		}
		return 1;
	}

	if (focus < ie_handlers.size && ie_handlers.a[focus].handler) {
		ie_reghandler_t *reg = &ie_handlers.a[focus];
		return reg->handler (event, reg->data);
	}
	return 0;
}

int
IE_Add_Handler (ie_handler_t *event_handler, void *data)
{
	size_t      handle;
	ie_reghandler_t reg = { event_handler, data };

	for  (handle = 0; handle < ie_handlers.size; handle++) {
		if (!ie_handlers.a[handle].handler) {
			ie_handlers.a[handle] = reg;
			return handle;
		}
	}
	DARRAY_APPEND (&ie_handlers, reg);
	return handle;
}

void
IE_Remove_Handler (int handle)
{
	if ((size_t) (ssize_t) handle < ie_handlers.size) {
		ie_handlers.a[handle].handler = 0;
	}
}

void
IE_Set_Focus (int handle)
{
	unsigned    h = handle;
	if (h < ie_handlers.size && ie_handlers.a[handle].handler && focus != h) {
		IE_event_t event;
		event.type = ie_lose_focus;
		IE_Send_Event (&event);
		focus = handle;
		event.type = ie_gain_focus;
		IE_Send_Event (&event);
	}
}

int
IE_Get_Focus (void)
{
	return focus;
}

static void
in_event_shutdown (void *data)
{
	Sys_MaskPrintf (SYS_input, "in_event_shutdown\n");
	DARRAY_CLEAR (&ie_handlers);
}

void
IN_Event_Init (void)
{
	Sys_RegisterShutdown (in_event_shutdown, 0);
}
