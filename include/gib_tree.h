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

*/

#ifndef __GIB_TREE_H
#define __GIB_TREE_H

#define TREE_NORMAL		0	// Normal node
// Flags for arguments
#define TREE_A_CONCAT		1	// Concatenate to previous
#define TREE_A_EMBED		2	// Embedded stuff needs to be processed
#define TREE_A_EXPAND		4	// Token needs to be expanded
// Flags for lines
#define TREE_L_NOT		1	// Invert condition
#define TREE_L_FORNEXT		4	// For loop is starting again
#define TREE_L_EMBED		8	// Embedded command (expect return value)

typedef char gib_tree_flags_t;

typedef struct gib_tree_s {
	const char *str;
	char delim;
	unsigned int start, end, refs;
	gib_tree_flags_t flags;
	enum gib_tree_type_e {
		TREE_T_CMD, // A command
		TREE_T_COND, // Conditional jump
		TREE_T_ASSIGN, // Assignment
		TREE_T_SEND, // Message sending
		TREE_T_JUMP, // Jump
		TREE_T_ARG, // Argument (not a line)
		TREE_T_FORNEXT, // Fetch next arg in for loop
		TREE_T_META, // Info node
		TREE_T_LABEL // Label (or no-op)
	} type;
	struct gib_tree_s *children, *next, *jump;
} gib_tree_t;

gib_tree_t *GIB_Tree_New (enum gib_tree_type_e type);
void GIB_Tree_Free_Recursive (gib_tree_t *tree);
void GIB_Tree_Ref (gib_tree_t **tp);
void GIB_Tree_Unref (gib_tree_t **tp);

#endif /* __GIB_TREE_H */
