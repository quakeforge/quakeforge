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

#include "QF/cbuf.h"
#include "QF/gib_tree.h"
#include "QF/dstring.h"

#define GIB_DATA(buffer) ((gib_buffer_data_t *)(buffer->data))


typedef struct gib_buffer_data_s {
	struct gib_tree_s *program, *ip;
	struct dstring_s *arg_composite;
	qboolean done, waitret;
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

void GIB_Buffer_Construct (struct cbuf_s *cbuf);
void GIB_Buffer_Destruct (struct cbuf_s *cbuf);
void GIB_Buffer_Add (cbuf_t *cbuf, const char *str);
void GIB_Buffer_Insert (cbuf_t *cbuf, const char *str);
void GIB_Buffer_Push_Sstack (struct cbuf_s *cbuf);
void GIB_Buffer_Pop_Sstack (struct cbuf_s *cbuf);
dstring_t *GIB_Buffer_Dsarray_Get (struct cbuf_s *cbuf);

extern struct cbuf_interpreter_s gib_interp;
