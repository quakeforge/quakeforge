/*
	gib_classes.c

	GIB built-in classes

	Copyright (C) 2003 Brian Koropoff

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

#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/gib.h"
#include "QF/va.h"
#include "QF/sys.h"
#include "QF/llist.h"
#include "gib_object.h"
#include "gib_thread.h"
#include "gib_classes.h"
#include "gib_tree.h"
#include "gib_vars.h"

/*
   Begin Object class

   The root of the GIB class hierarchy.  Responsible for retain/release
   reference counting, creating instances of an object, returning
   an object handle, and reporting object type.
*/

typedef struct baseobj_s {
	unsigned long int ref;
} baseobj_t;

static int
Object_Retain_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	baseobj_t *base = data;

	base->ref++;
	GIB_Object_Incref (obj);
	GIB_Reply (obj, mesg, 1, &obj->handstr);
	return 0;
}

static int
Object_Release_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	baseobj_t *base = data;
	if (base->ref) {
		base->ref--;
		GIB_Object_Decref (obj);
	}
	GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static int
Object_Init_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	GIB_Reply (obj, mesg, 1, &obj->handstr);
	return 0;
}

static int
Object_Class_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	GIB_Reply (obj, mesg, 1, &obj->class->name);
	return 0;
}

static int
Object_SuperClass_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	if (obj->class->parent)
		GIB_Reply (obj, mesg, 1, &obj->class->parent->name);
	else
		GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static int
Object_IsKindOf_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	gib_class_t *c;
	static const char *one = "1";
	static const char *zero = "0";

	if (mesg.argc < 2)
		return -1;

	for (c = obj->class; c; c = c->parent)
		if (!strcmp (mesg.argv[1], c->name)) {
			GIB_Reply (obj, mesg, 1, &one);
			return 0;
		}
	GIB_Reply (obj, mesg, 1, &zero);
	return 0;
}

static int
Object_Dispose_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	static const char *disposed = "disposed";
	GIB_Object_Signal_Emit (obj, 1, &disposed);
	GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static int
Object_SignalConnect_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	gib_object_t *other;

	if (mesg.argc >= 4 && (other = GIB_Object_Get (mesg.argv[2])))
		GIB_Object_Signal_Slot_Pair (obj, mesg.argv[1], other,
				mesg.argv[3]);
	GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static int
Object_SignalDisconnect_f (gib_object_t *obj, gib_method_t *method, void
		*data, gib_object_t *sender, gib_message_t mesg)
{
	gib_object_t *other;

	if (mesg.argc >= 4 && (other = GIB_Object_Get (mesg.argv[2])))
		GIB_Object_Signal_Slot_Destroy (obj, mesg.argv[1], other,
				mesg.argv[3]);
	GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static int
Object_Class_New_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	const char *old;
	gib_object_t *new;

	new = GIB_Object_Create (obj->class->name, false);
	old = mesg.argv[0];
	mesg.argv[0] = "init";
	GIB_Send (new, sender, mesg.argc, mesg.argv, mesg.reply, mesg.replydata);
	GIB_Object_Decref (obj);
	mesg.argv[0] = old;
	return 0;
}

static const char **g_occ_reply;
static unsigned int g_occ_i = 0;

static bool occ_iterator (gib_class_t *class, void *unused)
{
	g_occ_reply[g_occ_i++] = class->name;
	return false;
}

static int
Object_Class_Children_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	unsigned int size;

	g_occ_i = 0;

	size = llist_size (obj->class->children);
	if (size) {
		g_occ_reply = malloc (sizeof (char *) * size);
		llist_iterate (obj->class->children, LLIST_ICAST (occ_iterator));
		GIB_Reply (obj, mesg, size, g_occ_reply);
	} else
		GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static void *
Object_Construct (gib_object_t *obj)
{
	baseobj_t *base = malloc (sizeof (baseobj_t));

	base->ref = 1;

	return base;
}

static void
Object_Destruct (void *data)
{
	free (data);
	return;
}

static gib_methodtab_t Object_methods[] = {
	{"retain", Object_Retain_f, NULL},
	{"release", Object_Release_f, NULL},
	{"init", Object_Init_f, NULL},
	{"class", Object_Class_f, NULL},
	{"superClass", Object_SuperClass_f, NULL},
	{"isKindOf", Object_IsKindOf_f, NULL},
	{"dispose", Object_Dispose_f, NULL},
	{"signalConnect", Object_SignalConnect_f, NULL},
	{"signalDisconnect", Object_SignalDisconnect_f, NULL},
	{NULL, NULL, NULL}
};

static gib_methodtab_t Object_class_methods[] = {
	{"parent", Object_SuperClass_f, NULL},
	{"children", Object_Class_Children_f, NULL},
	{"new", Object_Class_New_f, NULL},
	{"signalConnect", Object_SignalConnect_f, NULL},
	{"signalDisconnect", Object_SignalDisconnect_f, NULL},
	{NULL, NULL, NULL}
};

static gib_classdesc_t Object_class = {
	"Object", NULL,
	Object_Construct, NULL,
	Object_Destruct,
	Object_methods, Object_class_methods
};

/* End Object class */



/*
   Begin Thread class

   Creates a front end to a separate GIB buffer that is executed every frame.
   The object is initialized with a GIB function and set of arguments to that
   function that gets executed immediately.  When the thread exits, the object
   gets sent a release message.  Causing the object to be destroyed before the
   thread exits will immediately terminate the thread.  The object can be
   retained so that it is not destroyed when the thread exits on its own; in
   this case, the object must be released manually later on, but otherwise
   will do nothing.
*/

typedef struct Thread_class_s {
	hashtab_t *methods;
} Thread_class_t;

typedef struct Thread_s {
	gib_object_t *obj;
	cbuf_t *thread;
	bool ended;
} Thread_t;

static int
Thread_Init_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	Thread_t *t = (Thread_t *) data;
	gib_function_t *f;

	if (mesg.argc < 2 || !(f = GIB_Function_Find (mesg.argv[1]))) {
		GIB_Object_Destroy (obj);
		return -1;
	} else {
		GIB_Function_Execute (t->thread, f, mesg.argv+1, mesg.argc-1);
		Cbuf_Execute_Stack (t->thread);
		return GIB_ForwardToSuper (mesg, obj, method);
	}
}

static void
Thread_Cbuf_Freed (cbuf_t *cbuf, void *data)
{
	Thread_t *t = (Thread_t *) data;
	static const char *release = "release";

	t->thread = NULL;
	t->ended = true;
	GIB_Send (t->obj, t->obj, 1, &release, NULL, NULL);
}

static void *
Thread_Construct (gib_object_t *obj)
{
	Thread_t *t = malloc (sizeof (Thread_t));
	t->thread = GIB_Thread_New ();
	GIB_DATA (t->thread)->dnotify = Thread_Cbuf_Freed;
	GIB_DATA (t->thread)->ddata = t;
	t->obj = obj;
	t->ended = false;

	return t;
}

static void
Thread_Destruct (void *data)
{
	Thread_t *t = (Thread_t *) data;

	if (!t->ended) {
		GIB_DATA (t->thread)->dnotify = NULL;
		GIB_Thread_Delete (t->thread);
	}
	free (t);
}

static gib_methodtab_t Thread_methods[] = {
	{"init", Thread_Init_f, NULL},
	{NULL, NULL, NULL}
};

static gib_methodtab_t Thread_class_methods[] = {
	{NULL, NULL, NULL}
};

static gib_classdesc_t Thread_class = {
	"Thread", "Object",
	Thread_Construct, NULL,
	Thread_Destruct,
	Thread_methods, Thread_class_methods
};

/*
   Object Hash class

   Stores references to objects in a Hash
*/

typedef struct ObjRef_s {
	const char *key;
	gib_object_t *obj;
} ObjRef_t;

typedef struct ObjectHash_s {
	hashtab_t *objects;
} ObjectHash_t;

static const char *
ObjRef_Get_Key (const void *ele, void *ptr)
{
	return ((ObjRef_t *) ele)->key;
}

static void
ObjRef_Free (void *ele, void *ptr)
{
	ObjRef_t *ref = ele;
	free ((void *)ref->key);
	GIB_Object_Decref (ref->obj);
	free (ref);
}

static void *
ObjectHash_Construct (gib_object_t *obj)
{
	ObjectHash_t *data = malloc (sizeof (ObjectHash_t));

	data->objects = Hash_NewTable (1024, ObjRef_Get_Key, ObjRef_Free, NULL, 0);

	return data;
}

static void
ObjectHash_Destruct (void *data)
{
	ObjectHash_t *objh = data;

	Hash_DelTable (objh->objects);
	free (objh);
}

static int
ObjectHash_Insert_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	ObjectHash_t *objh = data;
	gib_object_t *ins;
	ObjRef_t *new;
	int i;

	if (mesg.argc < 3)
		return -1;

	for (i = 2; i < mesg.argc; i++) {
		if ((ins = GIB_Object_Get (mesg.argv[i]))) {
			new = malloc (sizeof (ObjRef_t));
			new->key = strdup (mesg.argv[1]);
			new->obj = ins;
			GIB_Object_Incref (ins);
			Hash_Add (objh->objects, new);
		} else
			return -1;
	}

	GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static int
ObjectHash_Get_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	ObjectHash_t *objh = data;
	ObjRef_t **refs, **r;
	const char **reply;
	int i, len;

	if (mesg.argc < 2)
		return -1;

	if ((refs = (ObjRef_t **) Hash_FindList (objh->objects,
					mesg.argv[1]))) {
		for (r = refs, len = 0; *r; r++, len++);
		reply = malloc (sizeof (char *) * len);
		for (r = refs, i = 0; *r; r++, i++)
			reply[i] = (*r)->obj->handstr;
		GIB_Reply (obj, mesg, len, reply);
		free ((void*)reply);
	} else
		GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static int
ObjectHash_Remove_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	ObjectHash_t *objh = data;
	ObjRef_t **refs, **r;
	int i;

	if (mesg.argc < 2)
		return -1;

	if ((refs = (ObjRef_t **) Hash_FindList (objh->objects,
					mesg.argv[1]))) {
		if (mesg.argc == 2)
			for (r = refs; *r; r++) {
				Hash_DelElement (objh->objects, *r);
				Hash_Free (objh->objects, *r);
			}
		else
			for (r = refs; *r; r++)
				for (i = 2; i < mesg.argc; i++)
					if (!strcmp (mesg.argv[i],
								(*r)->obj->handstr))
						{
							Hash_DelElement
								(objh->objects,
								 *r);
							Hash_Free
								(objh->objects,
								 *r);
						}
	}
	GIB_Reply (obj, mesg, 0, NULL);
	return 0;
}

static gib_methodtab_t ObjectHash_methods[] = {
	{"insert", ObjectHash_Insert_f, NULL},
	{"get", ObjectHash_Get_f, NULL},
	{"remove", ObjectHash_Remove_f, NULL},
	{NULL, NULL, NULL}
};

static gib_methodtab_t ObjectHash_class_methods[] = {
	{NULL, NULL, NULL}
};

static gib_classdesc_t ObjectHash_class = {
	"ObjectHash", "Object",
	ObjectHash_Construct, NULL,
	ObjectHash_Destruct,
	ObjectHash_methods, ObjectHash_class_methods
};

/*
   Scripted object class factory

   These functions and structs are used to create a class that executes
   GIB functions in a new thread in response to messages.  This allows classes
   to be created in GIB itself.
*/

typedef struct Scrobj_s {
	hashtab_t *shared;
} Scrobj_t;

typedef struct Scrobj_method_s {
	gib_function_t *func;
} Scrobj_method_t;

static void *
Scrobj_Construct (gib_object_t *obj)
{
	//Scrobj_t *new = malloc (sizeof (Scrobj_t));
	//
	//new->shared = GIB_Var_Hash_New ();

	//return new;

	if (!obj->vars)
		obj->vars = GIB_Var_Hash_New ();

	return NULL;
}

static void *
Scrobj_Class_Construct (gib_object_t *obj)
{
	Scrobj_t *new = malloc (sizeof (Scrobj_t));

	new->shared = GIB_Domain_Get (obj->class->name);

	return new;
}

static void
Scrobj_Destruct (void *data)
{
	//Scrobj_t *s = (Scrobj_t *) data;

	//Hash_DelTable (s->shared);
	//free (s);
}

static void
Scrobj_Thread_Died (cbuf_t *thread, void *data)
{
	GIB_Reply (GIB_DATA(thread)->reply.obj, GIB_DATA(thread)->reply.mesg, 0, NULL);
}

static int
Scrobj_Method_f (gib_object_t *obj, gib_method_t *method, void *data,
		gib_object_t *sender, gib_message_t mesg)
{
	static char this[] = "this";
	static char send[] = "sender";
	unsigned int ind;
	cbuf_t *thread = GIB_Thread_New ();
	static hashtab_t *nhash = NULL;
	gib_var_t *var;

	if (GIB_Function_Execute (thread, ((Scrobj_method_t *)method->data)->func,
			mesg.argv, mesg.argc))
		return -1;

	GIB_DATA(thread)->dnotify = Scrobj_Thread_Died;
	GIB_DATA(thread)->reply.obj = obj;
	GIB_DATA(thread)->reply.method = method;
	GIB_DATA(thread)->reply.mesg = mesg;
	GIB_DATA(thread)->globals = obj->vars;
	var = GIB_Var_Get_Complex (&GIB_DATA(thread)->locals, &nhash, this,
			&ind, true);
	if (obj->handle)
		dsprintf (var->array[0].value, "%lu", obj->handle);
	else
		dstring_copystr (var->array[0].value, obj->class->name);
	var = GIB_Var_Get_Complex (&GIB_DATA(thread)->locals, &nhash, send,
			&ind, true);
	if (sender)
		dsprintf (var->array[0].value, "%lu", sender->handle);
	else
		dstring_copystr (var->array[0].value, "0");
	Cbuf_Execute_Stack (thread);

	return 0;
}

static void gcbs_mtabfree (void *mtab, void *unused)
{
	free (mtab);
}

static enum {CLASS, INSTANCE} g_gcbs_mode;
static const char *g_gcbs_name;

static const char *gcbs_fname (const char *str)
{
	if (g_gcbs_mode == INSTANCE)
		return va (0, "__%s_%s__", g_gcbs_name, str);
	else
		return va (0, "%s::%s", g_gcbs_name, str);
}

void
GIB_Classes_Build_Scripted (const char *name, const char *parentname,
		gib_tree_t *tree, gib_script_t *script)
{
	gib_tree_t *line;
	llist_t *methods, *cmethods;
	gib_methodtab_t *mtab, *cmtab;
	gib_classdesc_t desc;

	g_gcbs_mode = INSTANCE;
	g_gcbs_name = name;

	methods = llist_new (gcbs_mtabfree, NULL, NULL);
	cmethods = llist_new (gcbs_mtabfree, NULL, NULL);

	for (line = tree; line; line = line->next)
		switch (line->type) {
			case TREE_T_LABEL:
				if (!strcmp (line->str, "class"))
					g_gcbs_mode = CLASS;
				else if (!strcmp (line->str, "instance"))
					g_gcbs_mode = INSTANCE;
				break;
			case TREE_T_CMD:
				if (!strcmp (line->children->str,
							"function")) {
					gib_tree_t *cur, *last;
					gib_methodtab_t *new = malloc (sizeof
							(gib_methodtab_t));
					Scrobj_method_t *data = malloc (sizeof
							(Scrobj_method_t));
					for (last =
							line->children->next->next;
							last->next; last =
							last->next);
					data->func = GIB_Function_Define
						(gcbs_fname
						 (line->children->next->str),
						 last->str,
						 last->children,
						 script, NULL);
					llist_flush (data->func->arglist);
					data->func->minargs = 1;
					for (cur = line->children->next->next;
							cur != last; cur =
							cur->next) {
						llist_append
							(data->func->arglist,
							 strdup (cur->str));
						data->func->minargs++;
					}
					new->data = data;
					new->name = line->children->next->str;
					new->func = Scrobj_Method_f;
					if (g_gcbs_mode == INSTANCE)
						llist_append (methods, new);
					else
						llist_append (cmethods, new);
				}
				break;
			default:
				break;
		}

	llist_append (methods, calloc (1, sizeof (gib_methodtab_t)));
	llist_append (cmethods, calloc (1, sizeof (gib_methodtab_t)));

	mtab = llist_createarray (methods, sizeof (gib_methodtab_t));
	cmtab = llist_createarray (cmethods, sizeof (gib_methodtab_t));

	desc.name = name;
	desc.parentname = parentname;
	desc.construct = Scrobj_Construct;
	desc.class_construct = Scrobj_Class_Construct;
	desc.destruct = Scrobj_Destruct;
	desc.methods = mtab;
	desc.class_methods = cmtab;

	GIB_Class_Create (&desc);

	free (mtab);
	free (cmtab);
	llist_delete (methods);
	llist_delete (cmethods);
}

void
GIB_Classes_Init (void)
{
	qfZoneScoped (true);
	GIB_Class_Create (&Object_class);
	GIB_Class_Create (&Thread_class);
	GIB_Class_Create (&ObjectHash_class);
}
