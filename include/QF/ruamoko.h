/*
	ruamoko.h

	ruamoko prototypes

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie
	Date: 2002/1/19

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

#ifndef __QF_ruamoko_h
#define __QF_ruamoko_h

#include "QF/progs/pr_obj.h"

typedef struct progs_s progs_t;
typedef struct cbuf_s cbuf_t;

void RUA_Init (progs_t *pr, int secure);
void RUA_Cbuf_SetCbuf (progs_t *pr, cbuf_t *cbuf);
pr_func_t RUA_Obj_msg_lookup (progs_t *pr, pr_ptr_t _self,
							  pr_ptr_t __cmd);

void RUA_Game_Init (progs_t *pr, int secure);

// self is expected in param 0
int RUA_obj_increment_retaincount (progs_t *pr, void *data);
// self is expected in param 0
int RUA_obj_decrement_retaincount (progs_t *pr, void *data);

#endif//__QF_ruamoko_h
