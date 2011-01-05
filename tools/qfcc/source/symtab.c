#include <stdlib.h>
#include <string.h>

#include "QF/hash.h"

#include "class.h"
#include "def.h"
#include "qfcc.h"
#include "symtab.h"
#include "type.h"

static symtab_t *free_symtabs;
static symbol_t *free_symbols;

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

static symbol_t *
new_symbol (void)
{
	symbol_t   *symbol;
	ALLOC (256, symbol_t, symbols, symbol);
	return symbol;
}

static void
add_symbol (symtab_t *symtab, symbol_t *symbol)
{
	Hash_Add (symtab->tab, symbol);
	symbol->next = *symtab->symtail;
	*symtab->symtail = symbol;
	symtab->symtail = &symbol->next;
}

symbol_t *
symtab_add_type (symtab_t *symtab, const char *name, type_t *type)
{
	symbol_t   *symbol;
	if ((symbol = Hash_Find (symtab->tab, name))) {
		// duplicate symbol
	}
	symbol = new_symbol ();
	symbol->name = name;
	symbol->type = sym_type;
	symbol->s.type = type;
	add_symbol (symtab, symbol);
	return symbol;
}

symbol_t *
symtab_add_def (symtab_t *symtab, const char *name, def_t *def)
{
	symbol_t   *symbol;
	if ((symbol = Hash_Find (symtab->tab, name))) {
		// duplicate symbol
	}
	symbol = new_symbol ();
	symbol->name = name;
	symbol->type = sym_def;
	symbol->s.def = def;
	add_symbol (symtab, symbol);
	return symbol;
}

symbol_t *
symtab_add_enum (symtab_t *symtab, const char *name, struct enumval_s *enm)
{
	symbol_t   *symbol;
	if ((symbol = Hash_Find (symtab->tab, name))) {
		// duplicate symbol
	}
	symbol = new_symbol ();
	symbol->name = name;
	symbol->type = sym_enum;
	symbol->s.enm = enm;
	add_symbol (symtab, symbol);
	return symbol;
}

symbol_t *
symtab_add_class (symtab_t *symtab, const char *name, class_t *class)
{
	symbol_t   *symbol;
	if ((symbol = Hash_Find (symtab->tab, name))) {
		// duplicate symbol
	}
	symbol = new_symbol ();
	symbol->name = name;
	symbol->type = sym_class;
	symbol->s.class = class;
	add_symbol (symtab, symbol);
	return symbol;
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
			newsym = new_symbol ();
			*newsym = *symbol;
			add_symbol (newtab, newsym);
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
