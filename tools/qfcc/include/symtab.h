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

*/

#ifndef __symtab_h
#define __symtab_h

#include "expr.h"

struct defspace_s;
enum storage_class_e : unsigned;

/**	\defgroup qfcc_symtab Symbol Table Management
	\ingroup qfcc
*/
///@{

typedef enum vis_e {
	vis_public,
	vis_protected,
	vis_private,
	vis_anonymous,
} vis_t;

#define SY_TYPE(type) sy_##type,
typedef enum : unsigned {
#include "tools/qfcc/include/sy_type_names.h"
	sy_num_types	///< number of symtab types
} sy_type_e;

typedef struct symconv_s {
	const struct expr_s *(*conv) (struct symbol_s *symbol, void *data);
	void       *data;
} symconv_t;

typedef struct {
	const expr_t *lvalue;
	const expr_t *rvalue;
} sy_xvalue_t;

typedef struct var_s {
	enum storage_class_e storage;
} var_t;

typedef struct symbol_s {
	struct symbol_s *next;		///< chain of symbols in symbol table
	struct symtab_s *table;		///< symbol table that owns this symbol
	vis_t       visibility;		///< symbol visiblity. defaults to public
	const char *name;			///< symbol name
	sy_type_e   sy_type;		///< symbol type
	unsigned    id;
	const struct type_s *type;	///< type of object to which symbol refers
	struct param_s *params;		///< the parameters if a function
	bool        no_auto_init:1;	///< skip for non-designated initializers
	bool        lvalue:1;
	bool        is_constexpr:1;
	struct attribute_s *attributes;
	union {
		var_t       var;			///< sy_var
		int         offset;			///< sy_offset
		struct def_s *def;			///< sy_def
		struct ex_value_s *value;	///< sy_const
		const struct expr_s *expr;	///< sy_expr/sy_type_param
		struct metafunc_s *metafunc;///< sy_func
		symconv_t   convert;		///< sy_convert
		struct rua_macro_s *macro;	///< sy_macro
		struct symtab_s *namespace;	///< sy_namespace
		ex_list_t   list;			///< sy_list
		sy_xvalue_t xvalue;			///< sy_xvalue
	};
	void       *return_addr;
} symbol_t;

typedef enum {
	stab_global,				///< global (many symbols)
	stab_param,					///< local (few symbols: func)
	stab_local,					///< local (few symbols: func)
	stab_ivars,
	stab_struct,
	stab_union,
	stab_enum,
	stab_label,
	stab_block,
	stab_bypass,				///< symbols are added to parent
} stab_type_e;

typedef struct symtab_s {
	struct symtab_s *parent;	///< points to parent table
	struct symtab_s *next;		///< next in global collection of symtabs
	stab_type_e type;			///< type of symbol table
	int         storage;		///< storage class for stab_block
	int         size;			///< size of structure represented by symtab
	int         count;			///< number of real members in structure
	struct hashtab_s *tab;		///< symbols defined in this table
	symbol_t   *symbols;		///< chain of symbols in this table
	symbol_t  **symtail;		///< keep chain in declaration order
	struct defspace_s *space;	///< storage for vars in scope symtabs
	struct class_s *class;		///< owning class if ivar scope
	symbol_t *(*procsymbol) (const char *name, struct symtab_s *symtab);
	void       *procsymbol_data;
	void       *data;
	const char *name;
} symtab_t;

const char *symtype_str (sy_type_e type) __attribute__((const));

/**	Create a new, empty named symbol.

	Only the symbol name field will be filled in. \a name will be copied
	using save_string().

	\param name		The name of the symbol.
	\return			The new symbol.
*/
symbol_t *new_symbol (const char *name);

/**	Create a new, typed, named symbol.

	Only the symbol name and type fields will be filled in. \a name will
	be copied using save_string().

	\param name		The name of the symbol.
	\param type		The type of the symbol.
	\return			The new symbol.
*/
symbol_t *new_symbol_type (const char *name, const struct type_s *type);

/**	Create a new, empty symbol table.

	The symbol tables support scoping via their \c parent pointer. This
	supports both code block scoping and ivar inheritance.

	\param parent	Pointer to parent scope symbol table.
	\param type		The type of symbol table.
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

/**	Add a symbol to the symbol table.

	If there is already a symbol with the same name in the symbol table,
	the symbol will not be added to the table, and the symbol that was
	found in the table witll be returned.

	\param symtab	The symol table to which the symbol will be added.
	\param symbol	The symbol to be added to the symbol table.
	\return			The symbol as in the table, either \a symbol if no
					symbol with the same name is already in the symbol
					table, or the symbol that was found in the table.
*/
symbol_t *symtab_addsymbol (symtab_t *symtab, symbol_t *symbol);

/**	Append a symbol to the symbol table's symbols.

	The symbol is not checked for duplicate names, nor is it added to the
	hash table.

	\param symtab	The symol table to which the symbol will be added.
	\param symbol	The symbol to be added to the symbol table.
	\return			The symbol as in the table
*/
symbol_t *symtab_appendsymbol (symtab_t *symtab, symbol_t *symbol);

/**	Remove a symbol from the symbol table.

	\param symtab	The symol table from which the symbol will be removed.
	\param symbol	The symbol to be removed from the symbol table.
	\return			The symbol as was in the table, or NULL if not found.
*/
symbol_t *symtab_removesymbol (symtab_t *symtab, symbol_t *symbol);

/**	Make a copy of a symbol.

	The new symbol will not be associated with any table.

	\param symbol	The symbol to be copied.
	\return			The new symbol.
*/
symbol_t *copy_symbol (symbol_t *symbol);

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
	\param type		The type of symbol table.
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
symtab_t *symtab_flat_copy (symtab_t *symtab, symtab_t *parent,
							stab_type_e type);

/**	Create a global symbol and allocate space for a variable.

	If the symbol already exists, it must be of the correct type. If it
	is external, it will be converted to the given storage class and
	allocatged space from the given defspace, otherwise it will be
	returned.

	If the symbol is new, it will be allocated space from the given
	defspace with the given storage class.

	\param name		The name of the symbol.
	\param type		The type of the symbol.
	\param space	The defspace from which space will be allocated for the
					symbol. Ignored for sc_extern, must not be null for
					others.
	\param storage	The storage class for the symbol. Only sc_extern,
					sc_global, and sc_static are valid.
*/
symbol_t *make_symbol (const char *name, const struct type_s *type,
					   struct defspace_s *space, enum storage_class_e storage);

struct specifier_s;
symbol_t *declare_symbol (struct specifier_s spec, const expr_t *init,
						  symtab_t *symtab, expr_t *block, rua_ctx_t *ctx);
symbol_t *declare_field (struct specifier_s spec, symtab_t *symtab,
						 rua_ctx_t *ctx);

///@}

#endif//__symtab_h
