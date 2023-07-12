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

*/

#ifndef __QF_pr_obj_h
#define __QF_pr_obj_h

#include "QF/progs/pr_comp.h"

#define PR_BITS_PER_INT (sizeof (pr_int_t) * 8)

#define __PR_CLS_INFO(cls) ((cls)->info)
#define __PR_CLS_ISINFO(cls, mask) ((__PR_CLS_INFO (cls) & (mask)) == (mask))
#define __PR_CLS_SETINFO(cls, mask) (__PR_CLS_INFO (cls) |= (mask))

/*
	The structure is of type MetaClass
*/
#define _PR_CLS_META 0x2
#define PR_CLS_ISMETA(cls) ((cls) && __PR_CLS_ISINFO (cls, _PR_CLS_META))

/*
	The structure is of type Class
*/
#define _PR_CLS_CLASS 0x1
#define PR_CLS_ISCLASS(cls) ((cls) && __PR_CLS_ISINFO (cls, _PR_CLS_CLASS))

/*
	The class is initialized within the runtime.  This means that
	it has had correct super and sublinks assigned
*/
#define _PR_CLS_RESOLV 0x8
#define PR_CLS_ISRESOLV(cls) __PR_CLS_ISINFO (cls, _PR_CLS_RESOLV)
#define PR_CLS_SETRESOLV(cls) __PR_CLS_SETINFO (cls, _PR_CLS_RESOLV)

/*
	The class has been sent a +initialize message or such is not
	defined for this class
*/
#define _PR_CLS_INITIALIZED 0x8
#define PR_CLS_ISINITIALIZED(cls) __PR_CLS_ISINFO (cls, _PR_CLS_INITIALIZED)
#define PR_CLS_SETINITIALIZED(cls) __PR_CLS_SETINFO (cls, _PR_CLS_INITIALIZED)

/*
	The class number of this class.  This must be the same for both the
	class and its meta class boject
*/
#define PR_CLS_GETNUMBER(cls) (__CLS_INFO (cls) >> (PR_BITS_PER_INT / 2))
#define PR_CLS_SETNUMBER(cls, num) \
  (__PR_CLS_INFO (cls) = __PR_CLS_INFO (cls) & (~0U >> (PR_BITS_PER_INT / 2)) \
						| (num) << (PR_BITS_PER_INT / 2))

typedef struct pr_sel_s {
	pr_ptr_t    sel_id;
	pr_string_t sel_types;
} pr_sel_t;

typedef struct pr_id_s {
	pr_ptr_t    class_pointer;		// pr_class_t
} pr_id_t;

typedef struct pr_class_s {
	pr_ptr_t    class_pointer;		// pr_class_t
	pr_ptr_t    super_class;		// pr_class_t
	pr_string_t name;
	pr_int_t    version;
	pr_uint_t   info;
	pr_int_t    instance_size;
	pr_ptr_t    ivars;				// pr_ivar_list_t
	pr_ptr_t    methods;			// pr_method_list_t
	pr_ptr_t    dtable;			// resource index
	pr_ptr_t    subclass_list;		// pr_class_t
	pr_ptr_t    sibling_class;		// pr_class_t
	pr_ptr_t    protocols;			// pr_protocol_list_t
	pr_ptr_t    gc_object_type;
} pr_class_t;

typedef struct pr_protocol_s {
	pr_ptr_t    class_pointer;		// pr_class_t
	pr_string_t protocol_name;
	pr_ptr_t    protocol_list;		// pr_protocol_list_t
	pr_ptr_t    instance_methods;	// pr_method_description_list_t
	pr_ptr_t    class_methods;		// pr_method_description_list_t
} pr_protocol_t;

typedef struct pr_category_s {
	pr_string_t category_name;
	pr_string_t class_name;
	pr_ptr_t    instance_methods;	// pr_method_list_t
	pr_ptr_t    class_methods;		// pr_method_list_t
	pr_ptr_t    protocols;			// pr_protocol_list_t
} pr_category_t;

typedef struct pr_protocol_list_s {
	pr_ptr_t    next;
	pr_int_t    count;
	pr_ptr_t    list[1];			// pr_protocol_t
} pr_protocol_list_t;

typedef struct pr_method_list_s {
	pr_ptr_t    method_next;
	pr_int_t    method_count;
	struct pr_method_s {
		pr_ptr_t    method_name;	// pr_sel_t
		pr_string_t method_types;
		pr_func_t   method_imp;		// typedef id (id, SEL, ...) IMP
	} method_list[1];
} pr_method_list_t;
typedef struct pr_method_s pr_method_t;

typedef struct pr_method_description_list_s {
	pr_int_t    count;
	struct pr_method_description_s {
		pr_ptr_t    name;			// pr_sel_t
		pr_string_t types;
	} list[1];
} pr_method_description_list_t;
typedef struct pr_method_description_s pr_method_description_t;

typedef struct pr_ivar_list_s {
	pr_int_t    ivar_count;
	struct pr_ivar_s {
		pr_string_t ivar_name;
		pr_string_t ivar_type;
		pr_int_t    ivar_offset;
	} ivar_list[1];
} pr_ivar_list_t;
typedef struct pr_ivar_s pr_ivar_t;

typedef struct pr_static_instances_s {
	// one per staticly instanced class per module (eg, 3 instances of Object
	// will produce one of these structs with 3 pointers to those instances in
	// instances[]
	pr_string_t class_name;
	pr_ptr_t    instances[1];		// null terminated array of pr_id_t
} pr_static_instances_t;

typedef struct pr_symtab_s {
	pr_int_t    sel_ref_cnt;
	pr_ptr_t    refs;				// pr_sel_t
	pr_int_t    cls_def_cnt;
	pr_int_t    cat_def_cnt;
	pr_ptr_t    defs[1];			// variable array of cls_def_cnt class
									// pointers then cat_def_cnt category
									// pointers followed by a null terminated
									// array of pr_static_instances (not yet
									// implemented in qfcc)
} pr_symtab_t;

typedef struct pr_module_s {
	pr_int_t    version;
	pr_int_t    size;
	pr_string_t name;
	pr_ptr_t    symtab;			// pr_symtab_t
} pr_module_t;

typedef struct pr_super_s {
	pr_ptr_t    self;
	pr_ptr_t    class;
} pr_super_t;

#endif//__QF_pr_obj_h
