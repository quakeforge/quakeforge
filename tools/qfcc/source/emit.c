/*
	emit.c

	statement emittion

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/07/26

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

#include <QF/mathlib.h>
#include <QF/va.h>

#include "codespace.h"
#include "def.h"
#include "defspace.h"
#include "debug.h"
#include "diagnostic.h"
#include "emit.h"
#include "function.h"
#include "opcodes.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "statements.h"
#include "symtab.h"
#include "type.h"
#include "value.h"

static def_t zero_def;

static def_t *get_operand_def (expr_t *expr, operand_t *op);

static def_t *
get_tempop_def (expr_t *expr, tempop_t *tempop, type_t *type)
{
	if (tempop->def) {
		return tempop->def;
	}
	if (tempop->alias) {
		def_t      *tdef = get_operand_def (expr, tempop->alias);
		int         offset = tempop->offset;
		tempop->def = alias_def (tdef, type, offset);
	}
	if (!tempop->def) {
		tempop->def = temp_def (type);
	}
	return tempop->def;
}

static def_t *
get_value_def (expr_t *expr, ex_value_t *value, type_t *type)
{
	def_t      *def;

	if (is_short (type)) {
		def = new_def (0, &type_short, 0, sc_extern);
		def->offset = value->v.short_val;
		return def;
	}
	if (is_pointer (type) && value->v.pointer.tempop
		&& !value->v.pointer.def) {
		value->v.pointer.def = get_tempop_def (expr, value->v.pointer.tempop,
											   type->t.fldptr.type);
	}
	def = emit_value (value, 0);
	if (type != def->type)
		return alias_def (def, type, 0);
	return def;
}

static def_t *
get_operand_def (expr_t *expr, operand_t *op)
{
	if (!op)
		return 0;
	switch (op->op_type) {
		case op_def:
			return op->o.def;
		case op_value:
			return get_value_def (expr, op->o.value, op->type);
		case op_label:
			op->type = &type_short;
			zero_def.type = &type_short;
			return &zero_def;	//FIXME
		case op_temp:
			return get_tempop_def (expr, &op->o.tempop, op->type);
		case op_alias:
			return get_operand_def (expr, op->o.alias);
		case op_nil:
			internal_error (expr, "unexpected nil operand");
	}
	return 0;
}

static void
add_statement_def_ref (def_t *def, dstatement_t *st, int field)
{
	if (def) {
		int         st_ofs = st - pr.code->code;
		int         offset_reloc = 0;
		int         alias_depth = 0;
		expr_t      alias_depth_expr;

		alias_depth_expr.file = def->file;
		alias_depth_expr.line = def->line;
		while (def->alias) {
			alias_depth++;
			offset_reloc |= def->offset_reloc;
			def = def->alias;
		}
		if (alias_depth > 1) {
			internal_error (&alias_depth_expr, "alias chain detected: %d %s",
				 alias_depth, def->name);
		}
		if (offset_reloc)
			reloc_op_def_ofs (def, st_ofs, field);
		else
			reloc_op_def (def, st_ofs, field);
	}
}

static void
add_statement_op_ref (operand_t *op, dstatement_t *st, int field)
{
	if (op && op->op_type == op_label) {
		int         st_ofs = st - pr.code->code;
		reloc_t    *reloc = new_reloc (0, st_ofs, rel_op_a_op + field);

		reloc->next = op->o.label->dest->relocs;
		op->o.label->dest->relocs = reloc;
	}
}

static void
use_tempop (operand_t *op, expr_t *expr)
{
	if (!op || op->op_type != op_temp)
		return;
	while (op->o.tempop.alias)
		op = op->o.tempop.alias;
	if (--op->o.tempop.users == 0)
		free_temp_def (op->o.tempop.def);
	if (op->o.tempop.users <= -1)
		bug (expr, "temp users went negative: %s", operand_string (op));
}

static void
emit_statement (statement_t *statement)
{
	const char *opcode = statement->opcode;
	def_t      *def_a, *def_b, *def_c;
	opcode_t   *op;
	dstatement_t *s;

	def_a = get_operand_def (statement->expr, statement->opa);
	use_tempop (statement->opa, statement->expr);
	def_b = get_operand_def (statement->expr, statement->opb);
	use_tempop (statement->opb, statement->expr);
	def_c = get_operand_def (statement->expr, statement->opc);
	use_tempop (statement->opc, statement->expr);
	op = opcode_find (opcode, statement->opa, statement->opb, statement->opc);

	if (!op) {
		print_expr (statement->expr);
		print_statement (statement);
		internal_error (statement->expr, "ice ice baby");
	}
	if (options.code.debug) {
		expr_t     *e = statement->expr;
		pr_uint_t   line = (e ? e->line : pr.source_line) - lineno_base;

		if (line != pr.linenos[pr.num_linenos - 1].line) {
			pr_lineno_t *lineno = new_lineno ();

			lineno->line = line;
			lineno->fa.addr = pr.code->size;
		}
	}
	s = codespace_newstatement (pr.code);
	s->op = op->opcode;
	s->a = def_a ? def_a->offset : 0;
	s->b = def_b ? def_b->offset : 0;
	s->c = def_c ? def_c->offset : 0;

	add_statement_def_ref (def_a, s, 0);
	add_statement_def_ref (def_b, s, 1);
	add_statement_def_ref (def_c, s, 2);

	add_statement_op_ref (statement->opa, s, 0);
	add_statement_op_ref (statement->opb, s, 1);
	add_statement_op_ref (statement->opc, s, 2);
}

void
emit_statements (sblock_t *first_sblock)
{
	sblock_t   *sblock;
	statement_t *s;

	for (sblock = first_sblock; sblock; sblock = sblock->next) {
		sblock->offset = pr.code->size;
		for (s = sblock->statements; s; s = s->next)
			emit_statement (s);
	}

	for (sblock = first_sblock; sblock; sblock = sblock->next)
		relocate_refs (sblock->relocs, sblock->offset);
}
