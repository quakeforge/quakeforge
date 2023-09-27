/*
	switch.h

	qc switch statement support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/10/25

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

#ifndef __switch_h
#define __switch_h

typedef struct switch_block_s {
	const expr_t *test;
	struct hashtab_s	*labels;
} switch_block_t;

typedef struct case_label_s {
	const struct expr_s *label;
	const struct expr_s *value;
} case_label_t;

const struct expr_s *case_label_expr (switch_block_t *switch_block,
									  const struct expr_s *value);
switch_block_t *new_switch_block (void);
const struct expr_s *switch_expr (switch_block_t *switch_block,
								  const struct expr_s *break_label,
								  const struct expr_s *statements);

#endif//__switch_h
