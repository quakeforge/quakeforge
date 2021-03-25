/*
	gib_object.c

	GIB object functions

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
#include <ctype.h>

#include "QF/hash.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/gib.h"
#include "gib_object.h"
#include "gib_handle.h"
#include "gib_classes.h"

hashtab_t  *gib_classes;

/*
	Hashtable callbacks
*/
static const char *
GIB_Class_Get_Key (const void *ele, void *ptr)
{
	return ((gib_class_t *) ele)->name;
}

static void
GIB_Class_Free (void *ele, void *ptr)
{
	gib_class_t *b;

	b = (gib_class_t *) ele;
	free ((void *)b->name);
	free (b);
}

static const char *
GIB_Method_Get_Key (const void *ele, void *ptr)
{
	return ((gib_method_t *) ele)->name;
}

static void
GIB_Method_Free (void *ele, void *ptr)
{
	// FIXME: Do something here
}

static const char *
GIB_Signal_Get_Key (const void *ele, void *ptr)
{
	return ((gib_signal_t *) ele)->name;
}

static void
GIB_Signal_Free (void *ele, void *ptr)
{
	gib_signal_t *sig;
	gib_slot_t *slot;

	sig = (gib_signal_t *) ele;
	slot = llist_remove (llist_getnode (sig->receiver->slots,
				sig->slot));

	free ((void *)sig->name);
	free ((void *)slot->mesg);
	free (sig);
	free (slot);
}

// Linked list callbacks

static void
GIB_Slot_Free (void *ele, void *ptr)
{
	gib_signal_t *sig;
	gib_slot_t *slot;

	slot = (gib_slot_t *) ele;
	sig = Hash_DelElement (slot->sender->signals, slot->signal);

	free ((void *)sig->name);
	free ((void *)slot->mesg);
	free (sig);
	free (slot);
}

static hashtab_t *
GIB_Method_Build_Hash (gib_class_t *class, hashtab_t *inherited,
		gib_methodtab_t *methods)
{
	gib_methodtab_t *m;
	gib_method_t *method;
	hashtab_t *new = Hash_NewTable (1024, GIB_Method_Get_Key,
									GIB_Method_Free, 0, 0);

	for (m = methods; m->name; m++) {
		method = malloc (sizeof (gib_method_t));
		method->parent = inherited ? Hash_Find (inherited, m->name) : NULL;
		method->name = strdup (m->name);
		method->func = m->func;
		method->data = m->data;
		method->class = class;
		Hash_Add (new, method);
	}

	if (inherited) {
		void **list, **l;

		for (l = list = Hash_GetList (inherited); *l; l++)
			if (!Hash_Find (new, GIB_Method_Get_Key (*l, NULL)))
				Hash_Add (new, *l);
		free (list);
	}

	return new;
}

void
GIB_Class_Create (gib_classdesc_t *desc)
{
	static const char *init = "init";
	gib_class_t *parent = NULL, *class = calloc (1, sizeof (gib_class_t));

	if (desc->parentname && (parent = Hash_Find (gib_classes, desc->parentname))) {
		class->parent = parent;
		class->depth = parent->depth + 1;
		llist_append (parent->children, class);
	} else
		class->depth = 0;

	class->name = strdup (desc->name);

	class->construct = desc->construct;
	class->class_construct = desc->class_construct;
	class->destruct = desc->destruct;
	class->methods = GIB_Method_Build_Hash (class, parent ?
			parent->methods : NULL, desc->methods);
	class->class_methods = GIB_Method_Build_Hash (class, parent ?
			parent->class_methods : NULL, desc->class_methods);
	class->children = llist_new (NULL, NULL, NULL);

	Hash_Add (gib_classes, class);

	// Create a class object

	class->classobj = GIB_Object_Create (desc->name, true);
	GIB_Send (class->classobj, NULL, 1, &init, NULL, NULL);
}

/*
	GIB_Object_Create

	Creates a new object from a named class.
	Does not send object an initialization
	message!
*/

gib_object_t *
GIB_Object_Create (const char *classname, qboolean classobj)
{
	gib_class_t *temp, *class = Hash_Find (gib_classes, classname);
	gib_object_t *obj;
	int i;

	if (!class)
		return NULL;

	obj = calloc (1, sizeof (gib_object_t));
	obj->class = class;
	obj->data = malloc (sizeof (void *) * (class->depth+1));
	obj->methods = classobj ? class->class_methods : class->methods;
	obj->handle = classobj ? 0 : GIB_Handle_New (obj);
	obj->handstr = strdup (va (0, "%lu", obj->handle));
	obj->refs = 1;
	obj->signals = Hash_NewTable (128, GIB_Signal_Get_Key,
								  GIB_Signal_Free, NULL, 0);
	obj->slots = llist_new (GIB_Slot_Free, NULL, NULL);

	if (classobj) {
		for (temp = class, i = class->depth; temp; temp = temp->parent, i--)
			if (temp->class_construct)
				obj->data[i] = temp->class_construct (obj);
	} else {
		for (temp = class, i = class->depth; temp; temp = temp->parent, i--)
			if (temp->construct)
				obj->data[i] = temp->construct (obj);
	}
	return obj;
}

static void
GIB_Object_Finish_Destroy (int argc, const char **argv, void *data)
{
	gib_object_t *obj = (gib_object_t *) data;

	int i;
	gib_class_t *temp;

	for (temp = obj->class, i = obj->class->depth; temp; temp = temp->parent, i--)
		if (temp->destruct)
			temp->destruct (obj->data[i]);
	free (obj->data);
	GIB_Handle_Free (obj->handle);
	free ((void *) obj->handstr);
	Hash_DelTable (obj->signals);
	if (obj->vars)
		Hash_DelTable (obj->vars);
	llist_delete (obj->slots);
	free (obj);
}

/*
	GIB_Object_Destroy

	Sends an object a dispose message and then frees it upon reply.
*/

void
GIB_Object_Destroy (gib_object_t *obj)
{
	const static char *dispose = "dispose";
	GIB_Send (obj, NULL, 1, &dispose, GIB_Object_Finish_Destroy, obj);
}

void
GIB_Object_Incref (gib_object_t *obj)
{
	if (obj->refs > 0)
		obj->refs++;
}

void
GIB_Object_Decref (gib_object_t *obj)
{
	if (obj->refs > 0 && !--obj->refs)
		GIB_Object_Destroy (obj);
}

int
GIB_Send (gib_object_t *obj, gib_object_t *sender, int argc, const char
		**argv, gib_reply_handler reply, void *replydata)
{
	gib_message_t message;
	gib_method_t *method;

	if (!(method = Hash_Find (obj->methods, *argv)))
		return -1;

	message.argc = argc;
	message.argv = argv;
	message.reply = reply;
	message.replydata = replydata;

	if (reply)
		GIB_Object_Incref (obj);

	return method->func (obj, method, obj->data[method->class->depth],
			sender, message);
}

int
GIB_SendToMethod (gib_object_t *obj, gib_method_t *method, gib_object_t
		*sender, int argc, const char **argv, gib_reply_handler reply,
		void *replydata)
{
	gib_message_t message;

	message.argc = argc;
	message.argv = argv;
	message.reply = reply;
	message.replydata = replydata;

	if (reply)
		GIB_Object_Incref (obj);

	return method->func (obj, method, obj->data[method->class->depth],
			sender, message);
}

void
GIB_Reply (gib_object_t *obj, gib_message_t mesg, int argc, const char
		**argv)
{
	if (mesg.reply) {
		mesg.reply (argc, argv, mesg.replydata);
		GIB_Object_Decref (obj);
	}
}

gib_object_t *
GIB_Object_Get (const char *id)
{
	gib_class_t *class;

	if (isdigit ((byte) *id))
		return GIB_Handle_Get (atoi (id));
	else if ((class = Hash_Find (gib_classes, id)))
		return class->classobj;
	else
		return NULL;
}

void
GIB_Object_Signal_Slot_Pair (gib_object_t *sender, const char *signal,
		gib_object_t *receiver, const char *slot)
{
	gib_signal_t *si = malloc (sizeof (gib_signal_t));
	gib_slot_t *sl = malloc (sizeof (gib_slot_t));

	si->slot = sl;
	sl->signal = si;

	si->receiver = receiver;
	sl->sender = sender;

	si->name = strdup (signal);
	sl->mesg = strdup (slot);

	Hash_Add (sender->signals, si);
	llist_append (receiver->slots, sl);
}

void
GIB_Object_Signal_Slot_Destroy (gib_object_t *sender, const char *signal,
		gib_object_t *receiver, const char *slot)
{
	gib_signal_t **list, **cur;

	if ((list = (gib_signal_t **) Hash_FindList (sender->signals, signal))) {
		for (cur = list; *cur; cur++) {
			if ((*cur)->receiver == receiver && !strcmp
					((*cur)->slot->mesg, slot)) {
				Hash_Free (sender->signals, Hash_DelElement
						(sender->signals, *cur));
				break;
			}
		}
		free (list);
	}
}

void
GIB_Object_Signal_Emit (gib_object_t *sender, int argc, const char **argv)
{
	gib_signal_t **list, **cur;
	const char *old = *argv;

	if ((list = (gib_signal_t **) Hash_FindList (sender->signals, *argv))) {
		for (cur = list; *cur; cur++) {
			*argv = (*cur)->slot->mesg;
			GIB_Send ((*cur)->receiver, sender, argc, argv, NULL, NULL);
		}
		free (list);
	}
	*argv = old;
}

void
GIB_Object_Init (void)
{
	gib_classes = Hash_NewTable (1024, GIB_Class_Get_Key,
								 GIB_Class_Free, 0, 0);

	GIB_Classes_Init ();
}
