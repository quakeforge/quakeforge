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

#include <QF/mathlib.h>
#include <QF/va.h>

#include "def.h"
#include "debug.h"
#include "expr.h"
#include "immediate.h"
#include "opcodes.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "type.h"
#include "qc-parse.h"

def_t      *emit_sub_expr (expr_t *e, def_t *dest);

static expr_t zero;

void
add_statement_ref (def_t *def, dstatement_t *st, reloc_type type)
{
	if (def) {
		reloc_t    *ref = new_reloc (st - pr.statements, type);

		ref->next = def->refs;
		def->refs = ref;

		def->users--;
		def->used = 1;
	}
}

def_t *
emit_statement (int sline, opcode_t *op, def_t *var_a, def_t *var_b,
				def_t *var_c)
{
	dstatement_t *statement;
	def_t      *ret;

	if (!op) {
		expr_t      e;

		e.line = sline;
		e.file = s_file;
		error (&e, "ice ice baby\n");
		abort ();
	}
	if (options.code.debug) {
		int         line = sline - lineno_base;

		if (line != linenos[num_linenos - 1].line) {
			pr_lineno_t *lineno = new_lineno ();

			lineno->line = line;
			lineno->fa.addr = pr.num_statements;
		}
	}
	if (pr.num_statements >= pr.statements_size) {
		pr.statements_size += 16384;
		pr.statements = realloc (pr.statements,
								 pr.statements_size * sizeof (dstatement_t));
		pr.statement_linenums = realloc (pr.statement_linenums,
										 pr.statements_size * sizeof (int));
	}
	statement = &pr.statements[pr.num_statements];
	pr.num_statements++;
	pr.statement_linenums[statement - pr.statements] = pr_source_line;
	statement->op = op->opcode;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if (op->type_c == ev_void || op->right_associative) {
		// ifs, gotos, and assignments don't need vars allocated
		if (op->type_c == ev_void) {
			var_c = NULL;
			statement->c = 0;
		} else {
			statement->c = var_c->ofs;
		}
		ret = var_a;
	} else {							// allocate result space
		if (!var_c) {
			var_c = get_tempdef (types[op->type_c], current_scope);
			var_c->users += 2;
		}
		statement->c = var_c->ofs;
		ret = var_c;
	}
#if 0
	printf ("%s %s(%d) %s(%d) %s(%d)\n", op->opname,
			var_a ? var_a->name : "", statement->a,
			var_b ? var_b->name : "", statement->b,
			var_c ? var_c->name : "", statement->c);
#endif

	add_statement_ref (var_a, statement, rel_op_a_def);
	add_statement_ref (var_b, statement, rel_op_b_def);
	add_statement_ref (var_c, statement, rel_op_c_def);

	if (op->right_associative)
		return var_a;
	return var_c;
}

void
emit_branch (int line, opcode_t *op, expr_t *e, expr_t *l)
{
	dstatement_t *st;
	reloc_t    *ref;
	def_t      *def = 0;
	int         ofs;

	if (e)
		def = emit_sub_expr (e, 0);
	st = &pr.statements[ofs = pr.num_statements];
	emit_statement (line, op, def, 0, 0);
	if (l->e.label.ofs) {
		if (op == op_goto)
			st->a = l->e.label.ofs - ofs;
		else
			st->b = l->e.label.ofs - ofs;
	} else {
		ref = new_reloc (ofs, op == op_goto ? rel_op_a_op : rel_op_b_op);
		ref->next = l->e.label.refs;
		l->e.label.refs = ref;
	}
}

def_t *
emit_function_call (expr_t *e, def_t *dest)
{
	def_t      *func = emit_sub_expr (e->e.expr.e1, 0);
	def_t       parm;
	def_t      *arg;
	expr_t     *earg;
	opcode_t   *op;
	int         count = 0, ind;

	for (earg = e->e.expr.e2; earg; earg = earg->next)
		count++;
	ind = count;
	for (earg = e->e.expr.e2; earg; earg = earg->next) {
		ind--;
		parm = def_parms[ind];
		parm.type = types[extract_type (earg)];
		arg = emit_sub_expr (earg, &parm);
		if (earg->type != ex_expr && earg->type != ex_uexpr) {
			op = opcode_find ("=", arg, &parm, &def_void);
			emit_statement (e->line, op, arg, &parm, 0);
		}
	}
	op = opcode_find (va ("<CALL%d>", count), &def_function, &def_void,
						 &def_void);
	emit_statement (e->line, op, func, 0, 0);

	def_ret.type = func->type->aux_type;
	if (dest) {
		op = opcode_find ("=", dest, &def_ret, &def_void);
		emit_statement (e->line, op, &def_ret, dest, 0);
		return dest;
	} else {
		return &def_ret;
	}
}

def_t *
emit_assign_expr (int oper, expr_t *e)
{
	def_t      *def_a, *def_b, *def_c;
	opcode_t   *op;
	expr_t     *e1 = e->e.expr.e1;
	expr_t     *e2 = e->e.expr.e2;
	const char *operator = get_op_string (oper);

	if (e1->type == ex_temp && e1->e.temp.users < 2) {
		e1->e.temp.users--;
		return 0;
	}
	if (oper == '=') {
		def_a = emit_sub_expr (e1, 0);
		if (def_a->constant) {
			if (options.code.cow) {
				int         size = type_size (def_a->type);
				int         ofs = new_location (def_a->type, pr.near_data);

				memcpy (G_POINTER (void, ofs), G_POINTER (void, def_a->ofs),
						size);
				def_a->ofs = ofs;
				def_a->constant = 0;
				if (options.warnings.cow)
					warning (e1, "assignment to constant %s (Moooooooo!)",
							 def_a->name);
			} else {
				if (options.traditional)
					warning (e1, "assignment to constant %s", def_a->name);
				else
					error (e1, "assignment to constant %s", def_a->name);
			}
		}
		def_b = emit_sub_expr (e2, def_a);
		if (def_b != def_a) {
			op = opcode_find (operator, def_b, def_a, &def_void);
			emit_statement (e->line, op, def_b, def_a, 0);
		}
		return def_a;
	} else {
		def_b = emit_sub_expr (e2, 0);
		if (e->rvalue && def_b->managed)
			def_b->users++;
		if (e1->type == ex_expr && extract_type (e1->e.expr.e1) == ev_pointer
			&& e1->e.expr.e1->type < ex_string) {
			def_a = emit_sub_expr (e1->e.expr.e1, 0);
			def_c = emit_sub_expr (e1->e.expr.e2, 0);
			op = opcode_find (operator, def_b, def_a, def_c);
		} else {
			def_a = emit_sub_expr (e1, 0);
			def_c = 0;
			op = opcode_find (operator, def_b, def_a, &def_void);
		}
		emit_statement (e->line, op, def_b, def_a, def_c);
		return def_b;
	}
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
		def_t      *tmp = new_def (t2, 0, current_scope);

		tmp->ofs = def->ofs;
		tmp->users = e2->e.temp.users;
		tmp->freed = 1;				// don't free this offset when freeing def
		def = tmp;
	}
	e2->e.temp.def = def;
	return e2->e.temp.def;
}

def_t *
emit_address_expr (expr_t *e)
{
	def_t      *def_a, *def_b, *d;
	opcode_t   *op;

print_expr (e); printf (" %d\n", e->line);
	def_a = emit_sub_expr (e->e.expr.e1, 0);
	def_b = emit_sub_expr (e->e.expr.e2, 0);
	op = opcode_find ("&", def_a, def_b, 0);
	d = emit_statement (e->line, op, def_a, def_b, 0);
	return d;
}

def_t *
emit_deref_expr (expr_t *e, def_t *dest)
{
	def_t      *d;
	type_t     *type = e->e.expr.type;
	def_t      *z;
	opcode_t   *op;

	e = e->e.expr.e1;
	if (e->type == ex_pointer) {
		if (e->e.pointer.val > 0 && e->e.pointer.val < 65536) {
			d = new_def (e->e.pointer.type, 0, current_scope);
			d->ofs = e->e.pointer.val;
			d->absolute = e->e.pointer.abs;
		} else {
			d = ReuseConstant (e, 0);
			zero.type = ex_short;
			z = emit_sub_expr (&zero, 0);
			op = opcode_find (".", d, z, dest);
			d = emit_statement (e->line, op, d, z, dest);
		}
		return d;
	}
	if (!dest && (e->type != ex_pointer
				  || !(e->e.pointer.val > 0
					   && e->e.pointer.val < 65536))) {
		dest = get_tempdef (type, current_scope);
		dest->users += 2;
	}
print_expr (e); printf (" %d %p\n", e->line, dest);
	if (e->type == ex_expr
		&& e->e.expr.op == '&'
		&& e->e.expr.e1->type < ex_string)
		e->e.expr.op = '.';
	d = emit_sub_expr (e, dest);
print_expr (e); printf (" %d %p\n", e->line, dest);
	if (dest && d != dest) {
		zero.type = ex_short;
		z = emit_sub_expr (&zero, 0);
		op = opcode_find (".", d, z, dest);
		d = emit_statement (e->line, op, d, z, dest);
	} else {
		if (!d->name)
			d->type = type;
	}
	return d;
}

def_t *
emit_sub_expr (expr_t *e, def_t *dest)
{
	opcode_t   *op;
	const char *operator;
	def_t      *def_a, *def_b, *d = 0;
	def_t      *tmp = 0;

	switch (e->type) {
		case ex_block:
			if (e->e.block.result) {
				d = emit_sub_expr (e->e.block.result, dest);
				for (e = e->e.block.head; e; e = e->next)
					emit_expr (e);
				break;
			}
		case ex_name:
		case ex_nil:
		case ex_label:
		case ex_error:
			error (e, "internal error");
			abort ();
		case ex_expr:
			if (e->e.expr.op == 'c') {
				d = emit_function_call (e, dest);
				break;
			}
			if (e->e.expr.op == '=' || e->e.expr.op == PAS) {
				d = emit_assign_expr (e->e.expr.op, e);
				if (!d->managed)
					d->users++;
				break;
			}
			if (e->e.expr.op == '&' && e->e.expr.type->type == ev_pointer) {
				d = emit_address_expr (e);
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
				dest = get_tempdef (e->e.expr.type, current_scope);
				dest->users += 2;
			}
			op = opcode_find (operator, def_a, def_b, dest);
			d = emit_statement (e->line, op, def_a, def_b, dest);
			break;
		case ex_uexpr:
			switch (e->e.expr.op) {
				case '!':
					operator = "!";
					def_a = emit_sub_expr (e->e.expr.e1, 0);
					def_b = &def_void;
					break;
				case '~':
					operator = "~";
					def_a = emit_sub_expr (e->e.expr.e1, 0);
					def_b = &def_void;
					break;
				case '-':
					zero.type = expr_types[extract_type (e->e.expr.e1)];

					operator = "-";
					def_a = ReuseConstant (&zero, 0);
					def_b = emit_sub_expr (e->e.expr.e1, 0);
					if (!dest) {
						dest = get_tempdef (e->e.expr.type, current_scope);
						dest->users += 2;
					}
					break;
				case '&':
					zero.type = ex_short;

					operator = "&";
					if (e->e.expr.e1->type == ex_expr
						&& e->e.expr.e1->e.expr.op == '.') {
						tmp = get_tempdef (e->e.expr.type, current_scope);
						tmp->users += 2;
						def_b = emit_sub_expr (&zero, 0);
					} else {
						def_b = &def_void;
					}
					def_a = emit_sub_expr (e->e.expr.e1, tmp);
					if (!dest) {
						dest = get_tempdef (e->e.expr.type, current_scope);
						dest->users += 2;
					}
					break;
				case '.':
					return emit_deref_expr (e, dest);
				case 'C':
					def_a = emit_sub_expr (e->e.expr.e1, 0);
					if (def_a->type->type == ev_pointer
						&& e->e.expr.type->type == ev_pointer) {
						return def_a;
					}
					def_b = &def_void;
					if (!dest) {
						dest = get_tempdef (e->e.expr.type, current_scope);
						dest->users = 2;
					}
					operator = "=";
					break;
				default:
					abort ();
			}
			op = opcode_find (operator, def_a, def_b, dest);
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
					e->e.temp.def = get_tempdef (e->e.temp.type, current_scope);
				e->e.temp.def->users = e->e.temp.users;
				e->e.temp.def->expr = e;
				e->e.temp.def->managed = 1;
			}
			d = e->e.temp.def;
			break;
		case ex_pointer:
		case ex_string:
		case ex_float:
		case ex_vector:
		case ex_entity:
		case ex_field:
		case ex_func:
		case ex_quaternion:
		case ex_integer:
		case ex_uinteger:
			d = ReuseConstant (e, 0);
			break;
		case ex_short:
			d = new_def (&type_short, 0, current_scope);
			d->ofs = e->e.short_val;
			d->absolute = 1;
			d->users = 1;
			break;
	}
	free_tempdefs ();
	return d;
}

void
emit_expr (expr_t *e)
{
	def_t      *def;
	def_t      *def_a;
	def_t      *def_b;
	ex_label_t *label;

	//printf ("%d ", e->line);
	//print_expr (e);
	//puts ("");
	switch (e->type) {
		case ex_error:
			break;
		case ex_label:
			label = &e->e.label;
			label->ofs = pr.num_statements;
			break;
		case ex_block:
			for (e = e->e.block.head; e; e = e->next)
				emit_expr (e);
			break;
		case ex_expr:
			switch (e->e.expr.op) {
				case PAS:
				case '=':
					emit_assign_expr (e->e.expr.op, e);
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
		case ex_short:
		case ex_name:
		case ex_nil:
			warning (e, "Ignoring useless expression");
			break;
	}
	free_tempdefs ();
}
