/*
	pr_opcode.c

	Ruamoko instruction set opcode tables.

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/1/4

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/progs.h"

typedef pr_type_t pr_void_t;	// so size of void is 1

#define EV_TYPE(type) (sizeof (pr_##type##_t) / sizeof (pr_int_t)),
VISIBLE const pr_ushort_t pr_type_size[ev_type_count] = {
#include "QF/progs/pr_type_names.h"
	0,			// ev_invalid      not a valid/simple type
};

#define EV_TYPE(type) #type,
VISIBLE const char * const pr_type_name[ev_type_count] = {
#include "QF/progs/pr_type_names.h"
	"invalid",
};

const opcode_t pr_opcodes[512] = {
#include "libs/gamecode/pr_opcode.cinc"
};

const opcode_t *
PR_Opcode (pr_ushort_t opcode)
{
	opcode &= OP_MASK;
	return &pr_opcodes[opcode];
}
