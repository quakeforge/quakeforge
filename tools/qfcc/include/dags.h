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

#include "QF/progs/pr_comp.h"

#include "statements.h"

typedef struct dstring_s dstring_t;
typedef struct set_s set_t;
typedef struct flownode_s flownode_t;
typedef struct operand_s operand_t;
typedef struct dag_s dag_t;
typedef struct daglabel_s daglabel_t;
typedef struct dagnode_s dagnode_t;
typedef struct type_s type_t;

typedef struct daglabel_s {
	daglabel_t *next;
	daglabel_t *daglabel_chain;	///< all labels created for a dag
	int         number;			///< index into array of labels in dag_t
	bool        live:1;			///< accessed via an alias FIXME redundant?
	bool        not_src:1;		///< don't use attached identifier as source
	const char *opcode;			///< not if op
	operand_t  *op;				///< not if opcode;
	dagnode_t  *dagnode;		///< node with which this label is associated
	const struct expr_s *expr;	///< expression associated with this label
} daglabel_t;

typedef struct dagnode_s {
	dagnode_t  *next;
	int         number;			///< index into array of nodes in dag_t
	int         topo;			///< topological sort order
	set_t      *parents;		///< empty if root node
	int         cost;			///< cost of this node in temp vars
	dagnode_t  *killed;			///< node is unavailable for cse (by node)
	st_type_t   type;			///< type of node (st_none = leaf)
	daglabel_t *label;			///< ident/const if leaf node, or operator
	const type_t *vtype;		///< operand type
	operand_t  *value;	///< operand holding the value of this node
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
	dagnode_t  *children[3];
	const type_t *types[3];		///< desired type of each operand (to alias)
	set_t      *edges;			///< includes nodes pointed to by \a children
	int         offset;			///< for alias nodes
	//@}
	set_t      *identifiers;	///< set of identifiers attached to this node
	set_t      *reachable;		///< set of nodes reachable via edges (not
								///< parents) for ensuring cycles are not
								///< created
} dagnode_t;

typedef struct dag_s {
	dag_t      *next;
	dagnode_t **nodes;			///< array of all dagnodes in this dag
	int         num_nodes;
	int         killer_node;	///< last mass-killer node
	int        *topo;			///< nodes in topological sort order
	int         num_topo;		///< number of nodes in topo (may be <
								///< num_nodes after dead node removal)
	int         call_node;		///< number of last call node (for arg asign
								///< sequencing)
	int         num_labels;
	daglabel_t **labels;		///< array of all daglabels in this dag
	set_t      *roots;			///< set of root nodes
	set_t      *memory;			///< vars killed by memory write
	flownode_t *flownode;		///< flow node this dag represents
} dag_t;

const char *daglabel_string (daglabel_t *label);
void print_dag (dstring_t *dstr, dag_t *dag, const char *label);
void dot_dump_dag (void *_dag, const char *filename);

/** Make a dag for a single basic block.

	\param flownode	The flow graph node representing the basic block for which
					the dag will be created. The node should have its live
					variable information already computed.
	\return			The dag representing the basic block.
*/
dag_t *dag_create (flownode_t *flownode);

void dag_remove_dead_nodes (dag_t *dag);
void dag_generate (dag_t *dag, sblock_t *block);

///@}

#endif//dags_h
