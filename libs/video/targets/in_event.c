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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/sys.h"
#include "QF/in_event.h"

#define IE_MAX_DEPTH 100

float ie_time;

void
IE_Threshold_Event (ie_event_t *event, float value)
{
	int i, total;
	ie_threshold_data_t *data = event->data.p;

	// add new value to the history
	while (data->history_count
		   && data->history[data->history_count - 1].time
			  < ie_time - data->time)
		data->history_count--;
	data->history_count++;
	data->history = realloc (data->history, data->history_count);
	if (!data->history)
		Sys_Error ("IE_Event: memory allocation failure!");
	data->history[data->history_count - 1].time = ie_time;
	data->history[data->history_count - 1].value = value;

	// total up the values in the history
	for (i = 0, total = 0; i < data->history_count; i++)
		total += data->history[i].value;

	// call the handler
	if (total >= data->threshold)
		IE_CallHandler (data->handler, data->nextevent, total);
}

void
IE_Translation_Event (ie_event_t *event, float value)
{
	ie_translation_data_t *data = event->data.p;
	ie_translation_index_t *index = data->index;
	ie_event_t *nextevent = 0;

	while (!nextevent) {
		if (!index)
			break;
		if (index->table->maxevents > data->offset)
			nextevent = &index->table->events[data->offset];
		index = index->next;
	}
	if (!nextevent) // no handler for it
		return;

	IE_CallHandler (nextevent->handler, nextevent, value);
}

void
IE_Multiplier_Event (ie_event_t *event, float value)
{
	ie_multiplier_data_t *data = event->data.p;
	IE_CallHandler (data->handler, data->nextevent, value * data->multiplier);
}

void
IE_CallHandler (ie_handler handler, ie_event_t *event, float value)
{
	static int depth = 0;
	depth++;
	if (depth > IE_MAX_DEPTH)
		// FIXME: we may want to just issue a warning here
		Sys_Error ("IE_CallHandler: max recursion depth hit");
	else
		handler (event, value);
	depth--;
}



/*
static int (**event_handler_list)(const IE_event_t*);
static int eh_list_size;
static int focus;
*/

void
IE_Init (void)
{
/*
	eh_list_size = 8;	// start with 8 slots. will grow dynamicly if needed
	event_handler_list = calloc (eh_list_size, sizeof (event_handler_list[0]));
*/
}

void
IE_Init_Cvars (void)
{
}

void
IE_Shutdown (void)
{
}

/*
int
IE_Send_Event (const IE_event_t *event)
{
	if (event_handler_list[focus])
		return event_handler_list[focus](event);
	return 0;
}

int
IE_Add_Handler (int (*event_handler)(const IE_event_t*))
{
	int     i;

	while (1) {
		int (**t)(const IE_event_t*);
		for (i = 0; i < eh_list_size; i++) {
			if (!event_handler_list[i]) {
				event_handler_list[i] = event_handler;
				return i;
			}
		}
		if (!(t = realloc (event_handler_list, eh_list_size + 8)))
			return -1;
		event_handler_list = t;
		memset (event_handler_list + eh_list_size, 0,
				8 * sizeof (event_handler_list[0]));
		eh_list_size += 8;
	}
}

void
IE_Remove_Handler (int handle)
{
	if (handle >= 0 && handle < eh_list_size)
		event_handler_list[handle] = 0;
}

void
IE_Set_Focus (int handle)
{
	if (handle >= 0 && handle < eh_list_size
		&& event_handler_list[handle]
		&& focus != handle) {
		IE_event_t event;
		event.type = ie_lose_focus;
		IE_Send_Event (&event);
		focus = handle;
		event.type = ie_gain_focus;
		IE_Send_Event (&event);
	}
}
*/
