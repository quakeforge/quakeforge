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

#ifndef __function_h
#define __function_h

typedef struct param_s {
	struct param_s *next;
	const char *selector;
	type_t     *type;
	const char *name;
} param_t;

param_t *new_param (const char *selector, type_t *type, const char *name);

param_t *reverse_params (param_t *params);

type_t *parse_params (type_t *type, param_t *params);
void build_scope (function_t *f, def_t *func, param_t *params);
function_t *new_function (void);
void build_function (function_t *f);
void finish_function (function_t *f);
void emit_function (function_t *f, expr_t *e);

#endif//__function_h
