/*
	qc-parse.y

	parser for quakec

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/10/26

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

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

const char *storage_class_names[sc_count] = {
	"global",
	"system",
	"extern",
	"static",
	"param",
	"local",
	"argument",
	"inout",
	"in",
	"out",
};

language_t current_language;
function_t *current_func;
class_type_t *current_class;
expr_t     *local_expr;
vis_t       current_visibility;
storage_class_t current_storage = sc_global;
symtab_t   *current_symtab;
bool        no_flush_dag;

/*	When defining a new symbol, already existing symbols must be in a
	different scope. However, when they are in a different scope, we want a
	truly new symbol.
*/
symbol_t *
check_redefined (symbol_t *sym)
{
	if (sym->table == current_symtab)
		error (0, "%s redefined", sym->name);
	if (sym->table)		// truly new symbols are not in any symbol table
		sym = new_symbol (sym->name);
	return sym;
}

/*	Check for undefined symbols. If the symbol is undefined, default its type
	to float or int, depending on compiler mode.
*/
symbol_t *
check_undefined (symbol_t *sym)
{
	if (!sym->table) {	// truly new symbols are not in any symbol table
		error (0, "%s undefined", sym->name);
		if (options.code.progsversion == PROG_ID_VERSION)
			sym->type = &type_float;
		else
			sym->type = &type_int;
	}
	return sym;
}
