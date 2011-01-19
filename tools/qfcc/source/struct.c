/*
	struct.c

	structure support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/12/08

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

#include <QF/dstring.h>
#include <QF/hash.h>
#include <QF/pr_obj.h>
#include <QF/sys.h>
#include <QF/va.h>

#include "def.h"
#include "emit.h"
#include "expr.h"
#include "immediate.h"
#include "qfcc.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"

static symbol_t *
find_tag (ty_type_e ty, symbol_t *tag, type_t *type)
{
	const char *tag_name;
	symbol_t   *sym;
	static int  tag_num;

	if (tag)
		tag_name = va ("tag %s", tag->name);
	else
		tag_name = va ("tag .%s.%d.%d",
					   G_GETSTR (pr.source_file), pr.source_line, tag_num++);
	sym = symtab_lookup (current_symtab, tag_name);
	if (sym) {
		if (sym->table == current_symtab && sym->type->ty != ty)
			error (0, "%s defined as wrong kind of tag", tag->name);
		if (sym->type->ty == ty)
			return sym;
	}
	if (!type)
		type = new_type ();
	sym = new_symbol (tag_name);
	sym->type = type;
	sym->type->type = ev_invalid;
	sym->type->ty = ty;
	return sym;
}

symbol_t *
find_struct (int su, symbol_t *tag, type_t *type)
{
	ty_type_e   ty = ty_struct;

	if (su == 'u')
		ty = ty_union;

	return find_tag (ty, tag, type);
}

symbol_t *
build_struct (int su, symbol_t *tag, symtab_t *symtab, type_t *type)
{
	symbol_t   *sym = find_struct (su, tag, type);
	symbol_t   *s;

	symtab->parent = 0;		// disconnect struct's symtab from parent scope

	if (sym->table == current_symtab && sym->type->t.symtab) {
		error (0, "%s defined as wrong kind of tag", tag->name);
		return sym;
	}
	for (s = symtab->symbols; s; s = s->next) {
		if (s->sy_type != sy_var)
			continue;
		if (su == 's') {
			s->s.offset = symtab->size;
			symtab->size += type_size (s->type);
		} else {
			int         size = type_size (s->type);
			if (size > symtab->size)
				symtab->size = size;
		}
	}
	sym->sy_type = sy_type;
	sym->type->t.symtab = symtab;
	return sym;
}

symbol_t *
find_enum (symbol_t *tag)
{
	return find_tag (ty_enum, tag, 0);
}

symtab_t *
start_enum (symbol_t *sym)
{
	if (sym->table == current_symtab && sym->type->t.symtab) {
		error (0, "%s defined as wrong kind of tag", sym->name);
		sym = find_enum (0);
	}
	sym->type->t.symtab = new_symtab (current_symtab, stab_local);
	return sym->type->t.symtab;
}

void
add_enum (symbol_t *enm, symbol_t *name, expr_t *val)
{
	type_t     *enum_type = enm->type;
	symtab_t   *enum_tab;
	int         value;

	if (name->table == current_symtab)
		error (0, "%s redefined", name->name);
	if (name->table)
		name = new_symbol (name->name);
	name->sy_type = sy_const;
	name->type = enum_type;
	enum_tab = enum_type->t.symtab;
	value = 0;
	if (*enum_tab->symtail)
		value = ((symbol_t *)(*enum_tab->symtail))->s.value.v.integer_val + 1;
	if (val) {
		if (!is_constant (val))
			error (val, "non-constant initializer");
		else if (!is_integer_val (val))
			error (val, "invalid initializer type");
		else
			value = expr_integer (val);
	}
	name->s.value.v.integer_val = value;
	symtab_addsymbol (enum_tab, name);
}

symbol_t *
make_structure (const char *name, int su, struct_def_t *defs, type_t *type)
{
	symtab_t   *strct;
	symbol_t   *field;
	symbol_t   *sym = 0;
	
	if (su == 'u')
		strct = new_symtab (0, stab_union);
	else
		strct = new_symtab (0, stab_struct);
	while (defs->name) {
		field = new_symbol_type (defs->name, defs->type);
		if (!symtab_addsymbol (strct, field))
			internal_error (0, "duplicate symbol: %s", defs->name);
		defs++;
	}
	if (name)
		sym = new_symbol (name);
	sym = build_struct (su, sym, strct, type);
	return sym;
}
