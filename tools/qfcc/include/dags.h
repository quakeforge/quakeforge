/*
	dags.h

	DAG representation of basic blocks

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/05/08

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
#ifndef dags_h
#define dags_h

typedef struct daglabel_s {
	struct daglabel_s *next;
	const char *opcode;			///< not if op
	struct operand_s *op;		///< not if opcode;
} daglabel_t;

typedef struct dagnode_s {
	struct dagnode_s *next;
	daglabel_t *label;			///< ident/const if leaf node, or operator
	struct dagnode_s *left;		///< null if leaf node
	struct dagnode_s *right;	///< null if unary op
	daglabel_t *identifiers;	///< list of identifiers with value of this node
} dagnode_t;

const char *daglabel_string (daglabel_t *label);
void print_dag (dagnode_t *node, const char *filename);

#endif//dags_h
