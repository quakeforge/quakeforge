/*
	gib_tree.c

	GIB tree handling functions

	Copyright (C) 2003 Brian Koropoff

	Author: Brian Koropoff
	Date: #DATE#

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

#include <stdlib.h>
#include <string.h>

#include "QF/sys.h"
#include "QF/qtypes.h"
#include "gib_tree.h"

gib_tree_t *
GIB_Tree_New (enum gib_tree_type_e type)
{
	gib_tree_t *new = calloc (1, sizeof (gib_tree_t));

	new->type = type;
	// All nodes are created for a reason, so start with 1 ref
	new->refs = 1;
	return new;
}

void
GIB_Tree_Free_Recursive (gib_tree_t * tree)
{
	gib_tree_t *n;

	if (tree->refs)
		return;
	for (; tree; tree = n) {
		n = tree->next;
		if (tree->children)
			// Parent is about to bite the dust, meaning one less reference
			GIB_Tree_Unref (&tree->children);
		if (tree->str)
			free ((void *) tree->str);
		free (tree);
	}
}

void
GIB_Tree_Ref (gib_tree_t ** tp)
{
	(*tp)->refs++;
//	Sys_MaskPrintf (SYS_dev, "Ref: %p %u\n", *tp, (*tp)->refs);
}

void
GIB_Tree_Unref (gib_tree_t ** tp)
{
//	Sys_MaskPrintf (SYS_dev, "Unref: %p %u\n", *tp, (*tp)->refs - 1);
	if (!(--(*tp)->refs)) {
		GIB_Tree_Free_Recursive (*tp);
		*tp = 0;
	}
}
