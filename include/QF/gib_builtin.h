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

	$Id$
*/

#include "QF/cbuf.h" // For cbuf_active
#include "QF/dstring.h" // For ->str

typedef struct gib_builtin_s {
	struct dstring_s *name;
	void (*func) (void);
	enum gib_builtin_type_e {
		GIB_BUILTIN_NORMAL = 0, // Normal argument processing
		GIB_BUILTIN_NOPROCESS = 1, // Don't process arguments
		GIB_BUILTIN_FIRSTONLY = 2, // Process only the first argument
	} type;
} gib_builtin_t;

#define GIB_Argc() (cbuf_active->args->argc)
#define GIB_Argv(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->argv[(x)]->str : "")
#define GIB_Args(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->args[(x)] : "")
#define GIB_Argd(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->argv[(x)] : NULL)

void GIB_Arg_Strip_Delim (unsigned int arg);
void GIB_Return (const char *str);
void GIB_Builtin_Add (const char *name, void (*func) (void), enum gib_builtin_type_e type);
gib_builtin_t *GIB_Builtin_Find (const char *name);
void GIB_Builtin_Init (void);
