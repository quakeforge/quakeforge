/*
	llist.c

	Linked list functions

	Copyright (C) 2003 Brian Koropoff

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

#include "QF/llist.h"

static llist_node_t *
llist_newnode (llist_t *list, void *data)
{
	llist_node_t *node = calloc (1, sizeof (llist_node_t));
	node->parent = list;
	node->data = data;

	return node;
}

VISIBLE llist_t *
llist_new (void (*freedata)(void *element, void *userdata), bool (*cmpdata)(const void *element, const void *comparison, void *userdata), void *userdata)
{
	llist_t *new = calloc (1, sizeof (llist_t));

	new->freedata = freedata;
	new->cmpdata = cmpdata;
	new->userdata = userdata;

	return new;
}

VISIBLE void
llist_flush (llist_t *list)
{
	llist_node_t *node, *next;

	if (!list)
		return;

	for (node = list->start; node; node = next) {
		next = node->next;
		list->freedata (node->data, list->userdata);
		free (node);
	}
	list->start = list->end = 0;
}

VISIBLE void
llist_delete (llist_t *list)
{
	if (!list)
		return;

	llist_flush (list);
	free (list);
}

VISIBLE llist_node_t *
llist_append (llist_t *list, void *element)
{
	llist_node_t *node = llist_newnode (list, element);

	if (!list)
		return 0;

	if (list->end) {
		list->end->next = node;
		node->prev = list->end;
		list->end = node;
	} else
		list->start = list->end = node;
	return node;
}

VISIBLE llist_node_t *
llist_prefix (llist_t *list, void *element)
{
	llist_node_t *node = llist_newnode (list, element);

	if (!list)
		return 0;

	if (list->start) {
		list->start->prev = node;
		node->next = list->start;
		list->start = node;
	} else
		list->start = list->end = node;

	return node;
}

VISIBLE llist_node_t *
llist_getnode (llist_t *list, void *element)
{
	llist_node_t *node;

	if (!list)
		return 0;

	for (node = list->start; node; node = node->next)
		if (node->data == element)
			return node;
	return 0;
}

VISIBLE llist_node_t *
llist_insertafter (llist_node_t *ref, void *element)
{
	llist_node_t *node = llist_newnode (ref->parent, element);

	if (!ref)
		return 0;

	if (ref->next)
		ref->next->prev = node;
	else
		ref->parent->end = node;
	node->prev = ref;
	node->next = ref->next;
	ref->next = node;

	return node;
}

VISIBLE llist_node_t *
llist_insertbefore (llist_node_t *ref, void *element)
{
	llist_node_t *node = llist_newnode (ref->parent, element);

	if (!ref)
		return 0;

	if (ref->prev)
		ref->prev->next = node;
	else
		ref->parent->start = node;
	node->next = ref;
	node->prev = ref->prev;
	ref->prev = node;

	return node;
}

VISIBLE void *
llist_remove (llist_node_t *ref)
{
	void *element;

	if (!ref)
		return 0;

	if (ref == ref->parent->iter)
		ref->parent->iter = ref->next;
	if (ref->prev)
		ref->prev->next = ref->next;
	else
		ref->parent->start = ref->next;
	if (ref->next)
		ref->next->prev = ref->prev;
	else
		ref->parent->end = ref->prev;

	element = ref->data;
	free (ref);
	return element;
}

VISIBLE unsigned int
llist_size (llist_t *llist)
{
	unsigned int i;
	llist_node_t *n;

	if (!llist->start)
		return 0;
	else for (i = 0, n = llist->start; n; n = n->next, i++);

	return i;
}

VISIBLE void
llist_iterate (llist_t *list, llist_iterator_t iterate)
{
	llist_node_t *node;

	if (!list || !list->start)
		return;
	for (node = list->start; node; node = list->iter) {
		list->iter = node->next;
		if (!iterate (node->data, node))
			break;
	}
	list->iter = 0;
}

VISIBLE void *
llist_find (llist_t *list, void *comparison)
{
	llist_node_t *node;

	if (!list)
		return 0;
	for (node = list->start; node; node = node->next)
		if (list->cmpdata (node->data, comparison, list->userdata))
			return node->data;
	return 0;
}

VISIBLE llist_node_t *
llist_findnode (llist_t *list, void *comparison)
{
	llist_node_t *node;

	if (!list || !list->cmpdata)
		return 0;
	for (node = list->start; node; node = node->next)
		if (list->cmpdata (node->data, comparison, list->userdata))
			return node;
	return 0;
}

VISIBLE void *
llist_createarray (llist_t *list, size_t esize)
{
	void *ptr, *array = malloc (llist_size (list) * esize);
	llist_node_t *node;

	for (ptr = array, node = list->start; node; node = node->next, ptr = (char*)ptr + esize)
		memcpy (ptr, node->data, esize);

	return array;
}
