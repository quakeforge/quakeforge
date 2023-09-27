/*
	method.h

	QC method support code

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/05/08

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

#ifndef __method_h
#define __method_h

#include "function.h"

typedef struct method_s {
	struct method_s *next;
	int         instance;
	param_t    *selector;
	param_t    *params;
	struct type_s *type;
	struct def_s *def;
	struct function_s *func;
	char       *name;
	char       *types;
} method_t;

typedef struct selector_s {
	const char *name;
	const char *types;
	int         index;
} selector_t;

typedef struct methodlist_s {
	method_t   *head;
	method_t  **tail;
	int         count;			///< used only for emitting
	int         instance;		///< used only for emitting
} methodlist_t;

typedef struct methodset_s {
	struct hashtab_s *tab;
} methodset_t;

typedef struct keywordarg_s {
	// the first two fields match the first two fields of param_t in
	// functionl.h
	struct keywordarg_s *next;
	const char *selector;
	expr_t     *expr;
} keywordarg_t;

struct class_s;
struct expr_s;
struct dstring_s;

method_t *new_method (struct type_s *ret_type, param_t *selector,
					  param_t *opt_parms);
const char *method_name (method_t *method);
method_t *copy_method (method_t *method);
void add_method (methodlist_t *methodlist, method_t *method);
struct symbol_s *method_symbol (struct class_type_s *class_type,
								method_t *method);
void method_set_param_names (method_t *dst, method_t *src);

methodlist_t *new_methodlist (void);
methodset_t *new_methodset (void);
void methodset_add_methods (methodset_t *methodset, methodlist_t *methods);
int methodset_contains_method (methodset_t *methodset, method_t *method);
//NOTE frees the source list and any methods not copied
void merge_method_lists (methodlist_t *dst, methodlist_t *src);
void copy_methods (methodlist_t *dst, methodlist_t *src, methodset_t *except);
int method_compare (method_t *m1, method_t *m2);

keywordarg_t *new_keywordarg (const char *selector, struct expr_s *expr);
keywordarg_t *copy_keywordargs (const keywordarg_t *kwargs);

struct expr_s *send_message (int super);

method_t *find_method (const char *sel_name);
method_t *methodlist_find_method (methodlist_t *methodlist,
								  selector_t *selector, int instance)
								  __attribute__((pure));

void selector_name (struct dstring_s *sel_id, keywordarg_t *selector);
void method_types (struct dstring_s *sel_types, method_t *method);
int selector_index (const char *sel_id);
selector_t *get_selector (const struct expr_s *sel);
struct def_s *emit_selectors(void);

struct def_s *emit_methods (methodlist_t *methods, const char *name,
							int instance);
struct def_s *emit_method_descriptions (methodlist_t *_methods,
										const char *name, int instance);

void clear_selectors (void);

const struct expr_s *method_check_params (method_t *method,
										  const struct expr_s *args);

#endif//__method_h
