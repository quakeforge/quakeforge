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
#undef EX_EXPR

/**	Binary and unary expressions.

	This is used for both binary and unary expressions. Unary expressions do
	not use e2. The opcode is generally the parser token for the expression,
	though special codes are used for non-math expressions.
*/
typedef struct ex_expr_s {
	int            op;		///< op-code of this expression
	struct type_s *type;	///< the type of the result of this expression
	struct expr_s *e1;		///< left side of binary, sole of unary
	struct expr_s *e2;		///< right side of binary, null for unary
} ex_expr_t;

typedef struct ex_label_s {
	struct ex_label_s *next;
	struct reloc_s *refs;		///< relocations associated with this label
	struct sblock_s *dest;		///< the location of this label if known
	const char *name;			///< the name of this label
	struct symbol_s *symbol;	///< symbol used to define this label (maybe 0)
	int         used;			///< label is used as a target
	struct daglabel_s *daglabel;
} ex_label_t;

typedef struct {
	ex_label_t *label;
} ex_labelref_t;

typedef struct element_s {
	struct element_s *next;		///< next in chain
	int         offset;
	struct type_s *type;
	struct expr_s *expr;		///< initializer expression
	struct symbol_s *symbol;	///< for labeled initializers
} element_t;

typedef struct element_chain_s {
	element_t  *head;
	element_t **tail;
} element_chain_t;

typedef struct {
	struct expr_s *head;	///< the first expression in the block
	struct expr_s **tail;	///< last expression in the block, for appending
	struct expr_s *result;	///< the result of this block if non-void
	int         is_call;	///< this block exprssion forms a function call
	void       *return_addr;///< who allocated this
} ex_block_t;

typedef struct {
	struct operand_s *op;	///< The operand for the temporary variable, if
							///< allocated
	struct type_s *type;	///< The type of the temporary variable.
} ex_temp_t;

typedef struct {
	struct type_s *type;	///< Type of vector (vector/quaternion)
	struct expr_s *list;	///< Linked list of element expressions.
} ex_vector_t;

typedef struct {
	struct expr_s *sel_ref;	///< Reference to selector in selector table
	struct selector_s *sel;	///< selector
} ex_selector_t;

/**	Pointer constant expression.

	Represent a pointer to an absolute address in data space.
*/
typedef struct ex_pointer_s {
	int         val;
	struct type_s *type;
	struct def_s *def;
	struct operand_s *tempop;
} ex_pointer_t;

typedef struct ex_func_s {
	int         val;
	struct type_s *type;
} ex_func_t;

typedef struct {
	int         size;
	struct expr_s *e[1];
} ex_list_t;

typedef struct {
	ex_list_t  *true_list;
	ex_list_t  *false_list;
	struct expr_s *e;
} ex_bool_t;

typedef struct ex_memset_s {
	struct expr_s *dst;
	struct expr_s *val;
	struct expr_s *count;
	struct type_s *type;
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
	struct expr_s *frame;		///< the frame to which to change in this state
	struct expr_s *think;		///< think function for the next state
	struct expr_s *step;		///< time step until the next state
} ex_state_t;

typedef struct ex_value_s {
	struct ex_value_s *next;
	struct daglabel_s *daglabel;///< dag label for this value
	struct type_s *type;
	etype_t     lltype;
	union {
		const char *string_val;			///< string constant
		double      double_val;			///< double constant
		int64_t     long_val;			///< signed 64-bit constant
		uint64_t    ulong_val;			///< unsigned 64-bit constant
		float       float_val;			///< float constant
		float       vector_val[3];		///< vector constant
		int         entity_val;			///< entity constant
		ex_func_t   func_val;			///< function constant
		ex_pointer_t pointer;			///< pointer constant
		float       quaternion_val[4];	///< quaternion constant
		int         integer_val;		///< integer constant
		unsigned    uinteger_val;		///< unsigned integer constant
		short       short_val;			///< short constant
	} v;
} ex_value_t;

typedef struct {
	struct type_s *type;				///< type to view the expression
	struct expr_s *expr;				///< the expression to alias
	struct expr_s *offset;				///< offset for alias
} ex_alias_t;

typedef struct {
	struct type_s *type;				///< pointer type
	struct expr_s *lvalue;				///< the lvalue being addressed
	struct expr_s *offset;				///< offset from the address
} ex_address_t;

typedef struct {
	struct expr_s *dst;					///< destination of assignment
	struct expr_s *src;					///< source of assignment
} ex_assign_t;

typedef struct {
	pr_branch_e type;				///< type of branch
	struct expr_s *target;			///< destination of branch
	struct expr_s *index;			///< index for indirect branches
	struct expr_s *test;			///< test expression (null for jump/call)
	struct expr_s *args;			///< only for call
	struct type_s *ret_type;		///< void for non-call
} ex_branch_t;

typedef struct {
	struct expr_s *ret_val;
} ex_return_t;

#define POINTER_VAL(p) (((p).def ? (p).def->offset : 0) + (p).val)

typedef struct expr_s {
	struct expr_s *next;		///< the next expression in a block expression
	expr_type   type;			///< the type of the result of this expression
	int         line;			///< source line that generated this expression
	pr_string_t file;			///< source file that generated this expression
	int         printid;		///< avoid duplicate output when printing
	unsigned    paren:1;		///< the expression is enclosed in ()
	unsigned    rvalue:1;		///< the expression is on the right side of =
	unsigned    implicit:1;		///< don't warn for implicit casts
	union {
		ex_label_t  label;				///< label expression
		ex_labelref_t labelref;			///< label reference expression (&)
		ex_state_t  state;				///< state expression
		ex_bool_t   bool;				///< boolean logic expression
		ex_block_t  block;				///< statement block expression
		ex_expr_t   expr;				///< binary or unary expression
		struct def_s *def;				///< def reference expression
		struct symbol_s *symbol;		///< symbol reference expression
		ex_temp_t   temp;				///< temporary variable expression
		ex_vector_t vector;				///< vector expression list
		ex_selector_t selector;			///< selector ref and name
		ex_value_t *value;				///< constant value
		element_chain_t compound;		///< compound initializer
		ex_memset_t memset;				///< memset expr params
		ex_alias_t  alias;				///< alias expr params
		ex_address_t address;			///< alias expr params
		ex_assign_t assign;				///< assignment expr params
		ex_branch_t branch;				///< branch expr params
		ex_return_t retrn;				///< return expr params
		struct type_s *nil;				///< type for nil if known
	} e;
} expr_t;

extern const char *expr_names[];

/**	Report a type mismatch error.

	\a e1 is used for reporting the file and line number of the error.

	\param e1		Left side expression. Used for reporting the type.
	\param e2		Right side expression. Used for reporting the type.
	\param op		The opcode of the expression.
	\return			\a e1 with its type set to ex_error.
*/
expr_t *type_mismatch (expr_t *e1, expr_t *e2, int op);

expr_t *param_mismatch (expr_t *e, int param, const char *fn,
					    struct type_s *t1, struct type_s *t2);
expr_t *cast_error (expr_t *e, struct type_s *t1, struct type_s *t2);
expr_t *test_error (expr_t *e, struct type_s *t);

extern expr_t *local_expr;

/**	Get the type descriptor of the expression result.

	\param e		The expression from which to get the result type.
	\return         Pointer to the type description, or null if the expression
					type (expr_t::type) is inappropriate.
*/
struct type_s *get_type (expr_t *e);

/**	Get the basic type code of the expression result.

	\param e		The expression from which to get the result type.
	\return         Pointer to the type description, or ev_type_count if
					get_type() returns null.
*/
etype_t extract_type (expr_t *e);

/**	Create a new expression node.

	Sets the source file and line number information. The expression node is
	otherwise raw. This function is generally not used directly.

	\return			The new expression node.
*/
expr_t *new_expr (void);

/**	Create a deep copy of an expression tree.

	\param e		The root of the expression tree to copy.
	\return			A new expression tree giving the same expression.
*/
expr_t *copy_expr (expr_t *e);

/**	Copy source expression's file and line to the destination expression

	\param dst		The expression to receive the file and line
	\param src		The expression from which the file and line will be taken
	\return			\a dst
*/
expr_t *expr_file_line (expr_t *dst, const expr_t *src);

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
expr_t *named_label_expr (struct symbol_s *label);

/**	Create a new label reference expression node.

	Used for taking the address of a label (eg. jump tables).
	The label's \a used field is incremented.

	\return			The new label reference expression (::ex_labelref_t) node.
*/
expr_t *new_label_ref (ex_label_t *label);

/**	Create a new state expression node.

	The label name is set using new_label_name(), and the label is linked
	into the global list of labels for later resolution.

	\param frame	The expression giving the frame number.
	\param think	The expression giving the think function.
	\param step		The expression giving the time step value, or null if
					no time-step is specified (standard form).
	\return			The new state expression (::ex_state_t) node.
*/
expr_t *new_state_expr (expr_t *frame, expr_t *think, expr_t *step);

expr_t *new_bool_expr (ex_list_t *true_list, ex_list_t *false_list, expr_t *e);

/**	Create a new statement block expression node.

	The returned block expression is empty.  Use append_expr() to add
	expressions to the block expression.

	\return			The new block expression (::ex_block_t) node.
*/
expr_t *new_block_expr (void);

/**	Create a new statement block expression node from an expression list

	The returned block holds the expression list in reverse order. This makes
	it easy to build the list in a parser.

	\param expr_list The expression list to convert to an expression block.
					Note that the evaluation order will be reversed.
	\return		The new block expression (::ex_block_t) node.
*/
expr_t *build_block_expr (expr_t *expr_list);

element_t *new_element (expr_t *expr, struct symbol_s *symbol);
expr_t *new_compound_init (void);
expr_t *append_element (expr_t *compound, element_t *element);
expr_t *initialized_temp_expr (const struct type_s *type, expr_t *compound);
void assign_elements (expr_t *local_expr, expr_t *ptr,
					  element_chain_t *element_chain);
void build_element_chain (element_chain_t *element_chain,
						  const struct type_s *type,
						  expr_t *eles, int base_offset);
void free_element_chain (element_chain_t *element_chain);

/**	Create a new binary expression node node.

	If either \a e1 or \a e2 are error expressions, then that expression will
	be returned instead of a new binary expression.

	\param op		The op-ccode of the binary expression.
	\param e1		The left side of the binary expression.
	\param e2		The right side of the binary expression.
	\return			The new binary expression node (::ex_expr_t) if neither
					\a e1 nor \a e2 are error expressions, otherwise the
					expression that is an error expression.
*/
expr_t *new_binary_expr (int op, expr_t *e1, expr_t *e2);

/**	Create a new unary expression node node.

	If \a e1 is an error expression, then it will be returned instead of a
	new unary expression.

	\param op		The op-code of the unary expression.
	\param e1		The "right" side of the expression.
	\return			The new unary expression node (::ex_expr_t) if \a e1
					is not an error expression, otherwise \a e1.
*/
expr_t *new_unary_expr (int op, expr_t *e1);

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
expr_t *new_temp_def_expr (const struct type_s *type);

/**	Create a new nil expression node.

	nil represents 0 of any type.

	\return			The new nil expression node.
*/
expr_t *new_nil_expr (void);

/** Create a new value expression node.

	\param value	The value to put in the expression node.
	\return			The new value expression.
*/
expr_t *new_value_expr (ex_value_t *value);

/**	Create a new symbol expression node from a name.

	\param name		The name for the symbol.
	\return			The new symbol expression.
*/
expr_t *new_name_expr (const char *name);

/** Create a new string constant expression node.

	\param string_val	The string constant being represented.
	\return			The new string constant expression node
					(expr_t::e::string_val).
*/
expr_t *new_string_expr (const char *string_val);
const char *expr_string (expr_t *e) __attribute__((pure));

/** Create a new double constant expression node.

	\param double_val	The double constant being represented.
	\return			The new double constant expression node
					(expr_t::e::double_val).
*/
expr_t *new_double_expr (double double_val);
double expr_double (expr_t *e) __attribute__((pure));

/** Create a new float constant expression node.

	\param float_val	The float constant being represented.
	\return			The new float constant expression node
					(expr_t::e::float_val).
*/
expr_t *new_float_expr (float float_val);
float expr_float (expr_t *e) __attribute__((pure));

/** Create a new vector constant expression node.

	\param vector_val	The vector constant being represented.
	\return			The new vector constant expression node
					(expr_t::e::vector_val).
*/
expr_t *new_vector_expr (const float *vector_val);
const float *expr_vector (expr_t *e) __attribute__((pure));
expr_t *new_vector_list (expr_t *e);

/** Create a new entity constant expression node.

	\param entity_val	The entity constant being represented.
	\return			The new entity constant expression node
					(expr_t::e::entity_val).
*/
expr_t *new_entity_expr (int entity_val);

/** Create a new field constant expression node.

	\param field_val	offset? XXX
	\param type		The type of the field.
	\param def
	\return			The new field constant expression node
					(expr_t::e::field_val).
*/
expr_t *new_field_expr (int field_val, struct type_s *type, struct def_s *def);

/** Create a new function constant expression node.

	\param func_val	The function constant being represented.
	\param type		The type of the function
	\return			The new function constant expression node
					(expr_t::e::func_val).
*/
expr_t *new_func_expr (int func_val, struct type_s *type);

/** Create a new pointer constant expression node.

	\param val		The pointer constant (address) being represented. XXX
	\param type		The type of the referenced value.
	\param def
	\return			The new pointer constant expression node
					(expr_t::e::pointer_val).
*/
expr_t *new_pointer_expr (int val, struct type_s *type, struct def_s *def);

/** Create a new quaternion constant expression node.

	\param quaternion_val	The quaternion constant being represented.
	\return			The new quaternion constant expression node
					(expr_t::e::quaternion_val).
*/
expr_t *new_quaternion_expr (const float *quaternion_val);
const float *expr_quaternion (expr_t *e) __attribute__((pure));

/** Create a new integer constant expression node.

	\param integer_val	The integer constant being represented.
	\return			The new integer constant expression node
					(expr_t::e::integer_val).
*/
expr_t *new_integer_expr (int integer_val);
int expr_integer (expr_t *e) __attribute__((pure));

/** Create a new integer constant expression node.

	\param uinteger_val	The integer constant being represented.
	\return			The new integer constant expression node
					(expr_t::e::integer_val).
*/
expr_t *new_uinteger_expr (unsigned uinteger_val);
unsigned expr_uinteger (expr_t *e) __attribute__((pure));

/** Create a new short constant expression node.

	\param short_val	The short constant being represented.
	\return			The new short constant expression node
					(expr_t::e::short_val).
*/
expr_t *new_short_expr (short short_val);
short expr_short (expr_t *e) __attribute__((pure));

int expr_integral (expr_t *e) __attribute__((pure));

/**	Check if the expression refers to a constant value.

	\param e		The expression to check.
	\return			True if the expression is constant.
*/
int is_constant (expr_t *e) __attribute__((pure));

/** Check if the expression refers to a selector

	\param e		The expression to check.
	\return			True if the expression is a selector.
*/
int is_selector (expr_t *e) __attribute__((pure));

/**	Return a value expression representing the constant stored in \a e.

	If \a e does not represent a constant, or \a e is already a value or
	nil expression, then \a e is returned rather than a new expression.

	\param e		The expression from which to extract the value.
	\return			A new expression holding the value of \a e or \e itself.
*/
expr_t *constant_expr (expr_t *e);

/**	Check if the op-code is a comparison.

	\param op		The op-code to check.
	\return			True if the op-code is a comparison operator.
*/
int is_compare (int op) __attribute__((const));

/**	Check if the op-code is a math operator.

	\param op		The op-code to check.
	\return			True if the op-code is a math operator.
*/
int is_math_op (int op) __attribute__((const));

/**	Check if the op-code is a logic operator.

	\param op		The op-code to check.
	\return			True if the op-code is a logic operator.
*/
int is_logic (int op) __attribute__((const));

int has_function_call (expr_t *e) __attribute__((pure));

int is_nil (expr_t *e) __attribute__((pure));
int is_string_val (expr_t *e) __attribute__((pure));
int is_float_val (expr_t *e) __attribute__((pure));
int is_vector_val (expr_t *e) __attribute__((pure));
int is_quaternion_val (expr_t *e) __attribute__((pure));
int is_integer_val (expr_t *e) __attribute__((pure));
int is_uinteger_val (expr_t *e) __attribute__((pure));
int is_short_val (expr_t *e) __attribute__((pure));
int is_integral_val (expr_t *e) __attribute__((pure));
int is_pointer_val (expr_t *e) __attribute__((pure));

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
expr_t *new_ret_expr (struct type_s *type);

expr_t *new_alias_expr (struct type_s *type, expr_t *expr);
expr_t *new_offset_alias_expr (struct type_s *type, expr_t *expr, int offset);

expr_t *new_address_expr (struct type_s *lvtype, expr_t *lvalue,
						  expr_t *offset);
expr_t *new_assign_expr (expr_t *dst, expr_t *src);
expr_t *new_return_expr (expr_t *ret_val);

/**	Create an expression of the correct type that references the specified
	parameter slot.

	\param type		The type of the reference to the parameter slot.
	\param num		The index of the parameter (0-7).
	\return			A new expression referencing the parameter slot.
*/
expr_t *new_param_expr (struct type_s *type, int num);

/**	Convert a name to an expression of the appropriate type.

	Converts the expression in-place. If the exprssion is not a name
	expression (ex_name), no converision takes place.

	\param e		The expression to convert.
*/
void convert_name (expr_t *e);

expr_t *convert_vector (expr_t *e);

expr_t *append_expr (expr_t *block, expr_t *e);

expr_t *reverse_expr_list (expr_t *e);
void print_expr (expr_t *e);
void dump_dot_expr (void *e, const char *filename);

void convert_int (expr_t *e);
void convert_short (expr_t *e);
void convert_short_int (expr_t *e);
void convert_double (expr_t *e);
expr_t *convert_nil (expr_t *e, struct type_s *t);

expr_t *test_expr (expr_t *e);
void backpatch (ex_list_t *list, expr_t *label);
expr_t *convert_bool (expr_t *e, int block);
expr_t *convert_from_bool (expr_t *e, struct type_s *type);
expr_t *bool_expr (int op, expr_t *label, expr_t *e1, expr_t *e2);
expr_t *binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *field_expr (expr_t *e1, expr_t *e2);
expr_t *asx_expr (int op, expr_t *e1, expr_t *e2);
expr_t *unary_expr (int op, expr_t *e);
expr_t *build_function_call (expr_t *fexpr, const struct type_s *ftype,
							 expr_t *params);
expr_t *function_expr (expr_t *e1, expr_t *e2);
struct function_s;
expr_t *branch_expr (int op, expr_t *test, expr_t *label);
expr_t *goto_expr (expr_t *label);
expr_t *jump_table_expr (expr_t *table, expr_t *index);
expr_t *call_expr (expr_t *func, expr_t *args, struct type_s *ret_type);
expr_t *return_expr (struct function_s *f, expr_t *e);
expr_t *conditional_expr (expr_t *cond, expr_t *e1, expr_t *e2);
expr_t *incop_expr (int op, expr_t *e, int postop);
expr_t *array_expr (expr_t *array, expr_t *index);
expr_t *pointer_expr (expr_t *pointer);
expr_t *address_expr (expr_t *e1, expr_t *e2, struct type_s *t);
expr_t *build_if_statement (int not, expr_t *test, expr_t *s1, expr_t *els,
							expr_t *s2);
expr_t *build_while_statement (int not, expr_t *test, expr_t *statement,
							   expr_t *break_label, expr_t *continue_label);
expr_t *build_do_while_statement (expr_t *statement, int not, expr_t *test,
								  expr_t *break_label, expr_t *continue_label);
expr_t *build_for_statement (expr_t *init, expr_t *test, expr_t *next,
							 expr_t *statement,
							 expr_t *break_label, expr_t *continue_label);
expr_t *build_state_expr (expr_t *e);
expr_t *think_expr (struct symbol_s *think_sym);
int is_lvalue (const expr_t *expr) __attribute__((pure));
expr_t *assign_expr (expr_t *dst, expr_t *src);
expr_t *cast_expr (struct type_s *t, expr_t *e);

const char *get_op_string (int op) __attribute__((const));

struct keywordarg_s;
struct class_type_s;
expr_t *selector_expr (struct keywordarg_s *selector);
expr_t *protocol_expr (const char *protocol);
expr_t *encode_expr (struct type_s *type);
expr_t *super_expr (struct class_type_s *class_type);
expr_t *message_expr (expr_t *receiver, struct keywordarg_s *message);
expr_t *sizeof_expr (expr_t *expr, struct type_s *type);

expr_t *fold_constants (expr_t *e);

///@}

#endif//__expr_h
