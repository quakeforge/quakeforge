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
	const char *r;
	baseobj_t *base = data;
	
	base->ref++;
	GIB_Object_Incref (obj);
	r = va ("%lu", obj->handle);
	GIB_Reply (obj, mesg, 1, &r);
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
	const char *r;

	r = va ("%lu", obj->handle);
	GIB_Reply (obj, mesg, 1, &r);
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

gib_methodtab_t Object_methods[] = {
	{"retain", Object_Retain_f, NULL},
	{"release", Object_Release_f, NULL},
	{"init", Object_Init_f, NULL},
	{"class", Object_Class_f, NULL},
	{"dispose", Object_Dispose_f, NULL},
	{"signalConnect", Object_SignalConnect_f, NULL},
	{"signalDisconnect", Object_SignalDisconnect_f, NULL},
	{NULL, NULL, NULL}
};

gib_methodtab_t Object_class_methods[] = {
	{"new", Object_Class_New_f, NULL},
	{"signalConnect", Object_SignalConnect_f, NULL},
	{"signalDisconnect", Object_SignalDisconnect_f, NULL},
	{NULL, NULL, NULL}
};

gib_classdesc_t Object_class = {
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
	qboolean ended;
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

gib_methodtab_t Thread_methods[] = {
	{"init", Thread_Init_f, NULL},
	{NULL, NULL, NULL}
};

gib_methodtab_t Thread_class_methods[] = {
	{NULL, NULL, NULL}
};

gib_classdesc_t Thread_class = {
	"Thread", "Object",
	Thread_Construct, NULL,
	Thread_Destruct,
	Thread_methods, Thread_class_methods
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
	Scrobj_t *new = malloc (sizeof (Scrobj_t));
	
	new->shared = GIB_Var_Hash_New ();

	return new;
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
	Scrobj_t *s = (Scrobj_t *) data;

	Hash_DelTable (s->shared);
	free (s);
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
	
	
	GIB_DATA(thread)->dnotify = Scrobj_Thread_Died;
	GIB_DATA(thread)->reply.obj = obj;
	GIB_DATA(thread)->reply.method = method;
	GIB_DATA(thread)->reply.mesg = mesg;
	GIB_Function_Execute (thread, ((Scrobj_method_t *)method->data)->func,
			mesg.argv, mesg.argc);
	GIB_DATA(thread)->globals = ((Scrobj_t *)data)->shared;
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

void
GIB_Classes_Build_Scripted (const char *name, const char *parentname,
		gib_tree_t *tree, gib_script_t *script)
{
	gib_tree_t *line;
	llist_t *methods, *cmethods;
	gib_methodtab_t *mtab, *cmtab;
	gib_classdesc_t desc;
	enum {CLASS, INSTANCE} mode = INSTANCE;
	
	static void
	mtabfree (void *mtab, void *unused)
	{
		free (mtab);
	}

	static const char *
	fname (const char *str)
	{
		if (mode == INSTANCE)
			return va ("__%s_%s__", name, str);
		else
			return va ("%s::%s", name, str);
	}
	
	methods = llist_new (mtabfree, NULL, NULL);
	cmethods = llist_new (mtabfree, NULL, NULL);

	for (line = tree; line; line = line->next)
		switch (line->type) {
			case TREE_T_LABEL:
				if (!strcmp (line->str, "class"))
					mode = CLASS;
				else if (!strcmp (line->str, "instance"))
					mode = INSTANCE;
				break;
			case TREE_T_CMD:
				if (!strcmp (line->children->str,
							"function")) {
					gib_methodtab_t *new = malloc (sizeof
							(gib_methodtab_t));
					Scrobj_method_t *data = malloc (sizeof
							(Scrobj_method_t));
					data->func = GIB_Function_Define
						(fname
						 (line->children->next->str),
						 line->children->next->next->str,
						 line->children->next->next->children,
						 script, NULL);
					new->data = data;
					new->name = line->children->next->str;
					new->func = Scrobj_Method_f;
					if (mode == INSTANCE)
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
	GIB_Class_Create (&Object_class);
	GIB_Class_Create (&Thread_class);
}
