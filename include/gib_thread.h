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

#ifndef __gib_thread_h
#define __gib_thread_h

#include "gib_function.h"
#include "QF/cbuf.h"

cbuf_t *GIB_Thread_New (void);
void GIB_Thread_Delete (cbuf_t *thread);
void GIB_Thread_Init (void);

int GIB_Event_Register (const char *name, gib_function_t *func);
void GIB_Event_Init (void);

#endif
