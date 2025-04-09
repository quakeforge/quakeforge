/*
	struct.c

	structure support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>
	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/12/08
	Date: 2011/01/17

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

#include <QF/dstring.h>
#include <QF/hash.h>
#include <QF/sys.h>
#include <QF/va.h>

#include <QF/progs/pr_obj.h>

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/obj_type.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static symbol_t *
find_tag (ty_meta_e meta, symbol_t *tag, type_t *type)
{
	const char *tag_name;
	symbol_t   *sym;

	if (tag) {
		tag_name = va ("tag %s", tag->name);
	} else {
		const char *path = GETSTR (pr.loc.file);
		const char *file = strrchr (path, '/');
		if (!file++)
			file = path;
		tag_name = va ("tag .%s.%d", file, pr.loc.line);
	}
	sym = symtab_lookup (current_symtab, tag_name);
	if (sym) {
		if (sym->table == current_symtab && sym->type->meta != meta)
			error (0, "%s defined as wrong kind of tag", tag_name);
		if (sym->type->meta == meta)
			return sym;
	}
	sym = new_symbol (tag_name);
	type_t *t = type;
	if (!t)
		t = new_type ();
	if (!t->name)
		t->name = sym->name;
	t->type = ev_invalid;
	t->meta = meta;
	sym->type = t;
	sym->sy_type = sy_type;
	return sym;
}

symtab_t *
start_struct (int *su, symbol_t *tag, symtab_t *parent)
{
	symbol_t   *sym;
	sym = find_struct (*su, tag, 0);
	if (!sym->table) {
		symtab_addsymbol (parent, sym);
	} else {
		if (!sym->type) {
			internal_error (0, "broken structure symbol?");
		}
		if (tag) {
			tag->type = sym->type;
		}
		if (sym->type->meta == ty_enum) {
			error (0, "enum %s redefined", tag->name);
		} else if (sym->type->meta == ty_struct && sym->type->symtab) {
			static const char *su_names[] = {
				"???",
				"union",
				"block",
				"struct",
			};
			error (0, "%s %s redefined", su_names[*su & 3], tag->name);
			*su = 0;
		} else if (sym->type->meta != ty_struct) {
			internal_error (0, "%s is not a struct or union",
							tag->name);
		}
	}
	static stab_type_e stab_types[] = {
		stab_struct,
		stab_union,
		stab_block,
		stab_struct,
	};
	return new_symtab (parent, stab_types[*su & 3]);
}

symbol_t *
find_handle (symbol_t *tag, const type_t *type)
{
	if (type != &type_int && type != &type_long) {
		error (0, "@handle type must be int or long");
		type = &type_int;
	}
	symbol_t   *sym = find_tag (ty_handle, tag, 0);
	if (sym->type->type == ev_invalid) {
		type_t *t = (type_t *) sym->type;//FIXME
		t->type = type->type;
		t->width = 1;
		t->columns = 1;
		t->alignment = type->alignment;
	}
	if (sym->type->type != type->type) {
		error (0, "@handle %s redeclared with different base type", tag->name);
	}
	return sym;
}

symbol_t *
find_struct (int su, symbol_t *tag, type_t *type)
{
	ty_meta_e   meta = ty_struct;

	if (su == 'u')
		meta = ty_union;

	return find_tag (meta, tag, type);
}

symbol_t *
build_struct (int su, symbol_t *tag, symtab_t *symtab, type_t *type,
			  int base)
{
	symbol_t   *sym = find_struct (su, tag, type);
	symbol_t   *s;
	int         alignment = 1;
	symbol_t   *as;

	symtab->parent = 0;		// disconnect struct's symtab from parent scope

	if (sym->table == current_symtab && sym->type->symtab) {
		error (0, "%s defined as wrong kind of tag", tag->name);
		return sym;
	}
	int index = 0;
	int offset = 0;
	for (s = symtab->symbols; s; s = s->next) {
		if (s->sy_type != sy_offset)
			continue;
		if (!s->type) {
			if (su != 's' || strcmp (s->name, ".reset") != 0) {
				internal_error (0, "invalid struct field");
			}
			index = 0;
			offset = 0;
			continue;
		}
		if (is_class (s->type)) {
			error (0, "statically allocated instance of class %s",
				   s->type->class->name);
		}
		if (su == 's') {
			offset = RUP (offset + base, s->type->alignment) - base;
			s->offset = offset;
			offset += type_size (s->type);
		} else {
			int         size = type_size (s->type);
			s->offset = 0;
			if (size > symtab->size) {
				symtab->size = RUP (size, s->type->alignment);
			}
		}
		if (s->type->alignment > alignment) {
			alignment = s->type->alignment;
		}
		if (s->visibility == vis_anonymous) {
			symtab_t   *anonymous;
			symbol_t   *t = s->next;
			int         offset = s->offset;

			if (!is_struct (s->type) && !is_union (s->type)) {
				internal_error (0, "non-struct/union anonymous field");
			}
			anonymous = s->type->symtab;
			for (as = anonymous->symbols; as; as = as->next) {
				if (as->visibility == vis_anonymous
					|| as->sy_type != sy_offset) {
					continue;
				}
				if (Hash_Find (symtab->tab, as->name)) {
					error (0, "ambiguous field `%s' in anonymous %s",
						   as->name, su == 's' ? "struct" : "union");
				} else {
					s->next = copy_symbol (as);
					s = s->next;
					s->offset += offset;
					s->table = symtab;
					s->no_auto_init = true;
					s->id = index++;
					Hash_Add (symtab->tab, s);
				}
			}
			s->next = t;
		} else {
			s->id = index++;
		}
	}
	if (su == 's') {
		symtab->size = offset;
	}
	symtab->count = index;
	if (!type)
		sym->type = find_type (sym->type);	// checks the tag, not the symtab
	((type_t *) sym->type)->symtab = symtab;
	if (alignment > sym->type->alignment) {
		((type_t *) sym->type)->alignment = alignment;
	}
	//FIXME should not be necessary
	if (!type && type_encodings.a[sym->type->id]->external) {
		unsigned id = sym->type->id;
		type_encodings.a[id] = qfo_encode_type (sym->type, pr.type_data);
	}
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
	if (sym->table == current_symtab && sym->type->symtab) {
		error (0, "%s defined as wrong kind of tag", sym->name);
		sym = find_enum (0);
	}
	((type_t *) sym->type)->symtab = new_symtab (current_symtab, stab_enum);
	((type_t *) sym->type)->alignment = 1;
	((type_t *) sym->type)->width = 1;
	((type_t *) sym->type)->columns = 1;
	return sym->type->symtab;
}

symbol_t *
finish_enum (symbol_t *sym)
{
	symbol_t   *enum_sym;
	symbol_t   *name;
	symtab_t   *enum_tab;

	auto enum_type = sym->type = find_type (sym->type);
	enum_tab = enum_type->symtab;

	for (name = enum_tab->symbols; name; name = name->next) {
		name->type = sym->type;

		enum_sym = new_symbol_type (name->name, enum_type);
		enum_sym->sy_type = sy_const;
		enum_sym->value = name->value;
		symtab_addsymbol (enum_tab->parent, enum_sym);
	}
	return sym;
}

void
add_enum (symbol_t *enm, symbol_t *name, const expr_t *val)
{
	auto enum_type = enm->type;
	symtab_t   *enum_tab = enum_type->symtab;
	int         value;

	if (name->table == current_symtab || name->table == enum_tab)
		error (0, "%s redefined", name->name);
	if (name->table)
		name = new_symbol (name->name);
	name->sy_type = sy_const;
	name->type = enum_type;
	value = 0;
	if (enum_tab->symbols)
		value = ((symbol_t *)(enum_tab->symtail))->value->uint_val + 1;
	if (val) {
		if (!is_constant (val))
			error (val, "non-constant initializer");
		else if (!is_int_val (val))
			error (val, "invalid initializer type");
		else
			value = expr_int (val);
	}
	name->value = new_int_val (value);
	symtab_addsymbol (enum_tab, name);
}

bool
enum_as_bool (const type_t *enm, expr_t **zero, expr_t **one)
{
	symtab_t   *symtab = enm->symtab;
	symbol_t   *zero_sym = 0;
	symbol_t   *one_sym = 0;
	symbol_t   *sym;
	int         val, v;

	if (!symtab)
		return false;
	for (sym = symtab->symbols; sym; sym = sym->next) {
		if (sym->sy_type != sy_const)
			continue;
		val = sym->value->int_val;
		if (!val) {
			zero_sym = sym;
		} else {
			if (one_sym) {
				v = one_sym->value->int_val;
				if (val * val > v * v)
					continue;
			}
			one_sym = sym;
		}

	}
	if (!zero_sym || !one_sym)
		return false;
	*zero = new_symbol_expr (zero_sym);
	*one = new_symbol_expr (one_sym);
	return true;
}

symbol_t *
make_structure (const char *name, int su, struct_def_t *defs, type_t *type)
{
	symtab_t   *strct;
	symbol_t   *field;
	symbol_t   *sym = 0;

	if (name)
		sym = new_symbol (name);
	if (su == 'u')
		strct = new_symtab (0, stab_union);
	else
		strct = new_symtab (0, stab_struct);
	while (defs->name) {
		field = new_symbol_type (defs->name, defs->type);
		field->sy_type = sy_offset;
		if (!symtab_addsymbol (strct, field))
			internal_error (0, "duplicate symbol: %s", defs->name);
		defs++;
	}
	sym = build_struct (su, sym, strct, type, 0);
	return sym;
}

def_t *
emit_structure (const char *name, int su, struct_def_t *defs,
				const type_t *type, void *data, defspace_t *space,
				storage_class_t storage)
{
	int         i, j;
	int         saw_null = 0;
	int         saw_func = 0;
	symbol_t   *struct_sym;
	symbol_t   *field_sym;
	def_t      *struct_def;
	def_t       field_def;

	name = save_string (name);
	if (!type)
		type = make_structure (0, su, defs, 0)->type;
	if ((su == 's' && !is_struct (type)) || (su == 'u' && !is_union (type)))
		internal_error (0, "structure %s type mismatch", name);
	for (i = 0, field_sym = type->symtab->symbols; field_sym;
		 i++, field_sym = field_sym->next) {
		if (!defs[i].name)
			internal_error (0, "structure %s unexpected end of defs", name);
		if (field_sym->type != defs[i].type)
			internal_error (0, "structure %s.%s field type mismatch", name,
							defs[i].name);
		if ((!defs[i].emit && saw_func) || (defs[i].emit && saw_null))
			internal_error (0, "structure %s mixed emit/copy", name);
		if (!defs[i].emit)
			saw_null = 1;
		if (defs[i].emit)
			saw_func = 1;
	}
	if (defs[i].name)
		internal_error (0, "structure %s too many defs", name);
	if (storage != sc_global && storage != sc_static)
		internal_error (0, "structure %s must be global or static", name);

	if (!space) {
		space = pr.far_data;
	}
	struct_sym = make_symbol (name, type, space, storage);

	struct_def = struct_sym->def;
	if (struct_def->initialized)
		internal_error (0, "structure %s already initialized", name);
	struct_def->initialized = struct_def->constant = 1;
	struct_def->nosave = 1;

	for (i = 0, field_sym = type->symtab->symbols; field_sym;
		 i++, field_sym = field_sym->next) {
		field_def.type = field_sym->type;
		field_def.name = save_string (va ("%s.%s", name, field_sym->name));
		field_def.space = struct_def->space;
		field_def.offset = struct_def->offset + field_sym->offset;
		if (!defs[i].emit) {
			//FIXME relocs? arrays? structs?
			pr_type_t  *val = (pr_type_t *) data;
			memcpy (D_POINTER (pr_type_t, &field_def), val,
					type_size (field_def.type) * sizeof (pr_type_t));
			data = &val[type_size (field_def.type)];
		} else {
			if (is_array (field_def.type)) {
				auto type = dereference_type (field_def.type);
				for (j = 0; j < field_def.type->array.count; j++) {
					defs[i].emit (&field_def, data, j);
					field_def.offset += type_size (type);
				}
			} else {
				defs[i].emit (&field_def, data, 0);
			}
		}
	}
	return struct_def;
}

symbol_t *
make_handle (const char *name, type_t *type)
{
	auto tag = new_symbol (name);
	return find_tag (ty_handle, tag, type);
}
