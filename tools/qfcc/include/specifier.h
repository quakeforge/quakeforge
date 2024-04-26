/*
	specifier.h

	type system

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/12/11

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

#ifndef __specifier_h
#define __specifier_h

typedef struct type_s type_t;
typedef struct expr_s expr_t;
typedef struct symbol_s symbol_t;
typedef struct symtab_s symtab_t;

/** Specify the storage class of a def.
*/
typedef enum storage_class_e {
	sc_global,					///< def is globally visible across units
	sc_system,					///< def may be redefined once
	sc_extern,					///< def is externally allocated
	sc_static,					///< def is private to the current unit
	sc_param,					///< def is an incoming function parameter
	sc_local,					///< def is local to the current function
	sc_argument,				///< def is a function argument
} storage_class_t;

typedef struct specifier_s {
	const type_t *type;
	const expr_t *type_expr;
	struct param_s *params;
	symbol_t   *sym;
	symtab_t   *symtab;
	storage_class_t storage;
	union {
		struct {
			unsigned    multi_type:1;
			unsigned    multi_store:1;
			unsigned    is_signed:1;
			unsigned    is_unsigned:1;
			unsigned    is_short:1;
			unsigned    is_long:1;
			unsigned    is_typedef:1;
			unsigned    is_overload:1;
			unsigned    is_function:1;//FIXME do proper void(*)() -> ev_func
			unsigned    nosave:1;
			unsigned    no_va_list:1;
			unsigned    void_return:1;
		};
		unsigned    spec_bits;
	};
} specifier_t;

#endif//__type_h
