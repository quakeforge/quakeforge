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

#ifndef __method_h
#define __method_h

#include "function.h"

typedef struct method_s {
	struct method_s *next;
	int         instance;
	param_t    *selector;
	param_t    *params;
	type_t     *type;
	def_t      *def;
} method_t;

typedef struct {
	method_t   *head;
	method_t  **tail;
} methodlist_t;

typedef struct keywordarg_s {
	// the first two fields match the first two fiels of param_t in
	// functionl.h
	struct keywordarg_s *next;
	const char *selector;
	expr_t     *expr;
} keywordarg_t;

struct class_s;
struct category_s;
struct expr_s;
struct dstring_s;

method_t *new_method (type_t *ret_type, param_t *selector, param_t *opt_parms);
void add_method (methodlist_t *methodlist, method_t *method);
def_t *method_def (struct class_s *klass, struct category_s *category,
				   method_t *method);

keywordarg_t *new_keywordarg (const char *selector, struct expr_s *expr);

struct expr_s *send_message (void);

void selector_name (struct dstring_s *sel_id, keywordarg_t *selector);
void selector_types (struct dstring_s *sel_types, keywordarg_t *selector);
struct def_s *selector_def (const char *sel_id, const char *sel_types);

#endif//__method_h
