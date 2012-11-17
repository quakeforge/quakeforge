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

/** \defgroup qfcc_dags DAG building
	\ingroup qfcc
*/
//@{

#include "QF/pr_comp.h"

#include "statements.h"

struct dstring_s;
struct flownode_s;

typedef struct daglabel_s {
	/// \name attached identifer linked list
	//@{
	struct daglabel_s *next;
	struct daglabel_s **prev;
	//@}
	struct daglabel_s *daglabel_chain;	///< all labels created for a dag
	const char *opcode;			///< not if op
	struct operand_s *op;		///< not if opcode;
	struct dagnode_s *dagnode;	///< node with which this label is associated
	struct expr_s *expr;		///< expression associated with this label
} daglabel_t;

typedef struct dagnode_s {
	struct dagnode_s *next;
	int         print_count;	///< used to avoid double printing nodes
	int         is_child;		///< true if a child node
	int         cost;			///< cost of this node in temp vars
	st_type_t   type;			///< type of node (st_node = leaf)
	daglabel_t *label;			///< ident/const if leaf node, or operator
	etype_t     tl;
	/// \name child nodes
	/// All three child nodes will be null if this node is a leaf
	/// If \a a is null, both \a b and \a c must also be null. If \a is not
	/// null, either \a b or \a c or even both may be non-null. Both \a b and
	/// \a c being non-null is reserved for the few opcodes that take three
	/// inputs (rcall2+, 3 op state, indirect move, indexed pointer assignment)
	/// \a b and \a c are used to help keep track of the original statement
	/// operands
	//@{
	struct dagnode_s *a, *b, *c;
	etype_t     ta, tb, tc;		///< desired type of each operand (to alias)
	//@}
	daglabel_t *identifiers;	///< list of identifiers with value of this node
} dagnode_t;

const char *daglabel_string (daglabel_t *label);
void print_dag (struct dstring_s *dstr, dagnode_t *node);

/** Make a dag for a single basic block.

	\param node		The flow graph node representing the basic block for which
					the dag will be created. The node should have its live
					variable information already computed.
	\return			The dag representing the basic block.
*/
dagnode_t *dag_create (const struct flownode_s *flownode);

void dag_generate (sblock_t *block, const struct flownode_s *flownode);

//@}

#endif//dags_h
