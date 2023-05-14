/*
	flow.c

	Flow graph analysis

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/set.h"
#include "QF/va.h"

#include "tools/qfcc/include/dags.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/dot.h"
#include "tools/qfcc/include/flow.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

/// \addtogroup qfcc_flow
///@{

/**	Static operand definitions for the ever present return and parameter slots.
 */
static struct {
	const char *name;
	operand_t   op;
} flow_params[] = {
	{".return",		{0, op_def}},
	{".param_0",	{0, op_def}},
	{".param_1",	{0, op_def}},
	{".param_2",	{0, op_def}},
	{".param_3",	{0, op_def}},
	{".param_4",	{0, op_def}},
	{".param_5",	{0, op_def}},
	{".param_6",	{0, op_def}},
	{".param_7",	{0, op_def}},
};
static const int num_flow_params = sizeof(flow_params)/sizeof(flow_params[0]);

/**	\name Flow analysis memory management */
///@{
ALLOC_STATE (flowvar_t, vars);			///< flowvar pool
ALLOC_STATE (flowloop_t, loops);		///< flow loop pool
ALLOC_STATE (flownode_t, nodes);		///< flow node pool
ALLOC_STATE (flowgraph_t, graphs);		///< flow graph pool

/**	Allocate a new flow var.
 *
 *	The var's use and define sets are initialized to empty.
 */
static flowvar_t *
new_flowvar (void)
{
	flowvar_t *var;
	ALLOC (256, flowvar_t, vars, var);
	var->use = set_new ();
	var->define = set_new ();
	return var;
}

/**	Delete a flow var
 */
static void
delete_flowvar (flowvar_t *var)
{
	set_delete (var->use);
	set_delete (var->define);
	FREE (vars, var);
}

/**	Allocate a new flow loop.
 *
 *	The loop's nodes set is initialized to the empty set.
 */
static flowloop_t *
new_loop (void)
{
	flowloop_t *loop;
	ALLOC (256, flowloop_t, loops, loop);
	loop->nodes = set_new ();
	return loop;
}

/** Free a flow loop and its nodes set.
 */
static void
delete_loop (flowloop_t *loop)
{
	set_delete (loop->nodes);
	FREE (loops, loop);
}

/**	Allocate a new flow node.
 *
 *	The node is completely empty.
 */
static flownode_t *
new_node (void)
{
	flownode_t *node;
	ALLOC (256, flownode_t, nodes, node);
	return node;
}

/**	Free a flow node and its resources.
 *
 *	\bug not global_vars or the vars and defs sets?
 */
static void
delete_node (flownode_t *node)
{
	if (node->predecessors)
		set_delete (node->predecessors);
	if (node->successors)
		set_delete (node->successors);
	if (node->edges)
		set_delete (node->edges);
	if (node->dom)
		set_delete (node->dom);
	FREE (nodes, node);
}

/**	Allocate a new flow graph.
 *
 *	The graph is completely empty.
 */
static flowgraph_t *
new_graph (void)
{
	flowgraph_t *graph;
	ALLOC (256, flowgraph_t, graphs, graph);
	return graph;
}

/**	Return a flow graph and its resources to the pools.
 *
 *	\bug except loops?
 */
static void __attribute__((unused))
delete_graph (flowgraph_t *graph)
{
	int         i;

	if (graph->nodes) {
		for (i = 0; i < graph->num_nodes; i++)
			delete_node (graph->nodes[i]);
		free (graph->nodes);
	}
	if (graph->edges)
		free (graph->edges);
	if (graph->dfst)
		set_delete (graph->dfst);
	if (graph->depth_first)
		free (graph->depth_first);
	FREE (graphs, graph);
}
///@}

/**	\name Flowvar classification */
///@{
/**	Check if the flowvar refers to a global variable.
 *
 *	For the flowvar to refer to a global variable, the flowvar's operand
 *	must be a def operand (but the def itself may be an alias of the real def)
 *	and the rel def must not have its def_t::local flag set. This means that
 *	function-scope static variables are not considered local (ie, only
 *	non-static function-scope variables and function parameters are considered
 *	local (temp vars are local too, but are not represented by \a op_def)).
 */
static int
flowvar_is_global (flowvar_t *var)
{
	def_t      *def;

	if (var->op->op_type != op_def)
		return 0;
	def = var->op->def;
	if (def->alias)
		def = def->alias;
	if (def->local)
		return 0;
	return 1;
}

/**	Check if the flowvar refers to a function parameter.
 *
 *	For the flowvar to refer to a function parameter, the flowvar's operand
 *	must be a def operand (but the def itself may be an alias of the real def)
 *	and the real def must have both its def_t::local and def_t::param flags
 *	set.
 *
 *	Temp vars are are not represented by op_def, so no mistake can be made.
 */
static int
flowvar_is_param (flowvar_t *var)
{
	def_t      *def;

	if (var->op->op_type != op_def)
		return 0;
	def = var->op->def;
	if (def->alias)
		def = def->alias;
	if (!def->local)
		return 0;
	if (!def->param)
		return 0;
	return 1;
}

/**	Check if the flowvar refers to a function argument.
 *
 *	For the flowvar to refer to a function argument, the flowvar's operand
 *	must be a def operand (but the def itself may be an alias of the real def)
 *	and the real def must have both its def_t::local and def_t::argument flags
 *	set.
 *
 *	Temp vars are are not represented by op_def, so no mistake can be made.
 */
static int
flowvar_is_argument (flowvar_t *var)
{
	def_t      *def;

	if (var->op->op_type != op_def)
		return 0;
	def = var->op->def;
	if (def->alias)
		def = def->alias;
	if (!def->local)
		return 0;
	if (!def->argument)
		return 0;
	return 1;
}

/**	Check if the flowvar refers to a local variable.
 *
 *	As this is simply "neither global nor pamam nor argument", all other
 *	flowvars are considered local, in particular actual non-static function
 *	scope variables and temp vars.
 */
static int
flowvar_is_local (flowvar_t *var)
{
	return !(flowvar_is_global (var) || flowvar_is_param (var)
			 || flowvar_is_argument (var));
}
///@}

/**	Extract the def from a def or temp flowvar.
 *
 *	It is an error for the operand referenced by the flowvar to be anything
 *	other than a real def or temp.
 */
static __attribute__((pure)) def_t *
flowvar_get_def (flowvar_t *var)
{
	operand_t  *op = var->op;

	switch (op->op_type) {
		case op_def:
			return op->def;
		case op_value:
		case op_label:
			return 0;
		case op_temp:
			return op->tempop.def;
		case op_alias:
			internal_error (op->expr, "unexpected alias operand");
		case op_nil:
			internal_error (op->expr, "unexpected nil operand");
		case op_pseudo:
			internal_error (op->expr, "unexpected pseudo operand");
	}
	internal_error (op->expr, "oops, blue pill");
	return 0;
}

/**	Get a def or temp var operand's flowvar.
 *
 *	Other operand types never have a flowvar.
 *
 *	If the operand does not yet have a flowvar, one is created and assigned
 *	to the operand.
 */
flowvar_t *
flow_get_var (operand_t *op)
{
	if (!op)
		return 0;

	if (op->op_type == op_temp) {
		if (!op->tempop.flowvar)
			op->tempop.flowvar = new_flowvar ();
		return op->tempop.flowvar;
	}
	if (op->op_type == op_def) {
		if (!op->def->flowvar)
			op->def->flowvar = new_flowvar ();
		return op->def->flowvar;
	}
	if (op->op_type == op_pseudo) {
		if (!op->pseudoop->flowvar)
			op->pseudoop->flowvar = new_flowvar ();
		return op->pseudoop->flowvar;
	}
	return 0;
}

/**	Indicate whether the operand should be counted.
 *
 *	If the operand is a def or temp var operand, and it has not already been
 *	counted, then it is counted, otherwise it is not.
 *	\return		1 if the operand should be counted, 0 if not
 */
static int
count_operand (operand_t *op)
{
	flowvar_t  *var;

	if (!op)
		return 0;
	if (op->op_type == op_label)
		return 0;

	var = flow_get_var (op);
	/**	Flowvars are initialized with number == 0, and any global flowvar
	 *	used by a function will always have a number >= 0 after flow analysis,
	 *	and local flowvars will always be 0 before flow analysis, so use -1
	 *	to indicate the variable has been counted.
	 *
	 *	Also, since this is the beginning of flow analysis for this function,
	 *	ensure the define/use sets for global vars are empty. However, since
	 *	checking if a var is global is too much trouble, just clear them all.
	 */
	if (var && var->number != -1) {
		set_empty (var->use);
		set_empty (var->define);
		var->number = -1;
		return 1;
	}
	return 0;
}

static int
count_operand_chain (operand_t *op)
{
	int         count = 0;
	while (op) {
		count += count_operand (op);
		op = op->next;
	}
	return count;
}

/**	Allocate flow analysis pseudo address space.
 */
static int
get_pseudo_address (function_t *func, int size)
{
	int         addr = func->pseudo_addr;
	func->pseudo_addr += size;
	return addr;
}

/**	Allocate flow analysis pseudo address space to a temporary variable.
 *
 *	If the operand already has an address allocated (flowvar_t::flowaddr is
 *	not 0), then the already allocated address is returned.
 *
 *	If the operand refers to an alias, the alias chain is followed to the
 *	actual temp var operand and the real temp var is allocated space if it
 *	has not allready been alloced.
 *
 *	The operand is given the address of the real temp var operand plus whatever
 *	offset the operand has.
 *
 *	Real temp var operands must have a zero offset.
 *
 *	The operand address is set in \a op and returned.
 */
static int
get_temp_address (function_t *func, operand_t *op)
{
	operand_t  *top = op;
	if (op->tempop.flowaddr) {
		return op->tempop.flowaddr;
	}
	while (top->tempop.alias) {
		top = top->tempop.alias;
	}
	if (!top->tempop.flowaddr) {
		top->tempop.flowaddr = get_pseudo_address (func, top->size);
	}
	if (top->tempop.offset) {
		internal_error (top->expr, "real tempop with a non-zero offset");
	}
	op->tempop.flowaddr = top->tempop.flowaddr + op->tempop.offset;
	return op->tempop.flowaddr;
}

/**	Add an operand's flowvar to the function's list of variables.
 */
static void
add_operand (function_t *func, operand_t *op)
{
	flowvar_t  *var;

	if (!op)
		return;
	if (op->op_type == op_label)
		return;

	var = flow_get_var (op);
	/**	If the flowvar number is still -1, then the flowvar has not yet been
	 *	added to the list of variables referenced by the function.
	 *
	 *	The flowvar's flowvar_t::number is set to its index in the function's
	 *	list of flowvars.
	 *
	 *	Also, temp and local flowvars are assigned addresses from the flow
	 *	analysys pseudo address space so partial accesses can be analyzed.
	 */
	if (var && var->number == -1) {
		var->number = func->num_vars++;
		var->op = op;
		func->vars[var->number] = var;
		if (op->op_type == op_pseudo) {
			var->flowaddr = get_pseudo_address (func, 1);
		} else if (op->op_type == op_temp) {
			var->flowaddr = get_temp_address (func, op);
		} else if (flowvar_is_param (var)) {
			var->flowaddr = func->num_statements + def_offset (var->op->def);
			var->flowaddr += func->locals->space->size;
		} else if (flowvar_is_argument (var)) {
			var->flowaddr = func->num_statements + def_offset (var->op->def);
			var->flowaddr += func->locals->space->size;
			var->flowaddr += func->parameters->space->size;
		} else if (flowvar_is_local (var)) {
			var->flowaddr = func->num_statements + def_offset (var->op->def);
		}
	}
}

static void
add_operand_chain (function_t *func, operand_t *op)
{
	while (op) {
		add_operand (func, op);
		op = op->next;
	}
}

/**	Create symbols and defs for params/return if not already available.
 */
static symbol_t *
param_symbol (const char *name)
{
	symbol_t   *sym;
	sym = make_symbol (name, &type_param, pr.symtab->space, sc_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	return sym;
}

/**	Build an array of all the statements in a function.

	The array exists so statements can be referenced by number and thus used
	in sets.

	The statement references in the array (function_t::statements) are in the
	same order as they are within the statement blocks (function_t::sblock)
	and with the blocks in the same order as the linked list of blocks.
*/
static void
flow_build_statements (function_t *func)
{
	sblock_t   *sblock;
	statement_t *s;
	int         num_statements = 0;

	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		for (s = sblock->statements; s; s = s->next)
			s->number = num_statements++;
	}
	if (!num_statements)
		return;

	func->statements = malloc (num_statements * sizeof (statement_t *));
	func->num_statements = num_statements;
	for (sblock = func->sblock; sblock; sblock = sblock->next) {
		for (s = sblock->statements; s; s = s->next)
			func->statements[s->number] = s;
	}
}

static int flow_def_clear_flowvars (def_t *def, void *data)
{
	if (def->flowvar) {
		delete_flowvar (def->flowvar);
	}
	def->flowvar = 0;
	return 0;
}

static void
add_var_addrs (set_t *set, flowvar_t *var)
{
	for (int i = 0; i < var->op->size; i++) {
		set_add (set, var->flowaddr + i);
	}
}

/**	Build an array of all the variables used by a function
 *
 *	The array exists so variables can be referenced by number and thus used
 *	in sets. However, because larger variables may be aliased by smaller types,
 *	their representation is more complicated.
 *
 *	# Local variable representation
 *	Defined local vars add their address in local space to the number of
 *	statements in the function. Thus their flow analysis address is in the
 *	range:
 *
 *		([num_statements ... num_statements+localsize])
 *
 *	with a set element in flowvar_t::define for each word used by the var.
 *	That is, single word types (int, float, pointer, etc) have one element,
 *	doubles have two adjacant elements, and vectors and quaternions have
 *	three and four elements respectively (also adjacant). Structural types
 *	(struct, union, array) have as many adjacant elements as their size
 *	dictates.
 *
 *	Temporary vars are pseudo allocated and their addresses are added as
 *	for normal local vars.
 *
 *	Note, however, that flowvar_t::define also includes real function
 *	statements that assign to the variable.
 *
 *	# Pseudo Address Space
 *	Temporary variables are _effectively_ local variables and thus will
 *	be treated as such by the analyzer in that their addresses and sizes
 *	will be used to determine which and how many set elements to use.
 *
 *	However, at this stage, temporary variables do not have any address
 *	space assigned to them because their lifetimes are generally limited
 *	to a few statements and the memory used for the temp vars may be
 *	recycled. Thus, give temp vars a pseudo address space just past the
 *	address space used for source-defined local variables. As each temp
 *	var is added to the analyzer, get_temp_address() assigns the temp var
 *	an address using function_t::pseudo_addr as a starting point.
 *
 *	add_operand() takes care of setting flowvar_t::flowaddr for both locals
 *	and temps.
 */
static void
flow_build_vars (function_t *func)
{
	statement_t *s;
	operand_t   *operands[FLOW_OPERANDS];
	int         num_vars = 0;
	int         i, j;
	set_t      *stuse;
	set_t      *stdef;
	set_iter_t *var_i;
	flowvar_t  *var;

	// First, run through the statements making sure any accessed variables
	// have their flowvars reset.  Local variables will be fine, but global
	// variables may have had flowvars added in a previous function, and it's
	// easier to just clear them all.
	// This is done before .return and .param so they won't get reset just
	// after being counted
	for (i = 0; i < func->num_statements; i++) {
		s = func->statements[i];
		flow_analyze_statement (s, 0, 0, 0, operands);
		for (j = 0; j < FLOW_OPERANDS; j++) {
			if (operands[j] && operands[j]->op_type == op_def) {
				def_visit_all (operands[j]->def, 0,
							   flow_def_clear_flowvars, 0);
			}
		}
	}
	// count .return and .param_[0-7] as they are always needed
	for (i = 0; i < num_flow_params; i++) {
		def_t      *def = param_symbol (flow_params[i].name)->s.def;
		def_visit_all (def, 0, flow_def_clear_flowvars, 0);
		flow_params[i].op.def = def;
		num_vars += count_operand (&flow_params[i].op);
	}
	// then run through the statements in the function looking for accessed
	// variables
	for (i = 0; i < func->num_statements; i++) {
		s = func->statements[i];
		flow_analyze_statement (s, 0, 0, 0, operands);
		for (j = 0; j < 4; j++)
			num_vars += count_operand (operands[j]);
		// count any pseudo operands referenced by the statement
		num_vars += count_operand_chain (s->use);
		num_vars += count_operand_chain (s->def);
		num_vars += count_operand_chain (s->kill);
	}
	if (!num_vars)
		return;

	func->vars = malloc (num_vars * sizeof (flowvar_t *));

	stuse = set_new ();
	stdef = set_new ();

	// set up the pseudo address space for temp vars so accessing tmp vars
	// though aliases analyses correctly
	func->pseudo_addr = func->num_statements;
	func->pseudo_addr += func->locals->space->size;
	func->pseudo_addr += func->parameters->space->size;
	if (func->arguments) {
		func->pseudo_addr += func->arguments->size;
	}

	func->num_vars = 0;	// incremented by add_operand
	// first, add .return and .param_[0-7] as they are always needed
	for (i = 0; i < num_flow_params; i++)
		add_operand (func, &flow_params[i].op);
	// then run through the statements in the function adding accessed
	// variables
	for (i = 0; i < func->num_statements; i++) {
		s = func->statements[i];
		flow_analyze_statement (s, 0, 0, 0, operands);
		for (j = 0; j < 4; j++)
			add_operand (func, operands[j]);
		add_operand_chain (func, s->use);
		add_operand_chain (func, s->def);
		add_operand_chain (func, s->kill);
	}
	// and set the use/def sets for the vars (has to be a separate pass
	// because the alias handling reqruires the flow address to be valid
	// (ie, not -1)
	for (i = 0; i < func->num_statements; i++) {
		s = func->statements[i];
		flow_analyze_statement (s, stuse, stdef, 0, 0);
		for (var_i = set_first (stdef); var_i; var_i = set_next (var_i)) {
			var = func->vars[var_i->element];
			set_add (var->define, i);
		}
		for (var_i = set_first (stuse); var_i; var_i = set_next (var_i)) {
			var = func->vars[var_i->element];
			set_add (var->use, i);
		}
	}
	func->global_vars = set_new ();
	func->param_vars = set_new ();
	// mark all global vars (except .return and .param_N), and param vars
	for (i = num_flow_params; i < func->num_vars; i++) {
		if (flowvar_is_global (func->vars[i])) {
			set_add (func->global_vars, i);
		}
		if (flowvar_is_param (func->vars[i])) {
			add_var_addrs (func->param_vars, func->vars[i]);
		}
	}
	// Put the local varibals in their place (set var->defined to the addresses
	// spanned by the var)
	for (i = 0; i < func->num_vars; i++) {
		var = func->vars[i];
		if (flowvar_is_global (var)) {// || flowvar_is_param (var)) {
			continue;
		}
		add_var_addrs (var->define, var);
	}

	set_delete (stuse);
	set_delete (stdef);
}

/**	Add the tempop's spanned addresses to the kill set
 */
static int
flow_tempop_kill_aliases (tempop_t *tempop, void *_kill)
{
	set_t      *kill = (set_t *) _kill;
	flowvar_t  *var;
	var = tempop->flowvar;
	if (var)
		set_union (kill, var->define);
	return 0;
}

/**	Add the def's spanned addresses to the kill set
 */
static int
flow_def_kill_aliases (def_t *def, void *_kill)
{
	set_t      *kill = (set_t *) _kill;
	flowvar_t  *var;
	var = def->flowvar;
	if (var)
		set_union (kill, var->define);
	return 0;
}

/**	Add the flowvar's spanned addresses to the kill set
 *
 *	If the flowvar refers to an alias, then the real def/tempop and any
 *	overlapping aliases are aslo killed.
 *
 *	However, other aliases cannot kill anything in the uninitialized set.
 */
static void
flow_kill_aliases (set_t *kill, flowvar_t *var, const set_t *uninit)
{
	operand_t  *op;
	set_t      *tmp;

	set_union (kill, var->define);
	op = var->op;
	tmp = set_new ();
	// collect the kill sets from any aliases
	if (op->op_type == op_temp) {
		tempop_visit_all (&op->tempop, 1, flow_tempop_kill_aliases, tmp);
	} else if (op->op_type == op_def) {
		def_visit_all (op->def, 1, flow_def_kill_aliases, tmp);
	}
	// don't allow aliases to kill definitions in the entry dummy block
	if (uninit) {
		set_difference (tmp, uninit);
	}
	// merge the alias kills with the current def's kills
	set_union (kill, tmp);
	set_delete (tmp);
}

/**	Compute reaching defs
 */
static void
flow_reaching_defs (flowgraph_t *graph)
{
	int         i;
	int         changed;
	flownode_t *node;
	statement_t *st;
	set_t      *stdef = set_new ();
	set_t      *stgen = set_new ();
	set_t      *stkill = set_new ();
	set_t      *oldout = set_new ();
	set_t      *gen, *kill, *in, *out, *uninit;
	set_iter_t *var_i;
	set_iter_t *pred_i;
	flowvar_t  *var;

	// First, create out for the entry dummy node using fake statement numbers.
	//\f[ \bigcup\limits_{i=1}^{\infty} F_{i} \f]
	//\f[ \bigcap\limits_{i=1}^{\infty} F_{i} \f]

	/**	The dummy entry node reaching defs \a out set is initialized to:
	 *	\f[ out_{reaching}=[\bigcup\limits_{v \in \{locals\}} define_{v}]
	 *		\setminus \{statements\} \f]
	 *	where {\a locals} is the set of local def and tempop flowvars (does
	 *	not include parameters), \a define is the set of addresses spanned
	 *	by the flowvar (see flow_build_vars()) (XXX along with statement
	 *	gens), and {\a statements} is the set of all statements in the
	 *	function (ensures the \a out set does not include any initializers in
	 *	the code nodes).
	 *
	 *	All other entry node sets are initialized to empty.
	 */
	// kill represents the set of all statements in the function
	kill = set_new ();
	for (i = 0; i < graph->func->num_statements; i++)
		set_add (kill, i);
	// uninit
	uninit = set_new ();
	for (i = 0; i < graph->func->num_vars; i++) {
		var = graph->func->vars[i];
		set_union (uninit, var->define);// do not want alias handling here
	}
	/**	Any possible gens from the function code are removed from the
	 *	\a uninit set (which becomes the \a out set of the entry node's
	 *	reaching defs) in order to prevent them leaking into the real nodes.
	 */
	set_difference (uninit, kill);	// remove any gens from the function
	// initialize the reaching defs sets in the entry node
	graph->nodes[graph->num_nodes]->reaching_defs.out = uninit;
	graph->nodes[graph->num_nodes]->reaching_defs.in = set_new ();
	graph->nodes[graph->num_nodes]->reaching_defs.gen = set_new ();
	graph->nodes[graph->num_nodes]->reaching_defs.kill = set_new ();

	// Calculate gen and kill for each block, and initialize in and out
	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		gen = set_new ();
		kill = set_new ();
		for (st = node->sblock->statements; st; st = st->next) {
			flow_analyze_statement (st, 0, stdef, 0, 0);
			set_empty (stgen);
			set_empty (stkill);
			for (var_i = set_first (stdef); var_i; var_i = set_next (var_i)) {
				var = graph->func->vars[var_i->element];
				flow_kill_aliases (stkill, var, uninit);
				set_remove (stkill, st->number);
				set_add (stgen, st->number);
			}

			set_difference (gen, stkill);
			set_union (gen, stgen);

			set_difference (kill, stgen);
			set_union (kill, stkill);
		}
		node->reaching_defs.gen = gen;
		node->reaching_defs.kill = kill;
		node->reaching_defs.in = set_new ();
		node->reaching_defs.out = set_new ();
	}

	changed = 1;
	while (changed) {
		changed = 0;
		// flow down the graph
		for (i = 0; i < graph->num_nodes; i++) {
			node = graph->nodes[graph->depth_first[i]];
			in = node->reaching_defs.in;
			out = node->reaching_defs.out;
			gen = node->reaching_defs.gen;
			kill = node->reaching_defs.kill;
			for (pred_i = set_first (node->predecessors); pred_i;
				 pred_i = set_next (pred_i)) {
				flownode_t *pred = graph->nodes[pred_i->element];
				set_union (in, pred->reaching_defs.out);
			}
			set_assign (oldout, out);
			set_assign (out, in);
			set_difference (out, kill);
			set_union (out, gen);
			if (!set_is_equivalent (out, oldout))
				changed = 1;
		}
	}
	set_delete (oldout);
	set_delete (stdef);
	set_delete (stgen);
	set_delete (stkill);
}

/**	Update the node's \a use set from the statement's \a use set
 */
static void
live_set_use (set_t *stuse, set_t *use, set_t *def)
{
	// the variable is used before it is defined
	set_difference (stuse, def);
	set_union (use, stuse);
}

/**	Update the node's \a def set from the statement's \a def set
 */
static void
live_set_def (set_t *stdef, set_t *use, set_t *def)
{
	// the variable is defined before it is used
	set_difference (stdef, use);
	set_union (def, stdef);
}

static void
flow_live_vars (flowgraph_t *graph)
{
	int         i, j;
	flownode_t *node;
	set_t      *use;
	set_t      *def;
	set_t      *stuse = set_new ();
	set_t      *stdef = set_new ();
	set_t      *tmp = set_new ();
	set_iter_t *succ;
	statement_t *st;
	int         changed = 1;

	// first, calculate use and def for each block, and initialize the in and
	// out sets.
	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		use = set_new ();
		def = set_new ();
		for (st = node->sblock->statements; st; st = st->next) {
			flow_analyze_statement (st, stuse, stdef, 0, 0);
			live_set_use (stuse, use, def);
			live_set_def (stdef, use, def);
		}
		node->live_vars.use = use;
		node->live_vars.def = def;
		node->live_vars.in = set_new ();
		node->live_vars.out = set_new ();
	}
	// create in for the exit dummy block using the global vars used by the
	// function
	use = set_new ();
	set_assign (use, graph->func->global_vars);
	node = graph->nodes[graph->num_nodes + 1];
	node->live_vars.in = use;
	node->live_vars.out = set_new ();
	node->live_vars.use = set_new ();
	node->live_vars.def = set_new ();

	while (changed) {
		changed = 0;
		// flow UP the graph because live variable analysis uses information
		// from a node's successors rather than its predecessors.
		for (j = graph->num_nodes - 1; j >= 0; j--) {
			node = graph->nodes[graph->depth_first[j]];
			set_empty (tmp);
			for (succ = set_first (node->successors); succ;
				 succ = set_next (succ))
				set_union (tmp, graph->nodes[succ->element]->live_vars.in);
			if (!set_is_equivalent (node->live_vars.out, tmp)) {
				changed = 1;
				set_assign (node->live_vars.out, tmp);
			}
			set_assign (node->live_vars.in, node->live_vars.out);
			set_difference (node->live_vars.in, node->live_vars.def);
			set_union (node->live_vars.in, node->live_vars.use);
		}
	}
	set_delete (stuse);
	set_delete (stdef);
	set_delete (tmp);
}

static void
flow_uninit_scan_statements (flownode_t *node, set_t *defs, set_t *uninit)
{
	set_t      *stuse;
	set_t      *stdef;
	statement_t *st;
	set_iter_t *var_i;
	flowvar_t  *var;
	operand_t  *op;

	// defs holds only reaching definitions. make it hold only reaching
	// uninitialized definitions
	set_intersection (defs, uninit);
	stuse = set_new ();
	stdef = set_new ();
	for (st = node->sblock->statements; st; st = st->next) {
		flow_analyze_statement (st, stuse, stdef, 0, 0);
		for (var_i = set_first (stuse); var_i; var_i = set_next (var_i)) {
			var = node->graph->func->vars[var_i->element];
			if (set_is_intersecting (defs, var->define)) {
				if (var->op->op_type == op_pseudo) {
					pseudoop_t *op = var->op->pseudoop;
					if (op->uninitialized) {
						op->uninitialized (st->expr, op);
					} else {
						internal_error (0, "pseudoop uninitialized not set");
					}
				} else {
					def_t      *def = flowvar_get_def (var);
					if (def) {
						if (options.warnings.uninited_variable) {
							warning (st->expr, "%s may be used uninitialized",
									 def->name);
						}
					} else {
						bug (st->expr, "st %d, uninitialized temp %s",
							 st->number, operand_string (var->op));
					}
				}
			}
			// avoid repeat warnings in this node
			set_difference (defs, var->define);
		}
		for (var_i = set_first (stdef); var_i; var_i = set_next (var_i)) {
			var = node->graph->func->vars[var_i->element];
			// kill any reaching uninitialized definitions for this variable
			set_difference (defs, var->define);
			if (var->op->op_type == op_temp) {
				op = var->op;
				if (op->tempop.alias) {
					var = op->tempop.alias->tempop.flowvar;
					if (var)
						set_difference (defs, var->define);
				}
				for (op = op->tempop.alias_ops; op; op = op->next) {
					var = op->tempop.flowvar;
					if (var)
						set_difference (defs, var->define);
				}
			}
		}
	}
	set_delete (stuse);
	set_delete (stdef);
}

static void
flow_uninitialized (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;
	flowvar_t  *var;
	set_iter_t *var_i;
	set_t      *defs;
	set_t      *uninitialized;

	uninitialized = set_new ();
	node = graph->nodes[graph->num_nodes];
	set_assign (uninitialized, node->reaching_defs.out);
	// parameters are, by definition, initialized
	set_difference (uninitialized, graph->func->param_vars);
	defs = set_new ();

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[graph->depth_first[i]];
		set_empty (defs);
		// collect definitions of all variables "used" in this node. use from
		// the live vars analysis is perfect for the job
		for (var_i = set_first (node->live_vars.use); var_i;
			 var_i = set_next (var_i)) {
			var = graph->func->vars[var_i->element];
			set_union (defs, var->define);
		}
		// interested in only those defintions that actually reach this node
		set_intersection (defs, node->reaching_defs.in);
		// if any of the definitions come from the entry dummy block, then
		// the statements need to be scanned in case an aliasing definition
		// kills the dummy definition before the usage, and also so the line
		// number information can be obtained from the statement.
		if (set_is_intersecting (defs, uninitialized))
			flow_uninit_scan_statements (node, defs, uninitialized);
	}
	set_delete (defs);
	set_delete (uninitialized);
}

static void
flow_build_dags (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		node->dag = dag_create (node);
	}
	if (options.block_dot.dags)
		dump_dot ("dags", graph, dump_dot_flow_dags);
}

static void
flow_cleanup_dags (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		dag_remove_dead_nodes (node->dag);
	}
	if (options.block_dot.dags)
		dump_dot ("cleaned-dags", graph, dump_dot_flow_dags);
}

static sblock_t *
flow_generate (flowgraph_t *graph)
{
	int         i;
	sblock_t   *code = 0;
	sblock_t  **tail = &code;

	for (i = 0; i < graph->num_nodes; i++) {
		ex_label_t *label;
		sblock_t   *block;

		flownode_t *node = graph->nodes[i];
		*tail = block = new_sblock ();
		tail = &(*tail)->next;
		// first, transfer any labels on the old node to the new
		while ((label = node->sblock->labels)) {
			node->sblock->labels = label->next;
			label->next = block->labels;
			block->labels = label;
			label->dest = block;
		}
		// generate new statements from the dag;
		dag_generate (node->dag, block);
	}
	if (options.block_dot.post)
		dump_dot ("post", code, dump_dot_sblock);
	return code;
}

static int
flow_tempop_add_aliases (tempop_t *tempop, void *_set)
{
	set_t      *set = (set_t *) _set;
	flowvar_t  *var;
	var = tempop->flowvar;
	if (var)
		set_add (set, var->number);
	return 0;
}

static int
flow_def_add_aliases (def_t *def, void *_set)
{
	set_t      *set = (set_t *) _set;
	flowvar_t  *var;
	var = def->flowvar;
	if (var)
		set_add (set, var->number);
	return 0;
}

static void
flow_add_op_var (set_t *set, operand_t *op, int is_use)
{
	flowvar_t  *var;
	int         ol = is_use ? 1 : 2;

	if (!set)
		return;
	if (!(var = flow_get_var (op)))
		return;
	set_add (set, var->number);

	// FIXME XXX I think the curent implementation will have problems
	// for the def set when assigning to an alias as right now the real
	// var is being treated as assigned as well. Want to handle partial
	// defs properly, but I am as yet uncertain of how.
	if (op->op_type == op_temp) {
		tempop_visit_all (&op->tempop, ol, flow_tempop_add_aliases, set);
	} else if (op->op_type == op_def) {
		def_visit_all (op->def, ol, flow_def_add_aliases, set);
	}
}

static operand_t *
flow_analyze_pointer_operand (operand_t *ptrop, set_t *def)
{
	operand_t  *op = 0;

	if (ptrop->op_type == op_value && ptrop->value->lltype == ev_ptr) {
		ex_pointer_t *ptr = &ptrop->value->v.pointer;
		if (ptrop->value->v.pointer.def) {
			def_t      *alias;
			alias = alias_def (ptr->def, ptr->type, ptr->val);
			op = def_operand (alias, ptr->type, ptrop->expr);
		}
		if (ptrop->value->v.pointer.tempop) {
			op = ptrop->value->v.pointer.tempop;
		}
		if (op) {
			flow_add_op_var (def, op, 0);
		}
	}
	return op;
}

void
flow_analyze_statement (statement_t *s, set_t *use, set_t *def, set_t *kill,
						operand_t *operands[FLOW_OPERANDS])
{
	int         i, start, calln = -1;
	operand_t  *src_op = 0;
	operand_t  *res_op = 0;
	operand_t  *aux_op1 = 0;
	operand_t  *aux_op2 = 0;
	operand_t  *aux_op3 = 0;

	if (use) {
		set_empty (use);
		for (operand_t *op = s->use; op; op = op->next) {
			flow_add_op_var (use, op, 1);
		}
	}
	if (def) {
		set_empty (def);
		for (operand_t *op = s->def; op; op = op->next) {
			flow_add_op_var (def, op, 0);
		}
	}
	if (kill) {
		set_empty (kill);
		for (operand_t *op = s->kill; op; op = op->next) {
			flow_add_op_var (kill, op, 0);
		}
	}
	if (operands) {
		for (i = 0; i < FLOW_OPERANDS; i++)
			operands[i] = 0;
	}

	switch (s->type) {
		case st_none:
			internal_error (s->expr, "not a statement");
		case st_address:
			if (s->opb) {
				flow_add_op_var (use, s->opa, 1);
				flow_add_op_var (use, s->opb, 1);
			}
			flow_add_op_var (def, s->opc, 0);
			if (operands) {
				operands[0] = s->opc;
				operands[1] = s->opa;
				operands[2] = s->opb;
			}
			break;
		case st_expr:
			flow_add_op_var (def, s->opc, 0);
			flow_add_op_var (use, s->opa, 1);
			if (s->opb)
				flow_add_op_var (use, s->opb, 1);
			if (operands) {
				operands[0] = s->opc;
				operands[1] = s->opa;
				operands[2] = s->opb;
			}
			break;
		case st_assign:
			flow_add_op_var (def, s->opa, 0);
			flow_add_op_var (use, s->opc, 1);
			if (operands) {
				operands[0] = s->opa;
				operands[1] = s->opc;
			}
			break;
		case st_ptrassign:
		case st_move:
		case st_ptrmove:
		case st_memset:
		case st_ptrmemset:
			flow_add_op_var (use, s->opa, 1);
			flow_add_op_var (use, s->opb, 1);
			aux_op1 = s->opb;
			if (!strcmp (s->opcode, "move")
				|| !strcmp (s->opcode, "memset")) {
				flow_add_op_var (def, s->opc, 0);
				src_op = s->opa;
				res_op = s->opc;
			} else if (!strcmp (s->opcode, "movep")) {
				flow_add_op_var (use, s->opc, 0);
				aux_op3 = flow_analyze_pointer_operand (s->opa, use);
				res_op = flow_analyze_pointer_operand (s->opc, def);
				src_op = s->opa;
				aux_op2 = s->opc;
			} else if (!strcmp (s->opcode, "memsetp")) {
				flow_add_op_var (use, s->opc, 0);
				res_op = flow_analyze_pointer_operand (s->opc, def);
				src_op = s->opa;
				aux_op2 = s->opc;
			} else if (!strcmp (s->opcode, "store")) {
				flow_add_op_var (use, s->opc, 1);
				res_op = flow_analyze_pointer_operand (s->opa, def);
				src_op = s->opc;
				aux_op2 = s->opa;
			} else {
				internal_error (s->expr, "unexpected opcode '%s' for %d",
								s->opcode, s->type);
			}
			if (kill) {
				set_everything (kill);
			}
			if (operands) {
				operands[0] = res_op;
				operands[1] = src_op;
				operands[2] = aux_op1;
				operands[3] = aux_op2;
				operands[4] = aux_op3;
			}
			break;
		case st_state:
			flow_add_op_var (use, s->opa, 1);
			flow_add_op_var (use, s->opb, 1);
			if (s->opc)
				flow_add_op_var (use, s->opc, 1);
			//FIXME entity members
			if (operands) {
				operands[1] = s->opa;
				operands[2] = s->opb;
				operands[3] = s->opc;
			}
			break;
		case st_func:
			if (statement_is_return (s)) {
				if (s->opc) {
					// ruamoko
					// opc always short
					short       ret_mode = s->opc->value->v.short_val;
					// -1 is void
					// FIXME size and addressing
					if (ret_mode >= 0) {
						flow_add_op_var (use, s->opa, 1);
					}
				} else {
					// v6/v6p
					if (s->opa) {
						flow_add_op_var (use, s->opa, 1);
					}
				}
				if (use) {
					flow_add_op_var (use, &flow_params[0].op, 1);
				}
			}
			if (strcmp (s->opcode, "call") == 0) {
				// call uses opc to specify the destination of the return value
				// parameter usage is taken care of by the statement's use
				// list
				flow_add_op_var (def, s->opc, 0);
				// don't want old argument processing
				calln = -1;
				if (operands && s->opc->op_type != op_value) {
					operands[0] = s->opc;
				}
			} else if (strncmp (s->opcode, "call", 4) == 0) {
				start = 0;
				calln = s->opcode[5] - '0';
				flow_add_op_var (use, s->opa, 1);
			} else if (strncmp (s->opcode, "rcall", 5) == 0) {
				start = 2;
				calln = s->opcode[6] - '0';
				flow_add_op_var (use, s->opa, 1);
				flow_add_op_var (use, s->opb, 1);
				if (s->opc)
					flow_add_op_var (use, s->opc, 1);
			}
			if (calln >= 0) {
				if (use) {
					for (i = start; i < calln; i++) {
						flow_add_op_var (use, &flow_params[i + 1].op, 1);
					}
				}
				if (def) {
					for (i = 0; i < num_flow_params; i++) {
						flow_add_op_var (def, &flow_params[i].op, 0);
					}
				}
				if (kill) {
					for (i = 0; i < num_flow_params; i++) {
						flow_kill_aliases (kill,
										   flow_get_var (&flow_params[i].op),
										   0);
					}
				}
			}
			if (operands) {
				operands[1] = s->opa;
				operands[2] = s->opb;
				operands[3] = s->opc;
			}
			break;
		case st_flow:
			if (statement_is_goto (s)) {
				// opa is just a label
			} else if (statement_is_jumpb (s)) {
				flow_add_op_var (use, s->opa, 1);
				flow_add_op_var (use, s->opb, 1);
			} else if (statement_is_cond (s)) {
				flow_add_op_var (use, s->opc, 1);
			} else {
				internal_error (s->expr, "unexpected flow statement: %s",
								s->opcode);
			}
			if (operands) {
				operands[1] = s->opa;
				operands[2] = s->opb;
				operands[3] = s->opc;
			}
			break;
	}
}

static void
flow_find_successors (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;
	sblock_t   *sb;
	statement_t *st;
	sblock_t  **target_list, **target;

	// "convert" the basic blocks connections to flow-graph connections
	for (i = 0; i < graph->num_nodes + 2; i++) {
		node = graph->nodes[i];
		set_empty (node->successors);
		set_empty (node->predecessors);
		set_empty (node->edges);
	}
	graph->num_edges = 0;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		sb = node->sblock;
		st = 0;
		if (sb->statements)
			st = (statement_t *) sb->tail;
		//NOTE: if st is null (the sblock has no statements), statement_is_*
		//will return false
		//FIXME jump/jumpb
		if (statement_is_goto (st) || statement_is_jumpb (st)) {
			// sb's next is never followed.
			target_list = statement_get_targetlist (st);
			for (target = target_list; *target; target++)
				set_add (node->successors, (*target)->flownode->id);
			free (target_list);
		} else if (statement_is_cond (st)) {
			// branch: either sb's next or the conditional statment's
			// target will be followed.
			set_add (node->successors, sb->next->flownode->id);
			target_list = statement_get_targetlist (st);
			for (target = target_list; *target; target++)
				set_add (node->successors, (*target)->flownode->id);
			free (target_list);
		} else if (statement_is_return (st)) {
			// exit from function (dead end)
			// however, make the exit dummy block the node's successor
			set_add (node->successors, graph->num_nodes + 1);
		} else {
			// there is no flow-control statement in sb, so sb's next
			// must be followed
			if (sb->next) {
				set_add (node->successors, sb->next->flownode->id);
			} else {
				bug (st->expr, "code drops off the end of the function");
				// this shouldn't happen
				// however, make the exit dummy block the node's successor
				set_add (node->successors, graph->num_nodes + 1);
			}
		}
		graph->num_edges += set_count (node->successors);
	}
	// set the successor for the entry dummy node to the real entry node
	node = graph->nodes[graph->num_nodes];
	set_add (node->successors, 0);
	graph->num_edges += set_count (node->successors);
}

static void
flow_make_edges (flowgraph_t *graph)
{
	int         i, j;
	flownode_t *node;
	set_iter_t *succ;

	if (graph->edges)
		free (graph->edges);
	graph->edges = malloc (graph->num_edges * sizeof (flowedge_t));
	for (j = 0, i = 0; i < graph->num_nodes + 2; i++) {
		node = graph->nodes[i];
		for (succ = set_first (node->successors); succ;
			 succ = set_next (succ), j++) {
			set_add (node->edges, j);
			graph->edges[j].tail = i;
			graph->edges[j].head = succ->element;
		}
	}
}

static void
flow_find_predecessors (flowgraph_t *graph)
{
	int         i;
	flownode_t *node;
	set_iter_t *succ;

	for (i = 0; i < graph->num_nodes + 2; i++) {
		node = graph->nodes[i];
		for (succ = set_first (node->successors); succ;
			 succ = set_next (succ)) {
			set_add (graph->nodes[succ->element]->predecessors, i);
		}
	}
}

static void
flow_find_dominators (flowgraph_t *graph)
{
	set_t      *work;
	flownode_t *node;
	int         i;
	set_iter_t *pred;
	int         changed;

	if (!graph->num_nodes)
		return;

	// First, create a base set for the initial state of the non-initial nodes
	work = set_new ();
	for (i = 0; i < graph->num_nodes; i++)
		set_add (work, i);

	set_add (graph->nodes[0]->dom, 0);

	// initialize dom for the non-initial nodes
	for (i = 1; i < graph->num_nodes; i++) {
		set_assign (graph->nodes[i]->dom, work);
	}

	do {
		changed = 0;
		for (i = 1; i < graph->num_nodes; i++) {
			node = graph->nodes[i];
			set_empty (work);
			for (pred = set_first (node->predecessors); pred;
				 pred = set_next (pred))
				set_intersection (work, graph->nodes[pred->element]->dom);
			set_add (work, i);
			if (!set_is_equivalent (work, node->dom))
				changed = 1;
			set_assign (node->dom, work);
		}
	} while (changed);
	set_delete (work);
}

static void
insert_loop_node (flowloop_t *loop, unsigned n, set_t *stack)
{
	if (!set_is_member (loop->nodes, n)) {
		set_add (loop->nodes, n);
		set_add (stack, n);
	}
}

static flowloop_t *
make_loop (flowgraph_t *graph, unsigned n, unsigned d)
{
	flowloop_t *loop = new_loop ();
	flownode_t *node;
	set_t      *stack = set_new ();
	set_iter_t *pred;

	loop->head = d;
	set_add (loop->nodes, d);
	insert_loop_node (loop, n, stack);
	while (!set_is_empty (stack)) {
		set_iter_t *ss = set_first (stack);
		unsigned    m = ss->element;
		set_del_iter (ss);
		set_remove (stack, m);
		node = graph->nodes[m];
		for (pred = set_first (node->predecessors); pred;
			 pred = set_next (pred))
			insert_loop_node (loop, pred->element, stack);
	}
	set_delete (stack);
	return loop;
}

static void
flow_find_loops (flowgraph_t *graph)
{
	flownode_t *node;
	set_iter_t *succ;
	flowloop_t *loop, *l;
	flowloop_t *loop_list = 0;
	int         i;

	for (i = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		for (succ = set_first (node->successors); succ;
			 succ = set_next (succ)) {
			if (set_is_member (node->dom, succ->element)) {
				loop = make_loop (graph, node->id, succ->element);
				for (l = loop_list; l; l = l->next) {
					if (l->head == loop->head
						&& !set_is_subset (l->nodes, loop->nodes)
						&& !set_is_subset (loop->nodes, l->nodes)) {
						set_union (l->nodes, loop->nodes);
						delete_loop (loop);
						loop = 0;
						break;
					}
				}
				if (loop) {
					loop->next = loop_list;
					loop_list = loop;
				}
			}
		}
	}
	graph->loops = loop_list;
}

static void
df_search (flowgraph_t *graph, set_t *visited, int *i, int n)
{
	flownode_t *node;
	set_iter_t *edge;
	int         succ;

	set_add (visited, n);
	node = graph->nodes[n];
	for (edge = set_first (node->edges); edge; edge = set_next (edge)) {
		succ = graph->edges[edge->element].head;
		if (!set_is_member (visited, succ)) {
			set_add (graph->dfst, edge->element);
			df_search (graph, visited, i, succ);
		}
	}
	node->dfn = --*i;
	graph->depth_first[node->dfn] = n;
}

static void
flow_build_dfst (flowgraph_t *graph)
{
	set_t      *visited = set_new ();
	int         i;

	// mark the dummy nodes as visited to keep them out of the dfst
	set_add (visited, graph->num_nodes);
	set_add (visited, graph->num_nodes + 1);

	if (graph->depth_first)
		free (graph->depth_first);
	if (graph->dfst)
		set_delete (graph->dfst);
	graph->depth_first = calloc (graph->num_nodes, sizeof (int));
	graph->dfst = set_new ();
	i = graph->num_nodes;
	df_search (graph, visited, &i, 0);
	set_delete (visited);
}

static int
flow_remove_unreachable_nodes (flowgraph_t *graph)
{
	int         i, j;
	flownode_t *node;

	for (i = 0, j = 0; i < graph->num_nodes; i++) {
		node = graph->nodes[i];
		if (node->dfn < 0)	// skip over unreachable nodes
			continue;
		node->id = j;		// new node number
		graph->nodes[j++] = node;
	}
	graph->nodes[j] = graph->nodes[i];			// copy entry dummy node
	graph->nodes[j + 1] = graph->nodes[i + 1];	// copy exit dummy node

	// kill the pointers to unreachable nodes
	for (i = j; i < graph->num_nodes; i++)
		graph->nodes[i + 2] = 0;

	if (j < graph->num_nodes) {
		graph->num_nodes = j;
		return 1;
	}
	return 0;
}

static flownode_t *
flow_make_node (sblock_t *sblock, int id, function_t *func)
{
	flownode_t *node;

	node = new_node ();
	node->predecessors = set_new ();
	node->successors = set_new ();
	node->edges = set_new ();
	node->dom = set_new ();
	node->global_vars = func->global_vars;
	node->id = id;
	node->sblock = sblock;
	if (sblock)
		sblock->flownode = node;
	node->graph = func->graph;
	// Mark the node as unreachable. flow_build_dfst() will mark reachable
	// nodes with a value >= 0
	node->dfn = -1;
	return node;
}

/**	Build the flow graph for the function.
 *
 *	In addition to the nodes created by the statement blocks, there are two
 *	dummy blocks:
 *
 *	\dot
 *	digraph flow_build_graph {
 *		layout = dot; rankdir = TB; compound =true; nodesp = 1.0;
 *		dummy_entry [shape=box,label="entry"];
 *		sblock0 [label="code"]; sblock1 [label="code"];
 *		sblock2 [label="code"]; sblock3 [label="code"];
 *		dummy_exit [shape=box,label="exit"];
 *		dummy_entry -> sblock0; sblock0 -> sblock1;
 *		sblock1 -> sblock2; sblock2 -> sblock1;
 *		sblock2 -> dummy_exit; sblock1 -> sblock3;
 *		sblock3 -> dummy_exit;
 *	}
 *	\enddot
 *
 *	The entry block is used for detecting use of uninitialized local variables
 *	and the exit block is used for ensuring global variables are treated as
 *	live at function exit.
 *
 *	The exit block, which also is empty of statements, has its live vars
 *	\a use set initilized to the set of global defs, which are simply numbered
 *	by their index in the function's list of flowvars. All other exit node sets
 *	are initialized to empty.
 *	\f[ use_{live}=globals \f]
 */
static flowgraph_t *
flow_build_graph (function_t *func)
{
	sblock_t   *sblock = func->sblock;
	flowgraph_t *graph;
	flownode_t *node;
	sblock_t   *sb;
	int         i;
	int         pass = 0;

	graph = new_graph ();
	graph->func = func;
	func->graph = graph;
	for (sb = sblock; sb; sb = sb->next)
		graph->num_nodes++;
	// + 2 for the uninitialized dummy head block and the live dummy end block
	graph->nodes = malloc ((graph->num_nodes + 2) * sizeof (flownode_t *));
	for (i = 0, sb = sblock; sb; i++, sb = sb->next)
		graph->nodes[i] = flow_make_node (sb, i, func);
	// Create the dummy node for detecting uninitialized variables
	node = flow_make_node (0, graph->num_nodes, func);
	graph->nodes[graph->num_nodes] = node;
	// Create the dummy node for making global vars live at function exit
	node = flow_make_node (0, graph->num_nodes + 1, func);
	graph->nodes[graph->num_nodes + 1] = node;

	do {
		if (pass > 1)
			internal_error (0, "too many unreachable node passes");
		flow_find_successors (graph);
		flow_make_edges (graph);
		flow_build_dfst (graph);
		if (options.block_dot.flow)
			dump_dot (va (0, "flow-%d", pass), graph, dump_dot_flow);
		pass++;
	} while (flow_remove_unreachable_nodes (graph));
	flow_find_predecessors (graph);
	flow_find_dominators (graph);
	flow_find_loops (graph);
	return graph;
}

void
flow_data_flow (function_t *func)
{
	flowgraph_t *graph;

	flow_build_statements (func);
	flow_build_vars (func);
	graph = flow_build_graph (func);
	if (options.block_dot.statements)
		dump_dot ("statements", graph, dump_dot_flow_statements);
	flow_reaching_defs (graph);
	if (options.block_dot.reaching)
		dump_dot ("reaching", graph, dump_dot_flow_reaching);
	flow_live_vars (graph);
	if (options.block_dot.live)
		dump_dot ("live", graph, dump_dot_flow_live);
	flow_uninitialized (graph);
	flow_build_dags (graph);
	flow_cleanup_dags (graph);
	func->sblock = flow_generate (graph);
}

///@}
