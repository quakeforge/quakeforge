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
#include "QF/gib_buffer.h" // For GIB_DATA()
#include "QF/dstring.h" // For ->str

typedef struct gib_builtin_s {
	const char *name;
	void (*func) (void);
} gib_builtin_t;

extern char gib_null_string[];

#define GIB_Argc() (cbuf_active->args->argc)
#define GIB_Argv(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->argv[(x)]->str : gib_null_string)
#define GIB_Args(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->args[(x)] : gib_null_string)
#define GIB_Argd(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->argv[(x)] : NULL)
#define GIB_Argm(x) ((x) < cbuf_active->args->argc ? (gib_tree_t *)cbuf_active->args->argm[(x)] : NULL)

#define GIB_USAGE(x) (GIB_Error ("syntax", "%s: invalid syntax\nusage: %s %s", GIB_Argv(0), GIB_Argv(0), (x)))

#define GIB_CanReturn() (GIB_DATA(cbuf_active)->waitret)

void GIB_Arg_Strip_Delim (unsigned int arg);
dstring_t *GIB_Return (const char *str);
void GIB_Error (const char *type, const char *fmt, ...);
gib_builtin_t *GIB_Builtin_Add (const char *name, void (*func) (void));
void GIB_Builtin_Remove (const char *name);
qboolean GIB_Builtin_Exists (const char *name);
gib_builtin_t *GIB_Builtin_Find (const char *name);
void GIB_Builtin_Init (qboolean sandbox);
