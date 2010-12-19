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

	$Id$
*/

#ifndef __expr_h
#define __expr_h

#include "QF/pr_comp.h"

/**	\defgroup qfcc_expr Expressions
	\ingroup qfcc
*/
//@{

/**	Type of the exression node in an expression tree.
*/
typedef enum {
	ex_error,		///< error expression. used to signal an error
	ex_state,		///< state expression (::ex_state_t)
	ex_bool,		///< short circuit boolean logic expression (::ex_bool_t)
	ex_label,		///< goto/branch label (::ex_label_t)
	ex_block,		///< statement block expression (::ex_block_t)
	ex_expr,		///< binary expression (::ex_expr_t)
	ex_uexpr,		///< unary expression (::ex_expr_t)
	ex_def,			///< non-temporary variable (::def_t)
	ex_temp,		///< temporary variable (::ex_temp_t)
	ex_name,		///< unresolved name (expr_t::e::string_val)

	ex_nil,			///< umm, nil, null. nuff said (0 of any type)
	ex_string,		///< string constant (expr_t::e::string_val)
	ex_float,		///< float constant (expr_t::e::float_val)
	ex_vector,		///< vector constant (expr_t::e::vector_val)
	ex_entity,		///< entity constant (expr_t::e::entity_val)
	ex_field,		///< field constant
	ex_func,		///< function constant (expr_t::e::func_val)
	ex_pointer,		///< pointer constant (expr_t::e::pointer)
	ex_quaternion,	///< quaternion constant (expr_t::e::quaternion_val)
	ex_integer,		///< integer constant (expr_t::e::integer_val)
	ex_uinteger,	///< unsigned integer constant (expr_t::e::uinteger_val)
	ex_short,		///< short constant (expr_t::e::short_val)
} expr_type;

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
	struct ex_label_s *next;	///< next lable in global list of labels
	struct reloc_s *refs;		///< relocations associated with this label
	int         ofs;			///< the location of this label if known
	const char *name;			///< the name of this label
} ex_label_t;

typedef struct {
	struct expr_s *head;	///< the first expression in the block
	struct expr_s **tail;	///< last expression in the block, for appending
	struct expr_s *result;	///< the result of this block if non-void
	int         is_call;	///< this block exprssion forms a function call
} ex_block_t;

typedef struct {
	struct expr_s *expr;
	struct def_s *def;		///< The def for the temporary variable, if
							///< allocated
	struct type_s *type;	///< The type of the temporary variable.
	int         users;		///< Reference count. Source of much hair loss.
} ex_temp_t;

/**	Pointer constant expression.

	Represent a pointer to an absolute address in data space.
*/
typedef struct {
	int         val;
	struct type_s *type;
	struct def_s *def;
} ex_pointer_t;

typedef struct {
	int         size;
	struct expr_s *e[1];
} ex_list_t;

typedef struct {
	ex_list_t  *true_list;
	ex_list_t  *false_list;
	struct expr_s *e;
} ex_bool_t;

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

#define POINTER_VAL(p) (((p).def ? (p).def->ofs : 0) + (p).val)

typedef struct expr_s {
	struct expr_s *next;		///< the next expression in a block expression
	expr_type	type;			///< the type of the result of this expression
	int			line;			///< source line that generated this expression
	string_t	file;			///< source file that generated this expression
	unsigned	paren:1;		///< the expression is enclosed in ()
	unsigned	rvalue:1;		///< the expression is on the left side of =
	union {
		ex_label_t  label;				///< label expression
		ex_state_t  state;				///< state expression
		ex_bool_t   bool;				///< boolean logic expression
		ex_block_t  block;				///< statement block expression
		ex_expr_t   expr;				///< binary or unary expression
		struct def_s *def;				///< def reference expression
		ex_temp_t   temp;				///< temporary variable expression

		const char *string_val;			///< string constant
		float       float_val;			///< float constant
		float       vector_val[3];		///< vector constant
		int         entity_val;			///< entity constant
		int         func_val;			///< function constant
		ex_pointer_t pointer;			///< pointer constant
		float       quaternion_val[4];	///< quaternion constant
		int         integer_val;		///< integer constant
		unsigned    uinteger_val;		///< unsigned integer constant
		short       short_val;			///< short constant
	} e;
} expr_t;

extern etype_t qc_types[];
extern struct type_s *ev_types[];
extern expr_type expr_types[];

/**	Report a type mismatch error.

	\a e1 is used for reporting the file and line number of the error.

	\param e1		Left side expression. Used for reporting the type.
	\param e2		Right side expression. Used for reporting the type.
	\param op		The opcode of the expression.
	\return			\a e1 with its type set to ex_error.
*/
expr_t *type_mismatch (expr_t *e1, expr_t *e2, int op);

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

/**	Create a new label name.

	The label name is guaranteed to to the compilation. It is made up of the
	name of the current function plus an incrementing number. The number is
	not reset between functions.

	\return			The string representing the label name.
*/
const char *new_label_name (void);

/**	Create a new label expression node.

	The label name is set using new_label_name(), and the label is linked
	into the global list of labels for later resolution.

	\return			The new label expression (::ex_label_t) node.
*/
expr_t *new_label_expr (void);

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

/**	Create a new binary expression node node.

	If \a e1 or \a e2 represent temporary variables, their ex_temp_t::users
	field will be incremented.

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

	If \a e1 represents a temporary variable, its ex_temp_t::users field
	will be incremented.

	If \a e1 is an error expression, then it will be returned instead of a
	new binary expression.

	\param op		The op-code of the unary expression.
	\param e1		The "right" side of the expression.
	\return			The new unary expression node (::ex_expr_t) if \a e1
					is not an error expression, otherwise \a e1.
*/
expr_t *new_unary_expr (int op, expr_t *e1);

/**	Create a new def reference (non-temporary variable) expression node.

	\return 		The new def reference expression node (::def_t).
*/
expr_t *new_def_expr (struct def_s *def);

/**	Create a new temporary variable expression node.

	Does not allocate a new temporary variable.
	The ex_temp_t::users field will be 0.

	\param type		The type of the temporary variable.
	\return			The new temporary variable expression node (ex_temp_t).
*/
expr_t *new_temp_def_expr (struct type_s *type);

/**	Create a new nil expression node.

	nil represents 0 of any type.

	\return			The new nil expression node.
*/
expr_t *new_nil_expr (void);

/** Create a new name expression node.

	Name expression nodes represent as yet unresolved names.

	\param name		The name being represented.
	\return			The new name expression node (expr_t::e::string_val).
*/
expr_t *new_name_expr (const char *name);

/** Create a new string constant expression node.

	\param string_val	The string constant being represented.
	\return			The new string constant expression node
					(expr_t::e::string_val).
*/
expr_t *new_string_expr (const char *string_val);

/** Create a new float constant expression node.

	\param float_val	The float constant being represented.
	\return			The new float constant expression node
					(expr_t::e::float_val).
*/
expr_t *new_float_expr (float float_val);

/** Create a new vector constant expression node.

	\param vector_val	The vector constant being represented.
	\return			The new vector constant expression node
					(expr_t::e::vector_val).
*/
expr_t *new_vector_expr (float *vector_val);

/** Create a new entity constant expression node.

	\param entity_val	The entity constant being represented.
	\return			The new entity constant expression node
					(expr_t::e::entity_val).
*/
expr_t *new_entity_expr (int entity_val);

/** Create a new field constant expression node.

	\param field_val	XXX
	\param type		The type of the field.
	\param def
	\return			The new field constant expression node
					(expr_t::e::field_val).
*/
expr_t *new_field_expr (int field_val, struct type_s *type, struct def_s *def);

/** Create a new function constant expression node.

	\param func_val	The function constant being represented.
	\return			The new function constant expression node
					(expr_t::e::func_val).
*/
expr_t *new_func_expr (int func_val);

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
expr_t *new_quaternion_expr (float *quaternion_val);

/** Create a new integer constant expression node.

	\param integer_val	The integer constant being represented.
	\return			The new integer constant expression node
					(expr_t::e::integer_val).
*/
expr_t *new_integer_expr (int integer_val);

/** Create a new unsigned integer constant expression node.

	\param uinteger_val	The unsigned integer constant being represented.
	\return			The new unsigned integer constant expression node
					(expr_t::e::uinteger_val).
*/
expr_t *new_uinteger_expr (unsigned int uinteger_val);

/** Create a new short constant expression node.

	\param short_val	The short constant being represented.
	\return			The new short constant expression node
					(expr_t::e::short_val).
*/
expr_t *new_short_expr (short short_val);

/**	Check of the expression refers to a constant value.

	\param e		The expression to check.
	\return			True if the expression is constant.
*/
int is_constant (expr_t *e);

/**	Check if the op-code is a comparison.

	\param op		The op-code to check.
	\return			True if the op-code is a comparison operator.
*/
int is_compare (int op);

/**	Check if the op-code is a math operator.

	\param op		The op-code to check.
	\return			True if the op-code is a math operator.
*/
int is_math (int op);

/**	Check if the op-code is a logic operator.

	\param op		The op-code to check.
	\return			True if the op-code is a logic operator.
*/
int is_logic (int op);

/**	Convert a constant def to a constant expression.

	\param var		The def to convert.
	\return			A new constant expression of the appropriate type with
					the value of the constant def, or \a var if neither a def
					nor a constant def.
*/
expr_t *constant_expr (expr_t *var);

expr_t *new_bind_expr (expr_t *e1, expr_t *e2);
expr_t *new_self_expr (void);
expr_t *new_this_expr (void);
expr_t *new_ret_expr (struct type_s *type);
expr_t *new_param_expr (struct type_s *type, int num);
expr_t *new_move_expr (expr_t *e1, expr_t *e2, struct type_s *type);

void inc_users (expr_t *e);
void dec_users (expr_t *e);
void convert_name (expr_t *e);

expr_t *append_expr (expr_t *block, expr_t *e);

void print_expr (expr_t *e);

void convert_int (expr_t *e);
void convert_uint (expr_t *e);
void convert_short (expr_t *e);
void convert_uint_int (expr_t *e);
void convert_int_uint (expr_t *e);
void convert_short_int (expr_t *e);
void convert_short_uint (expr_t *e);
void convert_nil (expr_t *e, struct type_s *t);

expr_t *test_expr (expr_t *e, int test);
void backpatch (ex_list_t *list, expr_t *label);
expr_t *convert_bool (expr_t *e, int block);
expr_t *bool_expr (int op, expr_t *label, expr_t *e1, expr_t *e2);
expr_t *binary_expr (int op, expr_t *e1, expr_t *e2);
expr_t *asx_expr (int op, expr_t *e1, expr_t *e2);
expr_t *unary_expr (int op, expr_t *e);
expr_t *build_function_call (expr_t *fexpr, struct type_s *ftype,
							 expr_t *params);
expr_t *function_expr (expr_t *e1, expr_t *e2);
struct function_s;
expr_t *return_expr (struct function_s *f, expr_t *e);
expr_t *conditional_expr (expr_t *cond, expr_t *e1, expr_t *e2);
expr_t *incop_expr (int op, expr_t *e, int postop);
expr_t *array_expr (expr_t *array, expr_t *index);
expr_t *pointer_expr (expr_t *pointer);
expr_t *address_expr (expr_t *e1, expr_t *e2, struct type_s *t);
expr_t *assign_expr (expr_t *e1, expr_t *e2);
expr_t *cast_expr (struct type_s *t, expr_t *e);

void init_elements (struct def_s *def, expr_t *eles);

expr_t *error (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));
expr_t *warning (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));
expr_t *notice (expr_t *e, const char *fmt, ...) __attribute__((format(printf, 2,3)));

const char *get_op_string (int op);

extern int lineno_base;

struct keywordarg_s;
struct class_type_s;
expr_t *selector_expr (struct keywordarg_s *selector);
expr_t *protocol_expr (const char *protocol);
expr_t *encode_expr (struct type_s *type);
expr_t *super_expr (struct class_type_s *class_type);
expr_t *message_expr (expr_t *receiver, struct keywordarg_s *message);
expr_t *sizeof_expr (expr_t *expr, struct type_s *type);

expr_t *fold_constants (expr_t *e);

//@}

#endif//__expr_h
