/*
	pr_obj.h

	progs obj support

	Copyright (C) 2002       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/5/10

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

#ifndef __pr_obj_h
#define __pr_obj_h

#include "QF/pr_comp.h"

typedef struct pr_sel_s {
	pointer_t   sel_id;
	string_t    sel_types;
} pr_sel_t;

typedef struct pr_id_s {
	pointer_t   class_pointer;		// pr_class_t
} pr_id_t;

typedef struct pr_class_s {
	pointer_t   class_pointer;		// pr_class_t
	pointer_t   super_class;		// pr_class_t
	string_t    name;
	int         version;
	int         info;
	int         instance_size;
	pointer_t   ivars;				// pr_ivar_list_t
	pointer_t   methods;			// pr_method_list_t
	pointer_t   dtable;
	pointer_t   subclass_list;		// pr_class_t
	pointer_t   sibling_class;		// pr_class_t
	pointer_t   protocols;			// pr_protocol_list_t
	pointer_t   gc_object_type;
} pr_class_t;

typedef struct pr_protocol_s {
	pointer_t   class_pointer;		// pr_class_t
	string_t    protocol_name;
	pointer_t   protocol_list;		// pr_protocol_list_t
	pointer_t   instance_methods;	// pr_method_list_t
	pointer_t   class_methods;		// pr_method_list_t
} pr_protocol_t;

typedef struct pr_category_s {
	string_t    category_name;
	string_t    class_name;
	pointer_t   instance_methods;	// pr_method_list_t
	pointer_t   class_methods;		// pr_method_list_t
	pointer_t   protocols;			// pr_protocol_list_t
} pr_category_t;

typedef pr_protocol_list_s {
	pointer_t   next;
	int         count;
	pointer_t   list[1];
} pr_protocol_list_t;

typedef pr_method_list_s {
	pointer_t   method_next;
	int         method_count;
	struct pr_method_s {
		SEL         method_name;
		string_t    method_types;
		func_t      method_imp;		// typedef id (id, SEL, ...) IMP
	} method_list[1];
} pr_method_list_t;
typedef pr_method_s pr_method_t;

typedef pr_ivar_list_s {
	int         ivar_count;
	struct pr_ivar_s {
		string_t    ivar_name;
		string_t    ivar_type;
		int         ivar_offset;
	} ivar_list[1];
} pr_ivar_list_t;
typedef pr_ivar_s pr_ivar_t;

#endif//__pr_obj_h
