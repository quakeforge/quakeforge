/*
	expr.h

	expression construction and manipulations

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/06/15

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

#ifndef __expr_h
#define __expr_h

#include "QF/progs/pr_comp.h"

#include "rua-lang.h"

typedef struct type_s type_t;
typedef struct expr_s expr_t;
typedef struct algebra_s algebra_t;
typedef struct function_s function_t;

/**	\defgroup qfcc_expr Expressions
	\ingroup qfcc
*/
///@{

/**	Type of the exression node in an expression tree.
*/
#define EX_EXPR(expr) ex_##expr,
typedef enum {
#include "tools/qfcc/include/expr_names.h"
	ex_count,		///< number of valid expression types
} expr_type;

/**	Binary and unary expressions.

	This is used for both binary and unary expressions. Unary expressions do
	not use e2. The opcode is generally the parser token for the expression,
	though special codes are used for non-math expressions.
*/
typedef struct ex_expr_s {
	int            op;			///< op-code of this expression
	bool    commutative;		///< e1 and e2 can be swapped
	bool    anticommute;		///< e1 and e2 can be swapped with negation
	bool    associative;		///< a op (b op c) == (a op b) op c
	bool    constant;			///< constant that has/will not been folded
	const type_t *type;			///< the type of the result of this expression
	const expr_t *e1;			///< left side of binary, sole of unary
	const expr_t *e2;			///< right side of binary, null for unary
} ex_expr_t;

typedef struct ex_label_s {
	struct ex_label_s *next;
	struct sblock_s *dest;		///< the location of this label if known
	const char *name;			///< the name of this label
	struct symbol_s *symbol;	///< symbol used to define this label (maybe 0)
	int         used;			///< label is used as a target
	unsigned    id;
	struct daglabel_s *daglabel;
} ex_label_t;

typedef struct {
	const ex_label_t *label;
} ex_labelref_t;

typedef struct designator_s {
	struct designator_s *next;
	const expr_t *field;
	const expr_t *index;
} designator_t;

typedef struct {
	const type_t *type;
	symbol_t   *field;
	int         offset;
} initstate_t;

typedef struct element_s {
	struct element_s *next;		///< next in chain
	int         offset;
	const type_t *type;
	const expr_t *expr;			///< initializer expression
	designator_t *designator;	///< for labeled initializers
} element_t;

typedef struct element_chain_s {
	element_t  *head;
	element_t **tail;
	const type_t *type;			///< inferred if null
	const expr_t *type_expr;
} element_chain_t;

typedef struct ex_listitem_s {
	struct ex_listitem_s *next;
	const expr_t *expr;
} ex_listitem_t;

typedef struct ex_list_s {
	ex_listitem_t *head;
	ex_listitem_t **tail;
} ex_list_t;

typedef struct {
	ex_list_t   list;
	symtab_t   *scope;
	const expr_t *result;		///< the result of this block if non-void
	bool        is_call;		///< this block exprssion forms a function call
	bool        no_flush;		///< don't flush edags
	void       *return_addr;	///< who allocated this
} ex_block_t;

typedef struct {
	struct operand_s *op;		///< The operand for the temporary variable, if
								///< allocated
	const type_t *type;			///< The type of the temporary variable.
} ex_temp_t;

typedef struct {
	const type_t *type;			///< Type of vector (vector/quaternion)
	ex_list_t   list;			///< Linked list of element expressions.
} ex_vector_t;

typedef struct {
	const expr_t *sel_ref;		///< Reference to selector in selector table
	struct selector_s *sel;		///< selector
} ex_selector_t;

typedef struct {
	const expr_t *receiver;
	struct keywordarg_s *message;
} ex_message_t;

/**	Pointer constant expression.

	Represent a pointer to an absolute address in data space.
*/
typedef struct ex_pointer_s {
	int         val;
	const type_t *type;
	struct def_s *def;
	struct operand_s *tempop;
} ex_pointer_t;

typedef struct ex_func_s {
	int         val;
	const type_t *type;
} ex_func_t;

typedef struct {
	int         size;
	expr_t     *e[1];
} ex_boollist_t;

typedef struct {
	ex_boollist_t *true_list;
	ex_boollist_t *false_list;
	const expr_t *e;
} ex_bool_t;

typedef struct ex_memset_s {
	const expr_t *dst;
	const expr_t *val;
	const expr_t *count;
} ex_memset_t;

/**	State expression used for think function state-machines.

	State expressions are of the form <code>[framenum, nextthink]</code>
	(standard) or <code>[framenum, nextthink, timestep]</code> (QF extension)
	and come before the opening brace of the function. If the state
	expression is of the former form, then \c step will be null. Normally,
	\c framenum and \c nextthink must be constant (though \c nextthink may
	be a forward reference), but qfcc allows both \c framenum and
	\c nextthink, and also \c timestep, to be variable.

	\par From qcc:
		States are special functions made for convenience.  They
		automatically set frame, nextthink (implicitly), and think (allowing
		forward definitions).

	\verbatim
	void() name = [framenum, nextthink] {code};
	\endverbatim
	expands to:
	\verbatim
	void name ()
	{
	     self.frame=framenum;
	     self.nextthink = time + 0.1;
	     self.think = nextthink
	     [code]
	};
	\endverbatim

	Although the above expansion shows three expressions, a state expression
	using constant values is just one instruction: either
	<code>state framenum, nextthink</code> (standard) or
	<code>state.f framenum, nextthink, timestep</code> (QF, optional).
*/
typedef struct {
	const expr_t *frame;		///< the frame to which to change in this state
	const expr_t *think;		///< think function for the next state
	const expr_t *step;			///< time step until the next state
} ex_state_t;

typedef struct ex_buffer_s {
	const char *data;
	size_t      size;
} ex_buffer_t;

typedef struct ex_value_s {
	struct ex_value_s *next;
	struct daglabel_s *daglabel;///< dag label for this value
	const type_t *type;
	etype_t     lltype;					///< ev_invalid -> buffer
	unsigned    id;
	bool        is_constexpr;
	union {
		pr_type_t   raw_value;			///< for memcpy
		pr_dvec4_t  raw_matrix[4];		///< so ex_vector_t is big enough
		const char *string_val;			///< string constant
		ex_buffer_t buffer;				///< #embed buffer (ev_invalid)
		double      double_val;			///< double constant
		int64_t     long_val;			///< signed 64-bit constant
		uint64_t    ulong_val;			///< unsigned 64-bit constant
		float       float_val;			///< float constant
		float       vector_val[3];		///< vector constant
		int         entity_val;			///< entity constant
		ex_func_t   func_val;			///< function constant
		ex_pointer_t pointer;			///< pointer constant
		float       quaternion_val[4];	///< quaternion constant
		int         int_val;			///< int constant
		unsigned    uint_val;			///< unsigned int constant
		int16_t     short_val;			///< short constant
		uint16_t    ushort_val;			///< unsigned short constant
#define VEC_TYPE(type_name, base_type) pr_##type_name##_t type_name##_val;
#include "tools/qfcc/include/vec_types.h"
	};
} ex_value_t;
#define value_size (sizeof(ex_value_t) - offsetof(ex_value_t, raw_value))

typedef struct {
	const type_t *type;			///< type to view the expression
	const expr_t *expr;			///< the expression to alias
	const expr_t *offset;		///< offset for alias
} ex_alias_t;

typedef struct {
	const type_t *type;			///< pointer type
	const expr_t *lvalue;		///< the lvalue being addressed
	const expr_t *offset;		///< offset from the address
} ex_address_t;

typedef struct {
	const expr_t *dst;			///< destination of assignment
	const expr_t *src;			///< source of assignment
} ex_assign_t;

typedef struct {
	pr_branch_e type;			///< type of branch
	const expr_t *target;		///< destination of branch
	const expr_t *index;		///< index for indirect branches
	const expr_t *test;			///< test expression (null for jump/call)
	const expr_t *args;			///< only for call
	const type_t *ret_type;		///< void for non-call
} ex_branch_t;

typedef struct {
	const expr_t *in;			///< source expression for arg
	const expr_t *out;			///< destination expression for arg
} ex_inout_t;

typedef struct {
	const expr_t *ret_val;
	bool        at_return;		///< return void_return call through void
} ex_return_t;

typedef struct {
	short       mode;			///< currently must be 0
	short       offset;			///< amount by which stack will be adjusted
} ex_adjstk_t;

typedef struct {
	short       mode;
	short       reg;			///< base register to load
	const expr_t *with;			///< value to load
} ex_with_t;

typedef struct {
	int         op;				///< operation to perform
	const expr_t *vec;			///< vector expression on which to operate
	const type_t *type;			///< result type
} ex_horizontal_t;

typedef struct {
	const expr_t *src;			///< source expression
	unsigned    source[4];		///< src component indices
	unsigned    neg;			///< bitmask of dst components to negate
	unsigned    zero;			///< bitmask of dst components to 0
	const type_t *type;			///< result type
} ex_swizzle_t;

typedef struct {
	const expr_t *src;			///< source expression
	int         extend;			///< extend mode 0: 0, 1: 1, 2: copy/0 3:-1
	bool        reverse;		///< reverse resultant vector
	const type_t *type;			///< result type;
} ex_extend_t;

typedef struct {
	const type_t *type;			///< overall type of multivector
	algebra_t  *algebra;		///< owning algebra
	ex_list_t   components;		///< multivector components
} ex_multivec_t;

typedef struct {
	int         op;				///< type "function"
	const expr_t *params;		///< if dynamic
	const attribute_t *property;
	const type_t *type;
	const symbol_t *sym;
} ex_type_t;

typedef struct {
	int         op;
	bool        postop;
	const expr_t *expr;
} ex_incop_t;

typedef struct {
	const expr_t *test;
	const expr_t *true_expr;
	const expr_t *false_expr;
} ex_cond_t;

typedef struct {
	const expr_t *object;
	const expr_t *member;
	const type_t *type;
} ex_field_t;

typedef struct {
	const expr_t *base;
	const expr_t *index;
	const type_t *type;
} ex_array_t;

typedef struct {
	specifier_t spec;
	ex_list_t   list;
	symtab_t   *symtab;
} ex_decl_t;

typedef enum ex_vis_e {
	vis_public,
	vis_protected,
	vis_private,
	vis_anonymous,
} ex_vis_t;

typedef struct {
	const expr_t *test;
	const expr_t *body;
	const expr_t *continue_label;
	const expr_t *continue_body;
	const expr_t *break_label;
	bool        do_while;
	bool        not;
} ex_loop_t;

typedef struct {
	bool        not;
	const expr_t *test;
	const expr_t *true_body;
	const expr_t *els;
	const expr_t *false_body;
} ex_select_t;

typedef struct {
	const expr_t *opcode;
	const type_t *res_type;
	ex_list_t   operands;
	const expr_t *extra;
} ex_intrinsic_t;

typedef struct {
	const expr_t *test;
	const expr_t *body;
	const expr_t *break_label;
} ex_switch_t;

typedef struct {
	const expr_t *value;			///< null if default
	const expr_t *end_value;		///< for case value ranges (otherwise null)
} ex_caselabel_t;

typedef struct {
	const expr_t *expr;
	bool        lvalue;			///< rvalue if false
} ex_xvalue_t;

typedef struct {
	const expr_t *expr;
	function_t *function;		///< for better reporting in inline functions
} ex_process_t;

typedef struct expr_s {
	expr_t     *next;
	rua_loc_t   loc;			///< source location of expression
	struct operand_s *op;
	expr_type   type;			///< the type of the result of this expression
	int         printid;		///< avoid duplicate output when printing
	unsigned    id;
	bool        paren:1;		///< the expression is enclosed in ()
	bool        implicit:1;		///< don't warn for implicit casts
	bool        nodag:1;		///< prevent use of dags for this expression
								///< propagates up
	union {
		ex_label_t  label;				///< label expression
		ex_labelref_t labelref;			///< label reference expression (&)
		ex_state_t  state;				///< state expression
		ex_bool_t   boolean;			///< boolean logic expression
		ex_list_t list;					///< noninvasive expression list
		ex_block_t  block;				///< statement block expression
		ex_expr_t   expr;				///< binary or unary expression
		struct def_s *def;				///< def reference expression
		struct symbol_s *symbol;		///< symbol reference expression
		ex_temp_t   temp;				///< temporary variable expression
		ex_vector_t vector;				///< vector expression list
		ex_selector_t selector;			///< selector ref and name
		ex_message_t message;			///< message receiver and message
		ex_value_t *value;				///< constant value
		element_chain_t compound;		///< compound initializer
		ex_memset_t memset;				///< memset expr params
		ex_alias_t  alias;				///< alias expr params
		ex_address_t address;			///< alias expr params
		ex_assign_t assign;				///< assignment expr params
		ex_branch_t branch;				///< branch expr params
		ex_inout_t inout;				///< inout arg params
		ex_return_t retrn;				///< return expr params
		ex_adjstk_t adjstk;				///< stack adjust param
		ex_with_t   with;				///< with expr param
		const type_t *nil;				///< type for nil if known
		ex_horizontal_t hop;			///< horizontal vector operation
		ex_swizzle_t swizzle;			///< vector swizzle operation
		ex_extend_t extend;				///< vector extend operation
		ex_multivec_t multivec;			///< geometric algebra multivector
		ex_type_t typ;					///< type expression
		ex_incop_t incop;				///< incop expression
		ex_cond_t cond;					///< ?: conditional expression
		ex_field_t field;				///< field reference expression
		ex_array_t array;				///< array index expression
		ex_decl_t decl;					///< variable declaration expression
		ex_vis_t visibility;			///< set struct member visibity
		ex_loop_t loop;					///< loop construct expression
		ex_select_t select;				///< selection construct expression
		ex_intrinsic_t intrinsic;		///< intrinsic intruction expression
		ex_switch_t switchblock;		///< switch block expression
		ex_caselabel_t caselabel;		///< case label expression
		ex_xvalue_t xvalue;				///< lvalue/rvalue specific expression
		ex_process_t process;			///< expression than needs processing
	};
} expr_t;

extern const char *expr_names[];

/**	Report a type mismatch error.

	\a e1 is used for reporting the file and line number of the error.

	\param e1		Left side expression. Used for reporting the type.
	\param e2		Right side expression. Used for reporting the type.
	\param op		The opcode of the expression.
	\return			\a e1 with its type set to ex_error.
*/
const expr_t *type_mismatch (const expr_t *e1, const expr_t *e2, int op);

const expr_t *param_mismatch (const expr_t *e, int param, const char *fn,
							  const type_t *t1, const type_t *t2);
const expr_t *reference_error (const expr_t *e, const type_t *dst,
							   const type_t *src);
const expr_t *test_error (const expr_t *e, const type_t *t);

/** Set the current source location for subsequent new expressions.

	Returns error expression with saved source location
*/
expr_t *set_src_loc (const expr_t *e);
void restore_src_loc (expr_t **e);
#define scoped_src_loc(e) \
	__attribute__((cleanup(restore_src_loc))) \
	expr_t *srclocScope = set_src_loc(e)

/**	Get the type descriptor of the expression result.

	\param e		The expression from which to get the result type.
	\return         Pointer to the type description, or null if the expression
					type (expr_t::type) is inappropriate.
*/
const type_t *get_type (const expr_t *e);

/**	Get the basic type code of the expression result.

	\param e		The expression from which to get the result type.
	\return         Pointer to the type description, or ev_type_count if
					get_type() returns null.
*/
etype_t extract_type (const expr_t *e);

ex_listitem_t *new_listitem (const expr_t *e);
int list_count (const ex_list_t *list) __attribute__((pure));
void list_scatter (const ex_list_t *list, const expr_t **exprs);
void list_scatter_rev (const ex_list_t *list, const expr_t **exprs);
void list_gather (ex_list_t *dst, const expr_t **exprs, int count);
void list_append (ex_list_t *dst, const expr_t *expr);
void list_append_list (ex_list_t *dst, const ex_list_t *src);
void list_prepend (ex_list_t *dst, const expr_t *expr);
expr_t *new_list_expr (const expr_t *first);
expr_t *expr_append_expr (expr_t *list, const expr_t *expr);
expr_t *expr_prepend_expr (expr_t *list, const expr_t *expr);
expr_t *expr_append_list (expr_t *list, ex_list_t *append);
expr_t *expr_prepend_list (expr_t *list, ex_list_t *prepend);

/**	Create a new expression node.

	Sets the source file and line number information. The expression node is
	otherwise raw. This function is generally not used directly.

	\return			The new expression node.
*/
expr_t *new_expr (void);

expr_t *new_error_expr (void);

/**	Create a new label name.

	The label name is guaranteed to be unique to the compilation. It is made
	up of the name of the current function plus an incrementing number. The
	number is not reset between functions.

	\return			The string representing the label name.
*/
const char *new_label_name (void);

/**	Create a new label expression node.

	The label name is set using new_label_name().

	\return			The new label expression (::ex_label_t) node.
*/
expr_t *new_label_expr (void);

/**	Create a named label expression node.

	The label name is set using new_label_name(), but the symbol is used to add
	the label to the function's label scope symbol table. If the label already
	exists in the function's label scope, then the existing label is returned,
	allowing for forward label declarations.

	\param label	The name symbol to use for adding the label to the function
					label scope.
	\return			The new label expression (::ex_label_t) node.
*/
const expr_t *named_label_expr (struct symbol_s *label);

/**	Create a new label reference expression node.

	Used for taking the address of a label (eg. jump tables).
	The label's \a used field is incremented.

	\return			The new label reference expression (::ex_labelref_t) node.
*/
expr_t *new_label_ref (const ex_label_t *label);

/**	Create a new state expression node.

	The label name is set using new_label_name(), and the label is linked
	into the global list of labels for later resolution.

	\param frame	The expression giving the frame number.
	\param think	The expression giving the think function.
	\param step		The expression giving the time step value, or null if
					no time-step is specified (standard form).
	\return			The new state expression (::ex_state_t) node.
*/
expr_t *new_state_expr (const expr_t *frame, const expr_t *think,
						const expr_t *step);

expr_t *new_boolean_expr (ex_boollist_t *true_list, ex_boollist_t *false_list,
						  const expr_t *e);

/**	Create a new statement block expression node.

	The returned block expression is empty.  Use append_expr() to add
	expressions to the block expression.

	\param old		Old block expression to copy or null if a fresh block.
	\return			The new block expression (::ex_block_t) node.
*/
expr_t *new_block_expr (const expr_t *old);

/**	Create a new statement block expression node from an expression list

	\param list The expression list to convert to an expression block.
	\param set_result If true, the block's result will be set to the last
				expression in the list.
	\return		The new block expression (::ex_block_t) node.
*/
expr_t *build_block_expr (expr_t *list, bool set_result);

designator_t *new_designator (symbol_t *field, const expr_t *index);
element_t *new_element (const expr_t *expr, designator_t *designator);
expr_t *new_compound_init (void);
element_t *append_init_element (element_chain_t *element_chain,
								element_t *element);
expr_t *append_element (expr_t *compound, element_t *element);
bool skip_field (symbol_t *field)__attribute__((pure));
const expr_t *initialized_temp_expr (const type_t *type,
									 const expr_t *compound);
void assign_elements (expr_t *local_expr, const expr_t *ptr,
					  element_chain_t *element_chain);
void build_element_chain (element_chain_t *element_chain, const type_t *type,
						  const expr_t *eles, int base_offset);
void free_element_chain (element_chain_t *element_chain);

/**	Create a new binary expression node.

	If either \a e1 or \a e2 are error expressions, then that expression will
	be returned instead of a new binary expression.

	\param op		The op-ccode of the binary expression.
	\param e1		The left side of the binary expression.
	\param e2		The right side of the binary expression.
	\return			The new binary expression node (::ex_expr_t) if neither
					\a e1 nor \a e2 are error expressions, otherwise the
					expression that is an error expression.
*/
expr_t *new_binary_expr (int op, const expr_t *e1, const expr_t *e2);

/**	Create a new unary expression node.

	If \a e1 is an error expression, then it will be returned instead of a
	new unary expression.

	\param op		The op-code of the unary expression.
	\param e1		The "right" side of the expression.
	\return			The new unary expression node (::ex_expr_t) if \a e1
					is not an error expression, otherwise \a e1.
*/
expr_t *new_unary_expr (int op, const expr_t *e1);

const expr_t *paren_expr (const expr_t *e);

/**	Create a new horizontal vector operantion node.

	If \a vec is an error expression, then it will be returned instead of a
	new unary expression.

	\param op		The op-code of the horizontal operation.
	\param vec		The expression (must be a vector type) on which to operate.
	\param type     The result type (must be scalar type)
	\return			The new unary expression node (::ex_expr_t) if \a e1
					is not an error expression, otherwise \a e1.
*/
expr_t *new_horizontal_expr (int op, const expr_t *vec, const type_t *type);

const expr_t *new_swizzle_expr (const expr_t *src, const char *swizzle);

const expr_t *new_extend_expr (const expr_t *src, const type_t *type, int ext,
							   bool rev);

/**	Create a new def reference (non-temporary variable) expression node.

	\return			The new def reference expression node (::def_t).
*/
expr_t *new_def_expr (struct def_s *def);

/**	Create a new symbol reference (non-temporary variable) expression node.

	\return 		The new symbol reference expression node (::symbol_t).
*/
expr_t *new_symbol_expr (struct symbol_s *symbol);

/**	Create a new temporary variable expression node.

	Does not allocate a new temporary variable.
	The ex_temp_t::users field will be 0.

	\param type		The type of the temporary variable.
	\return			The new temporary variable expression node (ex_temp_t).
*/
expr_t *new_temp_def_expr (const type_t *type);

/**	Create a new nil expression node.

	nil represents 0 of any type.

	\return			The new nil expression node.
*/
expr_t *new_nil_expr (void);

/** Create a new args expression node

	Marker between real parameters and those passed through ...

	\return			The new args expression node.
*/
expr_t *new_args_expr (void);
expr_t *new_call_expr (const expr_t *func, const expr_t *args,
					   const type_t *ret_type);

/** Create a new value expression node.

	\param value	The value to put in the expression node.
	\param implicit	The value is implicitly typed
	\return			The new value expression.
*/
const expr_t *new_value_expr (ex_value_t *value, bool implicit);

/** Create a new typed zero value expression node.

	Similar to new_nil_expr, but is 0 of a specific type.

	\param type		The type to use for the zero.
	\return			The new value expression.
*/
const expr_t *new_zero_expr (const type_t *type);

/**	Create a new symbol expression node from a name.

	\param name		The name for the symbol.
	\return			The new symbol expression.
*/
const expr_t *new_name_expr (const char *name);

struct symbol_s *get_name (const expr_t *e) __attribute__((pure));

/** Create a new string constant expression node.

	\param string_val	The string constant being represented.
	\return			The new string constant expression node
					(expr_t::e::string_val).
*/
const expr_t *new_string_expr (const char *string_val);
const char *expr_string (const expr_t *e) __attribute__((pure));
const ex_buffer_t *expr_buffer (const expr_t *e) __attribute__((pure));

/** Create a new double constant expression node.

	\param double_val	The double constant being represented.
	\param implicit		The constant was implicit and should be auto-cast
						without diagnostics
	\return			The new double constant expression node
					(expr_t::e::double_val).
*/
const expr_t *new_double_expr (double double_val, bool implicit);
double expr_double (const expr_t *e) __attribute__((pure));

/** Create a new float constant expression node.

	\param float_val	The float constant being represented.
	\param implicit		The constant was implicit and should be auto-cast
						without diagnostics
	\return			The new float constant expression node
					(expr_t::e::float_val).
*/
const expr_t *new_float_expr (float float_val, bool implicit);
float expr_float (const expr_t *e) __attribute__((pure));

/** Create a new vector constant expression node.

	\param vector_val	The vector constant being represented.
	\return			The new vector constant expression node
					(expr_t::e::vector_val).
*/
const expr_t *new_vector_expr (const float *vector_val);
const float *expr_vector (const expr_t *e) __attribute__((pure));
const expr_t *new_vector_list (const expr_t *e);
const expr_t *new_vector_list_gather (const type_t *type,
									  const expr_t **elements, int count);
const expr_t *new_vector_list_expr (const expr_t *e);
const expr_t *new_vector_value (const type_t *ele_type, int width,
								int count, const expr_t **elements,
								bool implicit);
const expr_t *new_matrix_value (const type_t *ele_type, int cols, int rows,
								int count, const expr_t **elements,
								bool implicit);
const expr_t *vector_to_compound (const expr_t *vector);

/** Create a new entity constant expression node.

	\param entity_val	The entity constant being represented.
	\return			The new entity constant expression node
					(expr_t::e::entity_val).
*/
const expr_t *new_entity_expr (int entity_val);

/** Create a new field constant expression node.

	\param field_val	offset? XXX
	\param type		The type of the field.
	\param def
	\return			The new field constant expression node
					(expr_t::e::field_val).
*/
const expr_t *new_deffield_expr (int field_val, const type_t *type,
							     struct def_s *def);
struct symbol_s *get_struct_field (const type_t *t1, const expr_t *e1,
								   const expr_t *e2);

/** Create a new function constant expression node.

	\param func_val	The function constant being represented.
	\param type		The type of the function
	\return			The new function constant expression node
					(expr_t::e::func_val).
*/
const expr_t *new_func_expr (int func_val, const type_t *type);

/** Create a new pointer constant expression node.

	\param val		The pointer constant (address) being represented. XXX
	\param type		The type of the referenced value.
	\param def
	\return			The new pointer constant expression node
					(expr_t::e::pointer_val).
*/
const expr_t *new_pointer_expr (int val, const type_t *type, struct def_s *def);

/** Create a new quaternion constant expression node.

	\param quaternion_val	The quaternion constant being represented.
	\return			The new quaternion constant expression node
					(expr_t::e::quaternion_val).
*/
const expr_t *new_quaternion_expr (const float *quaternion_val);
const float *expr_quaternion (const expr_t *e) __attribute__((pure));

const expr_t *new_bool_expr (bool bool_val);
const expr_t *new_lbool_expr (bool lbool_val);

/** Create a new int constant expression node.

	\param int_val	The int constant being represented.
	\param implicit	The constant was implicit and should be auto-cast
					without diagnostics
	\return			The new int constant expression node
					(expr_t::e::int_val).
*/
const expr_t *new_int_expr (int int_val, bool implicit);
int expr_int (const expr_t *e) __attribute__((pure));

/** Create a new uint constant expression node.

	\param uint_val	The int constant being represented.
	\return			The new int constant expression node
					(expr_t::e::int_val).
*/
const expr_t *new_uint_expr (unsigned uint_val);
unsigned expr_uint (const expr_t *e) __attribute__((pure));

const expr_t *new_long_expr (pr_long_t long_val, bool implicit);
pr_long_t expr_long (const expr_t *e) __attribute__((pure));
pr_ulong_t expr_ulong (const expr_t *e) __attribute__((pure));
const expr_t *new_ulong_expr (pr_ulong_t ulong_val);

/** Create a new short constant expression node.

	\param short_val	The short constant being represented.
	\return			The new short constant expression node
					(expr_t::e::short_val).
*/
const expr_t *new_short_expr (short short_val);
short expr_short (const expr_t *e) __attribute__((pure));
const expr_t *new_ushort_expr (unsigned short short_val);
unsigned short expr_ushort (const expr_t *e) __attribute__((pure));

pr_long_t expr_integral (const expr_t *e) __attribute__((pure));
double expr_floating (const expr_t *e) __attribute__((pure));

bool is_error (const expr_t *e) __attribute__((pure));

/**	Check if the expression refers to a constant value.

	This does not included computed constants that have not been folded.

	\param e		The expression to check.
	\return			True if the expression is constant.
*/
bool is_constant (const expr_t *e) __attribute__((pure));

/**	Check if the expression refers to a constant expression or value.

	\param e		The expression to check.
	\return			True if the expression is constant.
*/
bool is_constexpr (const expr_t *e) __attribute__((pure));

/** Check if the expression refers to a variable.

	\param e		The expression to check.
	\return			True if the expression refers to a variable (def
					expression, var symbol expression, or temp expression).
*/
bool is_variable (const expr_t *e) __attribute__((pure));

/** Check if the expression refers to a selector

	\param e		The expression to check.
	\return			True if the expression is a selector.
*/
bool is_selector (const expr_t *e) __attribute__((pure));

/**	Return a value expression representing the constant stored in \a e.

	If \a e does not represent a constant, or \a e is already a value or
	nil expression, then \a e is returned rather than a new expression.

	\param e		The expression from which to extract the value.
	\return			A new expression holding the value of \a e or \e itself.
*/
const expr_t *constant_expr (const expr_t *e);

/**	Check if the op-code is a comparison.

	\param op		The op-code to check.
	\return			True if the op-code is a comparison operator.
*/
bool is_compare (int op) __attribute__((const));

/**	Check if the op-code is a bit-shift.

	\param op		The op-code to check.
	\return			True if the op-code is a bit-shift operator.
*/
bool is_shift (int op) __attribute__((const));

/**	Check if the op-code is a math operator.

	\param op		The op-code to check.
	\return			True if the op-code is a math operator.
*/
bool is_math_op (int op) __attribute__((const));

/**	Check if the op-code is a logic operator.

	\param op		The op-code to check.
	\return			True if the op-code is a logic operator.
*/
bool is_logic (int op) __attribute__((const));

bool is_deref (const expr_t *e) __attribute__((pure));
bool is_temp (const expr_t *e) __attribute__((pure));

bool has_function_call (const expr_t *e) __attribute__((pure));
bool is_function_call (const expr_t *e) __attribute__((pure));

bool is_nil (const expr_t *e) __attribute__((pure));
bool is_string_val (const expr_t *e) __attribute__((pure));
bool is_buffer_val (const expr_t *e) __attribute__((pure));
bool is_float_val (const expr_t *e) __attribute__((pure));
bool is_vector_val (const expr_t *e) __attribute__((pure));
bool is_quaternion_val (const expr_t *e) __attribute__((pure));
bool is_int_val (const expr_t *e) __attribute__((pure));
bool is_uint_val (const expr_t *e) __attribute__((pure));
bool is_short_val (const expr_t *e) __attribute__((pure));
bool is_long_val (const expr_t *e) __attribute__((pure));
bool is_ulong_val (const expr_t *e) __attribute__((pure));
bool is_double_val (const expr_t *e) __attribute__((pure));
bool is_integral_val (const expr_t *e) __attribute__((pure));
bool is_floating_val (const expr_t *e) __attribute__((pure));
bool is_pointer_val (const expr_t *e) __attribute__((pure));
bool is_math_val (const expr_t *e) __attribute__((pure));

/**	Create a reference to the global <code>.self</code> entity variable.

	This is used for <code>\@self</code>.
	\return			A new expression referencing the <code>.self</code> def.
*/
expr_t *new_self_expr (void);

/**	Create a reference to the <code>.this</code> entity field.

	This is used for <code>\@this</code>.
	\return			A new expression referencing the <code>.this</code> def.
*/
expr_t *new_this_expr (void);

/**	Create an expression of the correct type that references the return slot.

	\param type		The type of the reference to the return slot.
	\return			A new expression referencing the return slot.
*/
const expr_t *new_ret_expr (type_t *type);

const expr_t *new_alias_expr (const type_t *type, const expr_t *expr);
const expr_t *new_offset_alias_expr (const type_t *type, const expr_t *expr,
								     int offset);

expr_t *new_address_expr (const type_t *lvtype, const expr_t *lvalue,
						  const expr_t *offset);
expr_t *new_assign_expr (const expr_t *dst, const expr_t *src);
expr_t *new_return_expr (const expr_t *ret_val);
expr_t *new_adjstk_expr (int mode, int offset);
expr_t *new_with_expr (int mode, int reg, const expr_t *val);
expr_t *new_incop_expr (int op, const expr_t *e, bool postop);
expr_t *new_cond_expr (const expr_t *test, const expr_t *true_expr,
					   const expr_t *false_expr);
expr_t *new_field_expr (const expr_t *object, const expr_t *member);
expr_t *new_field_sym_expr (const expr_t *object, symbol_t *member);
expr_t *new_array_expr (const expr_t *base, const expr_t *index);
expr_t *new_decl_expr (specifier_t spec, symtab_t *symtab);
expr_t *new_decl (symbol_t *sym, const expr_t *init);
expr_t *new_visibility_expr (ex_vis_t visibity);
expr_t *append_decl (expr_t *decl, symbol_t *sym, const expr_t *init);
expr_t *append_decl_list (expr_t *decl, const expr_t *list);

expr_t *new_loop_expr (bool not, bool do_while,
					   const expr_t *test, const expr_t *body,
					   const expr_t *continue_label,
					   const expr_t *continue_body,
					   const expr_t *break_label);

expr_t *new_select_expr (bool not, const expr_t *test,
						 const expr_t *true_body,
						 const expr_t *els, const expr_t *false_body);

expr_t *new_intrinsic_expr (const expr_t *expr_list);
expr_t *new_switch_expr (const expr_t *test, const expr_t *body,
						 const expr_t *break_label);
expr_t *new_caselabel_expr (const expr_t *value, const expr_t *end_value);
expr_t *new_xvalue_expr (const expr_t *expr, bool lvalue);
expr_t *new_process_expr (const expr_t *expr);

/**	Create an expression of the correct type that references the specified
	parameter slot.

	\param type		The type of the reference to the parameter slot.
	\param num		The index of the parameter (0-7).
	\return			A new expression referencing the parameter slot.
*/
const expr_t *new_param_expr (const type_t *type, int num);

expr_t *new_memset_expr (const expr_t *dst, const expr_t *val,
						 const expr_t *count);

expr_t *new_type_expr (const type_t *type);
expr_t *new_type_function (int op, const expr_t *params);
const expr_t *type_function (int op, const expr_t *params);
symbol_t *type_parameter (symbol_t *sym, const expr_t *type);
const type_t *resolve_type (const expr_t *te, rua_ctx_t *ctx);
const expr_t *process_type (const expr_t *te, rua_ctx_t *ctx);
const type_t **expand_type (const expr_t *te, rua_ctx_t *ctx);
const expr_t *eval_type (const expr_t *te, rua_ctx_t *ctx);

expr_t *append_expr (expr_t *block, const expr_t *e);
expr_t *prepend_expr (expr_t *block, const expr_t *e);

void print_expr (const expr_t *e);
void dump_dot_expr (const void *e, const char *filename);

const expr_t *convert_nil (const expr_t *e, const type_t *t) __attribute__((warn_unused_result));
const expr_t *convert_buffer (const expr_t *e, const type_t *t) __attribute__((warn_unused_result));

const expr_t *test_expr (const expr_t *e);
void backpatch (ex_boollist_t *list, const expr_t *label);
const expr_t *convert_bool (const expr_t *e, bool block) __attribute__((warn_unused_result));
const expr_t *convert_from_bool (const expr_t *e, const type_t *type) __attribute__((warn_unused_result));
const expr_t *bool_expr (int op, const expr_t *label, const expr_t *e1,
					     const expr_t *e2);
const expr_t *binary_expr (int op, const expr_t *e1, const expr_t *e2);
const expr_t *field_expr (const expr_t *e1, const expr_t *e2);
const expr_t *asx_expr (int op, const expr_t *e1, const expr_t *e2);
const expr_t *unary_expr (int op, const expr_t *e);
const expr_t *build_function_call (const expr_t *fexpr, const type_t *ftype,
								   const expr_t *params, rua_ctx_t *ctx);
const expr_t *function_expr (const expr_t *e1, const expr_t *e2,
							 rua_ctx_t *ctx);
const expr_t *get_column (const expr_t *e, int i);
const expr_t *constructor_expr (const expr_t *e, const expr_t *params);
struct function_s;
const expr_t *branch_expr (int op, const expr_t *test, const expr_t *label);
const expr_t *goto_expr (const expr_t *label);
const expr_t *jump_table_expr (const expr_t *table, const expr_t *index);
const expr_t *return_expr (struct function_s *f, const expr_t *e);
const expr_t *at_return_expr (struct function_s *f, const expr_t *e);
const expr_t *conditional_expr (const expr_t *cond, const expr_t *e1,
							    const expr_t *e2);
const expr_t *incop_expr (int op, const expr_t *e, int postop);
const expr_t *array_expr (const expr_t *array, const expr_t *index);
const expr_t *deref_pointer_expr (const expr_t *pointer);
const expr_t *pointer_deref (const expr_t *pointer);
const expr_t *offset_pointer_expr (const expr_t *pointer, const expr_t *offset);
const expr_t *address_expr (const expr_t *e1, const type_t *t);
const expr_t *reference_expr (const expr_t *e1, const type_t *t);
const expr_t *build_if_statement (bool not, const expr_t *test,
								  const expr_t *s1, const expr_t *els,
								  const expr_t *s2);
const expr_t *build_while_statement (bool not, const expr_t *test,
								     const expr_t *statement,
								     const expr_t *break_label,
								     const expr_t *continue_label);
const expr_t *build_do_while_statement (const expr_t *statement, bool not,
										const expr_t *test,
										const expr_t *break_label,
										const expr_t *continue_label);
const expr_t *build_for_statement (const expr_t *init, const expr_t *test,
								   const expr_t *next, const expr_t *statement,
								   const expr_t *break_label,
								   const expr_t *continue_label);
const expr_t *build_state_expr (const expr_t *e, rua_ctx_t *ctx);
const expr_t *think_expr (struct symbol_s *think_sym, rua_ctx_t *ctx);
bool is_lvalue (const expr_t *expr) __attribute__((pure));
const expr_t *assign_expr (const expr_t *dst, const expr_t *src);
const expr_t *cast_expr (const type_t *t, const expr_t *e);
const expr_t *cast_error (const expr_t *e, const type_t *t1, const type_t *t2);

const char *get_op_string (int op) __attribute__((const));

struct keywordarg_s;
struct class_type_s;
const expr_t *selector_expr (struct keywordarg_s *selector);
const expr_t *protocol_expr (const char *protocol);
const expr_t *encode_expr (const type_t *type);
const expr_t *super_expr (struct class_type_s *class_type);
const expr_t *message_expr (const expr_t *receiver,
							struct keywordarg_s *message, rua_ctx_t *ctx);
const expr_t *new_message_expr (const expr_t *receiver,
								struct keywordarg_s *message);
const expr_t *sizeof_expr (const expr_t *expr, const type_t *type);

const expr_t *fold_constants (const expr_t *e);

void edag_flush (void);
const expr_t *edag_add_expr (const expr_t *e);

bool is_scale (const expr_t *expr) __attribute__((pure));
bool is_cross (const expr_t *expr) __attribute__((pure));
bool is_sum (const expr_t *expr) __attribute__((pure));
bool is_mult (const expr_t *expr) __attribute__((pure));
bool is_neg (const expr_t *expr) __attribute__((pure));

const expr_t *neg_expr (const expr_t *e);
const expr_t *ext_expr (const expr_t *src, const type_t *type, int extend,
						bool reverse);
const expr_t *scale_expr (const type_t *type, const expr_t *a, const expr_t *b);

const expr_t *traverse_scale (const expr_t *expr) __attribute__((pure));

const expr_t *typed_binary_expr (const type_t *type, int op,
								 const expr_t *e1, const expr_t *e2);

int count_terms (const expr_t *expr) __attribute__((pure));
void scatter_terms (const expr_t *sum,
					const expr_t **adds, const expr_t **subs);
const expr_t *gather_terms (const type_t *type,
							const expr_t **adds, const expr_t **subs);
int count_factors (const expr_t *expr) __attribute__((pure));
void scatter_factors (const expr_t *prod, const expr_t **factors);
const expr_t *gather_factors (const type_t *type, int op,
							  const expr_t **factors, int count);

typedef struct rua_ctx_s rua_ctx_t;
void decl_process (const expr_t *expr, rua_ctx_t *ctx);
const expr_t *expr_process (const expr_t *expr, rua_ctx_t *ctx);
specifier_t spec_process (specifier_t spec, rua_ctx_t *ctx);
void struct_process (symtab_t *symtab, const expr_t *declaration_list,
					 rua_ctx_t *ctx);
bool can_inline (const expr_t *expr, symbol_t *fsym);
bool proc_do_list (ex_list_t *out, const ex_list_t *in, rua_ctx_t *ctx);

///@}

#endif//__expr_h
