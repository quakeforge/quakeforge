/*
	pr_obj.c

	Progs Obj runtime support

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/7/21

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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/pr_obj.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "compat.h"

static void
call_function (progs_t *pr, func_t func)
{
	dfunction_t *newf;

	newf = pr->pr_functions + func;
	if (newf->first_statement < 0) {
		// negative statements are built in functions
		int         i = -newf->first_statement;

		if (i >= pr->numbuiltins || !pr->builtins[i]
			|| !pr->builtins[i]->proc)
			PR_RunError (pr, "Bad builtin call number");
		pr->builtins[i]->proc (pr);
	} else {
		PR_EnterFunction (pr, newf);
	}
}

static const char *
class_get_key (void *c, void *pr)
{
	return PR_GetString ((progs_t *)pr, ((pr_class_t *)c)->name);
}

static unsigned long
category_get_hash (void *_c, void *_pr)
{
	progs_t    *pr = (progs_t *) _pr;
	pr_category_t *cat = (pr_category_t *) _c;
	const char *category_name = PR_GetString (pr, cat->category_name);
	const char *class_name = PR_GetString (pr, cat->class_name);

	return Hash_String (category_name) ^ Hash_String (class_name);
}

static int
category_compare (void *_c1, void *_c2, void *_pr)
{
	progs_t    *pr = (progs_t *) _pr;
	pr_category_t *c1 = (pr_category_t *) _c1;
	pr_category_t *c2 = (pr_category_t *) _c2;
	const char *cat1 = PR_GetString (pr, c1->category_name);
	const char *cat2 = PR_GetString (pr, c2->category_name);
	const char *cls1 = PR_GetString (pr, c1->class_name);
	const char *cls2 = PR_GetString (pr, c2->class_name);
	return strcmp (cat1, cat2) == 0 && strcmp (cls1, cls2) == 0;
}


static void
dump_ivars (progs_t *pr, pointer_t _ivars)
{
	pr_ivar_list_t *ivars;
	int         i;

	if (!_ivars)
		return;
	ivars = &G_STRUCT (pr, pr_ivar_list_t, _ivars);
	for (i = 0; i < ivars->ivar_count; i++) {
		Sys_Printf ("        %s %s %d\n",
					PR_GetString (pr, ivars->ivar_list[i].ivar_name),
					PR_GetString (pr, ivars->ivar_list[i].ivar_type),
					ivars->ivar_list[i].ivar_offset);
	}
}

static string_t
object_get_class_name (progs_t *pr, pr_id_t *object)
{
	pr_class_t *class;

	if (object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			R_INT (pr) = class->name;
			return class->name;
		}
		if (PR_CLS_ISMETA (class)) {
			R_INT (pr) = ((pr_class_t *)object)->name;
			return ((pr_class_t *)object)->name;
		}
	}
	return PR_SetString (pr, "Nil");
}

//====================================================================

static void
finish_class (progs_t *pr, pr_class_t *class, pointer_t object_ptr)
{
	pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);
	pr_class_t *val;

	meta->class_pointer = object_ptr;
	if (class->super_class) {
		const char *super_class = PR_GetString (pr, class->super_class);
		const char *class_name = PR_GetString (pr, class->name);
		val = Hash_Find (pr->classes, super_class);
		if (!val)
			PR_Error (pr, "broken class %s: super class %s not found",
					  class_name, super_class);
		meta->super_class = val->class_pointer;
		class->super_class = POINTER_TO_PROG (pr, val);
	} else {
		pointer_t  *ml = &meta->methods;
		while (*ml)
			ml = &G_STRUCT (pr, pr_method_list_t, *ml).method_next;
		*ml = class->methods;
	}
	Sys_DPrintf ("    %d %d %d\n", meta->class_pointer, meta->super_class,
				 class->super_class);
}

static void
finish_category (progs_t *pr, pr_category_t *category)
{
	const char *class_name = PR_GetString (pr, category->class_name);
	const char *category_name = PR_GetString (pr, category->category_name);
	pr_class_t *class = Hash_Find (pr->classes, class_name);
	pr_method_list_t *method_list;

	if (!class)
		PR_Error (pr, "broken category %s (%s): class %s not found",
				  class_name, category_name, class_name);
	if (category->instance_methods) {
		method_list = &G_STRUCT (pr, pr_method_list_t,
								 category->instance_methods);
		method_list->method_next = class->methods;
		class->methods = category->instance_methods;
	}
	if (category->class_methods) {
		pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);
		method_list = &G_STRUCT (pr, pr_method_list_t,
								 category->class_methods);
		method_list->method_next = meta->methods;
		meta->methods = category->class_methods;
	}
}

static void
pr___obj_exec_class (progs_t *pr)
{
	pr_module_t *module = &P_STRUCT (pr, pr_module_t, 0);
	pr_symtab_t *symtab;
	pointer_t  *ptr;
	int         i;
	//int          d = developer->int_val;

	if (!module)
		return;
	//developer->int_val = 1;
	symtab = &G_STRUCT (pr, pr_symtab_t, module->symtab);
	if (!symtab)
		return;
	Sys_DPrintf ("Initializing %s module with symtab @ %d : %d class%s and %d "
				 "categor%s\n",
				 PR_GetString (pr, module->name), module->symtab,
				 symtab->cls_def_cnt, symtab->cls_def_cnt == 1 ? "" : "es",
				 symtab->cat_def_cnt, symtab->cat_def_cnt == 1 ? "y" : "ies");
	ptr = symtab->defs;
	for (i = 0; i < symtab->cls_def_cnt; i++) {
		pr_class_t *class = &G_STRUCT (pr, pr_class_t, *ptr);
		pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);
		Sys_DPrintf ("Class %s @ %d\n", PR_GetString (pr, class->name), *ptr);
		Sys_DPrintf ("    class pointer: %d\n", class->class_pointer);
		Sys_DPrintf ("    super class: %s\n",
					 PR_GetString (pr, class->super_class));
		Sys_DPrintf ("    instance variables: %d @ %d\n", class->instance_size,
					 class->ivars);
		if (developer->int_val)
			dump_ivars (pr, class->ivars);
		Sys_DPrintf ("    instance methods: %d\n", class->methods);
		Sys_DPrintf ("    protocols: %d\n", class->protocols);

		Sys_DPrintf ("    class methods: %d\n", meta->methods);
		Sys_DPrintf ("    instance variables: %d @ %d\n", meta->instance_size,
					 meta->ivars);
		if (developer->int_val)
			dump_ivars (pr, meta->ivars);

		Hash_Add (pr->classes, class);
		ptr++;
	}
	for (i = 0; i < symtab->cat_def_cnt; i++) {
		pr_category_t *category = &G_STRUCT (pr, pr_category_t, *ptr);
		Sys_DPrintf ("Category %s (%s) @ %d\n",
					 PR_GetString (pr, category->class_name),
					 PR_GetString (pr, category->category_name), *ptr);
		Sys_DPrintf ("    instance methods: %d\n", category->instance_methods);
		Sys_DPrintf ("    class methods: %d\n", category->class_methods);
		Sys_DPrintf ("    protocols: %d\n", category->protocols);
		Hash_AddElement (pr->categories, category);
		ptr++;
	}
}

//====================================================================

static func_t
obj_find_message (progs_t *pr, pr_class_t *class, pr_sel_t *selector)
{
	pr_class_t *c = class;
	pr_method_list_t *method_list;
	pr_method_t *method;
	int         i;

	while (c) {
		method_list = &G_STRUCT (pr, pr_method_list_t, c->methods);
		while (method_list) {
			for (i = 0, method = method_list->method_list;
				 i < method_list->method_count; i++, method++) {
				if (method->method_name.sel_id == selector->sel_id)
					return method->method_imp;
			}
			method_list = &G_STRUCT (pr, pr_method_list_t,
									 method_list->method_next);
		}
		c = c->super_class ? &G_STRUCT (pr, pr_class_t, c->super_class) : 0;
	}
	return 0;
}

static func_t
obj_msg_lookup (progs_t *pr, pr_id_t *receiver, pr_sel_t *op)
{
	pr_class_t *class;
	if (!receiver)
		return 0;
	class = &G_STRUCT (pr, pr_class_t, receiver->class_pointer);
	return obj_find_message (pr, class, op);
}

static func_t
obj_msg_lookup_super (progs_t *pr, pr_super_t *super, pr_sel_t *op)
{
	pr_class_t *class;

	if (!super->self)
		return 0;

	class = &G_STRUCT (pr, pr_class_t, super->class);
	return obj_find_message (pr, class, op);
}

static void
pr_obj_error (progs_t *pr)
{
	//pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	//int         code = P_INT (pr, 1);
	//const char *fmt = P_GSTRING (pr, 2);
	//...
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_obj_verror (progs_t *pr)
{
	//pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	//int         code = P_INT (pr, 1);
	//const char *fmt = P_GSTRING (pr, 2);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_obj_set_error_handler (progs_t *pr)
{
	//func_t      func = P_INT (pr, 0);
	//arglist
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_obj_msg_lookup (progs_t *pr)
{
	pr_id_t    *receiver = &P_STRUCT (pr, pr_id_t, 0);
	pr_sel_t   *op = &P_STRUCT (pr, pr_sel_t, 1);
	R_INT (pr) = obj_msg_lookup (pr, receiver, op);
}

static void
pr_obj_msg_lookup_super (progs_t *pr)
{
	pr_super_t *super = &P_STRUCT (pr, pr_super_t, 0);
	pr_sel_t   *_cmd = &P_STRUCT (pr, pr_sel_t, 1);

	R_INT (pr) = obj_msg_lookup_super (pr, super, _cmd);
}

static void
pr_obj_msg_sendv (progs_t *pr)
{
	pr_id_t    *receiver = &P_STRUCT (pr, pr_id_t, 0);
	pr_sel_t   *op = &P_STRUCT (pr, pr_sel_t, 1);
	pr_va_list_t args = P_STRUCT (pr, pr_va_list_t, 2);
	func_t      imp = obj_msg_lookup (pr, receiver, op);

	if (!imp)
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, object_get_class_name (pr, receiver)),
					 PR_GetString (pr, op->sel_id));
	if (args.count > 6)
		args.count = 6;
	memcpy (P_GPOINTER (pr, 2), G_GPOINTER (pr, args.list),
			args.count * 4 * pr->pr_param_size);
	call_function (pr, imp);
}

static void
pr_obj_malloc (progs_t *pr)
{
	int         size = P_INT (pr, 0) * sizeof (pr_type_t);
	void       *mem = PR_Zone_Malloc (pr, size);

	R_INT (pr) = POINTER_TO_PROG (pr, mem);
}

static void
pr_obj_atomic_malloc (progs_t *pr)
{
	int         size = P_INT (pr, 0) * sizeof (pr_type_t);
	void       *mem = PR_Zone_Malloc (pr, size);

	R_INT (pr) = POINTER_TO_PROG (pr, mem);
}

static void
pr_obj_valloc (progs_t *pr)
{
	int         size = P_INT (pr, 0) * sizeof (pr_type_t);
	void       *mem = PR_Zone_Malloc (pr, size);

	R_INT (pr) = POINTER_TO_PROG (pr, mem);
}

static void
pr_obj_realloc (progs_t *pr)
{
	void       *mem = (void*)P_GPOINTER (pr, 0);
	int         size = P_INT (pr, 1) * sizeof (pr_type_t);

	mem = PR_Zone_Realloc (pr, mem, size);
	R_INT (pr) = POINTER_TO_PROG (pr, mem);
}

static void
pr_obj_calloc (progs_t *pr)
{
	int         size = P_INT (pr, 0) * sizeof (pr_type_t);
	void       *mem = PR_Zone_Malloc (pr, size);

	memset (mem, 0, size);
	R_INT (pr) = POINTER_TO_PROG (pr, mem);
}

static void
pr_obj_free (progs_t *pr)
{
	void       *mem = (void*)P_GPOINTER (pr, 0);

	PR_Zone_Free (pr, mem);
}

static void
pr_obj_get_uninstalled_dtable (progs_t *pr)
{
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_obj_msgSend (progs_t *pr)
{
	pr_id_t    *self = &P_STRUCT (pr, pr_id_t, 0);
	pr_sel_t   *_cmd = &P_STRUCT (pr, pr_sel_t, 1);
	func_t      imp;

	if (!self) {
		R_INT (pr) = R_INT (pr);
		return;
	}
	if (!_cmd)
		PR_RunError (pr, "null selector");
	imp = obj_msg_lookup (pr, self, _cmd);
	if (!imp)
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, object_get_class_name (pr, self)),
					 PR_GetString (pr, _cmd->sel_id));

	call_function (pr, imp);
}

static void
pr_obj_msgSend_super (progs_t *pr)
{
	pr_super_t *super = &P_STRUCT (pr, pr_super_t, 0);
	pr_sel_t   *_cmd = &P_STRUCT (pr, pr_sel_t, 1);
	func_t      imp;

	imp = obj_msg_lookup_super (pr, super, _cmd);
	if (!imp) {
		pr_id_t    *self = &G_STRUCT (pr, pr_id_t, super->self);
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, object_get_class_name (pr, self)),
					 PR_GetString (pr, _cmd->sel_id));
	}
	P_POINTER (pr, 0) = super->self;
	call_function (pr, imp);
}

static void
pr_obj_get_class (progs_t *pr)
{
	const char *name = P_GSTRING (pr, 0);
	pr_class_t *class;

	class = Hash_Find (pr->classes, name);
	if (!class)
		PR_RunError (pr, "could not find class %s", name);
	R_INT (pr) = POINTER_TO_PROG (pr, class);
}

static void
pr_obj_lookup_class (progs_t *pr)
{
	const char *name = P_GSTRING (pr, 0);
	pr_class_t *class;

	class = Hash_Find (pr->classes, name);
	R_INT (pr) = POINTER_TO_PROG (pr, class);
}

static void
pr_obj_next_class (progs_t *pr)
{
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

//====================================================================

static void
pr_sel_get_name (progs_t *pr)
{
	pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 0);
	R_INT (pr) = sel->sel_id;
}

static void
pr_sel_get_type (progs_t *pr)
{
	pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 0);
	R_INT (pr) = sel->sel_types;
}

static void
pr_sel_get_uid (progs_t *pr)
{
	//const char *name = P_GSTRING (pr, 0);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_sel_get_any_uid (progs_t *pr)
{
	//const char *name = P_GSTRING (pr, 0);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_sel_get_any_typed_uid (progs_t *pr)
{
	//const char *name = P_GSTRING (pr, 0);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_sel_get_typed_uid (progs_t *pr)
{
	//const char *name = P_GSTRING (pr, 0);
	//const char *type = P_GSTRING (pr, 1);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_sel_register_name (progs_t *pr)
{
	//const char *name = P_GSTRING (pr, 0);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_sel_register_typed_name (progs_t *pr)
{
	//const char *name = P_GSTRING (pr, 0);
	//const char *type = P_GSTRING (pr, 1);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_sel_is_mapped (progs_t *pr)
{
	//pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 0);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

//====================================================================

static void
pr_class_get_class_method (progs_t *pr)
{
	//pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	//pr_sel_t   *aSel = &P_STRUCT (pr, pr_sel_t, 1);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_class_get_instance_method (progs_t *pr)
{
	//pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	//pr_sel_t   *aSel = &P_STRUCT (pr, pr_sel_t, 1);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr_class_pose_as (progs_t *pr)
{
	//pr_class_t *imposter = &P_STRUCT (pr, pr_class_t, 0);
	//pr_class_t *superclass = &P_STRUCT (pr, pr_class_t, 1);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static inline pr_id_t    *
class_create_instance (progs_t *pr, pr_class_t *class)
{
	int         size = class->instance_size * sizeof (pr_type_t);
	pr_id_t    *id;

	id = PR_Zone_Malloc (pr, size);
	memset (id, 0, size);
	id->class_pointer = POINTER_TO_PROG (pr, class);
	return id;
}

static void
pr_class_create_instance (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	pr_id_t    *id = class_create_instance (pr, class);
	
	R_INT (pr) = POINTER_TO_PROG (pr, id);
}

static void
pr_class_get_class_name (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->name
										: PR_SetString (pr, "Nil");
}

static void
pr_class_get_instance_size (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->instance_size : 0;
}

static void
pr_class_get_meta_class (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->class_pointer : 0;
}

static void
pr_class_get_super_class (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->super_class : 0;
}

static void
pr_class_get_version (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->version : -1;
}

static void
pr_class_is_class (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class);
}

static void
pr_class_is_meta_class (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISMETA (class);
}

static void
pr_class_set_version (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	if (PR_CLS_ISCLASS (class))
		class->version = P_INT (pr, 1);
}

static void
pr_class_get_gc_object_type (progs_t *pr)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->gc_object_type : 0;
}

static void
pr_class_ivar_set_gcinvisible (progs_t *pr)
{
	//pr_class_t *imposter = &P_STRUCT (pr, pr_class_t, 0);
	//const char *ivarname = P_GSTRING (pr, 1);
	//int         gcInvisible = P_INT (pr, 2);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

//====================================================================

static void
pr_method_get_imp (progs_t *pr)
{
	pr_method_t *method = &P_STRUCT (pr, pr_method_t, 0);

	R_INT (pr) = method->method_imp;
}

static void
pr_get_imp (progs_t *pr)
{
	//pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	//pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 1);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

//====================================================================

static void
pr_object_dispose (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	PR_Zone_Free (pr, object);
}

static void
pr_object_copy (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
	pr_id_t    *id;

	id = class_create_instance (pr, class);
	memcpy (id, object, sizeof (pr_type_t) * class->instance_size);
	R_INT (pr) = POINTER_TO_PROG (pr, id);
}

static void
pr_object_get_class (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_class_t *class;

	if (object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			R_INT (pr) = POINTER_TO_PROG (pr, class);
			return;
		}
		if (PR_CLS_ISMETA (class)) {
			R_INT (pr) = P_INT (pr, 0);
			return;
		}
	}
	R_INT (pr) = 0;
}

static void
pr_object_get_super_class (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_class_t *class;

	if (object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			R_INT (pr) = class->super_class;
			return;
		}
		if (PR_CLS_ISMETA (class)) {
			R_INT (pr) = ((pr_class_t *)object)->super_class;
			return;
		}
	}
	R_INT (pr) = 0;
}

static void
pr_object_get_meta_class (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_class_t *class;

	if (object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			R_INT (pr) = class->class_pointer;
			return;
		}
		if (PR_CLS_ISMETA (class)) {
			R_INT (pr) = ((pr_class_t *)object)->class_pointer;
			return;
		}
	}
	R_INT (pr) = 0;
}

static void
pr_object_get_class_name (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_class_t *class;

	if (object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			R_INT (pr) = class->name;
			return;
		}
		if (PR_CLS_ISMETA (class)) {
			R_INT (pr) = ((pr_class_t *)object)->name;
			return;
		}
	}
	RETURN_STRING (pr, "Nil");
}

static void
pr_object_is_class (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);

	if (object) {
		R_INT (pr) = PR_CLS_ISCLASS ((pr_class_t*)object);
		return;
	}
	R_INT (pr) = 0;
}

static void
pr_object_is_instance (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_class_t *class;

	if (object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		R_INT (pr) = PR_CLS_ISCLASS (class);
		return;
	}
	R_INT (pr) = 0;
}

static void
pr_object_is_meta_class (progs_t *pr)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);

	if (object) {
		R_INT (pr) = PR_CLS_ISMETA ((pr_class_t*)object);
		return;
	}
	R_INT (pr) = 0;
}

//====================================================================

static void
pr__i_Object__hash (progs_t *pr)
{
	R_INT (pr) = P_INT (pr, 0);
}

static void
pr__i_Object__compare_ (progs_t *pr)
{
	int          ret;
	ret = P_INT (pr, 0) != P_INT (pr, 2);
	if (ret) {
		ret = P_INT (pr, 0) > P_INT (pr, 2);
		if (!ret)
			ret = -1;
	}
	R_INT (pr) = ret;
}

static void
pr__c_Object__conformsTo_ (progs_t *pr)
{
	//pr_id_t    *self = &P_STRUCT (pr, pr_id_t, 0);
	//pr_protocol_t *protocol = &P_STRUCT (pr, pr_protocol_t, 2);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr__i_Object_error_error_ (progs_t *pr)
{
	//pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	//const char *fmt = P_GSTRING (pr, 2);
	//...
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
pr__c_Object__conformsToProtocol_ (progs_t *pr)
{
	//pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	//pr_protocol_t *proto = &P_STRUCT (pr, pr_protocol_t, 2);
	//...
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

//====================================================================

static struct {
	const char *name;
	void      (*func)(progs_t *pr);
} obj_methods [] = {
	{"__obj_exec_class",		pr___obj_exec_class},

	{"obj_error",				pr_obj_error},
	{"obj_verror",				pr_obj_verror},
	{"obj_set_error_handler",	pr_obj_set_error_handler},
	{"obj_msg_lookup",			pr_obj_msg_lookup},
	{"obj_msg_lookup_super",	pr_obj_msg_lookup_super},
	{"obj_msg_sendv",			pr_obj_msg_sendv},
	{"obj_malloc",				pr_obj_malloc},
	{"obj_atomic_malloc",		pr_obj_atomic_malloc},
	{"obj_valloc",				pr_obj_valloc},
	{"obj_realloc",				pr_obj_realloc},
	{"obj_calloc",				pr_obj_calloc},
	{"obj_free",				pr_obj_free},
	{"obj_get_uninstalled_dtable", pr_obj_get_uninstalled_dtable},
	{"obj_msgSend",				pr_obj_msgSend},
	{"obj_msgSend_super",		pr_obj_msgSend_super},

	{"obj_get_class",			pr_obj_get_class},
	{"obj_lookup_class",		pr_obj_lookup_class},
	{"obj_next_class",			pr_obj_next_class},

	{"sel_get_name",			pr_sel_get_name},
	{"sel_get_type",			pr_sel_get_type},
	{"sel_get_uid",				pr_sel_get_uid},
	{"sel_get_any_uid",			pr_sel_get_any_uid},
	{"sel_get_any_typed_uid",	pr_sel_get_any_typed_uid},
	{"sel_get_typed_uid",		pr_sel_get_typed_uid},
	{"sel_register_name",		pr_sel_register_name},
	{"sel_register_typed_name",	pr_sel_register_typed_name},
	{"sel_is_mapped",			pr_sel_is_mapped},

	{"class_get_class_method",	pr_class_get_class_method},
	{"class_get_instance_method", pr_class_get_instance_method},
	{"class_pose_as",			pr_class_pose_as},
	{"class_create_instance",	pr_class_create_instance},
	{"class_get_class_name",	pr_class_get_class_name},
	{"class_get_instance_size",	pr_class_get_instance_size},
	{"class_get_meta_class",	pr_class_get_meta_class},
	{"class_get_super_class",	pr_class_get_super_class},
	{"class_get_version",		pr_class_get_version},
	{"class_is_class",			pr_class_is_class},
	{"class_is_meta_class",		pr_class_is_meta_class},
	{"class_set_version",		pr_class_set_version},
	{"class_get_gc_object_type", pr_class_get_gc_object_type},
	{"class_ivar_set_gcinvisible", pr_class_ivar_set_gcinvisible},

	{"method_get_imp",			pr_method_get_imp},
	{"get_imp",					pr_get_imp},

	{"object_copy",				pr_object_copy},
	{"object_dispose",			pr_object_dispose},
	{"object_get_class",		pr_object_get_class},
	{"object_get_class_name",	pr_object_get_class_name},
	{"object_get_meta_class",	pr_object_get_meta_class},
	{"object_get_super_class",	pr_object_get_super_class},
	{"object_is_class",			pr_object_is_class},
	{"object_is_instance",		pr_object_is_instance},
	{"object_is_meta_class",	pr_object_is_meta_class},

	{"_i_Object__hash",			pr__i_Object__hash},
	{"_i_Object__compare_",		pr__i_Object__compare_},
	{"_c_Object__conformsTo_",	pr__c_Object__conformsTo_},
	{"_i_Object_error_error_",		pr__i_Object_error_error_},
	{"_c_Object__conformsToProtocol_",	pr__c_Object__conformsToProtocol_},
};

void
PR_Obj_Progs_Init (progs_t *pr)
{
	size_t      i;

	for (i = 0; i < sizeof (obj_methods) / sizeof (obj_methods[0]); i++) {
		PR_AddBuiltin (pr, obj_methods[i].name, obj_methods[i].func, -1);
	}
}

void
PR_InitRuntime (progs_t *pr)
{
	int         fnum;
	pr_class_t **class_list, **class;
	pr_category_t **category_list, **category;

	if (!pr->classes)
		pr->classes = Hash_NewTable (1021, class_get_key, 0, pr);
	else
		Hash_FlushTable (pr->classes);

	if (!pr->categories) {
		pr->categories = Hash_NewTable (1021, 0, 0, pr);
		Hash_SetHashCompare (pr->categories,
							 category_get_hash, category_compare);
	} else {
		Hash_FlushTable (pr->categories);
	}

	pr->fields.this = ED_GetFieldIndex (pr, ".this");
	for (fnum = 0; fnum < pr->progs->numfunctions; fnum++) {
		if (strequal (PR_GetString (pr, pr->pr_functions[fnum].s_name),
					  ".ctor")) {
			PR_ExecuteProgram (pr, fnum);
		}
	}

	class_list = (pr_class_t **) Hash_GetList (pr->classes);
	if (*class_list) {
		pr_class_t *object_class;
		pointer_t   object_ptr;

		object_class = Hash_Find (pr->classes, "Object");
		if (object_class && !object_class->super_class)
			object_ptr = (pr_type_t *)object_class - pr->pr_globals;
		else
			PR_Error (pr, "root class Object not found");
		for (class = class_list; *class; class++)
			finish_class (pr, *class, object_ptr);
	}
	free (class_list);

	category_list = (pr_category_t **) Hash_GetList (pr->categories);
	if (*category_list) {
		for (category = category_list; *category; category++)
			finish_category (pr, *category);
	}
	free (category_list);
}
