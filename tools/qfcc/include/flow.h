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
///@{

struct function_s;
struct sblock_s;
struct statement_s;
struct operand_s;

typedef struct flowvar_s {
	struct flowvar_s *next;		///< for ALLOC
	struct set_s *use;			///< set of statements that use this var
	struct set_s *define;		///< set of statements that define this var
	struct set_s *udchains;		///< set of ud-chains for this var
	struct set_s *duchains;		///< set of du-chains for this var
	struct operand_s *op;		///< an operand using this var
	int         number;			///< number of variable in func's ref list
	int         flowaddr;		///< psuedo address for local and temp vars
	int         minsize;		///< minimum size (via memset/move)
} flowvar_t;

typedef struct udchain_s {
	int         var;
	int         usest;
	int         defst;
} udchain_t;

typedef struct flowloop_s {
	struct flowloop_s *next;
	unsigned    head;
	struct set_s *nodes;
} flowloop_t;

typedef struct flowedge_s {
	unsigned    tail;
	unsigned    head;
} flowedge_t;

/** Represent a node in a flow graph.
*/
typedef struct flownode_s {
	struct flownode_s *next;	///< for ALLOC
	int         id;				///< index of this node in the flow graph
	int         dfn;			///< depth-first ordering of this node
	struct flowgraph_s *graph;	///< graph owning this node
	struct set_s *predecessors;	///< predecessors of this node
	struct set_s *successors;	///< successors of this node
	struct set_s *edges;		///< edges leaving this node to successor nodes
	struct set_s *dom;			///< dominating nodes
	struct set_s *global_vars;	///< global vars used by function
	struct {
		struct set_s *gen;
		struct set_s *kill;
		struct set_s *in;
		struct set_s *out;
	}           reaching_defs;
	struct {
		struct set_s *use;
		struct set_s *def;
		struct set_s *in;
		struct set_s *out;
	}           live_vars;
	struct sblock_s *sblock;	///< original statement block
	struct dag_s *dag;			///< dag for this node
} flownode_t;

typedef struct flowgraph_s {
	struct flowgraph_s *next;	///< for ALLOC
	struct function_s *func;	///< function to which this graph is attached
	flownode_t **nodes;			///< array of nodes in the graph
	int         num_nodes;		///< number of real nodes in the graph
	flowedge_t *edges;			///< array of all edges in the graph
	int         num_edges;
	struct set_s *dfst;			///< edges in the depth-first search tree
	int        *depth_first;	///< depth-first order of nodes
	flowloop_t *loops;			///< linked list of natural loops
} flowgraph_t;

flowvar_t *flow_get_var (struct operand_s *op);

#define FLOW_OPERANDS 5
void flow_analyze_statement (struct statement_s *s, struct set_s *use,
							 struct set_s *def, struct set_s *kill,
							 struct operand_s *operands[FLOW_OPERANDS]);

void flow_data_flow (struct function_s *func);

void dump_dot_flow (void *g, const char *filename);
void dump_dot_flow_dags (void *g, const char *filename);
void dump_dot_flow_live (void *g, const char *filename);
void dump_dot_flow_reaching (void *g, const char *filename);
void dump_dot_flow_statements (void *g, const char *filename);

///@}

#endif//flow_h
