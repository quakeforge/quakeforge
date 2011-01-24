#include <stdlib.h>
#include <string.h>

#include "QF/hash.h"

#include "class.h"
#include "def.h"
#include "defspace.h"
#include "function.h"
#include "qfcc.h"
#include "strpool.h"
#include "symtab.h"
#include "type.h"

static symtab_t *free_symtabs;
static symbol_t *free_symbols;

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
sym_getkey (void *k, void *unused)
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
	while (symtab) {
		if ((symbol = Hash_Find (symtab->tab, name)))
			return symbol;
		symtab = symtab->parent;
	}
	return 0;
}

symbol_t *
symtab_addsymbol (symtab_t *symtab, symbol_t *symbol)
{
	symbol_t   *s;
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
		abort ();
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
	while (symtab) {
		for (symbol = symtab->symbols; symbol; symbol = symbol->next) {
			if (Hash_Find (newtab->tab, symbol->name))
				continue;
			newsym = copy_symbol (symbol);
			symtab_addsymbol (newtab, newsym);
		}
		symtab = symtab->parent;
		// Set the tail pointer so symbols in ancestor tables come before
		// those in decendent tables.
		newtab->symtail = &newtab->symbols;
	}
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

	sym = symtab_lookup (pr.symtab, name);
	if (!sym) {
		sym = new_symbol_type (name, type);
	}
	if (sym->type != type) {
		error (0, "%s redefined", name);
		sym = new_symbol_type (name, type);
	}
	if (sym->s.def && sym->s.def->external && !storage != st_extern) {
		free_def (sym->s.def);
		sym->s.def = 0;
	}
	if (!sym->s.def)
		sym->s.def = new_def (name, type, space, storage);
	return sym;
}
