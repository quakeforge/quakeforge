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
///@{

#include "QF/pr_comp.h"

#include "statements.h"

struct dstring_s;
struct flownode_s;

typedef struct daglabel_s {
	struct daglabel_s *next;
	struct daglabel_s *daglabel_chain;	///< all labels created for a dag
	int         number;			///< index into array of labels in dag_t
	unsigned    live:1;			///< accessed via an alias FIXME redundant?
	const char *opcode;			///< not if op
	struct operand_s *op;		///< not if opcode;
	struct dagnode_s *dagnode;	///< node with which this label is associated
	struct expr_s *expr;		///< expression associated with this label
} daglabel_t;

typedef struct dagnode_s {
	struct dagnode_s *next;
	int         number;			///< index into array of nodes in dag_t
	int         topo;			///< topological sort order
	struct set_s *parents;		///< empty if root node
	int         cost;			///< cost of this node in temp vars
	unsigned    killed:1;		///< node is unavailable for cse
	st_type_t   type;			///< type of node (st_none = leaf)
	daglabel_t *label;			///< ident/const if leaf node, or operator
	struct type_s *tl;
	struct operand_s *value;	///< operand holding the value of this node
	/// \name child nodes
	/// if \a children[0] is null, the rest must be null as well. Similar for
	///	\a children[1].
	///
	/// \a edges is the set of all nodes upon which this node depends. ie,
	/// they must be evaluated before this node is evaluted. So while nodes
	/// in \a edges may not be true children of this node, they are effective
	/// children in the DAG. That is, \a edges is for producing a correct
	/// topological sort of the DAG.
	//@{
	struct dagnode_s *children[3];
	struct type_s *types[3];	///< desired type of each operand (to alias)
	struct set_s *edges;		///< includes nodes pointed to by \a children
	//@}
	struct set_s *identifiers;	///< set of identifiers attached to this node
} dagnode_t;

typedef struct dag_s {
	struct dag_s *next;
	dagnode_t **nodes;			///< array of all dagnodes in this dag
	int         num_nodes;
	int        *topo;			///< nodes in topological sort order
	int         num_topo;		///< number of nodes in topo (may be <
								///< num_nodes after dead node removal)
	daglabel_t **labels;		///< array of all daglabels in this dag
	int         num_labels;;
	struct set_s *roots;		///< set of root nodes
	struct flownode_s *flownode;///< flow node this dag represents
} dag_t;

const char *daglabel_string (daglabel_t *label);
void print_dag (struct dstring_s *dstr, dag_t *dag, const char *label);
void dot_dump_dag (void *_dag, const char *filename);

/** Make a dag for a single basic block.

	\param flownode	The flow graph node representing the basic block for which
					the dag will be created. The node should have its live
					variable information already computed.
	\return			The dag representing the basic block.
*/
dag_t *dag_create (struct flownode_s *flownode);

void dag_remove_dead_nodes (dag_t *dag);
void dag_generate (dag_t *dag, sblock_t *block);

///@}

#endif//dags_h
