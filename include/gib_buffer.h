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

#ifndef __gib_buffer_h
#define __gib_buffer_h

#include "QF/cbuf.h"
#include "gib_tree.h"
#include "QF/dstring.h"

void GIB_Buffer_Set_Program (cbuf_t *cbuf, gib_tree_t *program);
void GIB_Buffer_Push_Sstack (struct cbuf_s *cbuf);
void GIB_Buffer_Pop_Sstack (struct cbuf_s *cbuf);
dstring_t *GIB_Buffer_Dsarray_Get (struct cbuf_s *cbuf);
void GIB_Buffer_Reply_Callback (int argc, const char **argv, void *data);
void GIB_Buffer_Error (cbuf_t *cbuf, const char *type, const char *fmt, va_list args) __attribute__((format(PRINTF, 3, 0)));

extern struct cbuf_interpreter_s gib_interp;

#endif // __gib_buffer_h
