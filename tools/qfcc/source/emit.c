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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

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
#include "immediate.h"
#include "opcodes.h"
#include "options.h"
#include "qfcc.h"
#include "reloc.h"
#include "statements.h"
#include "symtab.h"
#include "type.h"

static def_t zero_def;

static def_t *
get_operand_def (operand_t *op)
{
	def_t      *def;

	if (!op)
		return 0;
	switch (op->op_type) {
		case op_symbol:
			switch (op->o.symbol->sy_type) {
				case sy_var:
					if (op->type != op->o.symbol->type->type)
						return alias_def (op->o.symbol->s.def,
										  ev_types[op->type]);
					return op->o.symbol->s.def;
				case sy_func:
					return op->o.symbol->s.func->def;
				case sy_const:
					//FIXME
				case sy_type:
				case sy_expr:
					internal_error (0, "invalid operand type");
			}
			break;
		case op_value:
			//FIXME share immediates
			def = new_def (".imm", ev_types[op->type], pr.near_data,
						   st_static);
			memcpy (D_POINTER (pr_type_t, def), &op->o.value,
					pr_type_size[op->type]);
			return def;
		case op_label:
			zero_def.type = &type_short;
			return &zero_def;	//FIXME
		case op_temp:
			if (!op->o.def)
				op->o.def = new_def (".tmp", ev_types[op->type],
									 current_func->symtab->space, st_local);
			return op->o.def;
	}
	return 0;
}

static void
add_statement_ref (def_t *def, dstatement_t *st, int field)
{
	if (def) {
		int         st_ofs = st - pr.code->code;

		if (def->alias) {
			reloc_op_def (def->alias, st_ofs, field);
			free_def (def);
		} else {
			reloc_op_def (def, st_ofs, field);
		}
	}
}

static void
emit_statement (statement_t *statement)
{
	const char *opcode = statement->opcode;
	def_t      *def_a = get_operand_def (statement->opa);
	def_t      *def_b = get_operand_def (statement->opb);
	def_t      *def_c = get_operand_def (statement->opc);
	opcode_t   *op = opcode_find (opcode, def_a, def_b, def_c);
	dstatement_t *s;

	puts (opcode);
	if (!op)
		internal_error (0, "ice ice baby");
	s = codespace_newstatement (pr.code);
	s->op = op->opcode;
	s->a = def_a ? def_a->offset : 0;
	s->b = def_b ? def_b->offset : 0;
	s->c = def_c ? def_c->offset : 0;

	add_statement_ref (def_a, s, 0);
	add_statement_ref (def_b, s, 0);
	add_statement_ref (def_c, s, 0);
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
