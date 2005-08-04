/*
	reloc.c

	relocation support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/07

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "def.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "qfcc.h"
#include "reloc.h"

static reloc_t *free_refs;

void
relocate_refs (reloc_t *refs, int ofs)
{
	int         o;

	while (refs) {
		switch (refs->type) {
			case rel_none:
				break;
			case rel_op_a_def:
				if (ofs > 65535)
					error (0, "def offset too large");
				else
					pr.code->code[refs->ofs].a = ofs;
				break;
			case rel_op_b_def:
				if (ofs > 65535)
					error (0, "def offset too large");
				else
					pr.code->code[refs->ofs].b = ofs;
				break;
			case rel_op_c_def:
				if (ofs > 65535)
					error (0, "def offset too large");
				else
					pr.code->code[refs->ofs].c = ofs;
				break;
			case rel_op_a_op:
				o = ofs - refs->ofs;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.code->code[refs->ofs].a = o;
				break;
			case rel_op_b_op:
				o = ofs - refs->ofs;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.code->code[refs->ofs].b = o;
				break;
			case rel_op_c_op:
				o = ofs - refs->ofs;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.code->code[refs->ofs].c = o;
				break;
			case rel_def_op:
				if (ofs > pr.code->size) {
					error (0, "invalid statement offset: %d >= %d, %d",
						   ofs, pr.code->size, refs->ofs);
				} else
					G_INT (refs->ofs) = ofs;
				break;
			case rel_def_def:
			case rel_def_func:
				G_INT (refs->ofs) = ofs;
				break;
			case rel_def_string:
				break;
			case rel_def_field:
				break;
			case rel_op_a_def_ofs:
				o = ofs + pr.code->code[refs->ofs].a;
				if (o < 0 || o > 65535)
					error (0, "def offset out of range");
				else
					pr.code->code[refs->ofs].a = o;
				break;
			case rel_op_b_def_ofs:
				o = ofs + pr.code->code[refs->ofs].b;
				if (o < 0 || o > 65535)
					error (0, "def offset out of range");
				else
					pr.code->code[refs->ofs].b = o;
				break;
			case rel_op_c_def_ofs:
				o = ofs + pr.code->code[refs->ofs].c;
				if (o < 0 || o > 65535)
					error (0, "def offset out of range");
				else
					pr.code->code[refs->ofs].c = o;
				break;
			case rel_def_def_ofs:
				G_INT (refs->ofs) += ofs;
				break;
			case rel_def_field_ofs:
				G_INT (refs->ofs) += G_INT (ofs);
				break;
		}
		refs = refs->next;
	}
}

reloc_t *
new_reloc (int ofs, reloc_type type)
{
	reloc_t    *ref;

	ALLOC (16384, reloc_t, refs, ref);
	ref->ofs = ofs;
	ref->type = type;
	ref->line = pr.source_line;
	ref->file = pr.source_file;
	return ref;
}

void
reloc_op_def (def_t *def, int ofs, int field)
{
	reloc_t    *ref = new_reloc (ofs, rel_op_a_def + field);
	ref->next = def->refs;
	def->refs = ref;
}

void
reloc_op_def_ofs (def_t *def, int ofs, int field)
{
	reloc_t    *ref = new_reloc (ofs, rel_op_a_def_ofs + field);
	ref->next = def->refs;
	def->refs = ref;
}

void
reloc_def_def (def_t *def, int ofs)
{
	reloc_t    *ref = new_reloc (ofs, rel_def_def);
	ref->next = def->refs;
	def->refs = ref;
}

void
reloc_def_def_ofs (def_t *def, int ofs)
{
	reloc_t    *ref = new_reloc (ofs, rel_def_def_ofs);
	ref->next = def->refs;
	def->refs = ref;
}

void
reloc_def_func (function_t *func, int ofs)
{
	reloc_t    *ref = new_reloc (ofs, rel_def_func);
	ref->next = func->refs;
	func->refs = ref;
}

void
reloc_def_string (int ofs)
{
	reloc_t    *ref = new_reloc (ofs, rel_def_string);
	ref->next = pr.relocs;
	pr.relocs = ref;
}

void
reloc_def_field (def_t *def, int ofs)
{
	reloc_t    *ref = new_reloc (ofs, rel_def_field);
	ref->next = def->refs;
	def->refs = ref;
}

void
reloc_def_field_ofs (def_t *def, int ofs)
{
	reloc_t    *ref = new_reloc (ofs, rel_def_field_ofs);
	ref->next = def->refs;
	def->refs = ref;
}

void
reloc_def_op (ex_label_t *label, int ofs)
{
	reloc_t    *ref = new_reloc (ofs, rel_def_op);
	ref->next = pr.relocs;
	ref->label = label;
	pr.relocs = ref;
}
