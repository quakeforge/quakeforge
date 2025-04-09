/*
	gib.h

	GIB scripting language

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

#ifndef __QF_gib_h
#define __QF_gib_h

#include "QF/qtypes.h"

// Object interface

typedef void (*gib_reply_handler) (int argc, const char **argv, void *data);

typedef struct gib_signal_s {
	const char *name;
	struct gib_object_s *receiver;
	struct gib_slot_s *slot;
} gib_signal_t;

typedef struct gib_slot_s {
	const char *mesg;
	struct gib_object_s *sender;
	struct gib_signal_s *signal;
} gib_slot_t;

typedef struct gib_object_s {
	struct gib_class_s *class;
	struct hashtab_s *methods;
	void **data;
	unsigned long int handle, refs;
	struct hashtab_s *signals, *vars;
	struct llist_s *slots;
	const char *handstr;
} gib_object_t;

typedef struct gib_message_s {
	int argc;
	const char **argv;
	gib_reply_handler reply;
	void *replydata;
} gib_message_t;

typedef struct gib_method_s {
	const char *name;
	int (*func) (struct gib_object_s *obj, struct gib_method_s *method,
			void *data, gib_object_t *sender, gib_message_t message);
	struct gib_method_s *parent;
	struct gib_class_s *class;
	void *data;
	bool own;
} gib_method_t;

typedef int (*gib_message_handler) (gib_object_t *obj, gib_method_t *method,
		void *data, gib_object_t *sender, gib_message_t message);
typedef void * (*gib_obj_constructor) (gib_object_t *obj);
typedef void (*gib_obj_destructor) (void *data);

typedef struct gib_class_s {
	const char *name;
	struct hashtab_s *methods, *class_methods;
	gib_obj_constructor construct, class_construct;
	gib_obj_destructor destruct;
	unsigned int depth;
	struct gib_object_s *classobj;
	struct gib_class_s *parent;
	struct llist_s *children;
} gib_class_t;

typedef struct gib_methodtab_s {
	const char *name;
	gib_message_handler func;
	void *data;
} gib_methodtab_t;

typedef struct gib_classdesc_s {
	const char *name;
	const char *parentname;
	gib_obj_constructor construct, class_construct;
	gib_obj_destructor destruct;
	struct gib_methodtab_s *methods, *class_methods;
} gib_classdesc_t;

#define GIB_ForwardToSuper(mesg,obj,method) ((method)->parent->func ((obj), \
			(method)->parent, \
			(obj)->data[(method)->parent->class->depth], \
			(obj), (mesg)))

void GIB_Class_Create (gib_classdesc_t *desc);
gib_object_t *GIB_Object_Create (const char *classname, bool classobj);
void GIB_Object_Destroy (gib_object_t *obj);
void GIB_Object_Incref (gib_object_t *obj);
void GIB_Object_Decref (gib_object_t *obj);
int GIB_Send (gib_object_t *obj, gib_object_t *sender, int argc, const char **argv, gib_reply_handler reply, void *replydata);
int GIB_SendToMethod (gib_object_t *obj, gib_method_t *method, gib_object_t
		*sender, int argc, const char **argv, gib_reply_handler reply,
		void *replydata);
void GIB_Reply (gib_object_t *obj, gib_message_t mesg, int argc, const char
		**argv);
gib_object_t *GIB_Object_Get (const char *id);
void GIB_Object_Signal_Slot_Pair (gib_object_t *sender, const char *signal,
		gib_object_t *receiver, const char *slot);
void GIB_Object_Signal_Slot_Destroy (gib_object_t *sender, const char *signal,
		gib_object_t *receiver, const char *slot);
void GIB_Object_Signal_Emit (gib_object_t *sender, int argc, const char
		**argv);
void GIB_Object_Init (void);

// Buffer access (required to use GIB_Arg* macros)

#define GIB_DATA(buffer) ((gib_buffer_data_t *)(buffer->data))

typedef struct gib_script_s {
	const char *text, *file;
	unsigned int refs;
} gib_script_t;

struct cbuf_s;

typedef struct gib_buffer_data_s {
	struct gib_script_s *script;
	struct gib_tree_s *program, *ip;
	struct dstring_s *arg_composite;
	bool waitret;
	struct gib_sstack_s {
		struct gib_dsarray_s {
			struct dstring_s **dstrs;
			unsigned int realsize, size;
		} *values;
		unsigned int size, p;
	} stack;
	struct {
		struct gib_object_s *obj;
		struct gib_method_s *method;
		struct gib_message_s mesg;
	} reply;
	struct hashtab_s *locals; // Local variables
	struct hashtab_s *globals; // Current domain
	void (*dnotify) (struct cbuf_s *cbuf, void *data);
	void *ddata;
} gib_buffer_data_t;

// Builtin function interface

extern char * const gib_null_string;

#define GIB_Argc() (cbuf_active->args->argc)
#define GIB_Argv(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->argv[(x)]->str : gib_null_string)
#define GIB_Args(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->args[(x)] : gib_null_string)
#define GIB_Argd(x) ((x) < cbuf_active->args->argc ? cbuf_active->args->argv[(x)] : NULL)
#define GIB_Argm(x) ((x) < cbuf_active->args->argc ? (gib_tree_t *)cbuf_active->args->argm[(x)] : NULL)

#define GIB_USAGE(x) (GIB_Error ("SyntaxError", "%s: invalid syntax\nusage: %s %s", GIB_Argv(0), GIB_Argv(0), (x)))

#define GIB_CanReturn() (GIB_DATA(cbuf_active)->waitret)

struct dstring_s *GIB_Return (const char *str);
void GIB_Error (const char *type, const char *fmt, ...) __attribute__((format(PRINTF, 2, 3)));
void GIB_Builtin_Add (const char *name, void (*func) (void));
void GIB_Builtin_Remove (const char *name);
bool GIB_Builtin_Exists (const char *name);

// Event interface

typedef struct gib_event_s {
	const char *name;
	struct gib_function_s *func;
} gib_event_t;

gib_event_t *GIB_Event_New (const char *name);
void GIB_Event_Callback (gib_event_t *event, unsigned int argc, ...);

// Interpreter interface (for creating GIB cbufs)

struct cbuf_interpreter_s *GIB_Interpreter (void) __attribute__((const));

// Thread interface

void GIB_Thread_Execute (void);
unsigned int GIB_Thread_Count (void) __attribute__((pure));

// Init interface

void GIB_Init (bool sandbox);

// Handle interface

unsigned long int GIB_Handle_New (gib_object_t *data);
void GIB_Handle_Free (unsigned long int num);
gib_object_t *GIB_Handle_Get (unsigned long int num) __attribute__((pure));

#endif//__QF_gib_h
