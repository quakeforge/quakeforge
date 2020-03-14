/*
	statements.c

	Internal statements

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/06/18

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

#include "qfalloca.h"

#include "QF/alloc.h"
#include "QF/mathlib.h"
#include "QF/va.h"

#include "dags.h"
#include "diagnostic.h"
#include "dot.h"
#include "expr.h"
#include "function.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "statements.h"
#include "strpool.h"
#include "symtab.h"
#include "type.h"
#include "value.h"
#include "qc-parse.h"

static const char *op_type_names[] = {
	"op_def",
	"op_value",
	"op_label",
	"op_temp",
	"op_alias",
	"op_nil",
};

const char *
optype_str (op_type_e type)
{
	if (type > op_temp)
		return "<invalid op_type>";
	return op_type_names[type];
}

const char *
operand_string (operand_t *op)
{
	if (!op)
		return "";
	switch (op->op_type) {
		case op_def:
			return op->o.def->name;
		case op_value:
			switch (op->o.value->lltype) {
				case ev_string:
					return va ("\"%s\"",
							   quote_string (op->o.value->v.string_val));
				case ev_double:
					return va ("%g", op->o.value->v.double_val);
				case ev_float:
					return va ("%g", op->o.value->v.float_val);
				case ev_vector:
					return va ("'%g %g %g'",
							   op->o.value->v.vector_val[0],
							   op->o.value->v.vector_val[1],
							   op->o.value->v.vector_val[2]);
				case ev_quat:
					return va ("'%g %g %g %g'",
							   op->o.value->v.quaternion_val[0],
							   op->o.value->v.quaternion_val[1],
							   op->o.value->v.quaternion_val[2],
							   op->o.value->v.quaternion_val[3]);
				case ev_pointer:
					if (op->o.value->v.pointer.def) {
						return va ("ptr %s+%d",
								   op->o.value->v.pointer.def->name,
								   op->o.value->v.pointer.val);
					} else {
						return va ("ptr %d", op->o.value->v.pointer.val);
					}
				case ev_field:
					return va ("field %d", op->o.value->v.pointer.val);
				case ev_entity:
					return va ("ent %d", op->o.value->v.integer_val);
				case ev_func:
					return va ("func %d", op->o.value->v.integer_val);
				case ev_integer:
					return va ("int %d", op->o.value->v.integer_val);
				case ev_uinteger:
					return va ("uint %u", op->o.value->v.uinteger_val);
				case ev_short:
					return va ("short %d", op->o.value->v.short_val);
				case ev_void:
					return "(void)";
				case ev_invalid:
					return "(invalid)";
				case ev_type_count:
					return "(type_count)";
			}
			break;
		case op_label:
			return op->o.label->name;
		case op_temp:
			if (op->o.tempop.alias)
				return va ("<tmp %s %p:%d:%p:%d:%d>",
						   pr_type_name[op->type->type],
						   op, op->o.tempop.users,
						   op->o.tempop.alias,
						   op->o.tempop.offset,
						   op->o.tempop.alias->o.tempop.users);
			return va ("<tmp %s %p:%d>", pr_type_name[op->o.tempop.type->type],
					   op, op->o.tempop.users);
		case op_alias:
			{
				const char *alias = operand_string (op->o.alias);
				char       *buf = alloca (strlen (alias) + 1);
				strcpy (buf, alias);
				return va ("alias(%s,%s)", pr_type_name[op->type->type], buf);
			}
		case op_nil:
			return va ("nil");
	}
	return ("??");
}

void
print_operand (operand_t *op)
{
	switch (op->op_type) {
		case op_def:
			printf ("(%s) ", pr_type_name[op->type->type]);
			printf ("%s", op->o.def->name);
			break;
		case op_value:
			printf ("(%s) ", pr_type_name[op->type->type]);
			switch (op->o.value->lltype) {
				case ev_string:
					printf ("\"%s\"", op->o.value->v.string_val);
					break;
				case ev_double:
					printf ("%g", op->o.value->v.double_val);
					break;
				case ev_float:
					printf ("%g", op->o.value->v.float_val);
					break;
				case ev_vector:
					printf ("'%g", op->o.value->v.vector_val[0]);
					printf (" %g", op->o.value->v.vector_val[1]);
					printf (" %g'", op->o.value->v.vector_val[2]);
					break;
				case ev_quat:
					printf ("'%g", op->o.value->v.quaternion_val[0]);
					printf (" %g", op->o.value->v.quaternion_val[1]);
					printf (" %g", op->o.value->v.quaternion_val[2]);
					printf (" %g'", op->o.value->v.quaternion_val[3]);
					break;
				case ev_pointer:
					printf ("(%s)[%d]",
							pr_type_name[op->o.value->v.pointer.type->type],
							op->o.value->v.pointer.val);
					break;
				case ev_field:
					printf ("%d", op->o.value->v.pointer.val);
					break;
				case ev_entity:
				case ev_func:
				case ev_integer:
					printf ("%d", op->o.value->v.integer_val);
					break;
				case ev_uinteger:
					printf ("%u", op->o.value->v.uinteger_val);
					break;
				case ev_short:
					printf ("%d", op->o.value->v.short_val);
					break;
				case ev_void:
				case ev_invalid:
				case ev_type_count:
					internal_error (op->expr, "weird value type");
			}
			break;
		case op_label:
			printf ("block %p", op->o.label->dest);
			break;
		case op_temp:
			printf ("tmp (%s) %p", pr_type_name[op->type->type], op);
			if (op->o.tempop.def)
				printf (" %s", op->o.tempop.def->name);
			break;
		case op_alias:
			printf ("alias(%s,", pr_type_name[op->type->type]);
			print_operand (op->o.alias);
			printf (")");
			break;
		case op_nil:
			printf ("nil");
			break;
	}
}

void
print_statement (statement_t *s)
{
	printf ("(%s, ", s->opcode);
	if (s->opa)
		print_operand (s->opa);
	printf (", ");
	if (s->opb)
		print_operand (s->opb);
	printf (", ");
	if (s->opc)
		print_operand (s->opc);
	printf (")\n");
}

static sblock_t *sblocks_freelist;
static statement_t *statements_freelist;
static operand_t *operands_freelist;

sblock_t *
new_sblock (void)
{
	sblock_t   *sblock;
	ALLOC (256, sblock_t, sblocks, sblock);
	sblock->tail = &sblock->statements;
	return sblock;
}

void
sblock_add_statement (sblock_t *sblock, statement_t *statement)
{
	// this should normally be null, but might be inserting
	statement->next = *sblock->tail;
	*sblock->tail = statement;
	sblock->tail = &statement->next;
}

statement_t *
new_statement (st_type_t type, const char *opcode, expr_t *expr)
{
	statement_t *statement;
	ALLOC (256, statement_t, statements, statement);
	statement->type = type;
	statement->opcode = save_string (opcode);
	statement->expr = expr;
	return statement;
}

static operand_t *
new_operand (op_type_e op, expr_t *expr, void *return_addr)
{
	operand_t *operand;
	ALLOC (256, operand_t, operands, operand);
	operand->op_type = op;
	operand->expr = expr;
	operand->return_addr = return_addr;
	return operand;
}

void
free_operand (operand_t *op)
{
	FREE (operands, op);
}

static void
free_statement (statement_t *s)
{
//	if (s->opa)
//		free_operand (s->opa);
//	if (s->opb)
//		free_operand (s->opb);
//	if (s->opc)
//		free_operand (s->opc);
	FREE (statements, s);
}

static void
free_sblock (sblock_t *sblock)
{
	while (sblock->statements) {
		statement_t *s = sblock->statements;
		sblock->statements = s->next;
		free_statement (s);
	}
	FREE (sblocks, sblock);
}

operand_t *
nil_operand (type_t *type, expr_t *expr)
{
	operand_t  *op;
	op = new_operand (op_nil, expr, __builtin_return_address (0));
	op->type = type;
	op->size = type_size (type);
	return op;
}

operand_t *
def_operand (def_t *def, type_t *type, expr_t *expr)
{
	operand_t  *op;

	if (!type)
		type = def->type;
	op = new_operand (op_def, expr, __builtin_return_address (0));
	op->type = type;
	op->size = type_size (type);
	op->o.def = def;
	return op;
}

operand_t *
return_operand (type_t *type, expr_t *expr)
{
	symbol_t   *return_symbol;
	return_symbol = make_symbol (".return", &type_param, pr.symtab->space,
								 sc_extern);
	return def_operand (return_symbol->s.def, type, expr);
}

operand_t *
value_operand (ex_value_t *value, expr_t *expr)
{
	operand_t  *op;
	op = new_operand (op_value, expr, __builtin_return_address (0));
	op->type = value->type;
	op->o.value = value;
	return op;
}

operand_t *
temp_operand (type_t *type, expr_t *expr)
{
	operand_t  *op = new_operand (op_temp, expr, __builtin_return_address (0));

	op->o.tempop.type = type;
	op->type = type;
	op->size = type_size (type);
	return op;
}

int
tempop_overlap (tempop_t *t1, tempop_t *t2)
{
	int         offs1 = t1->offset;
	int         offs2 = t2->offset;
	int         size1 = type_size (t1->type);
	int         size2 = type_size (t2->type);

	if (t1->alias) {
		offs1 += t1->alias->o.tempop.offset;
	}
	if (t2->alias) {
		offs2 += t2->alias->o.tempop.offset;
	}
	if (offs1 <= offs2 && offs1 + size1 >= offs2 + size2)
		return 2;	// t1 fully overlaps t2
	if (offs1 < offs2 + size2 && offs2 < offs1 + size1)
		return 1;	// t1 and t2 at least partially overlap
	return 0;
}

int
tempop_visit_all (tempop_t *tempop, int overlap,
			   int (*visit) (tempop_t *, void *), void *data)
{
	tempop_t   *start_tempop = tempop;
	operand_t  *top;
	int         ret;

	if ((ret = visit (tempop, data)))
		return ret;
	if (tempop->alias) {
		top = tempop->alias;
		if (top->op_type != op_temp) {
			internal_error (top->expr, "temp alias of non-temp operand");
		}
		tempop = &top->o.tempop;
		if ((ret = visit (tempop, data)))
			return ret;
	} else {
		overlap = 0;
	}
	for (top = tempop->alias_ops; top; top = top->next) {
		if (top->op_type != op_temp) {
			internal_error (top->expr, "temp alias of non-temp operand");
		}
		tempop = &top->o.tempop;
		if (tempop == start_tempop)
			continue;
		if (overlap && tempop_overlap (tempop, start_tempop) < overlap)
			continue;
		if ((ret = visit (tempop, data)))
			return ret;
	}
	return 0;
}

operand_t *
alias_operand (type_t *type, operand_t *op, expr_t *expr)
{
	operand_t  *aop;

	if (type_size (type) != type_size (op->type)) {
		internal_error (op->expr,
						"aliasing operand with type of different size: %d, %d",
						type_size (type), type_size (op->type));
	}
	aop = new_operand (op_alias, expr, __builtin_return_address (0));
	aop->o.alias = op;
	aop->type = type;
	aop->size = type_size (type);
	return aop;
}

operand_t *
label_operand (expr_t *label)
{
	operand_t  *lop;

	if (label->type != ex_label) {
		internal_error (label, "not a label expression");
	}
	lop = new_operand (op_label, label, __builtin_return_address (0));
	lop->o.label = &label->e.label;
	return lop;
}

static operand_t *
short_operand (short short_val, expr_t *expr)
{
	ex_value_t *val = new_short_val (short_val);
	return value_operand (val, expr);
}

static const char *
convert_op (int op)
{
	switch (op) {
		case OR:	return "||";
		case AND:	return "&&";
		case EQ:	return "==";
		case NE:	return "!=";
		case LE:	return "<=";
		case GE:	return ">=";
		case LT:	return "<";
		case GT:	return ">";
		case '=':	return "=";
		case '+':	return "+";
		case '-':	return "-";
		case '*':	return "*";
		case '/':	return "/";
		case '%':	return "%";
		case MOD:	return "%%";
		case '&':	return "&";
		case '|':	return "|";
		case '^':	return "^";
		case '~':	return "~";
		case '!':	return "!";
		case SHL:	return "<<";
		case SHR:	return ">>";
		case '.':	return ".";
		case 'i':	return "<IF>";
		case 'n':	return "<IFNOT>";
		case IFBE:	return "<IFBE>";
		case IFB:	return "<IFB>";
		case IFAE:	return "<IFAE>";
		case IFA:	return "<IFA>";
		case 'm':	return "<MOVE>";
		case 'M':	return "<MOVEP>";
		default:
			return 0;
	}
}

int
statement_is_cond (statement_t *s)
{
	if (!s)
		return 0;
	return !strncmp (s->opcode, "<IF", 3);
}

int
statement_is_goto (statement_t *s)
{
	if (!s)
		return 0;
	return !strcmp (s->opcode, "<GOTO>");
}

int
statement_is_jumpb (statement_t *s)
{
	if (!s)
		return 0;
	return !strcmp (s->opcode, "<JUMPB>");
}

int
statement_is_call (statement_t *s)
{
	if (!s)
		return 0;
	if (!strncmp (s->opcode, "<CALL", 5))
		return 1;
	if (!strncmp (s->opcode, "<RCALL", 6))
		return 2;
	return 0;
}

int
statement_is_return (statement_t *s)
{
	if (!s)
		return 0;
	return !strncmp (s->opcode, "<RETURN", 7);
}

sblock_t *
statement_get_target (statement_t *s)
{
	if (statement_is_cond (s))
		return s->opb->o.label->dest;
	if (statement_is_goto (s))
		return s->opa->o.label->dest;
	return 0;
}

sblock_t **
statement_get_targetlist (statement_t *s)
{
	sblock_t  **target_list;
	int         count = 0, i;
	def_t      *table = 0;
	element_t  *e;

	if (statement_is_cond (s)) {
		count = 1;
	} else if (statement_is_goto (s)) {
		count = 1;
	} else if (statement_is_jumpb (s)) {
		table = s->opa->o.def;
		count = table->type->t.array.size;
	}
	target_list = malloc ((count + 1) * sizeof (sblock_t *));
	target_list[count] = 0;
	if (statement_is_cond (s)) {
		target_list[0] = statement_get_target (s);
	} else if (statement_is_goto (s)) {
		target_list[0] = statement_get_target (s);
	} else if (statement_is_jumpb (s)) {
		if (table->alias)
			internal_error (s->opa->expr, "aliased jump table");
		e = table->initializer->e.compound.head;	//FIXME check!!!
		for (i = 0; i < count; e = e->next, i++)
			target_list[i] = e->expr->e.labelref.label->dest;
	}
	return target_list;
}

static void
invert_conditional (statement_t *s)
{
	if (!strcmp (s->opcode, "<IF>"))
		s->opcode = "<IFNOT>";
	else if (!strcmp (s->opcode, "<IFNOT>"))
		s->opcode = "<IF>";
	else if (!strcmp (s->opcode, "<IFBE>"))
		s->opcode = "<IFA>";
	else if (!strcmp (s->opcode, "<IFB>"))
		s->opcode = "<IFAE>";
	else if (!strcmp (s->opcode, "<IFAE>"))
		s->opcode = "<IFB>";
	else if (!strcmp (s->opcode, "<IFA>"))
		s->opcode = "<IFBE>";
}

typedef sblock_t *(*statement_f) (sblock_t *, expr_t *);
typedef sblock_t *(*expr_f) (sblock_t *, expr_t *, operand_t **);

static sblock_t *statement_subexpr (sblock_t *sblock, expr_t *e,
									operand_t **op);
static sblock_t *statement_slist (sblock_t *sblock, expr_t *e);

static sblock_t *
statement_branch (sblock_t *sblock, expr_t *e)
{
	statement_t *s = 0;
	const char *opcode;

	if (e->type == ex_uexpr && e->e.expr.op == 'g') {
		s = new_statement (st_flow, "<GOTO>", e);
		s->opa = label_operand (e->e.expr.e1);
	} else {
		if (e->e.expr.op == 'g') {
			s = new_statement (st_flow, "<JUMPB>", e);
			sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
			sblock = statement_subexpr (sblock, e->e.expr.e2, &s->opb);
		} else {
			opcode = convert_op (e->e.expr.op);
			s = new_statement (st_flow, opcode, e);
			sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
			s->opb = label_operand (e->e.expr.e2);
		}
	}

	sblock_add_statement (sblock, s);
	sblock->next = new_sblock ();
	return sblock->next;
}

static sblock_t *
expr_address (sblock_t *sblock, expr_t *e, operand_t **op)
{
	statement_t *s;
	s = new_statement (st_expr, "&", e);
	sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
	s->opc = temp_operand (e->e.expr.type, e);
	sblock_add_statement (sblock, s);
	*(op) = s->opc;
	return sblock;
}

static sblock_t *
operand_address (sblock_t *sblock, operand_t *reference, operand_t **op,
				 expr_t *e)
{
	statement_t *s;
	type_t     *type;

	switch (reference->op_type) {
		case op_def:
		case op_temp:
		case op_alias:
			// build an address expression so dags can extract the correct
			// type. address_expr cannot be used because reference might not
			// be something it likes
			e = expr_file_line (new_unary_expr ('&', e), e);
			type = pointer_type (reference->type);
			e->e.expr.type = type;

			s = new_statement (st_expr, "&", e);
			s->opa = reference;
			s->opc = temp_operand (type, e);
			sblock_add_statement (sblock, s);
			if (op) {
				*(op) = s->opc;
			}
			return sblock;
		case op_value:
		case op_label:
		case op_nil:
			break;
	}
	internal_error ((*op)->expr,
					"invalid operand type for operand address: %s",
					op_type_names[reference->op_type]);
}

static __attribute__((pure)) int
is_const_ptr (expr_t *e)
{
	if ((e->type != ex_value || e->e.value->lltype != ev_pointer)
		|| !(POINTER_VAL (e->e.value->v.pointer) >= 0
			 && POINTER_VAL (e->e.value->v.pointer) < 65536)) {
		return 1;
	}
	return 0;
}

static __attribute__((pure)) int
is_indirect (expr_t *e)
{
	if (e->type == ex_expr && e->e.expr.op == '.')
		return 1;
	if (!(e->type == ex_uexpr && e->e.expr.op == '.'))
		return 0;
	return is_const_ptr (e->e.expr.e1);
}

static sblock_t *
expr_assign_copy (sblock_t *sblock, expr_t *e, operand_t **op, operand_t *src)
{
	statement_t *s;
	expr_t     *dst_expr = e->e.expr.e1;
	expr_t     *src_expr = e->e.expr.e2;
	type_t     *dst_type = get_type (dst_expr);
	type_t     *src_type = get_type (src_expr);
	unsigned    count;
	expr_t     *count_expr;
	operand_t  *dst = 0;
	operand_t  *size = 0;
	static const char *opcode_sets[][2] = {
		{"<MOVE>", "<MOVEP>"},
		{"<MEMSET>", "<MEMSETP>"},
	};
	const unsigned max_count = 1 << 16;
	const char **opcode_set = opcode_sets[0];
	const char *opcode;
	int         need_ptr = 0;
	//operand_t  *dummy;

	if ((src && src->op_type == op_nil) || src_expr->type == ex_nil) {
		// switch to memset because nil is type agnostic 0 and structures
		// can be any size
		src_expr = new_integer_expr (0);
		sblock = statement_subexpr (sblock, src_expr, &src);
		opcode_set = opcode_sets[1];
		if (op) {
			*op = nil_operand (dst_type, src_expr);
		}
		if (is_indirect (dst_expr)) {
			goto dereference_dst;
		}
	} else {
		if (!src) {
			// This is the very right-hand node of a non-nil assignment chain
			// (there may be more chains somwhere within src_expr, but they
			// are not part of this chain as they are separated by another
			// expression).
			sblock = statement_subexpr (sblock, src_expr, &src);
		}
		// send the source operand back up through the assignment chain
		// before modifying src if its address is needed
		if (op) {
			*op = src;
		}
		if (is_indirect (dst_expr) || is_indirect (src_expr)) {
			sblock = operand_address (sblock, src, &src, src_expr);
			goto dereference_dst;
		}
	}
	if (0) {
dereference_dst:
		// dst_expr is a dereferenced pointer, so need to un-dereference it
		// to get the pointer and switch to storep instructions.
		dst_expr = expr_file_line (address_expr (dst_expr, 0, 0), e);
		need_ptr = 1;
	}
	sblock = statement_subexpr (sblock, dst_expr, &dst);

	if (type_size (dst_type) != type_size (src_type)) {
		bug (e, "dst and src sizes differ in expr_assign_copy: %d %d",
			 type_size (dst_type), type_size (src_type));
	}
	count = min (type_size (dst_type), type_size (src_type));
	if (count < (1 << 16)) {
		count_expr = expr_file_line (new_short_expr (count), e);
	} else {
		count_expr = expr_file_line (new_integer_expr (count), e);
	}
	sblock = statement_subexpr (sblock, count_expr, &size);

	if (count < max_count && !need_ptr) {
		opcode = opcode_set[0];
	} else {
		opcode = opcode_set[1];
	}

	s = new_statement (st_move, opcode, e);
	s->opa = src;
	s->opb = size;
	s->opc = dst;
	sblock_add_statement (sblock, s);
	return sblock;
}

static sblock_t *
expr_assign (sblock_t *sblock, expr_t *e, operand_t **op)
{
	statement_t *s;
	expr_t     *src_expr = e->e.expr.e2;
	expr_t     *dst_expr = e->e.expr.e1;
	type_t     *dst_type = get_type (dst_expr);
	operand_t  *src = 0;
	operand_t  *dst = 0;
	operand_t  *ofs = 0;
	const char *opcode = convert_op (e->e.expr.op);
	st_type_t   type;

	if (src_expr->type == ex_expr && src_expr->e.expr.op == '=') {
		sblock = statement_subexpr (sblock, src_expr, &src);
		if (is_structural (dst_type)) {
			return expr_assign_copy (sblock, e, op, src);
		}
		if (is_indirect (dst_expr)) {
			goto dereference_dst;
		} else {
			sblock = statement_subexpr (sblock, dst_expr, &dst);
		}
	} else {
		if (is_structural (dst_type)) {
			return expr_assign_copy (sblock, e, op, src);
		}

		if (is_indirect (dst_expr)) {
			// If both dst_expr and src_expr are indirect, then a staging temp
			// is needed, but emitting src_expr first generates that temp
			// because src is null. If src_expr is not indirect and is a simple
			// variable reference, then just the ref will be generated and thus
			// will be assigned to the dereferenced destination. If src_expr
			// is not simple, then a temp will be generated, so all good.
			sblock = statement_subexpr (sblock, src_expr, &src);
			goto dereference_dst;
		} else {
			// dst_expr is direct and known to be an l-value, so emitting
			// its expression will simply generate a reference to that l-value
			// which will be used as the default location to store src_expr's
			// result
			sblock = statement_subexpr (sblock, dst_expr, &dst);
			src = dst;
			sblock = statement_subexpr (sblock, src_expr, &src);
		}
	}
	type = st_assign;

	if (0) {
dereference_dst:
		// dst_expr is a dereferenced pointer, so need to un-dereference it
		// to get the pointer and switch to storep instructions.
		dst_expr = expr_file_line (address_expr (dst_expr, 0, 0), e);
		opcode = ".=";	// FIXME find a nicer representation (lose strings?)
		if (dst_expr->type == ex_expr && !is_const_ptr (dst_expr->e.expr.e1)) {
			sblock = statement_subexpr (sblock, dst_expr->e.expr.e1, &dst);
			sblock = statement_subexpr (sblock, dst_expr->e.expr.e2, &ofs);
		} else {
			sblock = statement_subexpr (sblock, dst_expr, &dst);
			ofs = 0;
		}
		type = st_ptrassign;
	}
	if (op) {
		*op = src;
	}
	if (src == dst) {
		return sblock;
	}

	s = new_statement (type, opcode, e);
	s->opa = src;
	s->opb = dst;
	s->opc = ofs;
	sblock_add_statement (sblock, s);
	return sblock;
}

static sblock_t *
expr_move (sblock_t *sblock, expr_t *e, operand_t **op)
{
	statement_t *s;
	type_t     *type = e->e.expr.type;
	expr_t     *dst_expr = e->e.expr.e1;
	expr_t     *src_expr = e->e.expr.e2;
	expr_t     *size_expr;
	operand_t  *dst = 0;
	operand_t  *src = 0;
	operand_t  *size = 0;

	if (!op)
		op = &dst;
	size_expr = new_short_expr (type_size (type));
	sblock = statement_subexpr (sblock, dst_expr, op);
	dst = *op;
	sblock = statement_subexpr (sblock, src_expr, &src);
	sblock = statement_subexpr (sblock, size_expr, &size);
	s = new_statement (st_move, convert_op (e->e.expr.op), e);
	s->opa = src;
	s->opb = size;
	s->opc = dst;
	sblock_add_statement (sblock, s);

	return sblock;
}

static sblock_t *
vector_call (sblock_t *sblock, expr_t *earg, expr_t *param, int ind,
			 operand_t **op)
{
	//FIXME this should be done in the expression tree
	expr_t     *a, *v, *n;
	int         i;
	static const char *names[] = {"x", "y", "z"};

	for (i = 0; i < 3; i++) {
		n = new_name_expr (names[i]);
		v = new_float_expr (earg->e.value->v.vector_val[i]);
		a = assign_expr (field_expr (param, n), v);
		param = new_param_expr (get_type (earg), ind);
		a->line = earg->line;
		a->file = earg->file;
		sblock = statement_slist (sblock, a);
	}
	sblock = statement_subexpr (sblock, param, op);
	return sblock;
}


static sblock_t *
expr_call (sblock_t *sblock, expr_t *call, operand_t **op)
{
	expr_t     *func = call->e.expr.e1;
	expr_t     *args = call->e.expr.e2;
	expr_t     *a;
	expr_t     *param;
	operand_t  *arguments[2] = {0, 0};
	int         count = 0;
	int         ind;
	const char *opcode;
	const char *pref = "";
	statement_t *s;

	for (a = args; a; a = a->next)
		count++;
	ind = count;
	for (a = args; a; a = a->next) {
		ind--;
		param = new_param_expr (get_type (a), ind);
		if (count && options.code.progsversion != PROG_ID_VERSION && ind < 2) {
			pref = "R";
			sblock = statement_subexpr (sblock, param, &arguments[ind]);
			if (options.code.vector_calls && a->type == ex_value
				&& a->e.value->lltype == ev_vector)
				sblock = vector_call (sblock, a, param, ind, &arguments[ind]);
			else
				sblock = statement_subexpr (sblock, a, &arguments[ind]);
			continue;
		}
		if (is_struct (get_type (param))) {
			//FIXME this should be done in the expression tree
			expr_t     *mov = assign_expr (param, a);
			mov->line = a->line;
			mov->file = a->file;
			sblock = statement_slist (sblock, mov);
		} else {
			if (options.code.vector_calls && a->type == ex_value
				&& a->e.value->lltype == ev_vector) {
				sblock = vector_call (sblock, a, param, ind, 0);
			} else {
				operand_t  *p = 0;
				operand_t  *arg;
				sblock = statement_subexpr (sblock, param, &p);
				arg = p;
				sblock = statement_subexpr (sblock, a, &arg);
				if (arg != p) {
					s = new_statement (st_assign, "=", a);
					s->opa = arg;
					s->opb = p;
					sblock_add_statement (sblock, s);
				}
			}
		}
	}
	opcode = va ("<%sCALL%d>", pref, count);
	s = new_statement (st_func, opcode, call);
	sblock = statement_subexpr (sblock, func, &s->opa);
	s->opb = arguments[0];
	s->opc = arguments[1];
	sblock_add_statement (sblock, s);
	sblock->next = new_sblock ();
	return sblock->next;
}

static statement_t *
lea_statement (operand_t *pointer, operand_t *offset, expr_t *e)
{
	statement_t *s = new_statement (st_expr, "&", e);
	s->opa = pointer;
	s->opb = offset;
	s->opc = temp_operand (&type_pointer, e);
	return s;
}

static statement_t *
address_statement (operand_t *value, expr_t *e)
{
	statement_t *s = new_statement (st_expr, "&", e);
	s->opa = value;
	s->opc = temp_operand (&type_pointer, e);
	return s;
}

static sblock_t *
expr_deref (sblock_t *sblock, expr_t *deref, operand_t **op)
{
	type_t     *type = deref->e.expr.type;
	expr_t     *e;

	e = deref->e.expr.e1;
	if (e->type == ex_uexpr && e->e.expr.op == '&'
		&& e->e.expr.e1->type == ex_symbol) {
		if (e->e.expr.e1->e.symbol->sy_type != sy_var)
			internal_error (e, "address of non-var");
		*op = def_operand (e->e.expr.e1->e.symbol->s.def, type, e);
	} else if (e->type == ex_expr && e->e.expr.op == '&') {
		statement_t *s;
		operand_t  *ptr = 0;
		operand_t  *offs = 0;
		sblock = statement_subexpr (sblock, e->e.expr.e1, &ptr);
		sblock = statement_subexpr (sblock, e->e.expr.e2, &offs);
		if (!*op)
			*op = temp_operand (type, e);
		if (low_level_type (type) == ev_void) {
			operand_t  *src_addr;
			operand_t  *dst_addr;

			s = lea_statement (ptr, offs, e);
			src_addr = s->opc;
			sblock_add_statement (sblock, s);

			//FIXME an address immediate would be nice.
			s = address_statement (*op, e);
			dst_addr = s->opc;
			sblock_add_statement (sblock, s);

			s = new_statement (st_move, "<MOVEP>", deref);
			s->opa = src_addr;
			s->opb = short_operand (type_size (type), e);
			s->opc = dst_addr;
			sblock_add_statement (sblock, s);
		} else {
			s = new_statement (st_expr, ".", deref);
			s->opa = ptr;
			s->opb = offs;
			s->opc = *op;
			sblock_add_statement (sblock, s);
		}
	} else if (e->type == ex_value && e->e.value->lltype == ev_pointer) {
		ex_pointer_t *ptr = &e->e.value->v.pointer;
		*op = def_operand (alias_def (ptr->def, ptr->type, ptr->val),
						   ptr->type, e);
	} else {
		statement_t *s;
		operand_t  *ptr = 0;

		sblock = statement_subexpr (sblock, e, &ptr);
		if (!*op)
			*op = temp_operand (type, e);
		s = new_statement (st_expr, ".", deref);
		s->opa = ptr;
		s->opb = short_operand (0, e);
		s->opc = *op;
		sblock_add_statement (sblock, s);
	}
	return sblock;
}

static sblock_t *
expr_block (sblock_t *sblock, expr_t *e, operand_t **op)
{
	if (!e->e.block.result)
		internal_error (e, "block sub-expression without result");
	sblock = statement_slist (sblock, e->e.block.head);
	sblock = statement_subexpr (sblock, e->e.block.result, op);
	return sblock;
}

static sblock_t *
expr_alias (sblock_t *sblock, expr_t *e, operand_t **op)
{
	operand_t *aop = 0;
	operand_t *top;
	type_t    *type;
	def_t     *def;
	int        offset = 0;

	if (e->type == ex_expr) {
		offset = expr_integer (e->e.expr.e2);
	}
	type = e->e.expr.type;
	sblock = statement_subexpr (sblock, e->e.expr.e1, &aop);
	if (type_compatible (aop->type, type)) {
		//FIXME type_compatible??? shouldn't that be type_size ==?
		if (offset) {
			internal_error (e, "offset alias of same size type");
		}
		*op = aop;
		return sblock;
	}
	if (aop->op_type == op_temp) {
		while (aop->o.tempop.alias) {
			aop = aop->o.tempop.alias;
			if (aop->op_type != op_temp)
				internal_error (e, "temp alias of non-temp var");
			if (aop->o.tempop.alias)
				bug (e, "aliased temp alias");
		}
		for (top = aop->o.tempop.alias_ops; top; top = top->next) {
			if (top->type == type && top->o.tempop.offset == offset) {
				break;
			}
		}
		if (!top) {
			top = temp_operand (type, e);
			top->o.tempop.alias = aop;
			top->o.tempop.offset = offset;
			top->next = aop->o.tempop.alias_ops;
			aop->o.tempop.alias_ops = top;
		}
		*op = top;
	} else if (aop->op_type == op_def) {
		def = aop->o.def;
		while (def->alias)
			def = def->alias;
		*op = def_operand (alias_def (def, type, offset), 0, e);
	} else if (aop->op_type == op_value) {
		*op = value_operand (aop->o.value, e);
		(*op)->type = type;
	} else {
		internal_error (e, "invalid alias target: %s: %s",
						optype_str (aop->op_type), operand_string (aop));
	}
	return sblock;
}

static sblock_t *
expr_expr (sblock_t *sblock, expr_t *e, operand_t **op)
{
	const char *opcode;
	statement_t *s;

	switch (e->e.expr.op) {
		case 'c':
			sblock = expr_call (sblock, e, op);
			break;
		case '=':
			sblock = expr_assign (sblock, e, op);
			break;
		case 'm':
		case 'M':
			sblock = expr_move (sblock, e, op);
			break;
		case 'A':
			sblock = expr_alias (sblock, e, op);
			break;
		default:
			opcode = convert_op (e->e.expr.op);
			if (!opcode)
				internal_error (e, "ice ice baby");
			s = new_statement (st_expr, opcode, e);
			sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
			sblock = statement_subexpr (sblock, e->e.expr.e2, &s->opb);
			if (!*op)
				*op = temp_operand (e->e.expr.type, e);
			s->opc = *op;
			sblock_add_statement (sblock, s);
			break;
	}
	return sblock;
}

static sblock_t *
expr_cast (sblock_t *sblock, expr_t *e, operand_t **op)
{
	type_t     *src_type;
	type_t     *type = e->e.expr.type;
	statement_t *s;

	src_type = get_type (e->e.expr.e1);
	if (is_scalar (src_type) && is_scalar (type)) {
		operand_t  *src = 0;
		sblock = statement_subexpr (sblock, e->e.expr.e1, &src);
		*op = temp_operand (e->e.expr.type, e);
		s = new_statement (st_expr, "<CONV>", e);
		s->opa = src;
		s->opc = *op;
		sblock_add_statement (sblock, s);
	} else {
		sblock = expr_alias (sblock, e, op);
	}
	return sblock;
}

static sblock_t *
expr_negate (sblock_t *sblock, expr_t *e, operand_t **op)
{
	expr_t     *neg;
	expr_t     *zero;

	zero = new_nil_expr ();
	zero->file = e->file;
	zero->line = e->line;
	convert_nil (zero, e->e.expr.type);
	neg = binary_expr ('-', zero, e->e.expr.e1);
	neg->file = e->file;
	neg->line = e->line;
	return statement_subexpr (sblock, neg, op);
}

static sblock_t *
expr_uexpr (sblock_t *sblock, expr_t *e, operand_t **op)
{
	const char *opcode;
	statement_t *s;

	switch (e->e.expr.op) {
		case '&':
			sblock = expr_address (sblock, e, op);
			break;
		case '.':
			sblock = expr_deref (sblock, e, op);
			break;
		case 'A':
			sblock = expr_alias (sblock, e, op);
			break;
		case 'C':
			sblock = expr_cast (sblock, e, op);
			break;
		case '-':
			// progs has no neg instruction!?!
			sblock = expr_negate (sblock, e, op);
			break;
		default:
			opcode = convert_op (e->e.expr.op);
			if (!opcode)
				internal_error (e, "ice ice baby");
			s = new_statement (st_expr, opcode, e);
			sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
			if (!*op)
				*op = temp_operand (e->e.expr.type, e);
			s->opc = *op;
			sblock_add_statement (sblock, s);
	}
	return sblock;
}

static sblock_t *
expr_symbol (sblock_t *sblock, expr_t *e, operand_t **op)
{
	symbol_t   *sym = e->e.symbol;

	if (sym->sy_type == sy_var) {
		*op = def_operand (sym->s.def, sym->type, e);
	} else if (sym->sy_type == sy_const) {
		*op = value_operand (sym->s.value, e);
	} else if (sym->sy_type == sy_func) {
		*op = def_operand (sym->s.func->def, 0, e);
	} else {
		internal_error (e, "unexpected symbol type: %s for %s",
						symtype_str (sym->sy_type), sym->name);
	}
	return sblock;
}

static sblock_t *
expr_temp (sblock_t *sblock, expr_t *e, operand_t **op)
{
	if (!e->e.temp.op)
		e->e.temp.op = temp_operand (e->e.temp.type, e);
	*op = e->e.temp.op;
	return sblock;
}

static sblock_t *
expr_vector_e (sblock_t *sblock, expr_t *e, operand_t **op)
{
	expr_t     *x, *y, *z, *w;
	expr_t     *s, *v;
	expr_t     *ax, *ay, *az, *aw;
	expr_t     *as, *av;
	expr_t     *tmp;
	type_t     *vec_type = get_type (e);
	int         file = pr.source_file;
	int         line = pr.source_line;

	pr.source_file = e->file;
	pr.source_line = e->line;

	tmp = new_temp_def_expr (vec_type);
	if (vec_type == &type_vector) {
		// guaranteed to have three elements
		x = e->e.vector.list;
		y = x->next;
		z = y->next;
		ax = new_name_expr ("x");
		ay = new_name_expr ("y");
		az = new_name_expr ("z");
		ax = assign_expr (field_expr (tmp, ax), x);
		ay = assign_expr (field_expr (tmp, ay), y);
		az = assign_expr (field_expr (tmp, az), z);
		sblock = statement_slist (sblock, ax);
		sblock = statement_slist (sblock, ay);
		sblock = statement_slist (sblock, az);
	} else {
		// guaranteed to have two or four elements
		if (e->e.vector.list->next->next) {
			// four vals: x, y, z, w
			x = e->e.vector.list;
			y = x->next;
			z = y->next;
			w = z->next;
			ax = new_name_expr ("x");
			ay = new_name_expr ("y");
			az = new_name_expr ("z");
			aw = new_name_expr ("w");
			ax = assign_expr (field_expr (tmp, ax), x);
			ay = assign_expr (field_expr (tmp, ay), y);
			az = assign_expr (field_expr (tmp, az), z);
			aw = assign_expr (field_expr (tmp, aw), w);
			sblock = statement_slist (sblock, ax);
			sblock = statement_slist (sblock, ay);
			sblock = statement_slist (sblock, az);
			sblock = statement_slist (sblock, aw);
		} else {
			// v, s
			v = e->e.vector.list;
			s = v->next;
			av = new_name_expr ("v");
			as = new_name_expr ("s");
			av = assign_expr (field_expr (tmp, av), v);
			as = assign_expr (field_expr (tmp, as), s);
			sblock = statement_slist (sblock, av);
			sblock = statement_slist (sblock, as);
		}
	}
	pr.source_file = file;
	pr.source_line = line;
	sblock = statement_subexpr (sblock, tmp, op);
	return sblock;
}

static sblock_t *
expr_nil (sblock_t *sblock, expr_t *e, operand_t **op)
{
	type_t     *nil = e->e.nil;
	expr_t     *ptr;
	if (!is_struct (nil) && !is_array (nil)) {
		*op = value_operand (new_nil_val (nil), e);
		return sblock;
	}
	ptr = expr_file_line (address_expr (new_temp_def_expr (nil), 0, 0), e);
	expr_file_line (ptr, e);
	sblock = statement_subexpr (sblock, ptr, op);
	e = expr_file_line (new_memset_expr (ptr, new_integer_expr (0), nil), e);
	sblock = statement_slist (sblock, e);
	return sblock;
}

static sblock_t *
expr_value (sblock_t *sblock, expr_t *e, operand_t **op)
{
	*op = value_operand (e->e.value, e);
	return sblock;
}

static sblock_t *
statement_subexpr (sblock_t *sblock, expr_t *e, operand_t **op)
{
	static expr_f sfuncs[] = {
		0,					// ex_error
		0,					// ex_state
		0,					// ex_bool
		0,					// ex_label
		0,					// ex_labelref
		expr_block,			// ex_block
		expr_expr,
		expr_uexpr,
		expr_symbol,
		expr_temp,
		expr_vector_e,		// ex_vector
		expr_nil,
		expr_value,
		0,					// ex_compound
		0,					// ex_memset
	};
	if (!e) {
		*op = 0;
		return sblock;
	}

	if (e->type > ex_memset)
		internal_error (e, "bad sub-expression type");
	if (!sfuncs[e->type])
		internal_error (e, "unexpected sub-expression type: %s",
						expr_names[e->type]);

	sblock = sfuncs[e->type] (sblock, e, op);
	return sblock;
}

static sblock_t *
statement_ignore (sblock_t *sblock, expr_t *e)
{
	return sblock;
}

static sblock_t *
statement_state (sblock_t *sblock, expr_t *e)
{
	statement_t *s;

	s = new_statement (st_state, "<STATE>", e);
	sblock = statement_subexpr (sblock, e->e.state.frame, &s->opa);
	sblock = statement_subexpr (sblock, e->e.state.think, &s->opb);
	sblock = statement_subexpr (sblock, e->e.state.step, &s->opc);
	sblock_add_statement (sblock, s);
	return sblock;
}

static void
build_bool_block (expr_t *block, expr_t *e)
{
	switch (e->type) {
		case ex_bool:
			build_bool_block (block, e->e.bool.e);
			return;
		case ex_label:
			e->next = 0;
			append_expr (block, e);
			return;
		case ex_expr:
			if (e->e.expr.op == OR || e->e.expr.op == AND) {
				build_bool_block (block, e->e.expr.e1);
				build_bool_block (block, e->e.expr.e2);
			} else if (e->e.expr.op == 'i') {
				e->next = 0;
				append_expr (block, e);
			} else if (e->e.expr.op == 'n') {
				e->next = 0;
				append_expr (block, e);
			} else {
				e->next = 0;
				append_expr (block, e);
			}
			return;
		case ex_uexpr:
			if (e->e.expr.op == 'g') {
				e->next = 0;
				append_expr (block, e);
				return;
			}
			break;
		case ex_block:
			if (!e->e.block.result) {
				expr_t     *t;
				for (e = e->e.block.head; e; e = t) {
					t = e->next;
					build_bool_block (block, e);
				}
				return;
			}
			break;
		default:
			;
	}
	internal_error (e, "bad boolean");
}

static int
is_goto_expr (expr_t *e)
{
	return e && e->type == ex_uexpr && e->e.expr.op == 'g';
}

static int
is_if_expr (expr_t *e)
{
	return e && e->type == ex_expr && e->e.expr.op == 'i';
}

static int
is_ifnot_expr (expr_t *e)
{
	return e && e->type == ex_expr && e->e.expr.op == 'n';
}

static sblock_t *
statement_bool (sblock_t *sblock, expr_t *e)
{
	expr_t    **s;
	expr_t     *l;
	expr_t     *block = new_block_expr ();

	build_bool_block (block, e);

	s = &block->e.block.head;
	while (*s) {
		if (is_if_expr (*s) && is_goto_expr ((*s)->next)) {
			l = (*s)->e.expr.e2;
			for (e = (*s)->next->next; e && e->type == ex_label; e = e->next) {
				if (e == l) {
					l->e.label.used--;
					e = *s;
					e->e.expr.op = 'n';
					e->e.expr.e2 = e->next->e.expr.e1;
					e->next = e->next->next;
					break;
				}
			}
			s = &(*s)->next;
		} else if (is_ifnot_expr (*s) && is_goto_expr ((*s)->next)) {
			l = (*s)->e.expr.e2;
			for (e = (*s)->next->next; e && e->type == ex_label; e = e->next) {
				if (e == l) {
					l->e.label.used--;
					e = *s;
					e->e.expr.op = 'i';
					e->e.expr.e2 = e->next->e.expr.e1;
					e->next = e->next->next;
					break;
				}
			}
			s = &(*s)->next;
		} else if (is_goto_expr (*s)) {
			l = (*s)->e.expr.e1;
			for (e = (*s)->next; e && e->type == ex_label; e = e->next) {
				if (e == l) {
					l->e.label.used--;
					*s = (*s)->next;
					l = 0;
					break;
				}
			}
			if (l)
				s = &(*s)->next;
		} else {
			s = &(*s)->next;
		}
	}
	sblock = statement_slist (sblock, block->e.block.head);
	return sblock;
}

static sblock_t *
statement_label (sblock_t *sblock, expr_t *e)
{
	if (sblock->statements) {
		sblock->next = new_sblock ();
		sblock = sblock->next;
	}
	if (e->e.label.used) {
		e->e.label.dest = sblock;
		e->e.label.next = sblock->labels;
		sblock->labels = &e->e.label;
	} else {
		if (e->e.label.symbol) {
			warning (e, "unused label %s", e->e.label.symbol->name);
		} else {
			debug (e, "dropping unused label %s", e->e.label.name);
		}
	}
	return sblock;
}

static sblock_t *
statement_block (sblock_t *sblock, expr_t *e)
{
	if (sblock->statements) {
		sblock->next = new_sblock ();
		sblock = sblock->next;
	}
	sblock = statement_slist (sblock, e->e.block.head);
	return sblock;
}

static sblock_t *
statement_expr (sblock_t *sblock, expr_t *e)
{
	switch (e->e.expr.op) {
		case 'c':
			sblock = expr_call (sblock, e, 0);
			break;
		case 'g':
		case 'i':
		case 'n':
		case IFBE:
		case IFB:
		case IFAE:
		case IFA:
			sblock = statement_branch (sblock, e);
			break;
		case '=':
			sblock = expr_assign (sblock, e, 0);
			break;
		case 'm':
		case 'M':
			sblock = expr_move (sblock, e, 0);
			break;
		default:
			if (e->e.expr.op < 256)
				debug (e, "e %c", e->e.expr.op);
			else
				debug (e, "e %d", e->e.expr.op);
			if (options.warnings.executable)
				warning (e, "Non-executable statement;"
						 " executing programmer instead.");
	}
	return sblock;
}

static sblock_t *
statement_uexpr (sblock_t *sblock, expr_t *e)
{
	const char *opcode;
	statement_t *s;

	switch (e->e.expr.op) {
		case 'r':
			debug (e, "RETURN");
			opcode = "<RETURN>";
			if (!e->e.expr.e1) {
				if (options.code.progsversion != PROG_ID_VERSION) {
					opcode = "<RETURN_V>";
				} else {
					e->e.expr.e1 = new_float_expr (0);
				}
			}
			s = new_statement (st_func, opcode, e);
			if (e->e.expr.e1) {
				s->opa = return_operand (get_type (e->e.expr.e1), e);
				sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
			}
			sblock_add_statement (sblock, s);
			sblock->next = new_sblock ();
			sblock = sblock->next;
			break;
		case 'g':
			sblock = statement_branch (sblock, e);
			break;
		default:
			debug (e, "e ue %d", e->e.expr.op);
			if (options.warnings.executable)
				warning (e, "Non-executable statement;"
						 " executing programmer instead.");
	}
	return sblock;
}

static sblock_t *
statement_memset (sblock_t *sblock, expr_t *e)
{
	expr_t     *dst = e->e.memset.dst;
	expr_t     *val = e->e.memset.val;
	expr_t     *count = e->e.memset.count;
	const char *opcode = "<MEMSET>";
	statement_t *s;

	if (is_constant (count)) {
		if (is_integer (get_type (count))
			&& (unsigned) expr_integer (count) < 0x10000) {
			count = new_short_expr (expr_integer (count));
		}
		if (is_uinteger (get_type (count)) && expr_integer (count) < 0x10000) {
			count = new_short_expr (expr_uinteger (count));
		}
	}
	s = new_statement (st_move, opcode, e);
	sblock = statement_subexpr (sblock, dst, &s->opc);
	sblock = statement_subexpr (sblock, count, &s->opb);
	sblock = statement_subexpr (sblock, val, &s->opa);
	sblock_add_statement (sblock, s);
	return sblock;
}

static sblock_t *
statement_nonexec (sblock_t *sblock, expr_t *e)
{
	if (!e->rvalue && options.warnings.executable)
		warning (e, "Non-executable statement; executing programmer instead.");
	return sblock;
}

static sblock_t *
statement_slist (sblock_t *sblock, expr_t *e)
{
	static statement_f sfuncs[] = {
		statement_ignore,	// ex_error
		statement_state,
		statement_bool,
		statement_label,
		0,					// ex_labelref
		statement_block,
		statement_expr,
		statement_uexpr,
		statement_nonexec,	// ex_symbol
		statement_nonexec,	// ex_temp
		statement_nonexec,	// ex_vector
		statement_nonexec,	// ex_nil
		statement_nonexec,	// ex_value
		0,					// ex_compound
		statement_memset,
	};

	for (/**/; e; e = e->next) {
		if (e->type > ex_memset)
			internal_error (e, "bad expression type");
		sblock = sfuncs[e->type] (sblock, e);
	}
	return sblock;
}

static void
move_labels (sblock_t *dst, sblock_t *src)
{
	ex_label_t *src_labels = src->labels;

	if (!src_labels)
		return;
	src_labels->dest = dst;
	while (src_labels->next) {
		src_labels = src_labels->next;
		src_labels->dest = dst;
	}
	src_labels->next = dst->labels;
	dst->labels = src->labels;
	src->labels = 0;
}

static void
move_code (sblock_t *dst, sblock_t *src)
{
	if (!src->statements)
		return;
	*dst->tail = src->statements;
	dst->tail = src->tail;
	src->statements = 0;
	src->tail = &src->statements;
}

static sblock_t *
merge_blocks (sblock_t *blocks)
{
	sblock_t  **sb;
	sblock_t   *sblock;
	statement_t *s;

	if (!blocks)
		return blocks;
	// merge any blocks that can be merged
	for (sblock = blocks; sblock; sblock = sblock->next) {
		if (sblock->statements && sblock->next) {
			s = (statement_t *) sblock->tail;
			// func and flow statements end blocks
			if (s->type >= st_func)
				continue;
			// labels begin blocks
			if (sblock->next->labels)
				continue;
			// blocks can be merged
			move_code (sblock, sblock->next);
		}
	}
	for (sb = &blocks; (*sb)->next;) {
		if (!(*sb)->statements) {
			// empty non-final block
			// move labels from empty block to next block
			if ((*sb)->labels)
				move_labels ((*sb)->next, (*sb));
			sblock = *sb;
			*sb = (*sb)->next;
			free_sblock (sblock);
			continue;
		}
		sb = &(*sb)->next;
	}
	// so long as blocks doesn't become null, remove an empty final block
	if (sb != &blocks) {
		if (!(*sb)->statements && !(*sb)->labels) {
			// empty final block with no labels
			sblock = *sb;
			*sb = (*sb)->next;
			free_sblock (sblock);
		}
	}
	return blocks;
}

static void
remove_label_from_dest (ex_label_t *label)
{
	sblock_t   *sblock;
	ex_label_t **l;

	if (!label || !label->dest)
		return;

	debug (0, "dropping deceased label %s", label->name);
	sblock = label->dest;
	label->dest = 0;
	for (l = &sblock->labels; *l; l = &(*l)->next) {
		if (*l == label) {
			*l = label->next;
			label->next = 0;
			break;
		}
	}
}

static void
unuse_label (ex_label_t *label)
{
	if (label && !--label->used)
		remove_label_from_dest (label);
}

static int
thread_jumps (sblock_t *blocks)
{
	sblock_t   *sblock;
	int         did_something = 0;

	if (!blocks)
		return 0;
	for (sblock = blocks; sblock; sblock = sblock->next) {
		statement_t *s;
		ex_label_t **label, *l;

		if (!sblock->statements)
			continue;
		s = (statement_t *) sblock->tail;
		if (statement_is_goto (s)) {
			label = &s->opa->o.label;
			if (!(*label)->dest && (*label)->symbol) {
				error (s->opa->expr, "undefined label `%s'",
					   (*label)->symbol->name);
				(*label)->symbol = 0;
			}
		} else if (statement_is_cond (s)) {
			label = &s->opb->o.label;
		} else {
			continue;
		}
		for (l = *label;
			 l->dest && l->dest->statements
			 && statement_is_goto (l->dest->statements);
			 l = l->dest->statements->opa->o.label) {
		}
		if (l != *label) {
			unuse_label (*label);
			l->used++;
			*label = l;
			did_something = 1;
		}
		if ((statement_is_goto (s) || statement_is_cond (s))
			&& (*label)->dest == sblock->next) {
			statement_t **p;
			unuse_label (*label);
			for (p = &sblock->statements; *p != s; p = &(*p)->next)
				;
			free_statement (s);
			*p = 0;
			sblock->tail = p;
			did_something = 1;
		}
	}
	return did_something;
}

static int
remove_dead_blocks (sblock_t *blocks)
{
	sblock_t   *sblock;
	int         did_something;
	int         did_anything = 0;
	int         pass = 0;

	if (!blocks)
		return 0;

	do {
		debug (0, "dead block pass %d", pass++);
		did_something = 0;
		blocks->reachable = 1;
		for (sblock = blocks; sblock->next; sblock = sblock->next) {
			sblock_t   *sb = sblock->next;
			statement_t *s;

			if (sb->labels) {
				sb->reachable = 1;
				continue;
			}
			if (!sblock->statements) {
				sb->reachable = 1;
				continue;
			}
			s = (statement_t *) sblock->tail;
			if (statement_is_cond (s)
				&& sb->statements && statement_is_goto (sb->statements)
				&& s->opb->o.label->dest == sb->next) {
				debug (0, "merging if/goto %p %p", sblock, sb);
				unuse_label (s->opb->o.label);
				s->opb->o.label = sb->statements->opa->o.label;
				s->opb->o.label->used++;
				invert_conditional (s);
				sb->reachable = 0;
				for (sb = sb->next; sb; sb = sb->next)
					sb->reachable = 1;
				break;
			} else if (!statement_is_goto (s) && !statement_is_return (s)) {
				sb->reachable = 1;
				continue;
			}
			sb->reachable = 0;
		}
		for (sblock = blocks; sblock; sblock = sblock->next) {
			while (sblock->next && !sblock->next->reachable) {
				sblock_t   *sb = sblock->next;
				statement_t *s;
				ex_label_t *label = 0;

				debug (0, "removing dead block %p", sb);

				if (sb->statements) {
					s = (statement_t *) sb->tail;
					if (statement_is_goto (s))
						label = s->opa->o.label;
					else if (statement_is_cond (s))
						label = s->opb->o.label;
				}
				unuse_label (label);
				did_something = 1;
				did_anything = 1;

				sblock->next = sb->next;
				free_sblock (sb);
			}
		}
	} while (did_something);
	return did_anything;
}

static void
check_final_block (sblock_t *sblock)
{
	statement_t *s = 0;

	if (!sblock)
		return;
	while (sblock->next)
		sblock = sblock->next;
	if (sblock->statements) {
		s = (statement_t *) sblock->tail;
		if (statement_is_goto (s))
			return;				// the end of function is the end of a loop
		if (statement_is_return (s))
			return;
	}
	if (current_func->sym->type->t.func.type != &type_void)
		warning (0, "control reaches end of non-void function");
	if (s && s->type >= st_func) {
		// func and flow end blocks, so we need to add a new block to take the
		// return
		sblock->next = new_sblock ();
		sblock = sblock->next;
	}
	s = new_statement (st_func, "<RETURN_V>", 0);
	if (options.traditional || options.code.progsversion == PROG_ID_VERSION) {
		s->opcode = save_string ("<RETURN>");
		s->opa = return_operand (&type_void, 0);
	}
	sblock_add_statement (sblock, s);
}

void
dump_dot_sblock (void *data, const char *fname)
{
	print_sblock ((sblock_t *) data, fname);
}

sblock_t *
make_statements (expr_t *e)
{
	sblock_t   *sblock = new_sblock ();
	int         did_something;
	int         pass = 0;

	if (options.block_dot.expr)
		dump_dot ("expr", e, dump_dot_expr);
	statement_slist (sblock, e);
	if (options.block_dot.initial)
		dump_dot ("initial", sblock, dump_dot_sblock);
	do {
		did_something = thread_jumps (sblock);
		if (options.block_dot.thread)
			dump_dot (va ("thread-%d", pass), sblock, dump_dot_sblock);
		did_something |= remove_dead_blocks (sblock);
		sblock = merge_blocks (sblock);
		if (options.block_dot.dead)
			dump_dot (va ("dead-%d", pass), sblock, dump_dot_sblock);
		pass++;
	} while (did_something);
	check_final_block (sblock);
	if (options.block_dot.final)
		dump_dot ("final", sblock, dump_dot_sblock);

	return sblock;
}

static void
count_temp (operand_t *op)
{
	if (!op)
		return;
	if (op->op_type == op_temp) {
		while (op->o.tempop.alias)
			op = op->o.tempop.alias;
		op->o.tempop.users++;
	}
}

void
statements_count_temps (sblock_t *sblock)
{
	statement_t *st;

	while (sblock) {
		for (st = sblock->statements; st; st = st->next) {
			count_temp (st->opa);
			count_temp (st->opb);
			count_temp (st->opc);
		}
		sblock = sblock->next;
	}
}
