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
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
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

static inline pointer_t
POINTER_TO_PROG (progs_t *pr, void *p)
{
	return p ? (pr_type_t *) p - pr->pr_globals : 0;
}


static const char *
class_get_key (void *c, void *pr)
{
	return PR_GetString ((progs_t *)pr, ((pr_class_t *)c)->name);
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

//====================================================================

static void
pr___obj_exec_class (progs_t *pr)
{
	pr_module_t *module;
	pr_symtab_t *symtab;
	pointer_t   *ptr;
	int          i;
	//int          d = developer->int_val;
	pr_class_t  *object_class;
	pointer_t    object_ptr;

	if (!G_INT (pr, OFS_PARM0))
		return;
	module = &G_STRUCT (pr, pr_module_t, G_INT (pr, OFS_PARM0));
	if (!module->symtab)
		return;
	//developer->int_val = 1;
	symtab = &G_STRUCT (pr, pr_symtab_t, module->symtab);
	Sys_DPrintf ("Initializing %s module with %d classes and %d categories\n",
				 PR_GetString (pr, module->name),
				 symtab->cls_def_cnt, symtab->cat_def_cnt);
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
		ptr++;
	}

	object_class = Hash_Find (pr->classes, "Object");
	if (object_class && !object_class->super_class)
		object_ptr = (pr_type_t *)object_class - pr->pr_globals;
	else
		PR_Error (pr, "root class Object not found");

	ptr = symtab->defs;
	for (i = 0; i < symtab->cls_def_cnt; i++) {
		pr_class_t *class = &G_STRUCT (pr, pr_class_t, *ptr);
		pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);
		pr_type_t  *val;

		meta->class_pointer = object_ptr;
		if (class->super_class) {
			val = Hash_Find (pr->classes, PR_GetString (pr,
														class->super_class));
			meta->super_class = class->super_class = val - pr->pr_globals;
		}
		Sys_DPrintf ("    %d %d %d\n", meta->class_pointer, meta->super_class,
					 class->super_class);

		ptr++;
	}
	for (i = 0; i < symtab->cat_def_cnt; i++) {
		//pr_category_t *category = &G_STRUCT (pr, pr_category_t, *ptr);
		ptr++;
	}

	//developer->int_val = d;
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
		if (c->methods) {
			method_list = &G_STRUCT (pr, pr_method_list_t, c->methods);
			for (i = 0, method = method_list->method_list;
				 i < method_list->method_count; i++) {
				if (method->method_name.sel_id == selector->sel_id)
					return method->method_imp;
			}
		}
		c = c->super_class ? &G_STRUCT (pr, pr_class_t, c->super_class) : 0;
	}
	return 0;
}

static void
pr_obj_error (progs_t *pr)
{
	//XXX
}

static void
pr_obj_verror (progs_t *pr)
{
	//XXX
}

static void
pr_obj_set_error_handler (progs_t *pr)
{
	//XXX
}

static void
pr_obj_msg_lookup (progs_t *pr)
{
	//XXX
}

static void
pr_obj_msg_lookup_super (progs_t *pr)
{
	//XXX
}

static void
pr_obj_msg_sendv (progs_t *pr)
{
	//XXX
}

static void
pr_obj_malloc (progs_t *pr)
{
	//XXX
}

static void
pr_obj_atomic_malloc (progs_t *pr)
{
	//XXX
}

static void
pr_obj_valloc (progs_t *pr)
{
	//XXX
}

static void
pr_obj_realloc (progs_t *pr)
{
	//XXX
}

static void
pr_obj_calloc (progs_t *pr)
{
	//XXX
}

static void
pr_obj_free (progs_t *pr)
{
	//XXX
}

static void
pr_obj_get_uninstalled_dtable (progs_t *pr)
{
	//XXX
}

static void
pr_obj_msgSend (progs_t *pr)
{
	pointer_t   _self = G_INT (pr, OFS_PARM0);
	pointer_t   __cmd = G_INT (pr, OFS_PARM1);
	pr_class_t *self;
	pr_sel_t   *_cmd;
	func_t      imp;

	if (!_self) {
		G_INT (pr, OFS_RETURN) = _self;
		return;
	}
	if (!__cmd)
		PR_RunError (pr, "null selector");
	self = &G_STRUCT (pr, pr_class_t, _self);
	_cmd = &G_STRUCT (pr, pr_sel_t, __cmd);
	imp = obj_find_message (pr, self, _cmd);
	if (!imp)
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, self->name),
					 PR_GetString (pr, _cmd->sel_id));
	PR_ExecuteProgram (pr, imp);
}

static void
pr_obj_msgSend_super (progs_t *pr)
{
	pointer_t   _self = G_INT (pr, OFS_PARM0);
	pointer_t   __cmd = G_INT (pr, OFS_PARM1);
	pr_class_t *self;
	pr_class_t *super;
	pr_sel_t   *_cmd;
	func_t      imp;

	if (!_self) {
		G_INT (pr, OFS_RETURN) = _self;
		return;
	}
	if (!__cmd)
		PR_RunError (pr, "null selector");
	self = &G_STRUCT (pr, pr_class_t, _self);
	_cmd = &G_STRUCT (pr, pr_sel_t, __cmd);
	if (!self->super_class)
		PR_RunError (pr, "%s has no super class",
					 PR_GetString (pr, self->name));
	super = &G_STRUCT (pr, pr_class_t, self->super_class);
	imp = obj_find_message (pr, super, _cmd);
	if (!imp)
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, super->name),
					 PR_GetString (pr, _cmd->sel_id));
	PR_ExecuteProgram (pr, imp);
}

static void
pr_obj_get_class (progs_t *pr)
{
	//XXX
}

static void
pr_obj_lookup_class (progs_t *pr)
{
	//XXX
}

static void
pr_obj_next_class (progs_t *pr)
{
	//XXX
}

//====================================================================

static void
pr_sel_get_name (progs_t *pr)
{
	//XXX
}

static void
pr_sel_get_type (progs_t *pr)
{
	//XXX
}

static void
pr_sel_get_uid (progs_t *pr)
{
	//XXX
}

static void
pr_sel_get_any_uid (progs_t *pr)
{
	//XXX
}

static void
pr_sel_get_any_typed_uid (progs_t *pr)
{
	//XXX
}

static void
pr_sel_get_typed_uid (progs_t *pr)
{
	//XXX
}

static void
pr_sel_register_name (progs_t *pr)
{
	//XXX
}

static void
pr_sel_register_typed_name (progs_t *pr)
{
	//XXX
}

static void
pr_sel_is_mapped (progs_t *pr)
{
	//XXX
}

//====================================================================

static void
pr_class_get_class_method (progs_t *pr)
{
	//XXX
}

static void
pr_class_get_instance_method (progs_t *pr)
{
	//XXX
}

static void
pr_class_pose_as (progs_t *pr)
{
	//XXX
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
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	pr_id_t    *id = class_create_instance (pr, class);
	
	G_INT (pr, OFS_RETURN) = POINTER_TO_PROG (pr, id);
}

static void
pr_class_get_class_name (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (class) ? class->name
													: PR_SetString (pr, "Nil");
}

static void
pr_class_get_instance_size (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (class) ? class->instance_size : 0;
}

static void
pr_class_get_meta_class (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (class) ? class->class_pointer : 0;
}

static void
pr_class_get_super_class (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (class) ? class->super_class : 0;
}

static void
pr_class_get_version (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (class) ? class->version : -1;
}

static void
pr_class_is_class (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (class);
}

static void
pr_class_is_meta_class (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_CLS_ISMETA (class);
}

static void
pr_class_set_version (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	if (PR_CLS_ISCLASS (class))
		class->version = G_INT (pr, OFS_PARM1);
}

static void
pr_class_get_gc_object_type (progs_t *pr)
{
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, G_INT (pr, OFS_PARM0));
	G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (class) ? class->gc_object_type : 0;
}

static void
pr_class_ivar_set_gcinvisible (progs_t *pr)
{
	//XXX
}

//====================================================================

static void
pr_method_get_imp (progs_t *pr)
{
	//XXX
}

static void
pr_get_imp (progs_t *pr)
{
	//XXX
}

//====================================================================

static void
pr_object_dispose (progs_t *pr)
{
	pr_id_t    *object = &G_STRUCT (pr, pr_id_t, G_INT (pr, OFS_PARM0));
	PR_Zone_Free (pr, object);
}

static void
pr_object_copy (progs_t *pr)
{
	pr_id_t    *object = &G_STRUCT (pr, pr_id_t, G_INT (pr, OFS_PARM0));
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
	pr_id_t    *id;

	id = class_create_instance (pr, class);
	memcpy (id, object, sizeof (pr_type_t) * class->instance_size);
	G_INT (pr, OFS_RETURN) = POINTER_TO_PROG (pr, id);
}

static void
pr_object_get_class (progs_t *pr)
{
	pointer_t   _object = G_INT (pr, OFS_PARM0);
	pr_id_t    *object = &G_STRUCT (pr, pr_id_t, _object);
	pr_class_t *class;

	if (_object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			G_INT (pr, OFS_RETURN) = POINTER_TO_PROG (pr, class);
			return;
		}
		if (PR_CLS_ISMETA (class)) {
			G_INT (pr, OFS_RETURN) = _object;
			return;
		}
	}
	G_INT (pr, OFS_RETURN) = 0;
}

static void
pr_object_get_super_class (progs_t *pr)
{
	pointer_t   _object = G_INT (pr, OFS_PARM0);
	pr_id_t    *object = &G_STRUCT (pr, pr_id_t, _object);
	pr_class_t *class;

	if (_object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			G_INT (pr, OFS_RETURN) = class->super_class;
			return;
		}
		if (PR_CLS_ISMETA (class)) {
			G_INT (pr, OFS_RETURN) = ((pr_class_t *)object)->super_class;
			return;
		}
	}
	G_INT (pr, OFS_RETURN) = 0;
}

static void
pr_object_get_meta_class (progs_t *pr)
{
	pointer_t   _object = G_INT (pr, OFS_PARM0);
	pr_id_t    *object = &G_STRUCT (pr, pr_id_t, _object);
	pr_class_t *class;

	if (_object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			G_INT (pr, OFS_RETURN) = class->class_pointer;
			return;
		}
		if (PR_CLS_ISMETA (class)) {
			G_INT (pr, OFS_RETURN) = ((pr_class_t *)object)->class_pointer;
			return;
		}
	}
	G_INT (pr, OFS_RETURN) = 0;
}

static void
pr_object_get_class_name (progs_t *pr)
{
	pointer_t   _object = G_INT (pr, OFS_PARM0);
	pr_id_t    *object = &G_STRUCT (pr, pr_id_t, _object);
	pr_class_t *class;

	if (_object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			G_INT (pr, OFS_RETURN) = class->name;
			return;
		}
		if (PR_CLS_ISMETA (class)) {
			G_INT (pr, OFS_RETURN) = ((pr_class_t *)object)->name;
			return;
		}
	}
	RETURN_STRING (pr, "Nil");
}

static void
pr_object_is_class (progs_t *pr)
{
	pointer_t   _object = G_INT (pr, OFS_PARM0);
	pr_class_t *object = &G_STRUCT (pr, pr_class_t, _object);

	if (_object) {
		G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (object);
		return;
	}
	G_INT (pr, OFS_RETURN) = 0;
}

static void
pr_object_is_instance (progs_t *pr)
{
	pointer_t   _object = G_INT (pr, OFS_PARM0);
	pr_id_t    *object = &G_STRUCT (pr, pr_id_t, _object);
	pr_class_t *class;

	if (_object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		G_INT (pr, OFS_RETURN) = PR_CLS_ISCLASS (class);
		return;
	}
	G_INT (pr, OFS_RETURN) = 0;
}

static void
pr_object_is_meta_class (progs_t *pr)
{
	pointer_t   _object = G_INT (pr, OFS_PARM0);
	pr_class_t *object = &G_STRUCT (pr, pr_class_t, _object);

	if (_object) {
		G_INT (pr, OFS_RETURN) = PR_CLS_ISMETA (object);
		return;
	}
	G_INT (pr, OFS_RETURN) = 0;
}

//====================================================================

static void
pr__i_Object__hash (progs_t *pr)
{
	G_INT (pr, OFS_RETURN) = G_INT (pr, OFS_PARM0);
}

static void
pr__i_Object__compare (progs_t *pr)
{
	int          ret;
	ret = G_INT (pr, OFS_PARM0) != G_INT (pr, OFS_PARM2);
	if (ret) {
		ret = G_INT (pr, OFS_PARM0) > G_INT (pr, OFS_PARM2);
		if (!ret)
			ret = -1;
	}
	G_INT (pr, OFS_RETURN) = ret;
}

static void
pr__c_Object__conformsTo (progs_t *pr)
{
	//XXX
}

static void
pr__i_Object__error (progs_t *pr)
{
	//XXX
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
	{"_i_Object__compare",		pr__i_Object__compare},
	{"_c_Object__conformsTo",	pr__c_Object__conformsTo},
	{"_i_Object__error",		pr__i_Object__error},
};

void
PR_Obj_Progs_Init (progs_t *pr)
{
	int         i;

	for (i = 0; i < sizeof (obj_methods) / sizeof (obj_methods[0]); i++) {
		PR_AddBuiltin (pr, obj_methods[i].name, obj_methods[i].func, -1);
	}
}

void
PR_InitRuntime (progs_t *pr)
{
	int         fnum;

	if (!pr->classes)
		pr->classes = Hash_NewTable (1021, class_get_key, 0, pr);
	else
		Hash_FlushTable (pr->classes);
	for (fnum = 0; fnum < pr->progs->numfunctions; fnum++) {
		if (strequal (PR_GetString (pr, pr->pr_functions[fnum].s_name),
					  ".ctor")) {
			PR_ExecuteProgram (pr, fnum);
		}
	}
}
