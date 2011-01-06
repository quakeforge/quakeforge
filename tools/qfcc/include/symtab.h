/*
	symtab.h

	Symbol table management

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/04

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

	$Id$
*/

#ifndef __symtab_h
#define __symtab_h

/**	\defgroup qfcc_symtab Symbol Table Management
	\ingroup qfcc
*/
//@{
//
typedef enum {
	sym_type,
	sym_def,
	sym_enum,
	sym_class,
} sym_type_e;

typedef struct symbol_s {
	struct symbol_s *next;		///< chain of symbols in symbol table
	const char *name;			///< symbol name
	sym_type_e  type;			///< symbol type (typedef, var, struct...)
	union {
		struct type_s *type;
		struct def_s *def;
		struct enumval_s *enm;
		struct class_s *class;
	} s;
} symbol_t;

typedef enum {
	stab_global,				///< global (many symbols)
	stab_local,					///< local (few symbols: func, struct)
	stab_union,					///< offset always 0
} stab_type_e;

typedef struct symtab_s {
	struct symtab_s *parent;	///< points to parent table
	struct symtab_s *next;		///< next in global collection of symtabs
	stab_type_e type;			///< type of symbol table
	struct hashtab_s *tab;		///< symbols defined in this table
	symbol_t   *symbols;		///< chain of symbols in this table
	symbol_t  **symtail;		///< keep chain in declaration order
} symtab_t;

/**	Create a new, empty symbol table.

	The symbol tables support scoping via their \c parent pointer. This
	supports both code block scoping and ivar inheritance.

	\param parent	Pointer to parent scope symbol table.
	\param type		The type of symbol table. Currently governs expected size.
	\return			The new, empty symbol table.
*/
symtab_t *new_symtab (symtab_t *parent, stab_type_e type);

/**	Lookup a name in the symbol table.

	The entire symbol table chain (symtab_t::parent) starting at symtab
	will be checked for \a name.

	\param symtab	The symbol table to search for \a name. If \a name is
					not in the symbol table, the tables's parent, if it
					exists, will be checked, and then its parent, until the
					end of the chain.
	\param name		The name to look up in the symbol table chain.
*/
symbol_t *symtab_lookup (symtab_t *symtab, const char *name);

/**	Add a named type (\c typedef) to the symbol table.

	Duplicate names will not be inserted (an error), but only the given
	symbol table is checked for duplicate names, not the ancestor chain.

	\param symtab	The symbol table to which the named type will be added.
	\param name		The name of the type to be added to the symbol table.
	\param type		The type to be added to the symbol table.
	\return			The new symbol representing the named type or null if
					an error occurred.
*/
symbol_t *symtab_add_type (symtab_t *symtab, const char *name,
						   struct type_s *type);

/**	Add a def to the symbol table.

	Duplicate names will not be inserted (an error), but only the given
	symbol table is checked for duplicate names, not the ancestor chain.

	\param symtab	The symbol table to which the def will be added.
	\param name		The name of the def to be added to the symbol table.
	\param def		The def to be added to the symbol table.
	\return			The new symbol representing the def or null if an
					error occurred.
*/
symbol_t *symtab_add_def (symtab_t *symtab, const char *name,
						  struct def_s *def);

/**	Add an enum to the symbol table.

	Duplicate names will not be inserted (an error), but only the given
	symbol table is checked for duplicate names, not the ancestor chain.

	\param symtab	The symbol table to which the enum will be added.
	\param name		The name of the enum to be added to the symbol table.
	\param enm		The enum to be added to the symbol table.
	\return			The new symbol representing the enum or null if an
					error occurred.
*/
symbol_t *symtab_add_enum (symtab_t *symtab, const char *name,
						   struct enumval_s *enm);

/**	Add a class to the symbol table.

	Duplicate names will not be inserted (an error), but only the given
	symbol table is checked for duplicate names, not the ancestor chain.

	\param symtab	The symbol table to which the class will be added.
	\param name		The name of the class to be added to the symbol table.
	\param class	The class to be added to the symbol table.
	\return			The new symbol representing the class or null if an
					error occurred.
*/
symbol_t *symtab_add_class (symtab_t *symtab, const char *name,
							struct class_s *class);

/**	Create a new single symbol table from the given symbol table chain.

	Create a new symbol table and add all of the symbols from the given
	symbol table chain to the new symbol table. However, in order to
	preserve scoping rules, duplicate names in ancestor tables will not be
	added to the new table.

	The new symbol table will be "local".

	The intended use is for creating the ivar scope for methods.

	\param symtab	The symbol table chain to be copied.
	\param parent	The parent symbol table of the new symbol table, or
					null.
	\return			The new symbol table.

	\dot
	digraph symtab_flat_copy {
		layout=dot; rankdir=LR; compound=true; nodesep=1.0;
		subgraph clusterI {
			node [shape=record];
			root [label="<p>parent|integer\ x;|integer\ y;|float\ z;"];
			base [label="<p>parent|float\ w;|float\ x;"];
			cur  [label="<p>parent|float\ y;"];
			symtab [shape=ellipse];
			cur:p -> base;
			base:p -> root;
		}
		subgraph clusterO {
			node [shape=record];
			out [label="<p>parent|float\ z;|float\ w;|float\ x;|float\ y;"];
			return [shape=ellipse];
			parent [shape=ellipse];
		}
		symtab -> cur;
		cur -> out [ltail=clusterI,lhead=clusterO];
		out:p -> parent;
		return -> out;
	}
	\enddot
*/
symtab_t *symtab_flat_copy (symtab_t *symtab, symtab_t *parent);

//@}

#endif//__symtab_h
