/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

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

#ifndef __emit_h
#define __emit_h

typedef struct codespace_s {
	struct statement_s *code;
	int         size;
	int         max_size;
} codespace_t;

codespace_t *codespace_new (void);
void codespace_delete (codespace_t *codespace);
void codespace_addcode (codespace_t *codespace, struct statement_s *code,
						int size);
struct statement_s *codespace_newstatement (codespace_t *codespace);

struct expr_s;
struct def_s *emit_statement (struct expr_s *e, opcode_t *op, struct def_s *var_a, struct def_s *var_b, struct def_s *var_c);
struct def_s *emit_sub_expr (struct expr_s*e, struct def_s *dest);
void emit_expr (struct expr_s *e);

#endif//__emit_h
