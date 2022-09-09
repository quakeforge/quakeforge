/*
	rua_obj.c

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/ruamoko.h"
#include "QF/sys.h"

#include "QF/progs/pr_obj.h"
#include "QF/progs/pr_type.h"

#include "compat.h"
#include "rua_internal.h"

#define always_inline inline __attribute__((__always_inline__))

/* Macros to help with setting up to call a function, and cleaning up
 * afterwards. The problem is that PR_CallFunction saves the CURRENT stack
 * pointer, which has been adjusted by PR_SetupParams in order to push
 * the function arguments.
 *
 * RUA_CALL_BEGIN and RUA_CALL_END must be used in pairs and this is enforced
 * by the unbalanced {}s in the macros.
 */
#define RUA_CALL_BEGIN(pr, argc) \
	{ \
		pr_ptr_t    saved_stack = 0; \
		int         call_depth = (pr)->pr_depth; \
		if ((pr)->globals.stack) { \
			saved_stack = *(pr)->globals.stack; \
		} \
		PR_SetupParams (pr, argc, 1); \
		(pr)->pr_argc = argc;

#define RUA_CALL_END(pr, imp) \
		if (PR_CallFunction ((pr), (imp), (pr)->pr_return)) { \
			/* the call is to a progs function so a frame was pushed,   */ \
			/* ensure the stack pointer is restored on return           */ \
			/* if there's no stack, then the following is effectively a */ \
			/* noop                                                     */ \
			(pr)->pr_stack[call_depth].stack_ptr = saved_stack; \
		} else if ((pr)->globals.stack) { \
			/* the call was to a builtin, restore the stack             */ \
			*(pr)->globals.stack = saved_stack; \
		} \
	}

// number of selectors to allocate at once (to minimize memory waste)
#define SELECTOR_BLOCK 64

typedef struct obj_list_s {
	struct obj_list_s *next;
	void       *data;
} obj_list;

typedef struct dtable_s {
	struct dtable_s *next;
	struct dtable_s **prev;
	size_t      size;
	pr_func_t  *imp;
} dtable_t;

typedef struct probj_resources_s {
	progs_t    *pr;
	unsigned    selector_index;
	unsigned    selector_index_max;
	pr_sel_t   *selector_block;
	int         available_selectors;
	obj_list  **selector_sels;
	pr_string_t *selector_names;
	pr_int_t   *selector_argc;
	PR_RESMAP (dtable_t) dtables;
	dtable_t   *dtable_list;
	pr_func_t   obj_forward;
	pr_sel_t   *forward_selector;
	dstring_t  *msg;
	hashtab_t  *selector_hash;
	hashtab_t  *classes;
	hashtab_t  *protocols;
	hashtab_t  *load_methods;
	obj_list   *obj_list_free_list;
	obj_list   *unresolved_classes;
	obj_list   *unclaimed_categories;
	obj_list   *uninitialized_statics;
	obj_list   *unclaimed_proto_list;
	obj_list   *module_list;
	obj_list   *class_tree_list;
} probj_t;

static dtable_t *
dtable_new (probj_t *probj)
{
	dtable_t   *dtable = PR_RESNEW (probj->dtables);
	dtable->next = probj->dtable_list;
	dtable->prev = &probj->dtable_list;
	if (probj->dtable_list) {
		probj->dtable_list->prev = &dtable->next;
	}
	probj->dtable_list = dtable;
	return dtable;
}

static void
dtable_reset (probj_t *probj)
{
	PR_RESRESET (probj->dtables);
	probj->dtable_list = 0;
}

static inline dtable_t *
dtable_get (probj_t *probj, int index)
{
	return PR_RESGET (probj->dtables, index);
}

static inline int __attribute__((pure))
dtable_index (probj_t *probj, dtable_t *dtable)
{
	return PR_RESINDEX (probj->dtables, dtable);
}

static always_inline dtable_t * __attribute__((pure))
get_dtable (probj_t *probj, const char *name, int index)
{
	dtable_t   *dtable = dtable_get (probj, index);

	if (!dtable) {
		PR_RunError (probj->pr, "invalid dtable index in %s", name);
	}
	return dtable;
}

static obj_list *
obj_list_new (probj_t *probj)
{
	int         i;
	obj_list   *l;

	if (!probj->obj_list_free_list) {
		probj->obj_list_free_list = calloc (128, sizeof (obj_list));
		for (i = 0; i < 127; i++)
			probj->obj_list_free_list[i].next = &probj->obj_list_free_list[i + 1];
	}
	l = probj->obj_list_free_list;
	probj->obj_list_free_list = l->next;
	l->next = 0;
	return l;
}

static void
obj_list_free (probj_t *probj, obj_list *l)
{
	obj_list   *e;

	if (!l)
		return;

	for (e = l; e->next; e = e->next)
		;
	e->next = probj->obj_list_free_list;
	probj->obj_list_free_list = l;
}

static inline obj_list *
list_cons (probj_t *probj, void *data, obj_list *next)
{
	obj_list   *l = obj_list_new (probj);
	l->data = data;
	l->next = next;
	return l;
}

static inline void
list_remove (probj_t *probj, obj_list **list)
{
	if ((*list)->next) {
		obj_list   *l = *list;
		*list = (*list)->next;
		l->next = 0;
		obj_list_free (probj, l);
	} else {
		obj_list_free (probj, *list);
		*list = 0;
	}
}

typedef struct class_tree {
	pr_class_t *class;
	obj_list   *subclasses;
} class_tree;

class_tree *class_tree_free_list;

static class_tree *
class_tree_new (void)
{
	int         i;
	class_tree *t;

	if (!class_tree_free_list) {
		class_tree_free_list = calloc (128, sizeof (class_tree));
		for (i = 0; i < 127; i++) {
			class_tree *x = &class_tree_free_list[i];
			x->subclasses = (obj_list *) (x + 1);
		}
	}
	t = class_tree_free_list;
	class_tree_free_list = (class_tree *) t->subclasses;
	t->subclasses = 0;
	return t;
}

static int
class_is_subclass_of_class (probj_t *probj, pr_class_t *class,
							pr_class_t *superclass)
{
	while (class) {
		if (class == superclass)
			return 1;
		if (!class->super_class)
			break;
		class = Hash_Find (probj->classes,
						   PR_GetString (probj->pr, class->super_class));
	}
	return 0;
}

static class_tree *
create_tree_of_subclasses_inherited_from (probj_t *probj, pr_class_t *bottom,
										  pr_class_t *upper)
{
	progs_t    *pr = probj->pr;
	const char *super_class = PR_GetString (pr, bottom->super_class);
	pr_class_t *superclass;
	class_tree *tree, *prev;

	superclass = bottom->super_class ? Hash_Find (probj->classes, super_class)
									 : 0;
	tree = prev = class_tree_new ();
	prev->class = bottom;
	while (superclass != upper) {
		tree = class_tree_new ();
		tree->class = superclass;
		tree->subclasses = list_cons (probj, prev, tree->subclasses);
		super_class = PR_GetString (pr, superclass->super_class);
		superclass = (superclass->super_class ? Hash_Find (probj->classes,
														   super_class)
											  : 0);
		prev = tree;
	}
	return tree;
}

static class_tree *
_obj_tree_insert_class (probj_t *probj, class_tree *tree, pr_class_t *class)
{
	progs_t    *pr = probj->pr;
	obj_list   *subclasses;
	class_tree *new_tree;

	if (!tree)
		return create_tree_of_subclasses_inherited_from (probj, class, 0);
	if (class == tree->class)
		return tree;
	if ((class->super_class ? Hash_Find (probj->classes,
										 PR_GetString (pr,
											 		   class->super_class))
							: 0) == tree->class) {
		obj_list   *list = tree->subclasses;
		class_tree *node;

		while (list) {
			if (((class_tree *) list->data)->class == class)
				return tree;
			list = list->next;
		}
		node = class_tree_new ();
		node->class = class;
		tree->subclasses = list_cons (probj, node, tree->subclasses);
		return tree;
	}
	if (!class_is_subclass_of_class (probj, class, tree->class))
		return 0;
	for (subclasses = tree->subclasses; subclasses;
		 subclasses = subclasses->next) {
		pr_class_t *aclass = ((class_tree *)subclasses->data)->class;
		if (class_is_subclass_of_class (probj, class, aclass)) {
			subclasses->data = _obj_tree_insert_class (probj, subclasses->data,
													   class);
			return tree;
		}
	}
	new_tree = create_tree_of_subclasses_inherited_from (probj, class,
														 tree->class);
	tree->subclasses = list_cons (probj, new_tree, tree->subclasses);
	return tree;
}

static void
obj_tree_insert_class (probj_t *probj, pr_class_t *class)
{
	obj_list   *list_node;
	class_tree *tree;

	list_node = probj->class_tree_list;
	while (list_node) {
		tree = _obj_tree_insert_class (probj, list_node->data, class);
		if (tree) {
			list_node->data = tree;
			break;
		} else {
			list_node = list_node->next;
		}
	}
	if (!list_node) {
		tree = _obj_tree_insert_class (probj, 0, class);
		probj->class_tree_list = list_cons (probj, tree,
											probj->class_tree_list);
	}
}

static void
obj_create_classes_tree (probj_t *probj, pr_module_t *module)
{
	progs_t    *pr = probj->pr;
	pr_symtab_t *symtab = &G_STRUCT (pr, pr_symtab_t, module->symtab);
	int         i;

	for (i = 0; i < symtab->cls_def_cnt; i++) {
		pr_class_t *class = &G_STRUCT (pr, pr_class_t, symtab->defs[i]);
		obj_tree_insert_class (probj, class);
	}
}

static void
obj_destroy_class_tree_node (probj_t *probj, class_tree *tree, int level)
{
	tree->subclasses = (obj_list *) class_tree_free_list;
	class_tree_free_list = tree;
}

static void
obj_preorder_traverse (probj_t *probj, class_tree *tree, int level,
					   void (*func) (probj_t *, class_tree *, int))
{
	obj_list   *node;

	func (probj, tree, level);
	for (node = tree->subclasses; node; node = node->next)
		obj_preorder_traverse (probj, node->data, level + 1, func);
}

static void
obj_postorder_traverse (probj_t *probj, class_tree *tree, int level,
						void (*func) (probj_t *, class_tree *, int))
{
	obj_list   *node;

	for (node = tree->subclasses; node; node = node->next)
		obj_postorder_traverse (probj, node->data, level + 1, func);
	func (probj, tree, level);
}

static const char *
selector_get_key (const void *s, void *_probj)
{
	__auto_type probj = (probj_t *) _probj;
	return PR_GetString (probj->pr, probj->selector_names[(intptr_t) s]);
}

static const char *
class_get_key (const void *c, void *_probj)
{
	__auto_type probj = (probj_t *) _probj;
	return PR_GetString (probj->pr, ((pr_class_t *)c)->name);
}

static const char *
protocol_get_key (const void *p, void *_probj)
{
	__auto_type probj = (probj_t *) _probj;
	return PR_GetString (probj->pr, ((pr_protocol_t *)p)->protocol_name);
}

static uintptr_t
load_methods_get_hash (const void *m, void *_probj)
{
	return (uintptr_t) m;
}

static int
load_methods_compare (const void *m1, const void *m2, void *_probj)
{
	return m1 == m2;
}

static inline int
sel_eq (pr_sel_t *s1, pr_sel_t *s2)
{
	if (!s1 || !s2)
		return s1 == s2;
	return s1->sel_id == s2->sel_id;
}

static int
object_is_instance (probj_t *probj, pr_id_t *object)
{
	progs_t *pr = probj->pr;
	pr_class_t *class;

	if (object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		return PR_CLS_ISCLASS (class);
	}
	return 0;
}

static pr_string_t
object_get_class_name (probj_t *probj, pr_id_t *object)
{
	progs_t    *pr = probj->pr;
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
finish_class (probj_t *probj, pr_class_t *class, pr_ptr_t object_ptr)
{
	progs_t    *pr = probj->pr;
	pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);
	pr_class_t *val;

	meta->class_pointer = object_ptr;
	if (class->super_class) {
		const char *super_class = PR_GetString (pr, class->super_class);
		const char *class_name = PR_GetString (pr, class->name);
		val = Hash_Find (probj->classes, super_class);
		if (!val)
			PR_Error (pr, "broken class %s: super class %s not found",
					  class_name, super_class);
		meta->super_class = val->class_pointer;
		class->super_class = PR_SetPointer (pr, val);
	} else {
		pr_ptr_t   *ml = &meta->methods;
		while (*ml)
			ml = &G_STRUCT (pr, pr_method_list_t, *ml).method_next;
		*ml = class->methods;
	}
	Sys_MaskPrintf (SYS_rua_obj, "    %x %x %x\n", meta->class_pointer,
					meta->super_class, class->super_class);
}

//====================================================================

static int
add_sel_name (probj_t *probj, const char *name)
{
	int         ind = ++probj->selector_index;
	int         size, i;

	if (probj->selector_index >= probj->selector_index_max) {
		size = probj->selector_index_max + 128;
		probj->selector_sels = realloc (probj->selector_sels,
										size * sizeof (obj_list *));
		probj->selector_names = realloc (probj->selector_names,
										 size * sizeof (pr_string_t));
		probj->selector_argc = realloc (probj->selector_argc,
										size * sizeof (pr_int_t));
		for (i = probj->selector_index_max; i < size; i++) {
			probj->selector_sels[i] = 0;
			probj->selector_names[i] = 0;
			probj->selector_argc[i] = 0;
		}
		probj->selector_index_max = size;
	}
	probj->selector_names[ind] = PR_SetString (probj->pr, name);
	return ind;
}

static pr_sel_t *
sel_register_typed_name (probj_t *probj, const char *name, const char *types,
						 pr_sel_t *sel)
{
	progs_t    *pr = probj->pr;
	intptr_t	index;
	int			is_new = 0;
	obj_list   *l;

	index = (intptr_t) Hash_Find (probj->selector_hash, name);
	if (index) {
		for (l = probj->selector_sels[index]; l; l = l->next) {
			pr_sel_t   *s = l->data;
			if (!types || !s->sel_types) {
				if (!s->sel_types && !types) {
					if (sel) {
						sel->sel_id = index;
						goto done;
					}
					return s;
				}
			} else if (strcmp (PR_GetString (pr, s->sel_types), types) == 0) {
				if (sel) {
					sel->sel_id = index;
					goto done;
				}
				return s;
			}
		}
	} else {
		Sys_MaskPrintf (SYS_rua_obj, "    Registering SEL %s %s\n",
						name, types);
		index = add_sel_name (probj, name);
		is_new = 1;
	}
	if (!sel) {
		if (!probj->available_selectors) {
			probj->available_selectors = SELECTOR_BLOCK;
			sel = PR_Zone_Malloc (pr, SELECTOR_BLOCK*sizeof (pr_sel_t));
			probj->selector_block = sel;
		}
		sel = &probj->selector_block[--probj->available_selectors];
	}

	sel->sel_id = index;
	sel->sel_types = PR_SetString (pr, types);

	l = obj_list_new (probj);
	l->data = sel;
	l->next = probj->selector_sels[index];
	probj->selector_sels[index] = l;

	if (sel->sel_types && pr->type_encodings) {
		const char *enc = PR_GetString (pr, sel->sel_types);
		__auto_type type = (qfot_type_t *) Hash_Find (pr->type_hash, enc);
		if (type->meta != ty_basic || type->type != ev_func) {
			PR_RunError (pr, "selector type encoing is not a function");
		}
		probj->selector_argc[index] = type->func.num_params;
	}

	if (is_new)
		Hash_Add (probj->selector_hash, (void *) index);
done:
	Sys_MaskPrintf (SYS_rua_obj, "        %d @ %x\n",
					sel->sel_id, PR_SetPointer (pr, sel));
	return sel;
}

static pr_sel_t *
sel_register_name (probj_t *probj, const char *name)
{
	return sel_register_typed_name (probj, name, "", 0);
}

static void
obj_register_selectors_from_description_list (probj_t *probj,
		pr_method_description_list_t *method_list)
{
	progs_t    *pr = probj->pr;
	int         i;

	if (!method_list) {
		return;
	}
	for (i = 0; i < method_list->count; i++) {
		pr_method_description_t *method = &method_list->list[i];
		const char *name = PR_GetString (pr, method->name);
		const char *types = PR_GetString (pr, method->types);
		pr_sel_t   *sel = sel_register_typed_name (probj, name, types, 0);
		method->name = PR_SetPointer (pr, sel);
	}
}

static void
obj_register_selectors_from_list (probj_t *probj,
								  pr_method_list_t *method_list)
{
	progs_t    *pr = probj->pr;
	int         i;

	for (i = 0; i < method_list->method_count; i++) {
		pr_method_t *method = &method_list->method_list[i];
		const char *name = PR_GetString (pr, method->method_name);
		const char *types = PR_GetString (pr, method->method_types);
		pr_sel_t   *sel = sel_register_typed_name (probj, name, types, 0);
		method->method_name = PR_SetPointer (pr, sel);
	}
}

static void
obj_register_selectors_from_class (probj_t *probj, pr_class_t *class)
{
	progs_t    *pr = probj->pr;
	pr_method_list_t *method_list = &G_STRUCT (pr, pr_method_list_t,
											   class->methods);
	while (method_list) {
		obj_register_selectors_from_list (probj, method_list);
		method_list = &G_STRUCT (pr, pr_method_list_t,
								 method_list->method_next);
	}
}

static void obj_init_protocols (probj_t *probj, pr_protocol_list_t *protos);

static void
obj_init_protocol (probj_t *probj, pr_class_t *proto_class,
				   pr_protocol_t *proto)
{
	progs_t    *pr = probj->pr;

	if (!proto->class_pointer) {
		const char *protocol_name = PR_GetString (pr, proto->protocol_name);
		proto->class_pointer = PR_SetPointer (pr, proto_class);
		obj_register_selectors_from_description_list (probj,
				&G_STRUCT (pr, pr_method_description_list_t,
						   proto->instance_methods));
		obj_register_selectors_from_description_list (probj,
				&G_STRUCT (pr, pr_method_description_list_t,
						   proto->class_methods));
		if (!Hash_Find (probj->protocols, protocol_name)) {
			Hash_Add (probj->protocols, proto);
		}
		obj_init_protocols (probj, &G_STRUCT (pr, pr_protocol_list_t,
											  proto->protocol_list));
	} else {
		if (proto->class_pointer != PR_SetPointer (pr, proto_class))
			PR_RunError (pr, "protocol broken");
	}
}

static void
obj_init_protocols (probj_t *probj, pr_protocol_list_t *protos)
{
	progs_t    *pr = probj->pr;
	pr_class_t *proto_class;
	pr_protocol_t *proto;
	int         i;

	if (!protos)
		return;

	if (!(proto_class = Hash_Find (probj->classes, "Protocol"))) {
		probj->unclaimed_proto_list = list_cons (probj, protos,
												 probj->unclaimed_proto_list);
		return;
	}

	for (i = 0; i < protos->count; i++) {
		proto = &G_STRUCT (pr, pr_protocol_t, protos->list[i]);
		obj_init_protocol (probj, proto_class, proto);
	}
}

static void
class_add_method_list (probj_t *probj, pr_class_t *class,
					   pr_method_list_t *list)
{
	progs_t    *pr = probj->pr;
	int         i;

	for (i = 0; i < list->method_count; i++) {
		pr_method_t *method = &list->method_list[i];
		if (method->method_name) {
			const char *name = PR_GetString (pr, method->method_name);
			const char *types = PR_GetString (pr, method->method_types);
			pr_sel_t   *sel = sel_register_typed_name (probj, name, types, 0);
			method->method_name = PR_SetPointer (pr, sel);
		}
	}

	list->method_next = class->methods;
	class->methods = PR_SetPointer (pr, list);
}

static void
obj_class_add_protocols (probj_t *probj, pr_class_t *class,
						 pr_protocol_list_t *protos)
{
	if (!protos)
		return;

	protos->next = class->protocols;
	class->protocols = protos->next;
}

static void
finish_category (probj_t *probj, pr_category_t *category, pr_class_t *class)
{
	progs_t    *pr = probj->pr;
	pr_method_list_t *method_list;
	pr_protocol_list_t *protocol_list;

	if (category->instance_methods) {
		method_list = &G_STRUCT (pr, pr_method_list_t,
								 category->instance_methods);
		class_add_method_list (probj, class, method_list);
	}
	if (category->class_methods) {
		pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);
		method_list = &G_STRUCT (pr, pr_method_list_t,
								 category->class_methods);
		class_add_method_list (probj, meta, method_list);
	}
	if (category->protocols) {
		protocol_list = &G_STRUCT (pr, pr_protocol_list_t,
								   category->protocols);
		obj_init_protocols (probj, protocol_list);
		obj_class_add_protocols (probj, class, protocol_list);
	}
}

static void
obj_send_message_in_list (probj_t *probj, pr_method_list_t *method_list,
						  pr_class_t *class, pr_sel_t *op)
{
	progs_t    *pr = probj->pr;
	int         i;

	if (!method_list)
		return;

	obj_send_message_in_list (probj, &G_STRUCT (pr, pr_method_list_t,
												method_list->method_next),
							  class, op);

	for (i = 0; i < method_list->method_count; i++) {
		pr_method_t *mth = &method_list->method_list[i];
		if (mth->method_name && sel_eq (&G_STRUCT (pr, pr_sel_t,
												   mth->method_name), op)
			&& !Hash_FindElement (probj->load_methods,
								  (void *) (intptr_t) mth->method_imp)) {
			Hash_AddElement (probj->load_methods,
							 (void *) (intptr_t) mth->method_imp);
			//FIXME need to wrap in save/restore params?
			PR_ExecuteProgram (pr, mth->method_imp);
			break;
		}
	}
}

static void
send_load (probj_t *probj, class_tree *tree, int level)
{
	progs_t    *pr = probj->pr;
	pr_sel_t   *load_sel = sel_register_name (probj, "load");
	pr_class_t *class = tree->class;
	pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);
	pr_method_list_t *method_list = &G_STRUCT (pr, pr_method_list_t,
											   meta->methods);

	obj_send_message_in_list (probj, method_list, class, load_sel);
}

static void
obj_send_load (probj_t *probj)
{
	progs_t    *pr = probj->pr;
	obj_list   *m;

	if (probj->unresolved_classes) {
		pr_class_t *class = probj->unresolved_classes->data;
		const char *super_class = PR_GetString (pr, class->super_class);
		while (Hash_Find (probj->classes, super_class)) {
			list_remove (probj, &probj->unresolved_classes);
			if (probj->unresolved_classes) {
				class = probj->unresolved_classes->data;
				super_class = PR_GetString (pr, class->super_class);
			} else {
				break;
			}
		}
		if (probj->unresolved_classes)
			return;
	}

	//XXX constant string stuff here (see init.c in libobjc source)

	for (m = probj->module_list; m; m = m->next)
		obj_create_classes_tree (probj, m->data);
	while (probj->class_tree_list) {
		obj_preorder_traverse (probj, probj->class_tree_list->data, 0,
							   send_load);
		obj_postorder_traverse (probj, probj->class_tree_list->data, 0,
								obj_destroy_class_tree_node);
		list_remove (probj, &probj->class_tree_list);
	}
	//XXX callback
	//for (m = probj->module_list; m; m = m->next)
	//	obj_create_classes_tree (probj, m->data);
	obj_list_free (probj, probj->module_list);
	probj->module_list = 0;
}

static pr_method_t *
obj_find_message (probj_t *probj, pr_class_t *class, pr_sel_t *selector)
{
	progs_t    *pr = probj->pr;
	pr_class_t *c = class;
	pr_method_list_t *method_list;
	pr_method_t *method;
	pr_sel_t   *sel;
	int         i;
	int         dev = developer;
	pr_string_t *names;

	if (dev & SYS_rua_msg) {
		names = probj->selector_names;
		Sys_Printf ("Searching for %s\n",
					PR_GetString (pr, names[selector->sel_id]));
	}
	while (c) {
		if (dev & SYS_rua_msg)
			Sys_Printf ("Checking class %s @ %x\n",
						PR_GetString (pr, c->name),
						PR_SetPointer (pr, c));
		method_list = &G_STRUCT (pr, pr_method_list_t, c->methods);
		while (method_list) {
			if (dev & SYS_rua_msg) {
				Sys_Printf ("method list %x\n",
							PR_SetPointer (pr, method_list));
			}
			for (i = 0, method = method_list->method_list;
				 i < method_list->method_count; i++, method++) {
				sel = &G_STRUCT (pr, pr_sel_t, method->method_name);
				if (developer & SYS_rua_msg) {
					names = probj->selector_names;
					Sys_Printf ("  %s\n",
								PR_GetString (pr, names[sel->sel_id]));
				}
				if (sel->sel_id == selector->sel_id) {
					if (dev & SYS_rua_msg) {
						names = probj->selector_names;
						Sys_Printf ("found %s: %x\n",
									PR_GetString (pr, names[selector->sel_id]),
									method->method_imp);
					}
					return method;
				}
			}
			method_list = &G_STRUCT (pr, pr_method_list_t,
									 method_list->method_next);
		}
		c = c->super_class ? &G_STRUCT (pr, pr_class_t, c->super_class) : 0;
	}
	return 0;
}

static void
obj_send_initialize (probj_t *probj, pr_class_t *class)
{
	progs_t    *pr = probj->pr;
	pr_method_list_t *method_list;
	pr_method_t *method;
	pr_sel_t   *sel;
	pr_class_t *class_pointer;
	pr_sel_t   *selector = sel_register_name (probj, "initialize");
	int         i;

	if (PR_CLS_ISINITIALIZED (class))
		return;
	class_pointer = &G_STRUCT (pr, pr_class_t, class->class_pointer);
	PR_CLS_SETINITIALIZED (class);
	PR_CLS_SETINITIALIZED (class_pointer);
	if (class->super_class)
		obj_send_initialize (probj, &G_STRUCT (pr, pr_class_t,
											   class->super_class));

	method_list = &G_STRUCT (pr, pr_method_list_t, class_pointer->methods);
	while (method_list) {
		for (i = 0, method = method_list->method_list;
			 i < method_list->method_count; i++, method++) {
			sel = &G_STRUCT (pr, pr_sel_t, method->method_name);
			if (sel->sel_id == selector->sel_id) {
				PR_PushFrame (pr);
				__auto_type params = PR_SaveParams (pr);
				// param 0 is known to be the class pointer
				P_POINTER (pr, 1) = method->method_name;
				// pr->pr_argc is known to be 2
				PR_ExecuteProgram (pr, method->method_imp);
				PR_RestoreParams (pr, params);
				PR_PopFrame (pr);
				return;
			}
		}
		method_list = &G_STRUCT (pr, pr_method_list_t,
								 method_list->method_next);
	}
}

static void
obj_install_methods_in_dtable (probj_t *probj, pr_class_t *class,
							   pr_method_list_t *method_list)
{
	progs_t    *pr = probj->pr;
	dtable_t   *dtable;

	if (!method_list) {
		return;
	}
	if (method_list->method_next) {
		obj_install_methods_in_dtable (probj, class,
									   &G_STRUCT (pr, pr_method_list_t,
												  method_list->method_next));
	}

	dtable = get_dtable (probj, __FUNCTION__, class->dtable);
	for (int i = 0; i < method_list->method_count; i++) {
		pr_method_t *method = &method_list->method_list[i];
		pr_sel_t   *sel = &G_STRUCT (pr, pr_sel_t, method->method_name);
		if (sel->sel_id < dtable->size) {
			dtable->imp[sel->sel_id] = method->method_imp;
		}
	}
}

static void
obj_install_dispatch_table_for_class (probj_t *probj, pr_class_t *class)
{
	progs_t    *pr = probj->pr;
	pr_class_t *super = &G_STRUCT (pr, pr_class_t, class->super_class);
	dtable_t   *dtable;

	Sys_MaskPrintf (SYS_rua_obj, "    install dispatch for class %s %x %d\n",
					PR_GetString (pr, class->name),
					class->methods,
					PR_CLS_ISMETA(class));

	if (super && !super->dtable) {
		obj_install_dispatch_table_for_class (probj, super);
	}

	dtable = dtable_new (probj);
	class->dtable = dtable_index (probj, dtable);
	dtable->size = probj->selector_index + 1;
	dtable->imp = calloc (dtable->size, sizeof (pr_func_t));
	if (super) {
		dtable_t   *super_dtable = get_dtable (probj, __FUNCTION__,
											   super->dtable);
		memcpy (dtable->imp, super_dtable->imp,
				super_dtable->size * sizeof (*dtable->imp));
	}
	obj_install_methods_in_dtable (probj, class,
								   &G_STRUCT (pr, pr_method_list_t,
											  class->methods));
}

static inline dtable_t *
obj_check_dtable_installed (probj_t *probj, pr_class_t *class)
{
	if (!class->dtable) {
		obj_install_dispatch_table_for_class (probj, class);
	}
	return get_dtable (probj, __FUNCTION__, class->dtable);
}

static pr_func_t
get_imp (probj_t *probj, pr_class_t *class, pr_sel_t *sel)
{
	pr_func_t  imp = 0;

	if (class->dtable) {
		dtable_t   *dtable = get_dtable (probj, __FUNCTION__, class->dtable);
		if (sel->sel_id < dtable->size) {
			imp = dtable->imp[sel->sel_id];
		}
	}
	if (!imp) {
		if (!class->dtable) {
			obj_install_dispatch_table_for_class (probj, class);
			imp = get_imp (probj, class, sel);
		} else {
			imp = probj->obj_forward;
		}
	}
	return imp;
}

static int
obj_reponds_to (probj_t *probj, pr_id_t *obj, pr_sel_t *sel)
{
	progs_t    *pr = probj->pr;
	pr_class_t *class;
	dtable_t   *dtable;
	pr_func_t   imp = 0;

	class = &G_STRUCT (pr, pr_class_t, obj->class_pointer);
	dtable = obj_check_dtable_installed (probj, class);

	if (sel->sel_id < dtable->size) {
		imp = dtable->imp[sel->sel_id];
	}
	return imp != 0;
}

static pr_func_t
obj_msg_lookup (probj_t *probj, pr_id_t *receiver, pr_sel_t *op)
{
	progs_t    *pr = probj->pr;
	pr_class_t *class;
	if (!receiver)
		return 0;
	class = &G_STRUCT (pr, pr_class_t, receiver->class_pointer);
	if (PR_CLS_ISCLASS (class)) {
		if (!PR_CLS_ISINITIALIZED (class))
			obj_send_initialize (probj, class);
	} else if (PR_CLS_ISMETA (class)
			   && PR_CLS_ISCLASS ((pr_class_t *) receiver)) {
		if (!PR_CLS_ISINITIALIZED ((pr_class_t *) receiver))
			obj_send_initialize (probj, (pr_class_t *) receiver);
	}
	return get_imp (probj, class, op);
}

static pr_func_t
obj_msg_lookup_super (probj_t *probj, pr_super_t *super, pr_sel_t *op)
{
	progs_t    *pr = probj->pr;
	pr_class_t *class;

	if (!super->self)
		return 0;

	class = &G_STRUCT (pr, pr_class_t, super->class);
	return get_imp (probj, class, op);
}

static void
obj_verror (probj_t *probj, pr_id_t *object, int code, const char *fmt, int count,
			pr_type_t **args)
{
	progs_t    *pr = probj->pr;
	__auto_type class = &G_STRUCT (pr, pr_class_t, object->class_pointer);

	PR_Sprintf (pr, probj->msg, "obj_verror", fmt, count, args);
	PR_RunError (pr, "%s: %s", PR_GetString (pr, class->name), probj->msg->str);
}

static void
dump_ivars (probj_t *probj, pr_ptr_t _ivars)
{
	progs_t    *pr = probj->pr;
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
obj_init_statics (probj_t *probj)
{
	progs_t    *pr = probj->pr;
	obj_list  **cell = &probj->uninitialized_statics;
	pr_ptr_t   *ptr;
	pr_ptr_t   *inst;

	Sys_MaskPrintf (SYS_rua_obj, "Initializing statics\n");
	while (*cell) {
		int         initialized = 1;

		for (ptr = (*cell)->data; *ptr; ptr++) {
			__auto_type statics = &G_STRUCT (pr, pr_static_instances_t, *ptr);
			const char *class_name = PR_GetString (pr, statics->class_name);
			pr_class_t *class = Hash_Find (probj->classes, class_name);

			Sys_MaskPrintf (SYS_rua_obj, "    %s %p\n", class_name, class);
			if (!class) {
				initialized = 0;
				continue;
			}

			if (strcmp (class_name, "Protocol") == 0) {
				// protocols are special
				for (inst = statics->instances; *inst; inst++) {
					obj_init_protocol (probj, class,
									   &G_STRUCT (pr, pr_protocol_t, *inst));
				}
			} else {
				for (inst = statics->instances; *inst; inst++) {
					pr_id_t    *id = &G_STRUCT (pr, pr_id_t, *inst);
					id->class_pointer = PR_SetPointer (pr, class);
				}
			}
		}

		if (initialized) {
			list_remove (probj, cell);
		} else {
			cell = &(*cell)->next;
		}
	}
}

static void
rua___obj_exec_class (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_module_t *module = &P_STRUCT (pr, pr_module_t, 0);
	pr_symtab_t *symtab;
	pr_sel_t   *sel;
	pr_ptr_t   *ptr;
	int         i;
	obj_list  **cell;

	if (!module)
		return;
	symtab = &G_STRUCT (pr, pr_symtab_t, module->symtab);
	if (!symtab)
		return;
	Sys_MaskPrintf (SYS_rua_obj, "Initializing %s module\n"
					"symtab @ %x : %d selector%s @ %x, "
					"%d class%s and %d categor%s\n"
					"static instance lists: %s\n",
					PR_GetString (pr, module->name), module->symtab,
					symtab->sel_ref_cnt, symtab->sel_ref_cnt == 1 ? "" : "s",
					symtab->refs,
					symtab->cls_def_cnt, symtab->cls_def_cnt == 1 ? "" : "es",
					symtab->cat_def_cnt,
					symtab->cat_def_cnt == 1 ? "y" : "ies",
					symtab->defs[symtab->cls_def_cnt
								 + symtab->cat_def_cnt] ? "yes" : "no");

	probj->module_list = list_cons (probj, module, probj->module_list);

	sel = &G_STRUCT (pr, pr_sel_t, symtab->refs);
	for (i = 0; i < symtab->sel_ref_cnt; i++) {
		const char *name, *types;
		name = PR_GetString (pr, sel->sel_id);
		types = PR_GetString (pr, sel->sel_types);
		sel_register_typed_name (probj, name, types, sel);
		sel++;
	}

	ptr = symtab->defs;
	for (i = 0; i < symtab->cls_def_cnt; i++, ptr++) {
		pr_class_t *class = &G_STRUCT (pr, pr_class_t, *ptr);
		pr_class_t *meta = &G_STRUCT (pr, pr_class_t, class->class_pointer);
		const char *super_class = PR_GetString (pr, class->super_class);

		Sys_MaskPrintf (SYS_rua_obj, "Class %s @ %x\n",
						PR_GetString (pr, class->name), *ptr);
		Sys_MaskPrintf (SYS_rua_obj, "    class pointer: %x\n",
						class->class_pointer);
		Sys_MaskPrintf (SYS_rua_obj, "    super class: %s\n",
						PR_GetString (pr, class->super_class));
		Sys_MaskPrintf (SYS_rua_obj, "    instance variables: %d @ %x\n",
						class->instance_size,
						class->ivars);
		if (developer & SYS_rua_obj)
			dump_ivars (probj, class->ivars);
		Sys_MaskPrintf (SYS_rua_obj, "    instance methods: %x\n",
						class->methods);
		Sys_MaskPrintf (SYS_rua_obj, "    protocols: %x\n", class->protocols);

		Sys_MaskPrintf (SYS_rua_obj, "    class methods: %x\n", meta->methods);
		Sys_MaskPrintf (SYS_rua_obj, "    instance variables: %d @ %x\n",
						meta->instance_size,
						meta->ivars);
		if (developer & SYS_rua_obj)
			dump_ivars (probj, meta->ivars);

		class->subclass_list = 0;

		Hash_Add (probj->classes, class);

		obj_register_selectors_from_class (probj, class);
		obj_register_selectors_from_class (probj, meta);

		if (class->protocols) {
			pr_protocol_list_t *protocol_list;
			protocol_list = &G_STRUCT (pr, pr_protocol_list_t,
									   class->protocols);
			obj_init_protocols (probj, protocol_list);
		}

		if (class->super_class && !Hash_Find (probj->classes, super_class))
			probj->unresolved_classes = list_cons (probj, class,
												   probj->unresolved_classes);
	}

	for (i = 0; i < symtab->cat_def_cnt; i++, ptr++) {
		pr_category_t *category = &G_STRUCT (pr, pr_category_t, *ptr);
		const char *class_name = PR_GetString (pr, category->class_name);
		pr_class_t *class = Hash_Find (probj->classes, class_name);

		Sys_MaskPrintf (SYS_rua_obj, "Category %s (%s) @ %x\n",
						PR_GetString (pr, category->class_name),
						PR_GetString (pr, category->category_name), *ptr);
		Sys_MaskPrintf (SYS_rua_obj, "    instance methods: %x\n",
						category->instance_methods);
		Sys_MaskPrintf (SYS_rua_obj, "    class methods: %x\n",
						category->class_methods);
		Sys_MaskPrintf (SYS_rua_obj, "    protocols: %x\n",
						category->protocols);

		if (class) {
			finish_category (probj, category, class);
		} else {
			probj->unclaimed_categories
				= list_cons (probj, category, probj->unclaimed_categories);
		}
	}

	if (*ptr) {
		Sys_MaskPrintf (SYS_rua_obj, "Static instances lists: %x\n", *ptr);
		probj->uninitialized_statics
			= list_cons (probj, &G_STRUCT (pr, pr_ptr_t, *ptr),
						 probj->uninitialized_statics);
	}
	if (probj->uninitialized_statics) {
		obj_init_statics (probj);
	}

	for (cell = &probj->unclaimed_categories; *cell; ) {
		pr_category_t *category = (*cell)->data;
		const char *class_name = PR_GetString (pr, category->class_name);
		pr_class_t *class = Hash_Find (probj->classes, class_name);

		if (class) {
			list_remove (probj, cell);
			finish_category (probj, category, class);
		} else {
			cell = &(*cell)->next;
		}
	}

	if (probj->unclaimed_proto_list
		&& Hash_Find (probj->classes, "Protocol")) {
		for (cell = &probj->unclaimed_proto_list; *cell; ) {
			obj_init_protocols (probj, (*cell)->data);
			list_remove (probj, cell);
		}
	}

	Sys_MaskPrintf (SYS_rua_obj, "Finished initializing %s module\n",
					PR_GetString (pr, module->name));
	obj_send_load (probj);
	Sys_MaskPrintf (SYS_rua_obj, "Leaving %s module init\n",
					PR_GetString (pr, module->name));
}

static void
rua___obj_forward (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *obj = &P_STRUCT (pr, pr_id_t, 0);
	pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 1);
	pr_sel_t   *fwd_sel = probj->forward_selector;
	pr_sel_t   *err_sel;
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, obj->class_pointer);
	pr_func_t   imp;

	if (!fwd_sel) {
		//FIXME sel_register_typed_name is really not the way to go about
		//looking for a selector by name
		fwd_sel = sel_register_typed_name (probj, "forward::", "", 0);
		probj->forward_selector = fwd_sel;
	}
	if (obj_reponds_to (probj, obj, fwd_sel)) {
		imp = get_imp (probj, class, fwd_sel);
		// forward:(SEL) sel :(@va_list) args
		// args is full param list as a va_list
		pr_string_t args_block = 0;
		int         argc;
		pr_type_t  *argv;
		if (pr->globals.stack) {
			argv = pr->pr_params[0];
			argc = probj->selector_argc[sel->sel_id];
			if (argc < 0) {
				// -ve values indicate varargs functions and is the ones
				// complement of the number of real parameters before the
				// ellipsis. However, Ruamoko ISA progs pass va_list through
				// ... so in the end, a -ve value indicates the total number
				// of arguments (including va_list) passed to the function.
				argc = -argc;
			}
		} else {
			size_t      parm_size = pr->pr_param_size * sizeof(pr_type_t);
			size_t      size = pr->pr_argc * parm_size;
			args_block = PR_AllocTempBlock (pr, size);

			argc = pr->pr_argc;
			argv = (pr_type_t *) PR_GetString (pr, args_block);
			// can't memcpy all params because 0 and 1 could be anywhere
			memcpy (argv + 0, &P_INT (pr, 0), 4 * sizeof (pr_type_t));
			memcpy (argv + 4, &P_INT (pr, 1), 4 * sizeof (pr_type_t));
			memcpy (argv + 8, &P_INT (pr, 2), (argc - 2) * parm_size);
		}

		RUA_CALL_BEGIN (pr, 4);
		P_POINTER (pr, 0) = PR_SetPointer (pr, obj);
		P_POINTER (pr, 1) = PR_SetPointer (pr, fwd_sel);
		P_POINTER (pr, 2) = PR_SetPointer (pr, sel);
		P_PACKED  (pr, pr_va_list_t, 3).count = argc;
		P_PACKED  (pr, pr_va_list_t, 3).list = PR_SetPointer (pr, argv);
		if (args_block) {
			PR_PushTempString (pr, args_block);
		}
		RUA_CALL_END (pr, imp);
		return;
	}
	err_sel = sel_register_typed_name (probj, "doesNotRecognize:", "", 0);
	if (obj_reponds_to (probj, obj, err_sel)) {
		RUA_CALL_BEGIN (pr, 3)
		P_POINTER (pr, 0) = PR_SetPointer (pr, obj);
		P_POINTER (pr, 1) = PR_SetPointer (pr, err_sel);
		P_POINTER (pr, 2) = PR_SetPointer (pr, sel);
		RUA_CALL_END (pr, get_imp (probj, class, err_sel))
		return;
	}

	dsprintf (probj->msg, "(%s) %s does not recognize %s",
			  PR_CLS_ISMETA (class) ? "class" : "instance",
			  PR_GetString (pr, class->name),
			  PR_GetString (pr, probj->selector_names[sel->sel_id]));

	err_sel = sel_register_typed_name (probj, "error:", "", 0);
	if (obj_reponds_to (probj, obj, err_sel)) {
		RUA_CALL_BEGIN (pr, 3)
		P_POINTER (pr, 0) = PR_SetPointer (pr, obj);
		P_POINTER (pr, 1) = PR_SetPointer (pr, err_sel);
		P_POINTER (pr, 2) = PR_SetTempString (pr, probj->msg->str);
		RUA_CALL_END (pr, get_imp (probj, class, err_sel))
		return;
	}

	PR_RunError (pr, "%s", probj->msg->str);
}

static void
rua___obj_responds_to (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *obj = &P_STRUCT (pr, pr_id_t, 0);
	pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 1);

	R_INT (pr) = obj_reponds_to (probj, obj, sel);
}

static void
rua_obj_error (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	int         code = P_INT (pr, 1);
	const char *fmt = P_GSTRING (pr, 2);
	int         count = pr->pr_argc - 3;
	pr_type_t **args = &pr->pr_params[3];

	obj_verror (probj, object, code, fmt, count, args);
}

static void
rua_obj_verror (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	int         code = P_INT (pr, 1);
	const char *fmt = P_GSTRING (pr, 2);
	pr_va_list_t *val = (pr_va_list_t *) pr->pr_params[3];
	pr_type_t  *params = &G_STRUCT (pr, pr_type_t, val->list);
	pr_type_t **args = alloca (val->count * sizeof (pr_type_t *));
	int         i;

	for (i = 0; i < val->count; i++)
		args[i] = params + i * pr->pr_param_size;
	obj_verror (probj, object, code, fmt, val->count, args);
}

static void
rua_obj_set_error_handler (progs_t *pr, void *data)
{
	//probj_t    *probj = pr->pr_objective_resources;
	//pr_func_t   func = P_INT (pr, 0);
	//arglist
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static const char *
rua_at_handler (progs_t *pr, pr_ptr_t at_param, void *_probj)
{
	probj_t    *probj = _probj;
	pr_id_t    *obj = &G_STRUCT (pr, pr_id_t, at_param);
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, obj->class_pointer);
	//FIXME sel_register_typed_name is really not the way to go about
	//looking for a selector by name
	pr_sel_t   *describe_sel=sel_register_typed_name (probj, "describe", "", 0);
	pr_func_t   imp = get_imp (probj, class, describe_sel);

	PR_PushFrame (pr);
	PR_RESET_PARAMS (pr);
	P_POINTER (pr, 0) = at_param;
	P_POINTER (pr, 1) = PR_SetPointer (pr, describe_sel);
	pr->pr_argc = 2;
	PR_ExecuteProgram (pr, imp);
	PR_PopFrame (pr);
	//FIXME the lifetime of the string may be a problem
	return PR_GetString (pr, R_STRING (pr));
}

static void
rua_obj_msg_lookup (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *receiver = &P_STRUCT (pr, pr_id_t, 0);
	pr_sel_t   *op = &P_STRUCT (pr, pr_sel_t, 1);

	R_INT (pr) = obj_msg_lookup (probj, receiver, op);
}

static void
rua_obj_msg_lookup_super (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_super_t *super = &P_STRUCT (pr, pr_super_t, 0);
	pr_sel_t   *_cmd = &P_STRUCT (pr, pr_sel_t, 1);

	R_INT (pr) = obj_msg_lookup_super (probj, super, _cmd);
}

static void
rua_obj_msg_sendv (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_ptr_t    obj = P_POINTER (pr, 0);
	pr_id_t    *receiver = &P_STRUCT (pr, pr_id_t, 0);
	pr_ptr_t    sel = P_POINTER (pr, 1);
	pr_sel_t   *op = &P_STRUCT (pr, pr_sel_t, 1);
	pr_func_t   imp = obj_msg_lookup (probj, receiver, op);

	__auto_type args = &P_PACKED (pr, pr_va_list_t, 2);
	int         count = args->count;
	pr_type_t  *params = G_GPOINTER (pr, args->list);

	if (count < 2 || count > PR_MAX_PARAMS) {
		PR_RunError (pr, "bad args count in obj_msg_sendv: %d", count);
	}
	if (pr_boundscheck) {
		PR_BoundsCheckSize (pr, args->list, count * pr->pr_param_size);
	}

	if (!imp) {
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, object_get_class_name (probj, receiver)),
					 PR_GetString (pr, probj->selector_names[op->sel_id]));
	}

	RUA_CALL_BEGIN (pr, count)
	// skip over the first two parameters because receiver and op will
	// replace them
	count -= 2;
	params += 2 * pr->pr_param_size;

	P_POINTER (pr, 0) = obj;
	P_POINTER (pr, 1) = sel;
	if (count) {
		memcpy (&P_INT (pr, 2), params,
				count * sizeof (pr_type_t) * pr->pr_param_size);
	}
	RUA_CALL_END (pr, imp)
}

#define RETAIN_COUNT(obj) PR_PTR (int, &(obj)[-1])

static void
rua_obj_increment_retaincount (progs_t *pr, void *data)
{
	pr_type_t  *obj = &P_STRUCT (pr, pr_type_t, 0);
	R_INT (pr) = ++RETAIN_COUNT (obj);
}

static void
rua_obj_decrement_retaincount (progs_t *pr, void *data)
{
	pr_type_t  *obj = &P_STRUCT (pr, pr_type_t, 0);
	R_INT (pr) = --RETAIN_COUNT (obj);
}

static void
rua_obj_get_retaincount (progs_t *pr, void *data)
{
	pr_type_t  *obj = &P_STRUCT (pr, pr_type_t, 0);
	R_INT (pr) = RETAIN_COUNT (obj);
}

static void
rua_obj_malloc (progs_t *pr, void *data)
{
	int         size = P_INT (pr, 0) * sizeof (pr_type_t);
	void       *mem = PR_Zone_Malloc (pr, size);

	RETURN_POINTER (pr, mem);
}

static void
rua_obj_atomic_malloc (progs_t *pr, void *data)
{
	int         size = P_INT (pr, 0) * sizeof (pr_type_t);
	void       *mem = PR_Zone_Malloc (pr, size);

	RETURN_POINTER (pr, mem);
}

static void
rua_obj_valloc (progs_t *pr, void *data)
{
	int         size = P_INT (pr, 0) * sizeof (pr_type_t);
	void       *mem = PR_Zone_Malloc (pr, size);

	RETURN_POINTER (pr, mem);
}

static void
rua_obj_realloc (progs_t *pr, void *data)
{
	void       *mem = (void*)P_GPOINTER (pr, 0);
	int         size = P_INT (pr, 1) * sizeof (pr_type_t);

	mem = PR_Zone_Realloc (pr, mem, size);
	RETURN_POINTER (pr, mem);
}

static void
rua_obj_calloc (progs_t *pr, void *data)
{
	int         size = P_INT (pr, 0) * P_INT (pr, 1) * sizeof (pr_type_t);
	void       *mem = PR_Zone_Malloc (pr, size);

	memset (mem, 0, size);
	RETURN_POINTER (pr, mem);
}

static void
rua_obj_free (progs_t *pr, void *data)
{
	void       *mem = (void*)P_GPOINTER (pr, 0);

	PR_Zone_Free (pr, mem);
}

static void
rua_obj_get_uninstalled_dtable (progs_t *pr, void *data)
{
	//probj_t    *probj = pr->pr_objective_resources;
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

static void
rua_obj_msgSend (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *self = &P_STRUCT (pr, pr_id_t, 0);
	pr_sel_t   *_cmd = &P_STRUCT (pr, pr_sel_t, 1);
	pr_func_t   imp;

	if (!self) {
		R_INT (pr) = P_INT (pr, 0);
		return;
	}
	if (P_UINT (pr, 0) >= pr->globals_size) {
		PR_RunError (pr, "invalid self: %x", P_UINT (pr, 0));
	}
	if (!_cmd)
		PR_RunError (pr, "null selector");
	imp = obj_msg_lookup (probj, self, _cmd);
	if (!imp)
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, object_get_class_name (probj, self)),
					 PR_GetString (pr, probj->selector_names[_cmd->sel_id]));

	PR_CallFunction (pr, imp, pr->pr_return);
}

static void
rua_obj_msgSend_super (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_super_t *super = &P_STRUCT (pr, pr_super_t, 0);
	pr_sel_t   *_cmd = &P_STRUCT (pr, pr_sel_t, 1);
	pr_func_t   imp;

	imp = obj_msg_lookup_super (probj, super, _cmd);
	if (!imp) {
		pr_id_t    *self = &G_STRUCT (pr, pr_id_t, super->self);
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, object_get_class_name (probj, self)),
					 PR_GetString (pr, probj->selector_names[_cmd->sel_id]));
	}
	if (pr->progs->version < PROG_VERSION) {
		pr->pr_params[0] = pr->pr_real_params[0];
	}
	P_POINTER (pr, 0) = super->self;
	PR_CallFunction (pr, imp, pr->pr_return);
}

static void
rua_obj_get_class (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	const char *name = P_GSTRING (pr, 0);
	pr_class_t *class;

	class = Hash_Find (probj->classes, name);
	if (!class)
		PR_RunError (pr, "could not find class %s", name);
	RETURN_POINTER (pr, class);
}

static void
rua_obj_lookup_class (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	const char *name = P_GSTRING (pr, 0);
	pr_class_t *class;

	class = Hash_Find (probj->classes, name);
	RETURN_POINTER (pr, class);
}

static void
rua_obj_next_class (progs_t *pr, void *data)
{
	//probj_t    *probj = pr->pr_objective_resources;
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

//====================================================================

static void
rua_sel_get_name (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 0);

	if (sel->sel_id > 0 && sel->sel_id <= probj->selector_index)
		R_STRING (pr) = probj->selector_names[sel->sel_id];
	else
		R_STRING (pr) = 0;
}

static void
rua_sel_get_type (progs_t *pr, void *data)
{
	pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 0);

	R_INT (pr) = sel->sel_types;
}

static void
rua_sel_get_uid (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	const char *name = P_GSTRING (pr, 0);

	RETURN_POINTER (pr, sel_register_typed_name (probj, name, "", 0));
}

static void
rua_sel_register_name (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	const char *name = P_GSTRING (pr, 0);

	RETURN_POINTER (pr, sel_register_typed_name (probj, name, "", 0));
}

static void
rua_sel_is_mapped (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	// FIXME might correspond to a string
	pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 0);
	R_INT (pr) = sel->sel_id > 0 && sel->sel_id <= probj->selector_index;
}

//====================================================================

static void
rua_class_get_class_method (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	pr_sel_t   *aSel = &P_STRUCT (pr, pr_sel_t, 1);
	pr_method_t *method;
	class = &G_STRUCT (pr, pr_class_t, class->class_pointer);
	method = obj_find_message (probj, class, aSel);
	RETURN_POINTER (pr, method);
}

static void
rua_class_get_instance_method (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	pr_sel_t   *aSel = &P_STRUCT (pr, pr_sel_t, 1);
	pr_method_t *method = obj_find_message (probj, class, aSel);
	RETURN_POINTER (pr, method);
}

#define CLASSOF(x) (&G_STRUCT (pr, pr_class_t, (x)->class_pointer))

static void
rua_class_pose_as (progs_t *pr, void *data)
{
	pr_class_t *impostor = &P_STRUCT (pr, pr_class_t, 0);
	pr_class_t *superclass = &P_STRUCT (pr, pr_class_t, 1);
	pr_ptr_t   *subclass;

	subclass = &superclass->subclass_list;
	while (*subclass) {
		pr_class_t *sub = &P_STRUCT (pr, pr_class_t, *subclass);
		pr_ptr_t    nextSub = sub->sibling_class;
		if (sub != impostor) {
			sub->sibling_class = impostor->subclass_list;
			sub->super_class = P_POINTER (pr, 0);	// impostor
			impostor->subclass_list = *subclass;	// sub
			CLASSOF (sub)->sibling_class = CLASSOF (impostor)->sibling_class;
			CLASSOF (sub)->super_class = impostor->class_pointer;
			CLASSOF (impostor)->subclass_list = sub->class_pointer;
		}
		*subclass = nextSub;
	}
	superclass->subclass_list = P_POINTER (pr, 0);	// impostor
	CLASSOF (superclass)->subclass_list = impostor->class_pointer;

	impostor->sibling_class = 0;
	CLASSOF (impostor)->sibling_class = 0;

	//XXX how much do I need to do here?!?
	//class_table_replace (super_class, impostor);
	R_INT (pr) = P_POINTER (pr, 0);	// impostor
}
#undef CLASSOF

static inline pr_id_t *
class_create_instance (progs_t *pr, pr_class_t *class)
{
	int         size = class->instance_size * sizeof (pr_type_t);
	pr_type_t  *mem;
	pr_id_t    *id = 0;

	mem = PR_Zone_TagMalloc (pr, size, class->name);
	if (mem) {
		memset (mem, 0, size);
		id = (pr_id_t *) mem;
		id->class_pointer = PR_SetPointer (pr, class);
	}
	return id;
}

static void
rua_class_create_instance (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	pr_id_t    *id = class_create_instance (pr, class);

	RETURN_POINTER (pr, id);
}

static void
rua_class_get_class_name (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->name
										: PR_SetString (pr, "Nil");
}

static void
rua_class_get_instance_size (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->instance_size : 0;
}

static void
rua_class_get_meta_class (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->class_pointer : 0;
}

static void
rua_class_get_super_class (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->super_class : 0;
}

static void
rua_class_get_version (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->version : -1;
}

static void
rua_class_is_class (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class);
}

static void
rua_class_is_meta_class (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISMETA (class);
}

static void
rua_class_set_version (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	if (PR_CLS_ISCLASS (class))
		class->version = P_INT (pr, 1);
}

static void
rua_class_get_gc_object_type (progs_t *pr, void *data)
{
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	R_INT (pr) = PR_CLS_ISCLASS (class) ? class->gc_object_type : 0;
}

static void
rua_class_ivar_set_gcinvisible (progs_t *pr, void *data)
{
	//probj_t    *probj = pr->pr_objective_resources;
	//pr_class_t *impostor = &P_STRUCT (pr, pr_class_t, 0);
	//const char *ivarname = P_GSTRING (pr, 1);
	//int         gcInvisible = P_INT (pr, 2);
	//XXX
	PR_RunError (pr, "%s, not implemented", __FUNCTION__);
}

//====================================================================

static void
rua_method_get_imp (progs_t *pr, void *data)
{
	pr_method_t *method = &P_STRUCT (pr, pr_method_t, 0);

	R_INT (pr) = method ? method->method_imp : 0;
}

static void
rua_get_imp (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	pr_sel_t   *sel = &P_STRUCT (pr, pr_sel_t, 1);

	R_INT (pr) = get_imp (probj, class, sel);
}

//====================================================================

static void
rua_object_dispose (progs_t *pr, void *data)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_type_t  *mem = (pr_type_t *) object;
	PR_Zone_Free (pr, mem);
}

static void
rua_object_copy (progs_t *pr, void *data)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_class_t *class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
	pr_id_t    *id;

	id = class_create_instance (pr, class);
	if (id) {
		memcpy (id, object, sizeof (pr_type_t) * class->instance_size);
		// copy the retain count
		((pr_type_t *) id)[-1] = ((pr_type_t *) object)[-1];
	}
	RETURN_POINTER (pr, id);
}

static void
rua_object_get_class (progs_t *pr, void *data)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);
	pr_class_t *class;

	if (object) {
		class = &G_STRUCT (pr, pr_class_t, object->class_pointer);
		if (PR_CLS_ISCLASS (class)) {
			RETURN_POINTER (pr, class);
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
rua_object_get_super_class (progs_t *pr, void *data)
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
rua_object_get_meta_class (progs_t *pr, void *data)
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
rua_object_get_class_name (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);

	R_STRING (pr) = object_get_class_name (probj, object);
}

static void
rua_object_is_class (progs_t *pr, void *data)
{
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);

	if (object) {
		R_INT (pr) = PR_CLS_ISCLASS ((pr_class_t*)object);
		return;
	}
	R_INT (pr) = 0;
}

static void
rua_object_is_instance (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *object = &P_STRUCT (pr, pr_id_t, 0);

	R_INT (pr) = object_is_instance (probj, object);
}

static void
rua_object_is_meta_class (progs_t *pr, void *data)
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
rua__i_Object__hash (progs_t *pr, void *data)
{
	R_INT (pr) = P_INT (pr, 0);
}

static void
rua__i_Object_error_error_ (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *self = &P_STRUCT (pr, pr_id_t, 0);
	const char *fmt = P_GSTRING (pr, 2);
	int         count = pr->pr_argc - 3;
	pr_type_t **args = &pr->pr_params[3];

	dsprintf (probj->msg, "error: %s (%s)\n%s",
			  PR_GetString (pr, object_get_class_name (probj, self)),
			  object_is_instance (probj, self) ? "instance" : "class", fmt);
	obj_verror (probj, self, 0, probj->msg->str, count, args);
}

static int
obj_protocol_conformsToProtocol (probj_t *probj, pr_protocol_t *proto,
								 pr_protocol_t *protocol)
{
	progs_t    *pr = probj->pr;

	pr_protocol_list_t *proto_list;

	if (!proto || !protocol) {
		return 0;
	}
	if (proto == protocol) {
		return 1;
	}
	if (proto->protocol_name == protocol->protocol_name
		|| strcmp (PR_GetString (pr, proto->protocol_name),
				   PR_GetString (pr, protocol->protocol_name)) == 0) {
		return 1;
	}
	proto_list = &G_STRUCT (pr, pr_protocol_list_t, proto->protocol_list);
	while (proto_list) {
		Sys_MaskPrintf (SYS_rua_obj, "%x %x %d\n",
						PR_SetPointer (pr, proto_list), proto_list->next,
						proto_list->count);
		for (int i = 0; i < proto_list->count; i++) {
			proto = &G_STRUCT (pr, pr_protocol_t, proto_list->list[i]);
			if (proto == protocol
				|| obj_protocol_conformsToProtocol (probj, proto, protocol)) {
				return 1;
			}
		}

		proto_list = &G_STRUCT (pr, pr_protocol_list_t, proto_list->next);
	}
	return 0;
}

static void
rua__c_Object__conformsToProtocol_ (progs_t *pr, void *data)
{
	probj_t    *probj = pr->pr_objective_resources;
	// class points to _OBJ_CLASS_foo, and class->class_pointer points to
	// _OBJ_METACLASS_foo
	pr_class_t *class = &P_STRUCT (pr, pr_class_t, 0);
	// protocol->class_pointer must point to _OBJ_CLASS_Protocol (ie, if
	// protocol is not actually a protocol, then the class cannot conform
	// to it)
	pr_protocol_t *protocol = &P_STRUCT (pr, pr_protocol_t, 2);
	pr_protocol_list_t *proto_list;

	if (!class || !protocol) {
		goto not_conforms;
	}
	if (protocol->class_pointer != PR_SetPointer (pr,
												  Hash_Find (probj->classes,
															 "Protocol"))) {
		goto not_conforms;
	}
	proto_list = &G_STRUCT (pr, pr_protocol_list_t, class->protocols);
	while (proto_list) {
		Sys_MaskPrintf (SYS_rua_obj, "%x %x %d\n",
						PR_SetPointer (pr, proto_list), proto_list->next,
						proto_list->count);
		for (int i = 0; i < proto_list->count; i++) {
			__auto_type proto = &G_STRUCT (pr, pr_protocol_t,
										   proto_list->list[i]);
			if (proto == protocol
				|| obj_protocol_conformsToProtocol (probj, proto, protocol)) {
				goto conforms;
			}
		}

		proto_list = &G_STRUCT (pr, pr_protocol_list_t, proto_list->next);
	}
not_conforms:
	Sys_MaskPrintf (SYS_rua_obj, "does not conform\n");
	R_INT (pr) = 0;
	return;
conforms:
	Sys_MaskPrintf (SYS_rua_obj, "conforms\n");
	R_INT (pr) = 1;
	return;
}

static void
rua_PR_FindGlobal (progs_t *pr, void *data)
{
	const char *name = P_GSTRING (pr, 0);
	pr_def_t   *def;

	R_POINTER (pr) = 0;
	def = PR_FindGlobal (pr, name);
	if (def)
		R_POINTER (pr) = def->ofs;
}

//====================================================================

#define bi(x,np,params...) {#x, rua_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t obj_methods [] = {
	bi(__obj_exec_class,               1, p(ptr)),
	bi(__obj_forward,                  -3, p(ptr), p(ptr)),
	bi(__obj_responds_to,              2, p(ptr), p(ptr)),

	bi(obj_error,                      -4, p(ptr), p(int), p(string)),
	bi(obj_verror,                     4, p(ptr), p(int), p(string), P(1, 2)),
	bi(obj_set_error_handler,          1, p(func)),
	bi(obj_msg_lookup,                 2, p(ptr), p(ptr)),
	bi(obj_msg_lookup_super,           2, p(ptr), p(ptr)),
	bi(obj_msg_sendv,                  3, p(ptr), p(ptr), P(1, 2)),
	bi(obj_increment_retaincount,      1, p(ptr)),
	bi(obj_decrement_retaincount,      1, p(ptr)),
	bi(obj_get_retaincount,            1, p(ptr)),
	bi(obj_malloc,                     1, p(int)),
	bi(obj_atomic_malloc,              1, p(int)),
	bi(obj_valloc,                     1, p(int)),
	bi(obj_realloc,                    2, p(ptr), p(int)),
	bi(obj_calloc,                     2, p(int), p(int)),
	bi(obj_free,                       1, p(ptr)),
	bi(obj_get_uninstalled_dtable,     0),
	bi(obj_msgSend,                    2, p(ptr), p(ptr)),//magic
	bi(obj_msgSend_super,              2, p(ptr), p(ptr)),//magic

	bi(obj_get_class,                  1, p(string)),
	bi(obj_lookup_class,               1, p(string)),
	bi(obj_next_class,                 1, p(ptr)),

	bi(sel_get_name,                   1, p(ptr)),
	bi(sel_get_type,                   1, p(ptr)),
	bi(sel_get_uid,                    1, p(string)),
	bi(sel_register_name,              1, p(string)),
	bi(sel_is_mapped,                  1, p(ptr)),

	bi(class_get_class_method,         2, p(ptr), p(ptr)),
	bi(class_get_instance_method,      2, p(ptr), p(ptr)),
	bi(class_pose_as,                  2, p(ptr), p(ptr)),
	bi(class_create_instance,          1, p(ptr)),
	bi(class_get_class_name,           1, p(ptr)),
	bi(class_get_instance_size,        1, p(ptr)),
	bi(class_get_meta_class,           1, p(ptr)),
	bi(class_get_super_class,          1, p(ptr)),
	bi(class_get_version,              1, p(ptr)),
	bi(class_is_class,                 1, p(ptr)),
	bi(class_is_meta_class,            1, p(ptr)),
	bi(class_set_version,              2, p(ptr), p(int)),
	bi(class_get_gc_object_type,       1, p(ptr)),
	bi(class_ivar_set_gcinvisible,     3, p(ptr), p(string), p(int)),

	bi(method_get_imp,                 1, p(ptr)),
	bi(get_imp,                        1, p(ptr), p(ptr)),

	bi(object_copy,                    1, p(ptr)),
	bi(object_dispose,                 1, p(ptr)),
	bi(object_get_class,               1, p(ptr)),
	bi(object_get_class_name,          1, p(ptr)),
	bi(object_get_meta_class,          1, p(ptr)),
	bi(object_get_super_class,         1, p(ptr)),
	bi(object_is_class,                1, p(ptr)),
	bi(object_is_instance,             1, p(ptr)),
	bi(object_is_meta_class,           1, p(ptr)),

	bi(_i_Object__hash,                2, p(ptr), p(ptr)),
	bi(_i_Object_error_error_,         -4, p(ptr), p(ptr), p(string)),
	bi(_c_Object__conformsToProtocol_, 3, p(ptr), p(ptr), p(ptr)),

	bi(PR_FindGlobal,                  1, p(string)),//FIXME
	{0}
};

static int
rua_init_finish (progs_t *pr)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_class_t **class_list, **class;

	class_list = (pr_class_t **) Hash_GetList (probj->classes);
	if (*class_list) {
		pr_class_t *object_class;
		pr_ptr_t    object_ptr;

		object_class = Hash_Find (probj->classes, "Object");
		if (object_class && !object_class->super_class)
			object_ptr = (pr_type_t *)object_class - pr->pr_globals;
		else
			PR_Error (pr, "root class Object not found");
		for (class = class_list; *class; class++)
			finish_class (probj, *class, object_ptr);
	}
	free (class_list);

	return 1;
}

static int
rua_obj_init_runtime (progs_t *pr)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_def_t   *def;
	dfunction_t *obj_forward;

	if ((def = PR_FindField (pr, ".this")))
		pr->fields.this = def->ofs;

	probj->obj_forward = 0;
	if ((obj_forward = PR_FindFunction (pr, "__obj_forward"))) {
		probj->obj_forward = (intptr_t) (obj_forward - pr->pr_functions);
	}

	PR_AddLoadFinishFunc (pr, rua_init_finish);
	return 1;
}

static void
rua_obj_cleanup (progs_t *pr, void *data)
{
	unsigned    i;

	__auto_type probj = (probj_t *) data;
	pr->pr_objective_resources = probj;

	Hash_FlushTable (probj->selector_hash);
	probj->selector_index = 0;
	probj->available_selectors = 0;
	probj->selector_block = 0;
	for (i = 0; i < probj->selector_index_max; i++) {
		obj_list_free (probj, probj->selector_sels[i]);
		probj->selector_sels[i] = 0;
		probj->selector_names[i] = 0;
	}

	for (dtable_t *dtable = probj->dtable_list; dtable;
		 dtable = dtable->next) {
		free (dtable->imp);
	}
	dtable_reset (probj);

	Hash_FlushTable (probj->classes);
	Hash_FlushTable (probj->protocols);
	Hash_FlushTable (probj->load_methods);
	probj->unresolved_classes = 0;
	probj->unclaimed_categories = 0;
	probj->unclaimed_proto_list = 0;
	probj->module_list = 0;
	probj->class_tree_list = 0;
}

static void
rua_obj_destroy (progs_t *pr, void *_res)
{
	probj_t    *probj = _res;

	dstring_delete (probj->msg);
	Hash_DelTable (probj->selector_hash);
	Hash_DelTable (probj->classes);
	Hash_DelTable (probj->protocols);
	Hash_DelTable (probj->load_methods);
}

void
RUA_Obj_Init (progs_t *pr, int secure)
{
	probj_t    *probj = calloc (1, sizeof (*probj));

	probj->pr = pr;
	probj->selector_hash = Hash_NewTable (1021, selector_get_key, 0, probj,
										  pr->hashctx);
	probj->classes = Hash_NewTable (1021, class_get_key, 0, probj, pr->hashctx);
	probj->protocols = Hash_NewTable (1021, protocol_get_key, 0, probj,
									  pr->hashctx);
	probj->load_methods = Hash_NewTable (1021, 0, 0, probj, pr->hashctx);
	probj->msg = dstring_newstr();
	Hash_SetHashCompare (probj->load_methods, load_methods_get_hash,
						 load_methods_compare);

	PR_Sprintf_SetAtHandler (pr, rua_at_handler, probj);

	PR_Resources_Register (pr, "RUA_ObjectiveQuakeC", probj, rua_obj_cleanup,
						   rua_obj_destroy);
	PR_RegisterBuiltins (pr, obj_methods, probj);

	PR_AddLoadFunc (pr, rua_obj_init_runtime);
}

pr_func_t
RUA_Obj_msg_lookup (progs_t *pr, pr_ptr_t _self, pr_ptr_t __cmd)
{
	probj_t    *probj = pr->pr_objective_resources;
	pr_id_t    *self = &G_STRUCT (pr, pr_id_t, _self);
	pr_sel_t   *_cmd = &G_STRUCT (pr, pr_sel_t, __cmd);
	pr_func_t   imp;

	if (!self)
		return 0;
	if (!_cmd)
		PR_RunError (pr, "null selector");
	imp = obj_msg_lookup (probj, self, _cmd);
	if (!imp)
		PR_RunError (pr, "%s does not respond to %s",
					 PR_GetString (pr, object_get_class_name (probj, self)),
					 PR_GetString (pr, probj->selector_names[_cmd->sel_id]));

	return imp;
}

int
RUA_obj_increment_retaincount (progs_t *pr, void *data)
{
	rua_obj_increment_retaincount (pr, data);
	return R_INT (pr);
}

int
RUA_obj_decrement_retaincount (progs_t *pr, void *data)
{
	rua_obj_decrement_retaincount (pr, data);
	return R_INT (pr);
}
