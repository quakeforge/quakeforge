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

#include "tools/qfcc/include/attribute.h"
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

static const char *
get_file_name (void)
{
	const char *path = GETSTR (pr.loc.file);
	const char *file = strrchr (path, '/');
	if (!file++) {
		file = path;
	}
	return file;
}

static symbol_t *
find_tag (const type_t *tag_type, symbol_t *tag, type_t *type)
{
	const char *tag_name;
	symbol_t   *sym;

	if (tag) {
		tag_name = va ("tag %s", tag->name);
	} else {
		tag_name = va ("tag .%s.%d", get_file_name (), pr.loc.line);
	}
	sym = symtab_lookup (current_symtab, tag_name);
	if (sym) {
		if (sym->table == current_symtab && sym->type->meta != tag_type->meta)
			error (0, "%s defined as wrong kind of tag", tag_name);
		if (sym->type->meta == tag_type->meta)
			return sym;
	}
	sym = new_symbol (tag_name);
	if (type) {
		type->type = tag_type->type;
		type->meta = tag_type->meta;
	} else {
		type = copy_type (tag_type);
	}
	if (!type->name)
		type->name = sym->name;
	sym->type = type;
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
	type = unalias_type (type);
	if (type != &type_int && type != &type_long) {
		error (0, "@handle type must be int or long");
		type = &type_int;
	}
	auto handle_type = copy_type (type);
	handle_type->meta = ty_handle;
	symbol_t   *sym = find_tag (handle_type, tag, 0);
	free_type (handle_type);
	if (sym->type->type != type->type) {
		error (0, "@handle %s redeclared with different base type", tag->name);
	}
	return sym;
}

symbol_t *
find_struct (int su, symbol_t *tag, type_t *type)
{
	auto struct_type = copy_type (type);
	struct_type->type = ev_invalid;
	struct_type->meta = su == 'u' ? ty_union : ty_struct;

	auto sym = find_tag (struct_type, tag, type);
	free_type (struct_type);

	return sym;
}

typedef struct {
	symbol_t   *symbol_list;
	symbol_t  **symbol_tail;
	symtab_t   *symtab;

	const type_t *bit_type;
	int         su;
	int         base;
	int         index;
	int         offset;
	int         alignment;
	int         bit_offset;
	int         bit_index;
} struct_state_t;

static void
append_symbol (struct_state_t *state, symbol_t *s)
{
	*state->symbol_tail = s;
	state->symbol_tail = &s->next;
}

static void
struct_offset (struct_state_t *state, symbol_t *s)
{
	if (state->su == 's') {
		int offset = state->offset + state->base;
		offset = RUP (offset, s->type->alignment) - state->base;
		s->offset = offset;
		state->offset = offset + type_size (s->type);
	} else {
		int         size = type_size (s->type);
		s->offset = 0;
		if (size > state->symtab->size) {
			state->symtab->size = RUP (size, s->type->alignment);
		}
	}
	if (s->type->alignment > state->alignment) {
		state->alignment = s->type->alignment;
	}
}

static symbol_t *
bitdata_sym (struct_state_t *state)
{
	auto s = new_symbol_type (va (".bits%d", state->bit_index++),
							  state->bit_type);
	state->bit_offset = 0;
	state->bit_type = nullptr;
	struct_offset (state, s);

	s->sy_type = sy_offset;
	s->table = state->symtab;
	s->no_auto_init = true;
	s->id = state->index++;
	Hash_Add (state->symtab->tab, s);
	return s;
}

typedef struct {
	int         start;
	int         length;
	const expr_t *member;
} bitfield_t;

static const expr_t *
bitfield_assign (const expr_t *dst, const expr_t *src)
{
	scoped_src_loc (dst);
	if (dst->type != ex_bitfield) {
		internal_error (dst, "not a bitfield expression");
	}

	auto lvalue = dst->bitfield.src;
	auto rvalue = new_expr ();
	rvalue->type = ex_bitfield;
	rvalue->bitfield = dst->bitfield;
	rvalue->bitfield.insert = src;

	return assign_expr (lvalue, rvalue);
}

static const expr_t *
bitfield_expr (symbol_t *symbol, void *data, const expr_t *obj)
{
	bitfield_t *bitfield = data;
	scoped_src_loc (obj);

	auto start = new_int_expr (bitfield->start, false);
	auto length = new_int_expr (bitfield->length, false);
	auto field = get_struct_field (get_type (obj), obj, bitfield->member);
	auto src = new_field_expr (obj, new_symbol_expr (field));
	src->field.type = field->type;
	auto expr = new_expr ();
	expr->type = ex_bitfield;
	expr->bitfield = (ex_bitfield_t) {
		.src = edag_add_expr (src),
		.start = edag_add_expr (start),
		.length = edag_add_expr (length),
		// .insert will be filled in for lvalues
		.type = symbol->type,
	};

	auto xvalue = new_xvalue_expr (edag_add_expr (expr), true);
	xvalue->xvalue.assign = bitfield_assign;

	return xvalue;
}

static symbol_t *
bitfield_sym (struct_state_t *state, symbol_t *s)
{
	auto member = new_symbol (va (".bits%d", state->bit_index));
	bitfield_t *bitfield = malloc (sizeof (bitfield_t));
	*bitfield = (bitfield_t) {
		.start = state->bit_offset,
		.length = s->offset,
		.member = new_symbol_expr (member),
	};
	s->sy_type = sy_convert;
	s->convert = (symconv_t) {
		.conv_expr = bitfield_expr,
		.data = bitfield,
	};
	return s;
}

symbol_t *
build_struct (int su, symbol_t *tag, symtab_t *symtab, type_t *type,
			  int base)
{
	symbol_t   *sym = find_struct (su, tag, type);
	symbol_t   *s;
	symbol_t   *as;

	symtab->parent = 0;		// disconnect struct's symtab from parent scope

	if (sym->table == current_symtab && sym->type->symtab) {
		error (0, "%s defined as wrong kind of tag", tag->name);
		return sym;
	}

	struct_state_t state = {
		.symbol_tail = &state.symbol_list,
		.symtab = symtab,
		.su = su,
		.base = base,
		.alignment = 1,
	};

	for (s = symtab->symbols; s; s = s->next) {
		if (s->sy_type != sy_offset && s->sy_type != sy_bitfield) {
			append_symbol (&state, s);
			continue;
		}
		if (!s->type) {
			if (su != 's' || strcmp (s->name, ".reset") != 0) {
				internal_error (0, "invalid struct field");
			}
			if (state.bit_type) {
				append_symbol (&state, bitdata_sym (&state));
			}
			state.index = 0;
			state.offset = 0;
			continue;
		}
		if (is_class (s->type)) {
			error (0, "statically allocated instance of class %s",
				   s->type->class->name);
		}
		if (state.bit_type) {
			if (s->sy_type != sy_bitfield
				|| (s->sy_type == sy_bitfield && !s->offset)) {
				append_symbol (&state, bitdata_sym (&state));
			}
		}
		if (s->sy_type == sy_bitfield) {
			if (!state.bit_type
				|| type_size (s->type) > type_size (state.bit_type)) {
				state.bit_type = uint_type (s->type);
			}
			if (s->offset < 0) {
				error (0, "bitfield widths must be zero or positive");
				s->offset = 1;
			}
			if (!s->offset) {
				// 0-width bitfields align to the next bounary, but that
				// is handled above
				if (s->name) {
					append_symbol (&state, s);
				}
				continue;
			}
			if (s->offset > type_size (s->type) * 32) {
				warning (0, "clamping bitfield width to type bit-width");
				s->offset = type_size (s->type) * 32;
			}
			int bit_end = state.bit_offset + s->offset;
			if (bit_end > type_size (state.bit_type) * 32) {
				// overflowing the backing word, need a new one
				append_symbol (&state, bitdata_sym (&state));
				state.bit_type = uint_type (s->type);
				bit_end = s->offset;
			}
			if (s->name) {
				append_symbol (&state, bitfield_sym (&state, s));
			}
			state.bit_offset = bit_end;
			continue;
		}
		struct_offset (&state, s);
		if (s->visibility == vis_anonymous) {
			symtab_t   *anonymous;
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
					auto am = copy_symbol (as);
					am->offset += offset;
					am->table = symtab;
					am->no_auto_init = true;
					am->id = state.index++;
					Hash_Add (symtab->tab, am);
					append_symbol (&state, am);
				}
			}
		} else {
			s->id = state.index++;
			append_symbol (&state, s);
		}
	}
	if (state.bit_type) {
		append_symbol (&state, bitdata_sym (&state));
	}
	symtab->symbols = state.symbol_list;

	if (su == 's') {
		symtab->size = state.offset;
	}
	symtab->count = state.index;
	if (!type)
		sym->type = find_type (sym->type);	// checks the tag, not the symtab
	((type_t *) sym->type)->symtab = symtab;
	if (state.alignment > sym->type->alignment) {
		((type_t *) sym->type)->alignment = state.alignment;
	}
	if (!type && type_encodings.a[sym->type->id]->external) {
		unsigned id = sym->type->id;
		type_encodings.a[id] = qfo_encode_type (sym->type, pr.type_data);
	}
	return sym;
}

symbol_t *
find_enum (symbol_t *tag, const type_t *type)
{
	type = unalias_type (type);
	if (type != &type_int && type != &type_long
		&& type != &type_uint && type != &type_ulong) {
		error (0, "enum type must be int, uint, long, or ulong");
		type = &type_int;
	}
	auto enum_type = copy_type (type);
	enum_type->meta = ty_enum;
	auto sym = find_tag (enum_type, tag, 0);
	free_type (enum_type);
	return sym;
}

symtab_t *
start_enum (symbol_t *sym)
{
	if (sym->table == current_symtab && sym->type->symtab) {
		error (0, "%s defined as wrong kind of tag", sym->name);
		sym = find_enum (nullptr, nullptr);
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
	bool        namespace = false;

	for (auto attr = sym->attributes; attr; attr = attr->next) {
		if (strcmp (attr->name, "namespace") == 0) {
			namespace = true;
		} else {
			warning (0, "skipping unknown enum attribute '%s'", attr->name);
		}
	}

	auto enum_type = sym->type;
	enum_tab = enum_type->symtab;

	if (namespace) {
		const char *name = save_string (sym->name + 4);
		enum_sym = new_symbol (name);
		enum_sym->sy_type = sy_namespace;
		enum_sym->namespace = enum_tab;
		symtab_addsymbol (enum_tab->parent, enum_sym);
	} else {
		for (name = enum_tab->symbols; name; name = name->next) {
			name->type = sym->type;

			enum_sym = new_symbol_type (name->name, enum_type);
			enum_sym->sy_type = sy_const;
			enum_sym->value = name->value;
			symtab_addsymbol (enum_tab->parent, enum_sym);
		}
	}
	if (type_encodings.a[enum_type->id]->external) {
		unsigned id = enum_type->id;
		type_encodings.a[id] = qfo_encode_type (enum_type, pr.type_data);
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
	if (!type) {
		const char *tag = va ("%s:.%s.%d", name, get_file_name (), pr.loc.line);
		type = make_structure (tag, su, defs, 0)->type;
	}
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
	auto handle_type = copy_type (type);
	handle_type->meta = ty_handle;
	symbol_t   *sym = find_tag (handle_type, tag, 0);
	free_type (handle_type);
	return sym;
}
