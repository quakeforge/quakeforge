/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2003 #AUTHOR#

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

#ifndef __gib_h
#define __gib_h

// Dependencies

#include "QF/dstring.h"
#include "QF/cbuf.h"

// Buffer access (required to use GIB_Arg* macros)

#define GIB_DATA(buffer) ((gib_buffer_data_t *)(buffer->data))

typedef struct gib_script_s {
	const char *text, *file;
	unsigned int refs;
} gib_script_t;

typedef struct gib_buffer_data_s {
	struct gib_script_s *script;
	struct gib_tree_s *program, *ip;
	struct dstring_s *arg_composite;
	qboolean waitret;
	struct gib_sstack_s {
		struct gib_dsarray_s {
			struct dstring_s **dstrs;
			unsigned int realsize, size;
		} *values;
		unsigned int size, p;
	} stack;
	struct hashtab_s *locals; // Local variables
	struct hashtab_s *globals; // Current domain
} gib_buffer_data_t;

// Builtin function interface

extern char gib_null_string[];

#define GIB_Argc() (cbuf_active->args->argc)
#define GIB_Argv(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->argv[(x)]->str : gib_null_string)
#define GIB_Args(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->args[(x)] : gib_null_string)
#define GIB_Argd(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->argv[(x)] : NULL)
#define GIB_Argm(x) ((x) < cbuf_active->args->argc ? (gib_tree_t *)cbuf_active->args->argm[(x)] : NULL)

#define GIB_USAGE(x) (GIB_Error ("syntax", "%s: invalid syntax\nusage: %s %s", GIB_Argv(0), GIB_Argv(0), (x)))

#define GIB_CanReturn() (GIB_DATA(cbuf_active)->waitret)

dstring_t *GIB_Return (const char *str);
void GIB_Error (const char *type, const char *fmt, ...);
void GIB_Builtin_Add (const char *name, void (*func) (void));
void GIB_Builtin_Remove (const char *name);
qboolean GIB_Builtin_Exists (const char *name);

// Event interface

typedef struct gib_event_s {
	const char *name;
	struct gib_function_s *func;
} gib_event_t;

gib_event_t *GIB_Event_New (const char *name);
void GIB_Event_Callback (gib_event_t *event, unsigned int argc, ...);

// Interpreter interface (for creating GIB cbufs)

cbuf_interpreter_t *GIB_Interpreter (void);

// Thread interface

void GIB_Thread_Execute (void);

// Init interface

void GIB_Init (qboolean sandbox);

// Handle interface

unsigned long int GIB_Handle_New (void *data, unsigned short int class);
void GIB_Handle_Free (unsigned long int num, unsigned short int class);
void *GIB_Handle_Get (unsigned long int num, unsigned short int class);
unsigned short int GIB_Handle_Class_New (void);

#endif
