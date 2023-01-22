/*
	entity.c

	Entity management

	Copyright (C) 2022 Bill Currke

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

#include "QF/set.h"

#define IMPLEMENT_ENTITY_Funcs

#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

entqueue_t *
EntQueue_New (int num_queues)
{
	int         size = sizeof (entqueue_t) + num_queues * sizeof (entityset_t);
	entqueue_t *queue = calloc (1, size);
	queue->queued_ents = set_new ();
	queue->ent_queues = (entityset_t *) (queue + 1);
	queue->num_queues = num_queues;
	for (int i = 0; i < num_queues; i++) {
		queue->ent_queues[i].grow = 64;
	}
	return queue;
}

void
EntQueue_Delete (entqueue_t *queue)
{
	for (int i = 0; i < queue->num_queues; i++) {
		DARRAY_CLEAR (&queue->ent_queues[i]);
	}
	set_delete (queue->queued_ents);
	free (queue);
}

void
EntQueue_Clear (entqueue_t *queue)
{
	for (int i = 0; i < queue->num_queues; i++) {
		queue->ent_queues[i].size = 0;
	}
	set_empty (queue->queued_ents);
}
