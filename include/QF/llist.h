/*
	llist.h

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

#ifndef _LLIST_H
#define _LLIST_H

#include "QF/qtypes.h"

/**	\defgroup llist Linked lists
	\ingroup utils
*/
//@{

typedef struct llist_node_s {
	struct llist_s *parent;		///< The list owning this node.
	struct llist_node_s *prev;	///< The previous node in the list, or null.
	struct llist_node_s *next;	///< The flowing node in the list, or null.
	void *data;					///< The actual list item.
} llist_node_t;

typedef struct llist_s {
	struct llist_node_s *start;	///< The first node in the list, or null.
	struct llist_node_s *end;	///< The last node in the list, or null.
	struct llist_node_s *iter;
	/// Function called when deleting a list item.
	/// \param element	The item being deleted.
	///	\param userdata	Pointer to user data supplied to llist_new().
	void (*freedata)(void *element, void *userdata);
	qboolean (*cmpdata)(const void *element, const void *comparison,
						void *userdata);
	void *userdata;
} llist_t;

typedef qboolean (*llist_iterator_t)(void *element, llist_node_t *node);

#define LLIST_ICAST(x) (llist_iterator_t)(x)
#define LLIST_DATA(node, type) ((type *)((node)->data))

/**	Create a new, empty, linked list.

	\param freedata	Function to call when deleting a list item.
	\param cmpdata	Function to call to compare two list items. It must
					return true when the items are the same and false when
					they differ.
	\param userdata	User data pointer. Set to whatever you want, it will be
					passed to the \a freedata and \a cmpdata functions as
					their final parameter.
	\return			Pointer to the list's control structure, which is to be
					passed to the other functions accessing the list.
*/
llist_t *llist_new (void (*freedata)(void *element, void *userdata),
					qboolean (*cmpdata)(const void *element,
										const void *comparison,
										void *userdata),
					void *userdata);

/** Empty a linked list.
	All of the items in the list will be deleted via the list's \a freedata
	function and the list will become empty.

	\param list		Pointer to the list's control structure created by
					llist_new(). May be null, in which case no operation is
					performed.
*/
void llist_flush (llist_t *list);

/** Delete a linked list.
	All of the items in the list will be deleted via the list's \a freedata
	function and the list will be destroyed. Do not attempt to use the list
	pointer after destroying the list.

	\param list		Pointer to the list's control structure created by
					llist_new(). May be null, in which case no operation is
					performed.
*/
void llist_delete (llist_t *list);
llist_node_t *llist_append (llist_t *list, void *element);
llist_node_t *llist_prefix (llist_t *list, void *element);
llist_node_t *llist_getnode (llist_t *list, void *element) __attribute__((pure));
llist_node_t *llist_insertafter (llist_node_t *ref, void *element);
llist_node_t *llist_insertbefore (llist_node_t *ref, void *element);
void *llist_remove (llist_node_t *ref);
unsigned int llist_size (llist_t *llist) __attribute__((pure));
void llist_iterate (llist_t *list, llist_iterator_t iterate);
void *llist_find (llist_t *list, void *comparison);
llist_node_t *llist_findnode (llist_t *list, void *comparison);
void *llist_createarray (llist_t *list, size_t esize);

//@}

#endif
