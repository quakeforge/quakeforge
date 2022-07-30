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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/alloc.h"

#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"

static reloc_t *refs_freelist;

const char * const reloc_name[] = {
	"rel_none",
	"rel_op_a_def",
	"rel_op_b_def",
	"rel_op_c_def",
	"rel_op_a_op",
	"rel_op_b_op",
	"rel_op_c_op",
	"rel_def_op",
	"rel_def_def",
	"rel_def_func",
	"rel_def_string",
	"rel_def_field",
	"rel_op_a_def_ofs",
	"rel_op_b_def_ofs",
	"rel_op_c_def_ofs",
	"rel_def_def_ofs",
	"rel_def_field_ofs",
};

#define RELOC(r) (r)->space->data[(r)->offset].value

void
relocate_refs (reloc_t *reloc, int offset)
{
	int         o;

	while (reloc) {
		debug (0, "reloc %s:%x %s %x",
			   reloc->space ? (reloc->space == pr.near_data ? "near" : "far")
			   				: "code",
			   reloc->offset, reloc_name[reloc->type], offset);
		switch (reloc->type) {
			case rel_none:
				break;
			case rel_op_a_def:
				if (offset > 65535)
					error (0, "def offset too large");
				else
					pr.code->code[reloc->offset].a = offset;
				break;
			case rel_op_b_def:
				if (offset > 65535)
					error (0, "def offset too large");
				else
					pr.code->code[reloc->offset].b = offset;
				break;
			case rel_op_c_def:
				if (offset > 65535)
					error (0, "def offset too large");
				else
					pr.code->code[reloc->offset].c = offset;
				break;
			case rel_op_a_op:
				o = offset - reloc->offset;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.code->code[reloc->offset].a = o;
				break;
			case rel_op_b_op:
				o = offset - reloc->offset;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.code->code[reloc->offset].b = o;
				break;
			case rel_op_c_op:
				o = offset - reloc->offset;
				if (o < -32768 || o > 32767)
					error (0, "relative offset too large");
				else
					pr.code->code[reloc->offset].c = o;
				break;
			case rel_def_op:
				if (offset > pr.code->size) {
					error (0, "invalid statement offset: %d >= %d, %d",
						   offset, pr.code->size, reloc->offset);
				} else
					RELOC (reloc) = offset;
				break;
			case rel_def_def:
			case rel_def_func:
				RELOC (reloc) = offset;
				break;
			case rel_def_string:
				break;
			case rel_def_field:
				break;
			case rel_op_a_def_ofs:
				o = offset + pr.code->code[reloc->offset].a;
				if (o < 0 || o > 65535)
					error (0, "def offset out of range");
				else
					pr.code->code[reloc->offset].a = o;
				break;
			case rel_op_b_def_ofs:
				o = offset + pr.code->code[reloc->offset].b;
				if (o < 0 || o > 65535)
					error (0, "def offset out of range");
				else
					pr.code->code[reloc->offset].b = o;
				break;
			case rel_op_c_def_ofs:
				o = offset + pr.code->code[reloc->offset].c;
				if (o < 0 || o > 65535)
					error (0, "def offset out of range");
				else
					pr.code->code[reloc->offset].c = o;
				break;
			case rel_def_def_ofs:
				RELOC (reloc) += offset;
				break;
			case rel_def_field_ofs:
				//FIXME what is correct here?
				//RELOC (reloc) += pr.data->data[offset].int_var;
				RELOC (reloc) += PR_PTR (int, &pr.near_data->data[offset]);
				break;
		}
		reloc = reloc->next;
	}
}

reloc_t *
new_reloc (defspace_t *space, int offset, reloc_type type)
{
	reloc_t    *ref;

	ALLOC (16384, reloc_t, refs, ref);
	ref->space = space;
	ref->offset = offset;
	ref->type = type;
	ref->line = pr.source_line;
	ref->file = pr.source_file;
	ref->return_address = __builtin_return_address (0);
	return ref;
}

void
reloc_op_def (def_t *def, int offset, int field)
{
	reloc_t    *ref = new_reloc (0, offset, rel_op_a_def + field);
	ref->return_address = __builtin_return_address (0);
	ref->next = def->relocs;
	def->relocs = ref;
}

void
reloc_op_def_ofs (def_t *def, int offset, int field)
{
	reloc_t    *ref = new_reloc (0, offset, rel_op_a_def_ofs + field);
	ref->return_address = __builtin_return_address (0);
	ref->next = def->relocs;
	def->relocs = ref;
}

void
reloc_def_def (def_t *def, const def_t *location)
{
	reloc_t    *ref;

	ref = new_reloc (location->space, location->offset, rel_def_def);
	ref->return_address = __builtin_return_address (0);
	ref->next = def->relocs;
	def->relocs = ref;
}

void
reloc_def_def_ofs (def_t *def, const def_t *location)
{
	reloc_t    *ref;

	ref = new_reloc (location->space, location->offset, rel_def_def_ofs);
	ref->return_address = __builtin_return_address (0);
	ref->next = def->relocs;
	def->relocs = ref;
}

void
reloc_def_func (function_t *func, const def_t *location)
{
	reloc_t    *ref;

	ref = new_reloc (location->space, location->offset, rel_def_func);
	ref->return_address = __builtin_return_address (0);
	ref->next = func->refs;
	func->refs = ref;
}

void
reloc_def_string (const def_t *location)
{
	reloc_t    *ref;

	ref = new_reloc (location->space, location->offset, rel_def_string);
	ref->return_address = __builtin_return_address (0);
	ref->next = pr.relocs;
	pr.relocs = ref;
}

void
reloc_def_field (def_t *def, const def_t *location)
{
	reloc_t    *ref;

	ref = new_reloc (location->space, location->offset, rel_def_field);
	ref->return_address = __builtin_return_address (0);
	ref->next = def->relocs;
	def->relocs = ref;
}

void
reloc_def_field_ofs (def_t *def, const def_t *location)
{
	reloc_t    *ref;

	ref = new_reloc (location->space, location->offset, rel_def_field_ofs);
	ref->return_address = __builtin_return_address (0);
	ref->next = def->relocs;
	def->relocs = ref;
}

void
reloc_def_op (const ex_label_t *label, const def_t *location)
{
	reloc_t    *ref;

	ref = new_reloc (location->space, location->offset, rel_def_op);
	ref->return_address = __builtin_return_address (0);
	ref->next = pr.relocs;
	ref->label = label;
	pr.relocs = ref;
}

void
reloc_attach_relocs (reloc_t *relocs, reloc_t **location)
{
	reloc_t    *r;

	if (!relocs)
		return;
	for (r = relocs; r->next; r = r->next)
		;
	r->next = *location;
	*location = relocs;
}
