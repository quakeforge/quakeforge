/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2003 #AUTHOR#

	Author: #AUTHOR#
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

	$Id$
*/

#ifndef __GIB_TREE_H
#define __GIB_TREE_H

#define TREE_NORMAL		0	// Normal node
// Flags for tokens
#define TREE_CONCAT		1	// Concatenate to previous
#define TREE_P_EMBED	2	// Embedded stuff needs to be processed
#define TREE_SPLIT		4	// Token is the name of an array that should be split
// Flags for lines
#define TREE_COND		1 // Conditional jump (if or while command)
#define TREE_NOT		2 // Invert condition
#define TREE_END	4 // Node ends a loop or conditional
#define TREE_FORNEXT		8 // For loop is starting again
#define TREE_EMBED	16	// Embedded command (expect return value)

typedef char gib_tree_flags_t;

typedef struct gib_tree_s {
	const char *str;
	char delim;
	unsigned int start, end, refs;
	gib_tree_flags_t flags;
	struct gib_tree_s *children, *next, *jump;
} gib_tree_t;

gib_tree_t *GIB_Tree_New (gib_tree_flags_t flags);
void GIB_Tree_Free_Recursive (gib_tree_t *tree);
void GIB_Tree_Ref (gib_tree_t **tp);
void GIB_Tree_Unref (gib_tree_t **tp);
		
#endif /* __GIB_TREE_H */
