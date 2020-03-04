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

#include "class.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "function.h"
#include "qfcc.h"
#include "reloc.h"
#include "strpool.h"
#include "symtab.h"
#include "type.h"

static symtab_t *symtabs_freelist;
static symbol_t *symbols_freelist;

static const char *sy_type_names[] = {
	"sy_name",
	"sy_var",
	"sy_const",
	"sy_type",
	"sy_expr",
	"sy_func",
	"sy_class",
};

const char *
symtype_str (sy_type_e type)
{
	if (type > sy_class)
		return "<invalid sy_type>";
	return sy_type_names[type];
}

symbol_t *
new_symbol (const char *name)
{
	symbol_t   *symbol;
	ALLOC (256, symbol_t, symbols, symbol);
	symbol->name = save_string (name);
	return symbol;
}

symbol_t *
new_symbol_type (const char *name, type_t *type)
{
	symbol_t   *symbol;
	symbol = new_symbol (name);
	symbol->type = type;
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
	symtab->tab = Hash_NewTable (tabsize, sym_getkey, 0, 0);
	symtab->symtail = &symtab->symbols;
	return symtab;
}

symbol_t *
symtab_lookup (symtab_t *symtab, const char *name)
{
	symbol_t   *symbol;
	do {
		if ((symbol = Hash_Find (symtab->tab, name)))
			return symbol;
		symtab = symtab->parent;
	} while (symtab);
	return 0;
}

symbol_t *
symtab_addsymbol (symtab_t *symtab, symbol_t *symbol)
{
	symbol_t   *s;
	if (symbol->table)
		internal_error (0, "symbol '%s' is already in another symbol table",
						symbol->name);
	if ((s = Hash_Find (symtab->tab, symbol->name)))
		return s;
	Hash_Add (symtab->tab, symbol);

	symbol->next = *symtab->symtail;
	*symtab->symtail = symbol;
	symtab->symtail = &symbol->next;

	symbol->table = symtab;

	return symbol;
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
	sym->type = symbol->type;
	sym->params = copy_params (symbol->params);
	sym->sy_type = symbol->sy_type;
	sym->s = symbol->s;
	return sym;
}

symtab_t *
symtab_flat_copy (symtab_t *symtab, symtab_t *parent)
{
	symtab_t   *newtab;
	symbol_t   *newsym;
	symbol_t   *symbol;

	newtab = new_symtab (parent, stab_local);
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
make_symbol (const char *name, type_t *type, defspace_t *space,
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
			&& !sym->type->t.array.size) {
			sym->type = type;
		} else {
			error (0, "%s redefined", name);
			sym = new_symbol_type (name, type);
		}
	}
	if (sym->s.def && sym->s.def->external && storage != sc_extern) {
		//FIXME this really is not the right way
		relocs = sym->s.def->relocs;
		free_def (sym->s.def);
		sym->s.def = 0;
	}
	if (!sym->s.def) {
		sym->s.def = new_def (name, type, space, storage);
		reloc_attach_relocs (relocs, &sym->s.def->relocs);
	}
	sym->sy_type = sy_var;
	return sym;
}
