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
#define TREE_PERM		1	// Permanent (part of a function)
#define TREE_CONCAT		2	// Concatenate to previous
#define TREE_EMBED	4	// Embedded command (expect return value)
#define TREE_P_EMBED	8	// Embedded stuff needs to be processed
#define TREE_P_MATH		16	// Math needs be evaluated
#define TREE_ASPLIT		32	// Token is the name of an array that should be split
#define TREE_FUNC 		64	// Node is the first in a function
#define TREE_COND		128 // Conditional jump (if or while command)
#define TREE_END	256 // Node ends a loop or conditional
#define TREE_FORNEXT		512 // For loop is starting again

typedef struct gib_tree_s {
	const char *str;
	char delim;
	struct gib_tree_s *children, *next, *parent, *jump;
	unsigned int flags, start, end, refs;
} gib_tree_t;

gib_tree_t *GIB_Tree_New (unsigned int flags);
void GIB_Tree_Free_Recursive (gib_tree_t *tree);
#endif /* __GIB_TREE_H */
