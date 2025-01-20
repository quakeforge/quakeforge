/*
	symtab.c

	Symbol table management.

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/05

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

#include <stdlib.h>
#include <string.h>

#include "QF/alloc.h"
#include "QF/hash.h"

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

ALLOC_STATE (symtab_t, symtabs);
ALLOC_STATE (symbol_t, symbols);

#define SY_TYPE(type) #type,
static const char * const sy_type_names[sy_num_types] = {
#include "tools/qfcc/include/sy_type_names.h"
};

const char *
symtype_str (sy_type_e type)
{
	if (type < sy_num_types) {
		return sy_type_names[type];
	}
	return "<invalid sy_type>";
}

symbol_t *
new_symbol (const char *name)
{
	symbol_t   *symbol;
	ALLOC (256, symbol_t, symbols, symbol);
	if (name) {
		symbol->name = save_string (name);
	}
	symbol->return_addr = __builtin_return_address (0);
	return symbol;
}

symbol_t *
new_symbol_type (const char *name, const type_t *type)
{
	symbol_t   *symbol;
	symbol = new_symbol (name);
	symbol->type = type;
	symbol->return_addr = __builtin_return_address (0);
	return symbol;
}

static const char *
sym_getkey (const void *k, void *unused)
{
	return ((symbol_t *) k)->name;
}

symtab_t *
new_symtab (symtab_t *parent, stab_type_e type)
{
	symtab_t    *symtab;
	int          tabsize = 63;

	ALLOC (16, symtab_t, symtabs, symtab);

	symtab->parent = parent;
	symtab->type = type;
	if (symtab->type == stab_global)
		tabsize = 1023;
	symtab->tab = Hash_NewTable (tabsize, sym_getkey, 0, 0, 0);
	symtab->symtail = &symtab->symbols;
	return symtab;
}

symbol_t *
symtab_lookup (symtab_t *symtab, const char *name)
{
	symbol_t   *symbol;
	do {
		if ((symbol = Hash_Find (symtab->tab, name))) {
			return symbol;
		}
		if (symtab->procsymbol
			&& (symbol = symtab->procsymbol (name, symtab))) {
			return symbol;
		}
		symtab = symtab->parent;
	} while (symtab);
	return 0;
}

symbol_t *
symtab_appendsymbol (symtab_t *symtab, symbol_t *symbol)
{
	if (symbol->table)
		internal_error (0, "symbol '%s' is already in another symbol table",
						symbol->name);

	symbol->next = *symtab->symtail;
	*symtab->symtail = symbol;
	symtab->symtail = &symbol->next;

	symbol->table = symtab;

	return symbol;
}

symbol_t *
symtab_addsymbol (symtab_t *symtab, symbol_t *symbol)
{
	symbol_t   *s;
	if (symbol->table)
		internal_error (0, "symbol '%s' is already in another symbol table",
						symbol->name);
	if (symtab->type == stab_bypass) {
		if (!symtab->parent) {
			internal_error (0, "bypass symbol table with no parent");
		}
		symtab = symtab->parent;
	}
	if ((s = Hash_Find (symtab->tab, symbol->name)))
		return s;
	Hash_Add (symtab->tab, symbol);

	return symtab_appendsymbol (symtab, symbol);
}

symbol_t *
symtab_removesymbol (symtab_t *symtab, symbol_t *symbol)
{
	symbol_t  **s;

	if (!(symbol = Hash_Del (symtab->tab, symbol->name)))
		return 0;
	for (s = &symtab->symbols; *s && *s != symbol; s = & (*s)->next)
		;
	if (!*s)
		internal_error (0, "attempt to remove symbol not in symtab");
	*s = (*s)->next;
	if (symtab->symtail == &symbol->next)
		symtab->symtail = s;
	symbol->next = 0;
	symbol->table = 0;
	return symbol;
}

symbol_t *
copy_symbol (symbol_t *symbol)
{
	symbol_t   *sym = new_symbol (symbol->name);
	*sym = *symbol;
	sym->next = nullptr;
	sym->table = nullptr;
	sym->params = copy_params (symbol->params);
	return sym;
}

symtab_t *
symtab_flat_copy (symtab_t *symtab, symtab_t *parent, stab_type_e type)
{
	symtab_t   *newtab;
	symbol_t   *newsym;
	symbol_t   *symbol;

	newtab = new_symtab (parent, type);
	do {
		for (symbol = symtab->symbols; symbol; symbol = symbol->next) {
			if (symbol->visibility == vis_anonymous
				|| Hash_Find (newtab->tab, symbol->name))
				continue;
			newsym = copy_symbol (symbol);
			symtab_addsymbol (newtab, newsym);
		}
		symtab = symtab->parent;
		// Set the tail pointer so symbols in ancestor tables come before
		// those in decendent tables.
		newtab->symtail = &newtab->symbols;
	} while (symtab);
	// Reset the tail pointer so any symbols added to newtab come after
	// those copied from the input symbol table chain.
	for (symbol = newtab->symbols; symbol && symbol->next;
		 symbol = symbol->next)
		;
	newtab->symtail = symbol ? &symbol->next : &newtab->symbols;
	return newtab;
}

symbol_t *
make_symbol (const char *name, const type_t *type, defspace_t *space,
			 storage_class_t storage)
{
	symbol_t   *sym;
	struct reloc_s *relocs = 0;

	if (storage != sc_extern && storage != sc_global && storage != sc_static)
		internal_error (0, "invalid storage class for %s", __FUNCTION__);
	if (storage != sc_extern && !space)
		internal_error (0, "null space for non-external storage");
	sym = symtab_lookup (pr.symtab, name);
	if (!sym) {
		sym = new_symbol_type (name, type);
	}
	if (sym->type != type) {
		if (is_array (sym->type) && is_array (type)
			&& !sym->type->array.count) {
			sym->type = type;
		} else {
			error (0, "%s redefined", name);
			sym = new_symbol_type (name, type);
		}
	}
	if (sym->def && sym->def->external && storage != sc_extern) {
		//FIXME this really is not the right way
		relocs = sym->def->relocs;
		free_def (sym->def);
		sym->def = 0;
	}
	if (!sym->def) {
		sym->def = new_def (name, type, space, storage);
		reloc_attach_relocs (relocs, &sym->def->relocs);
	}
	sym->sy_type = sy_def;
	sym->lvalue = !sym->def->readonly;
	return sym;
}

static bool
shadows_param (symbol_t *sym, symtab_t *symtab)
{
	symbol_t   *check = symtab_lookup (symtab, sym->name);

	if (check && check->table == symtab->parent
		&& symtab->parent->type == stab_param) {
		error (0, "%s shadows a parameter", sym->name);
		return true;
	}
	return false;
}

static bool
init_type_ok (const type_t *dstType, const expr_t *init)
{
	auto init_type = get_type (init);
	if (init->implicit || type_promotes (dstType, init_type)) {
		return true;
	}
	if (is_pointer (dstType) && is_pointer (init_type)) {
		return true;
	}
	if (is_math (dstType) && !is_scalar (dstType)) {
		// vector or matrix type
		return true;
	}
	if (is_integral (dstType) && is_integral (init_type)
		&& (type_size (dstType) == type_size (init_type))) {
		return true;
	}
	return false;
}

symbol_t *
declare_symbol (specifier_t spec, const expr_t *init, symtab_t *symtab,
				expr_t *block, rua_ctx_t *ctx)
{
	symbol_t   *sym = spec.sym;

	if (sym->table) {
		// due to the way declarations work, we need a new symbol at all times.
		// redelcarations will be checked later
		sym = new_symbol (sym->name);
		*sym = *spec.sym;
		sym->table = nullptr;
	}

	if (spec.is_typedef || !spec.is_function) {
		spec = spec_process (spec, ctx);
	}
	if (!spec.storage) {
		spec.storage = current_storage;
	}

	sym->type = spec.type;

	if (spec.is_typedef) {
		if (init) {
			error (0, "typedef %s is initialized", sym->name);
		}
		sym->sy_type = sy_type;
		sym->type = find_type (sym->type);
		sym->type = find_type (alias_type (sym->type, sym->type, sym->name));
		symtab_addsymbol (symtab, sym);
	} else {
		if (spec.is_function) {
			if (init) {
				error (0, "function %s is initialized", sym->name);
			}
			sym = function_symbol (spec, ctx);
		} else {
			if (!shadows_param (sym, symtab)) {
				sym->type = find_type (sym->type);
				spec.sym = sym;
				if (init && init->type != ex_compound && !is_auto (sym->type)) {
					auto init_type = get_type (init);
					if (!type_same (init_type, sym->type)
						&& type_assignable (sym->type, init_type)) {
						if (!init_type_ok (sym->type, init)) {
							warning (init, "initialization of %s with %s"
									 " (use a cast)\n)",
									 get_type_string (spec.type),
									 get_type_string (init_type));
						}
						if (init->type != ex_bool) {
							init = cast_expr (sym->type, init);
						}
					}
				}
				current_target.declare_sym (spec, init, symtab, block);
			}
		}
	}
	return sym;
}

symbol_t *
declare_field (specifier_t spec, symtab_t *symtab, rua_ctx_t *ctx)
{
	symbol_t   *s = spec.sym;
	spec = spec_process (spec, ctx);
	s->type = find_type (append_type (s->type, spec.type));
	s->sy_type = sy_offset;
	s->visibility = current_visibility;
	symtab_addsymbol (current_symtab, s);
	if (!s->table) {
		error (0, "duplicate field `%s'", s->name);
	}
	return s;
}
