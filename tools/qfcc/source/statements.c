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

#include "QF/va.h"

#include "expr.h"
#include "options.h"
#include "qc-parse.h"
#include "qfcc.h"
#include "statements.h"
#include "symtab.h"
#include "type.h"

static __attribute__ ((used)) const char rcsid[] = "$Id$";

static void
print_operand (operand_t *op)
{
	switch (op->type) {
		case op_symbol:
			printf ("%s", op->o.symbol->name);
			break;
		case op_value:
			switch (op->o.value->type) {
				case ev_string:
					printf ("\"%s\"", op->o.value->v.string_val);
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
				case ev_short:
					printf ("%d", op->o.value->v.short_val);
					break;
				case ev_void:
				case ev_invalid:
				case ev_type_count:
					internal_error (0, "weird value type");
			}
			break;
		case op_label:
			printf ("%p", op->o.label->dest);
			break;
		case op_temp:
			printf ("%p", op);
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

static sblock_t *free_sblocks;
static statement_t *free_statements;
static operand_t *free_operands;

static sblock_t *
new_sblock (void)
{
	sblock_t   *sblock;
	ALLOC (256, sblock_t, sblocks, sblock);
	sblock->tail = &sblock->statements;
	return sblock;
}

static void
sblock_add_statement (sblock_t *sblock, statement_t *statement)
{
	// this should normally be null, but might be inserting
	statement->next = *sblock->tail;
	*sblock->tail = statement;
	sblock->tail = &statement->next;
}

static statement_t *
new_statement (const char *opcode)
{
	statement_t *statement;
	ALLOC (256, statement_t, statements, statement);
	statement->opcode = save_string (opcode);
	return statement;
}

static operand_t *
new_operand (op_type_e op)
{
	operand_t *operand;
	ALLOC (256, operand_t, operands, operand);
	operand->type = op;
	return operand;
}

static const char *
convert_op (int op)
{
	switch (op) {
		case PAS:	return ".=";
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
		case 'C':	return "=";
		case 'M':	return "<MOVE>";
		default:
			return 0;
	}
}
static sblock_t *statement_subexpr (sblock_t *sblock, expr_t *e,
									operand_t **op);

static sblock_t *
statement_call (sblock_t *sblock, expr_t *call)
{
	expr_t     *func = call->e.expr.e1;
	expr_t     *args = call->e.expr.e2;
	expr_t     *a;
	int         count = 0;
	const char *opcode;
	const char *pref = "";
	statement_t *s;

	for (a = args; a; a = a->next)
		count++;
	if (count && options.code.progsversion != PROG_ID_VERSION)
		pref = "R";
	opcode = va ("<%sCALL%d>", pref, count);
	s = new_statement (opcode);
	sblock = statement_subexpr (sblock, func, &s->opa);
	sblock_add_statement (sblock, s);
	sblock->next = new_sblock ();
	return sblock->next;
}

static sblock_t *
statement_branch (sblock_t *sblock, expr_t *e)
{
	statement_t *s = 0;

	if (e->type == ex_uexpr && e->e.expr.op == 'g') {
		s = new_statement ("<GOTO>");
		s->opa = new_operand (op_label);
		s->opa->o.label = &e->e.expr.e1->e.label;
	}

	sblock_add_statement (sblock, s);
	sblock->next = new_sblock ();
	return sblock->next;
}

static sblock_t *
statement_subexpr (sblock_t *sblock, expr_t *e, operand_t **op)
{
	statement_t *s;
	const char *opcode;

	if (!e) {
		*op = 0;
		return sblock;
	}

	switch (e->type) {
		case ex_error:
		case ex_state:
		case ex_bool:
		case ex_label:
		case ex_block:
		case ex_nil:
			internal_error (e, 0);
		case ex_expr:
			switch (e->e.expr.op) {
				case 'c':
					sblock = statement_call (sblock, e);
					break;
				default:
					opcode = convert_op (e->e.expr.op);
					if (!opcode)
						internal_error (e, "ice ice baby");
					s = new_statement (opcode);
					sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
					sblock = statement_subexpr (sblock, e->e.expr.e2, &s->opb);
					*op = s->opc = new_operand (op_temp);
					sblock_add_statement (sblock, s);
					break;
			}
			break;
		case ex_uexpr:
			break;
		case ex_symbol:
			*op = new_operand (op_symbol);
			(*op)->o.symbol = e->e.symbol;
			break;
		case ex_temp:
			*op = new_operand (op_temp);
			break;
		case ex_value:
			*op = new_operand (op_value);
			(*op)->o.value = &e->e.value;
			break;
	}
	return sblock;
}

static sblock_t *
statement_expr (sblock_t *sblock, expr_t *e)
{
	sblock_t   *start = sblock;
	statement_t *s;
	expr_t     *se;
	const char *opcode;

	for (/**/; e && e->type == ex_label; e = e->next)
		e->e.label.dest = sblock;
	for (/**/; e; e = e->next) {
		switch (e->type) {
			case ex_error:
				break;
			case ex_state:
				s = new_statement ("<STATE>");
				sblock = statement_subexpr (sblock, e->e.state.frame, &s->opa);
				sblock = statement_subexpr (sblock, e->e.state.think, &s->opb);
				sblock = statement_subexpr (sblock, e->e.state.step, &s->opc);
				sblock_add_statement (sblock, s);
				break;
			case ex_bool:
				break;
			case ex_label:
				sblock->next = new_sblock ();
				sblock = sblock->next;
				e->e.label.dest = sblock;
				for (/**/; e->next && e->next->type == ex_label; e = e->next)
					e->e.label.dest = sblock;
				break;
			case ex_block:
				sblock->next = new_sblock ();
				sblock = sblock->next;
				for (se = e->e.block.head; se; se = se->next)
					sblock = statement_expr (sblock, se);
				break;
			case ex_expr:
				switch (e->e.expr.op) {
					case 'c':
						sblock = statement_call (sblock, e);
						break;
					case 'i':
					case 'n':
					case IFBE:
					case IFB:
					case IFAE:
					case IFA:
						opcode = convert_op (e->e.expr.op);
						s = new_statement (opcode);
						sblock = statement_subexpr (sblock, e->e.expr.e1,
													&s->opa);
						s->opb = new_operand (op_label);
						s->opb->o.label = &e->e.label;
						sblock_add_statement (sblock, s);
						sblock->next = new_sblock ();
						sblock = sblock->next;
						break;
					default:
						if (e->e.expr.op < 256)
							notice (e, "e e %c", e->e.expr.op);
						else
							notice (e, "e e %d", e->e.expr.op);
						goto non_executable;
				}
				break;
			case ex_uexpr:
				switch (e->e.expr.op) {
					case 'r':
						notice (e, "RETURN");
						opcode = "<RETURN>";
						if (!e->e.expr.e1 && !options.traditional)
							opcode = "<RETURN_V>";
						s = new_statement (opcode);
						sblock = statement_subexpr (sblock, e->e.expr.e1,
													&s->opa);
						sblock_add_statement (sblock, s);
						sblock->next = new_sblock ();
						sblock = sblock->next;
					case 'g':
						sblock = statement_branch (sblock, e);
						break;
					default:
						notice (e, "e ue %d", e->e.expr.op);
						goto non_executable;
				}
				break;
			case ex_symbol:
			case ex_temp:
			case ex_nil:
			case ex_value:
				notice (e, "e %d", e->type);
non_executable:
				warning (e, "Non-executable statement;"
						 " executing programmer instead.");
				break;
		}
	}
	return start;
}

sblock_t *
make_statements (expr_t *e)
{
	return statement_expr (new_sblock (), e);
}
