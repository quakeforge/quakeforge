/*
	iface_block.h

	Shader interface block support

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

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

#ifndef __block_h
#define __block_h

#include "QF/darray.h"

typedef struct specifier_s specifier_t;
typedef struct attribute_s attribute_t;
typedef struct expr_s expr_t;
typedef struct ex_list_s ex_list_t;
typedef struct type_s type_t;
typedef struct symbol_s symbol_t;
typedef struct symtab_s symtab_t;
typedef struct language_s language_t;
typedef struct defspace_s defspace_t;
typedef struct rua_ctx_s rua_ctx_t;

typedef enum interface_e : unsigned {
	iface_in,
	iface_out,
	iface_uniform,
	iface_buffer,
	iface_shared,
	iface_push_constant,

	iface_num_interfaces
} interface_t;
#define iftype_from_sc(sc) ((interface_t)((sc) - sc_in))
#define sc_from_iftype(it) (((storage_class_t)(it)) + sc_in)

extern const char *interface_names[iface_num_interfaces];

typedef struct iface_block_s {
	struct iface_block_s *next;
	symbol_t   *name;
	interface_t interface;
	symtab_t   *attributes;
	symtab_t   *members;
	const expr_t *member_decls;
	defspace_t *space;
	symbol_t   *instance_name;
} iface_block_t;

void block_clear (void);
iface_block_t *create_block (symbol_t *block_sym);
void finish_block (iface_block_t *block);
void declare_block_instance (specifier_t spec, iface_block_t *block,
							 symbol_t *instance_name, rua_ctx_t *ctx);
iface_block_t *get_block (const char *name, interface_t interface);
const type_t *iface_block_type (const type_t *type, const char *pre_tag);

#endif//__block_h
