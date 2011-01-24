/*
	emit.h

	statement emittion

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/07/08

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

#ifndef __emit_h
#define __emit_h

struct expr_s;
struct def_s *emit_statement (struct expr_s *e, opcode_t *op, struct def_s *var_a, struct def_s *var_b, struct def_s *var_c);
struct def_s *emit_sub_expr (struct expr_s*e, struct def_s *dest);
void emit_expr (struct expr_s *e);

#define EMIT_STRING(s,dest,str)						\
	do {											\
		(dest) = ReuseString (str);					\
		reloc_def_string (POINTER_OFS (s, &(dest)));\
	} while (0)

#define EMIT_DEF(s,dest,def)							\
	do {												\
		def_t      *d = (def);							\
		(dest) = d ? d->offset : 0;						\
		if (d)											\
			reloc_def_def (d, POINTER_OFS (s, &(dest)));\
	} while (0)

#define EMIT_DEF_OFS(s,dest,def)							\
	do {													\
		def_t      *d = (def);								\
		(dest) = d ? d->offset : 0;							\
		if (d)												\
			reloc_def_def_ofs (d, POINTER_OFS (s, &(dest)));\
	} while (0)

#endif//__emit_h
