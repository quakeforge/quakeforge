/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
static const char rcsid[] = 
	"$Id$";

#include <stdlib.h>

#include <QF/mathlib.h>
#include <QF/va.h>

#include "qfcc.h"
#include "scope.h"
#include "qc-parse.h"

def_t *emit_sub_expr (expr_t *e, def_t *dest);

def_t *
emit_statement (int sline, opcode_t *op, def_t *var_a, def_t *var_b, def_t *var_c)
{
	dstatement_t    *statement;
	def_t			*ret;

	if (!op) {
		expr_t e;
		e.line = sline;
		e.file = s_file;
		error (&e, "ice ice baby\n");
		abort ();
	}
	if (options.code.debug) {
		int				line = sline - lineno_base;

		if (line != linenos[num_linenos - 1].line) {
			pr_lineno_t		*lineno = new_lineno ();
			lineno->line = line;
			lineno->fa.addr = numstatements;
		}
	}
	statement = &statements[numstatements];
	numstatements++;
	statement_linenums[statement - statements] = pr_source_line;
	statement->op = op->opcode;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if (op->type_c == ev_void || op->right_associative) {
		// ifs, gotos, and assignments don't need vars allocated
		if (op->type_c == ev_void) {
			var_c = NULL;
			statement->c = 0;
		}
		ret = var_a;
	} else {	// allocate result space
		if (!var_c) {
			var_c = PR_GetTempDef (types[op->type_c], pr_scope);
			var_c->users += 2;
		}
		statement->c = var_c->ofs;
		ret = var_c;
	}

	PR_AddStatementRef (var_a, statement, 0);
	PR_AddStatementRef (var_b, statement, 1);
	PR_AddStatementRef (var_c, statement, 2);

	if (op->right_associative)
		return var_a;
	return var_c;
}

void
emit_branch (int line, opcode_t *op, expr_t *e, expr_t *l)
{
	dstatement_t *st;
	statref_t *ref;
	def_t *def = 0;

	if (e)
		def = emit_sub_expr (e, 0);
	st = &statements[numstatements];
	emit_statement (line, op, def, 0, 0);
	if (l->e.label.statement) {
		if (op == op_goto)
			st->a = l->e.label.statement - st;
		else
			st->b = l->e.label.statement - st;
	} else {
		ref = PR_NewStatref (st, op != op_goto);
		ref->next = l->e.label.refs;
		l->e.label.refs = ref;
	}
}

def_t *
emit_function_call (expr_t *e, def_t *dest)
{
	def_t *func = emit_sub_expr (e->e.expr.e1, 0);
	def_t parm;
	def_t *arg;
	expr_t *earg;
	opcode_t *op;
	int count = 0, ind;

	for (earg = e->e.expr.e2; earg; earg = earg->next)
		count++;
	ind = count;
	for (earg = e->e.expr.e2; earg; earg = earg->next) {
		ind--;
		parm = def_parms[ind];
		parm.type = types[extract_type (earg)];
		arg = emit_sub_expr (earg, &parm);
		if (earg->type != ex_expr && earg->type != ex_uexpr) {
			op = PR_Opcode_Find ("=", arg, &parm, &def_void);
			emit_statement (e->line, op, arg, &parm, 0);
		}
	}
	op = PR_Opcode_Find (va ("<CALL%d>", count), &def_function,  &def_void, &def_void);
	emit_statement (e->line, op, func, 0, 0);

	def_ret.type = func->type->aux_type;
	if (dest) {
		op = PR_Opcode_Find ("=", dest, &def_ret, &def_void);
		emit_statement (e->line, op, &def_ret, dest, 0);
		return dest;
	} else {
		return &def_ret;
	}
}

def_t *
emit_assign_expr (expr_t *e)
{
	def_t	*def_a, *def_b, *def_c;
	opcode_t *op;
	expr_t *e1 = e->e.expr.e1;
	expr_t *e2 = e->e.expr.e2;

	if (e1->type == ex_temp && e1->e.temp.users < 2) {
		e1->e.temp.users--;
		return 0;
	}
	if (e1->type == ex_expr && e1->e.expr.op == '.'
		&& extract_type (e1->e.expr.e1) == ev_pointer) {
		def_a = emit_sub_expr (e2, 0);
		def_c = emit_sub_expr (e1->e.expr.e2, 0);
		def_b = emit_sub_expr (e1->e.expr.e1, 0);
		op = PR_Opcode_Find ("=", def_a, def_b, def_c);
		emit_statement (e->line, op, def_a, def_b, def_c);
	} else {
		def_a = emit_sub_expr (e1, 0);
		if (def_a->type->type == ev_pointer) {
			def_b = emit_sub_expr (e2, 0);
			op = PR_Opcode_Find ("=", def_b, def_a, &def_void);
			emit_statement (e->line, op, def_b, def_a, 0);
		} else {
			if (def_a->constant) {
				if (options.code.cow) {
					int size = pr_type_size [def_a->type->type];
					int ofs = PR_NewLocation (def_a->type);

					memcpy (pr_globals + ofs, pr_globals + def_a->ofs, size);
					def_a->ofs = ofs;
					def_a->constant = 0;
					if (options.warnings.cow)
						warning (e1, "assignment to constant %s (Moooooooo!)",
								 def_a->name);
				} else {
					error (e1, "assignment to constant %s", def_a->name);
				}
			}
			def_b = emit_sub_expr (e2, def_a);
			if (def_b != def_a) {
				op = PR_Opcode_Find ("=", def_b, def_a, &def_void);
				emit_statement (e->line, op, def_b, def_a, 0);
			}
		}
	}
	if (def_a->type->type != ev_pointer)
		return def_a;
	return def_b;
}

def_t *
emit_bind_expr (expr_t *e1, expr_t *e2)
{
	type_t     *t1 = get_type (e1);
	type_t     *t2 = get_type (e2);
	def_t      *def;

	if (!e2 || e2->type != ex_temp) {
		error (e1, "internal error");
		abort ();
	}
	def = emit_sub_expr (e1, e2->e.temp.def);
	if (t1 != t2) {
		def_t      *tmp = PR_NewDef (t2, 0, def->scope);
		tmp->ofs = def->ofs;
		tmp->users = e2->e.temp.users;
		tmp->freed = 1;			// don't free this offset when freeing def
		def = tmp;
	}
	e2->e.temp.def = def;
	return e2->e.temp.def;
}

def_t *
emit_sub_expr (expr_t *e, def_t *dest)
{
	opcode_t   *op;
	const char *operator;
	def_t      *def_a, *def_b, *d = 0;

	switch (e->type) {
		case ex_block:
			if (e->e.block.result) {
				d = emit_sub_expr (e->e.block.result, dest);
				for (e = e->e.block.head; e; e = e->next)
					emit_expr (e);
				break;
			}
		case ex_nil:
		case ex_label:
			error (e, "internal error");
			abort ();
		case ex_expr:
			if (e->e.expr.op == 'c') {
				d = emit_function_call (e, dest);
				break;
			}
			if (e->e.expr.op == '=') {
				d = emit_assign_expr (e);
				break;
			}
			if (e->e.expr.e1->type == ex_block
				&& e->e.expr.e1->e.block.is_call) {
				def_b = emit_sub_expr (e->e.expr.e2, 0);
				def_a = emit_sub_expr (e->e.expr.e1, 0);
			} else {
				def_a = emit_sub_expr (e->e.expr.e1, 0);
				def_b = emit_sub_expr (e->e.expr.e2, 0);
			}
			operator = get_op_string (e->e.expr.op);
			if (!dest) {
				dest = PR_GetTempDef (e->e.expr.type, pr_scope);
				dest->users += 2;
			}
			op = PR_Opcode_Find (operator, def_a, def_b, dest);
			d = emit_statement (e->line, op, def_a, def_b, dest);
			break;
		case ex_uexpr:
			if (e->e.expr.op == '!') {
				operator = "!";
				def_a = emit_sub_expr (e->e.expr.e1, 0);
				def_b = &def_void;
			} else if (e->e.expr.op == '~') {
				operator = "~";
				def_a = emit_sub_expr (e->e.expr.e1, 0);
				def_b = &def_void;
			} else if (e->e.expr.op == '-') {
				static expr_t zero;

				zero.type = expr_types[extract_type (e->e.expr.e1)];

				operator = "-";
				def_a = PR_ReuseConstant (&zero, 0);
				def_b = emit_sub_expr (e->e.expr.e1, 0);
				if (!dest) {
					dest = PR_GetTempDef (e->e.expr.type, pr_scope);
					dest->users += 2;
				}
			} else {
				abort ();
			}
			op = PR_Opcode_Find (operator, def_a, def_b, dest);
			d = emit_statement (e->line, op, def_a, def_b, dest);
			break;
		case ex_def:
			d = e->e.def;
			break;
		case ex_temp:
			if (!e->e.temp.def) {
				if (dest)
					e->e.temp.def = dest;
				else
					e->e.temp.def = PR_GetTempDef (e->e.temp.type, pr_scope);
				e->e.temp.def->users = e->e.temp.users;
				e->e.temp.def->expr = e;
			}
			d = e->e.temp.def;
			break;
		case ex_string:
		case ex_float:
		case ex_vector:
		case ex_entity:
		case ex_field:
		case ex_func:
		case ex_pointer:
		case ex_quaternion:
		case ex_integer:
		case ex_uinteger:
			d = PR_ReuseConstant (e, 0);
			break;
	}
	PR_FreeTempDefs ();
	return d;
}

void
emit_expr (expr_t *e)
{
	def_t *def;
	def_t *def_a;
	def_t *def_b;
	statref_t *ref;
	elabel_t *label;

	switch (e->type) {
		case ex_label:
			label = &e->e.label;
			label->statement = &statements[numstatements];
			for (ref = label->refs; ref; ref = ref->next) {
				switch (ref->field) {
					case 0:
						ref->statement->a = label->statement - ref->statement;
						break;
					case 1:
						ref->statement->b = label->statement - ref->statement;
						break;
					case 2:
						ref->statement->c = label->statement - ref->statement;
						break;
					case 3:
						*(int*)ref->statement = label->statement - statements;
						break;
					default:
						abort();
				}
			}
			break;
		case ex_block:
			for (e = e->e.block.head; e; e = e->next)
				emit_expr (e);
			break;
		case ex_expr:
			switch (e->e.expr.op) {
				case '=':
					emit_assign_expr (e);
					break;
				case 'n':
					emit_branch (e->line, op_ifnot, e->e.expr.e1, e->e.expr.e2);
					break;
				case 'i':
					emit_branch (e->line, op_if, e->e.expr.e1, e->e.expr.e2);
					break;
				case IFBE:
					emit_branch (e->line, op_ifbe, e->e.expr.e1, e->e.expr.e2);
					break;
				case IFB:
					emit_branch (e->line, op_ifb, e->e.expr.e1, e->e.expr.e2);
					break;
				case IFAE:
					emit_branch (e->line, op_ifae, e->e.expr.e1, e->e.expr.e2);
					break;
				case IFA:
					emit_branch (e->line, op_ifa, e->e.expr.e1, e->e.expr.e2);
					break;
				case 'c':
					emit_function_call (e, 0);
					break;
				case 's':
					def_a = emit_sub_expr (e->e.expr.e1, 0);
					def_b = emit_sub_expr (e->e.expr.e2, 0);
					emit_statement (e->line, op_state, def_a, def_b, 0);
					break;
				case 'b':
					emit_bind_expr (e->e.expr.e1, e->e.expr.e2);
					break;
				case 'g':
					def_a = emit_sub_expr (e->e.expr.e1, 0);
					def_b = emit_sub_expr (e->e.expr.e2, 0);
					emit_statement (e->line, op_jumpb, def_a, def_b, 0);
					break;
				default:
					warning (e, "Ignoring useless expression");
					break;
			}
			break;
		case ex_uexpr:
			switch (e->e.expr.op) {
				case 'r':
					def = 0;
					if (e->e.expr.e1)
						def = emit_sub_expr (e->e.expr.e1, 0);
					emit_statement (e->line, op_return, def, 0, 0);
					break;
				case 'g':
					emit_branch (e->line, op_goto, 0, e->e.expr.e1);
					break;
				default:
					warning (e, "useless expression");
					emit_expr (e->e.expr.e1);
					break;
			}
			break;
		case ex_def:
		case ex_temp:
		case ex_string:
		case ex_float:
		case ex_vector:
		case ex_entity:
		case ex_field:
		case ex_func:
		case ex_pointer:
		case ex_quaternion:
		case ex_integer:
		case ex_uinteger:
			warning (e, "Ignoring useless expression");
			break;
		case ex_nil:
			error (e, "internal error");
			abort ();
	}
	PR_FreeTempDefs ();
}
