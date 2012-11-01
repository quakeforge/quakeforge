/*
	flow.h

	Flow graph analysis.

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/10/30

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
#ifndef flow_h
#define flow_h

/** \defgroup qfcc_flow Flow graph analysis
	\ingroup qfcc
*/
//@{

struct function_s;
struct sblock_s;
struct statement_s;

typedef struct flowloop_s {
	struct flowloop_s *next;
	unsigned    head;
	struct set_s *nodes;
} flowloop_t;

typedef struct flowedge_s {
	unsigned    tail;		//< node index
	unsigned    head;		//< successor index
} flowedge_t;

/** Represent a node in a flow graph.

	With the \a siblings and \a num_siblings fields, the entire graph can be
	accessed via any node within that graph.
*/
typedef struct flownode_s {
	struct flownode_s *next;
	unsigned    id;
	int         num_pred;
	unsigned   *predecessors;	//< indices into siblings
	int         num_succ;
	unsigned   *successors;		//< indices into siblings
	unsigned    num_nodes;		//< number of nodes or sblocks
	unsigned    region;			//< the region of which this node is a member
	unsigned   *depth_first;	//< indices into siblings in depth-first order
	/// \name Node pointers.
	/// Only one of \a sblocks or \a nodes will be non-null. If \a sblocks is
	/// non-null, then this is an innermost flow-node, otherwise \a nodes
	/// will be non-null and this is a higher level flow-node representing a
	/// region.
	///
	/// \a sblocks points to the array of all sblocks in the function
	/// (it is a copy of function_t's graph member). \a id acts as an index
	/// into \a sblocks to identify the sblock associated with this node.
	///
	/// \a nodes is an array of all flow-nodes nested within the region
	/// represented by this node.
	///
	/// \a siblings is an array of all flow-nodes contained within the same
	/// region as this node.
	//@{
	struct sblock_s **sblocks;
	struct flownode_s **nodes;
	struct flownode_s **siblings;
	unsigned    num_siblings;
	//@}
	struct set_s *dom;
} flownode_t;

int flow_is_cond (struct statement_s *s);
int flow_is_goto (struct statement_s *s);
int flow_is_return (struct statement_s *s);
struct sblock_s *flow_get_target (struct statement_s *s);
void flow_build_vars (struct function_s *func);
void flow_build_graph (struct function_s *func);
void print_flowgraph (flownode_t *flow, const char *filename);

//@}

#endif//flow_h
