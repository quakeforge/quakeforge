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

static const char *
class_get_key (void *c, void *pr)
{
	return PR_GetString ((progs_t *)pr, ((pr_class_t *)c)->name);
}

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

static void
pr_return_self (progs_t *pr)
{
	G_INT (pr, OFS_RETURN) = G_INT (pr, OFS_PARM0);
}

void
PR_Obj_Progs_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "obj_msgSend", pr_obj_msgSend, -1);
	PR_AddBuiltin (pr, "obj_msgSend_super", pr_obj_msgSend_super, -1);
	PR_AddBuiltin (pr, "__obj_exec_class", pr___obj_exec_class, -1);

	PR_AddBuiltin (pr, "_c_Object__initialize", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__init", pr_return_self, -1);
	PR_AddBuiltin (pr, "_c_Object__alloc", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__free", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__shallowCopy", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__deepen", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__class", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__superClass", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__metaClass", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__name", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__self", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__hash", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__isEqual", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__compare", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__isMetaClass", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__isClass", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__isInstance", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__isKindOf", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__isMemberOf", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__isKindOfClassNamed", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__isMemberOfClassNamed", pr_return_self, -1);
	PR_AddBuiltin (pr, "_c_Object__instancesRespondTo", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__respondsTo", pr_return_self, -1);
	PR_AddBuiltin (pr, "_c_Object__conformsTo", pr_return_self, -1);
	PR_AddBuiltin (pr, "_c_Object__instanceMethodFor", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__methodFor", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__perform", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__perform_with", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__perform_with_with", pr_return_self, -1);
	PR_AddBuiltin (pr, "_c_Object__poseAs", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__transmuteClassTo", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__subclassResponsibility", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__notImplemented", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__shouldNotImplement", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__doesNotRecognize", pr_return_self, -1);
	PR_AddBuiltin (pr, "_i_Object__error", pr_return_self, -1);
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
