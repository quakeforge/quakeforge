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

#include "QF/pr_obj.h"

struct progs_s;
struct cbuf_s;

void RUA_Init (struct progs_s *pr, int secure);
void RUA_Cbuf_SetCbuf (struct progs_s *pr, struct cbuf_s *cbuf);
func_t RUA_Obj_msg_lookup (struct progs_s *pr, pointer_t _self,
						   pointer_t __cmd);

void RUA_Game_Init (struct progs_s *pr, int secure);

// self is expected in param 0
int RUA_obj_increment_retaincount (struct progs_s *pr);
// self is expected in param 0
int RUA_obj_decrement_retaincount (struct progs_s *pr);

#endif//__QF_ruamoko_h
