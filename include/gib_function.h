/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#ifndef __gib_function_h
#define __gib_function_h

#include "QF/dstring.h"
#include "QF/hash.h"

#include "gib_tree.h"

typedef struct gib_function_s {
	const char *name;
	struct gib_script_s *script;
	struct dstring_s *text;
	struct gib_tree_s *program;
	struct hashtab_s *globals;
	struct llist_s *arglist;
	unsigned int minargs;
	bool exported;
} gib_function_t;

gib_function_t *GIB_Function_Define (const char *name, const char *text, gib_tree_t *program, gib_script_t *script, hashtab_t *globals);
gib_function_t *GIB_Function_Find (const char *name);
int GIB_Function_Execute (cbuf_t *cbuf, gib_function_t *func, const char **args, unsigned int argc);
int GIB_Function_Execute_D (cbuf_t *cbuf, gib_function_t *func, dstring_t **args, unsigned int argc);

#endif
