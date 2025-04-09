/*
	target.h

	Shared data stuctures for backend targets.

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

#ifndef __target_h
#define __target_h

typedef struct attribute_s attribute_t;
typedef struct expr_s expr_t;
typedef struct rua_ctx_s rua_ctx_t;
typedef struct specifier_s specifier_t;
typedef struct symbol_s symbol_t;
typedef struct symtab_s symtab_t;
typedef struct type_s type_t;
typedef struct function_s function_t;
typedef struct metafunc_s metafunc_t;

typedef struct {
	void      (*init) (void);
	bool      (*value_too_large) (const type_t *val_type);
	void      (*build_scope) (symbol_t *fsym);
	void      (*build_code) (function_t *func, const expr_t *statements);
	void      (*field_attributes) (attribute_t **attributes, symbol_t *sym);
	void      (*var_attributes) (specifier_t *spec, attribute_t **attributes);
	void      (*declare_sym) (specifier_t spec, const expr_t *init,
							  symtab_t *symtab, expr_t *block);
	void      (*vararg_int) (const expr_t *e);

	bool      (*create_entry_point) (const char *name, const char *model_name);

	const expr_t *(*initialized_temp) (const type_t *type, const expr_t *src);
	const expr_t *(*assign_vector) (const expr_t *dst, const expr_t *src);
	const expr_t *(*proc_switch) (const expr_t *expr, rua_ctx_t *ctx);
	const expr_t *(*proc_caselabel) (const expr_t *expr, rua_ctx_t *ctx);
	const expr_t *(*proc_address) (const expr_t *expr, rua_ctx_t *ctx);
	// for both vector and quaternion types
	const expr_t *(*vector_compare)(int op, const expr_t *e1, const expr_t *e2);
	const expr_t *(*shift_op)(int op, const expr_t *e1, const expr_t *e2);
	const expr_t *(*test_expr) (const expr_t *expr);
	const expr_t *(*cast_expr) (const type_t *dstType, const expr_t *expr);
	const expr_t *(*check_types_compatible) (const expr_t *dst,
											 const expr_t *src);
	bool      (*type_assignable) (const type_t *dst, const type_t *src);
	bool      (*init_type_ok) (const type_t *dst, const type_t *src);

	bool      (*setup_intrinsic_symtab) (symtab_t *symtab);

	bool      (*function_attr) (const attribute_t *attr, metafunc_t *func);

	unsigned    label_id;
} target_t;

extern target_t current_target;
extern target_t v6_target;
extern target_t v6p_target;
extern target_t ruamoko_target;
extern target_t spirv_target;

bool target_set_backend (const char *tgt);

const expr_t *ruamoko_proc_switch (const expr_t *expr, rua_ctx_t *ctx);
const expr_t *ruamoko_proc_caselabel (const expr_t *expr, rua_ctx_t *ctx);
const expr_t *ruamoko_field_array (const expr_t *e);
const expr_t *ruamoko_proc_address (const expr_t *expr, rua_ctx_t *ctx);
const expr_t *ruamoko_test_expr (const expr_t *expr);

#endif//__target_h
