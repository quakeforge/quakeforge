/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include <stdlib.h>
#include <string.h>

#include "QF/va.h"
#include "QF/sys.h"
#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/hash.h"
#include "QF/dstring.h"
#include "QF/gib_parse.h"
#include "QF/gib_builtin.h"
#include "QF/gib_buffer.h"
#include "QF/gib_function.h"
#include "QF/gib_vars.h"
#include "QF/gib_thread.h"

hashtab_t *gib_builtins;

/*
	Hashtable callbacks
*/
const char * 
GIB_Builtin_Get_Key (void *ele, void *ptr)
{
	return ((gib_builtin_t *)ele)->name->str;
}
void
GIB_Builtin_Free (void *ele, void *ptr)
{
	gib_builtin_t *b;
	b = (gib_builtin_t *) ele;
	dstring_delete (b->name);
	free (b);
}

/*
	GIB_Builtin_Add
	
	Registers a new builtin GIB command.
*/
void
GIB_Builtin_Add (const char *name, void (*func) (void), enum gib_builtin_type_e type)
{
	gib_builtin_t *new;
	
	if (!gib_builtins)
		gib_builtins = Hash_NewTable (1024, GIB_Builtin_Get_Key, GIB_Builtin_Free, 0);
	
	new = calloc (1, sizeof (gib_builtin_t));
	new->func = func;
	new->name = dstring_newstr();
	new->type = type;
	dstring_appendstr (new->name, name);
	Hash_Add (gib_builtins, new);
}

/*
	GIB_Builtin_Find
	
	Looks up the builtin name in the builtin hash,
	returning a pointer to the struct on success,
	zero otherwise.
*/
gib_builtin_t *
GIB_Builtin_Find (const char *name)
{
	if (!gib_builtins)
		return 0;
	return (gib_builtin_t *) Hash_Find (gib_builtins, name);
}

/*
	GIB_Argc
	
	Returns the number of arguments available
	in the current buffer.
*/
unsigned int
GIB_Argc (void)
{
	return cbuf_active->args->argc;
}

/*
	Returns a specific argument in the current
	buffer.
*/
const char *
GIB_Argv (unsigned int arg)
{
	if (arg < cbuf_active->args->argc)
		return cbuf_active->args->argv[arg]->str;
	else
		return "";
}

/*
	GIB_Args
	
	Returns a pointer to the composite command
	line starting at token arg.
*/
const char *
GIB_Args (unsigned int arg)
{
	if (arg < cbuf_active->args->argc)
		return cbuf_active->args->args[arg];
	else
		return "";
}

/*
	GIB_Arg_Strip_Delim
	
	Strips any wrapping characters off of the
	specified argument.  Useful for GIB_BUILTIN_NOPROCESS
	or GIB_BUILTIN_FIRSTONLY builtins.
*/
void
GIB_Arg_Strip_Delim (unsigned int arg)
{
	char *p = cbuf_active->args->argv[arg]->str;
	if (*p == '{' || *p == '\"') {
		dstring_snip (cbuf_active->args->argv[arg], 0, 1);
		p[strlen(p)-1] = 0;
	}
}

void
GIB_Return (const char *str)
{
	if (GIB_DATA(cbuf_active)->type != GIB_BUFFER_PROXY)
		return;
	dstring_clearstr (GIB_DATA(cbuf_active->up)->ret.retval);
	dstring_appendstr (GIB_DATA(cbuf_active->up)->ret.retval, str);
	GIB_DATA(cbuf_active->up)->ret.available = true;
}

/*
	GIB Builtin functions
	
	See GIB docs for information.
*/
void
GIB_Function_f (void)
{
	if (GIB_Argc () != 3)
		Cbuf_Error ("syntax", 
					"function: invalid syntax\n"
					"usage: function function_name {program}");
	else
		GIB_Function_Define (GIB_Argv(1), GIB_Argv(2));
}			

void
GIB_FunctionDotGet_f (void)
{
	if (GIB_Argc () != 2)
		Cbuf_Error ("syntax",
					"function.get: invalid syntax\n"
					"usage: function.get function_name");
	else {
		gib_function_t *f;
		if ((f = GIB_Function_Find (GIB_Argv (1))))
			GIB_Return (f->program->str);
		else
			GIB_Return ("");
	}
}

void
GIB_Local_f (void)
{
	if (GIB_Argc () != 2)
		Cbuf_Error ("syntax",
					"lset: invalid syntax\n"
					"usage: local variable");
	else
		GIB_Var_Set_Local (cbuf_active, GIB_Argv(1), "");
}

void
GIB_Global_f (void)
{
	if (GIB_Argc () != 2)
		Cbuf_Error ("syntax",
					"global: invalid syntax\n"
					"usage: global variable");
	else {
		char *a = strdup (GIB_Argv(1));
		GIB_Var_Set_Global (a, "");
		free(a);
	}
}

void
GIB_Return_f (void)
{
	cbuf_t *sp;
	
	if (GIB_Argc () > 2)
		Cbuf_Error ("syntax",
					"return: invalid syntax\n"
					"usage: return <value>");
	else {
		sp = cbuf_active;
		while (sp->interpreter == &gib_interp && GIB_DATA(sp)->type == GIB_BUFFER_LOOP) { // Get out of loops
			GIB_DATA(sp)->type = GIB_BUFFER_PROXY;
			dstring_clearstr (sp->buf);
			dstring_clearstr (sp->line);
			sp = sp->up;
		}
		dstring_clearstr (sp->buf);
		dstring_clearstr (sp->line);
		if (GIB_Argc () == 1)
			return;
		if (!sp->up || // Nothing above us on the stack
		  GIB_DATA(sp->up)->type != GIB_BUFFER_PROXY || // No proxy buffer created
		  !sp->up->up ||  // Nothing above proxy buffer on the stack
		  sp->up->up->interpreter != &gib_interp || // Not a GIB buffer
		  !GIB_DATA(sp->up->up)->ret.waiting) // Buffer doesn't want a return value
			Sys_Printf("Warning: unwanted return value discarded.\n"); // Not a serious error
		else {
			dstring_clearstr (GIB_DATA(sp->up->up)->ret.retval);
			dstring_appendstr (GIB_DATA(sp->up->up)->ret.retval, GIB_Argv(1));
			GIB_DATA(sp->up->up)->ret.available = true;
		}
	}
}

void
GIB_If_f (void)
{
	int condition;
	if ((!strcmp (GIB_Argv (3), "else") && GIB_Argc() >= 5) || // if condition {program} else ...
	    (GIB_Argc() == 3)) { // if condition {program}
	    	condition = atoi(GIB_Argv(1));
	    	if (!strcmp (GIB_Argv(0), "ifnot"))
	    		condition = !condition;
	    	if (condition) {
		    	GIB_Arg_Strip_Delim (2);
	    		Cbuf_InsertText (cbuf_active, GIB_Argv(2));
	    	} else if (GIB_Argc() == 5) {
	    		GIB_Arg_Strip_Delim (4);
	    		Cbuf_InsertText (cbuf_active, GIB_Argv(4));
	    	} else if (GIB_Argc() > 5)
	    		Cbuf_InsertText (cbuf_active, GIB_Args (4));
	} else
		Cbuf_Error ("syntax",
					"if: invalid syntax\n"
					"usage: if condition {program} [else ...]"
					);
}

void
GIB_While_f (void)
{
	if (GIB_Argc() != 3) {
		Cbuf_Error ("syntax",
					"while: invalid syntax\n"
					"usage: while condition {program}"
					);
	} else {
		cbuf_t *sub = Cbuf_New (&gib_interp);
		GIB_DATA(sub)->type = GIB_BUFFER_LOOP;
		GIB_DATA(sub)->locals = GIB_DATA(cbuf_active)->locals;
		GIB_DATA(sub)->loop_program = dstring_newstr ();
		if (cbuf_active->down)
			Cbuf_DeleteStack (cbuf_active->down);
		cbuf_active->down = sub;
		sub->up = cbuf_active;
		GIB_Arg_Strip_Delim (2);
		dstring_appendstr (GIB_DATA(sub)->loop_program, va("ifnot %s break\n%s", GIB_Argv (1), GIB_Argv (2)));
		Cbuf_AddText (sub, GIB_DATA(sub)->loop_program->str);
		cbuf_active->state = CBUF_STATE_STACK;
	}
}

void
GIB_Break_f (void)
{
	if (GIB_DATA(cbuf_active)->type != GIB_BUFFER_LOOP)
		Cbuf_Error ("syntax",
					"break attempted outside of a loop"
					);
	else {
		GIB_DATA(cbuf_active)->type = GIB_BUFFER_PROXY; // If we set it to normal locals will get freed
		dstring_clearstr (cbuf_active->buf);
	}					
}

void
GIB_Runexported_f (void)
{
	gib_function_t *f;
	
	if (!(f = GIB_Function_Find (Cmd_Argv (0))))
		Sys_Printf ("Error:  No function found for exported command \"%s\".\n"
					"This is most likely a bug, please report it to"
					"The QuakeForge developers.", Cmd_Argv(0));
	else {
		cbuf_t *sub = Cbuf_New (&gib_interp);
		GIB_Function_Execute (sub, f, cbuf_active->args);
		cbuf_active->down = sub;
		sub->up = cbuf_active;
		cbuf_active->state = CBUF_STATE_STACK;
	}
}

void
GIB_Export_f (void)
{
	gib_function_t *f;
	
	if (GIB_Argc() != 2)
		Cbuf_Error ("syntax",
					"export: invalid syntax\n"
					"usage: export function");
	else if (!(f = GIB_Function_Find (GIB_Argv (1))))
		Cbuf_Error ("existance", "export: function '%s' not found", GIB_Argv (1));
	else if (!f->exported) {
		Cmd_AddCommand (f->name->str, GIB_Runexported_f, "Exported GIB function.");
		f->exported = true;
	}
}

void
GIB_String_Length_f (void)
{
	if (GIB_Argc() != 2)
		Cbuf_Error ("syntax",
	  	"string.length: invalid syntax\n"
		  "usage: string.length string");
	else
		GIB_Return (va("%i", strlen(GIB_Argv(1))));
}

void
GIB_String_Equal_f (void)
{
	if (GIB_Argc() != 3)
		Cbuf_Error ("syntax",
	  	"string.length: invalid syntax\n"
		  "usage: string.equal string1 string2");
	else
		GIB_Return (va("%i", !strcmp(GIB_Argv(1), GIB_Argv(2))));
}

void
GIB_Thread_Create_f (void)
{
	if (GIB_Argc() != 2)
		Cbuf_Error ("syntax",
	  	"thread.create: invalid syntax\n"
		  "usage: thread.create program");
	else {
		gib_thread_t *thread = GIB_Thread_New ();
		Cbuf_AddText (thread->cbuf, GIB_Argv(1));
		GIB_Thread_Add (thread);
		GIB_Return (va("%lu", thread->id));
	}
}

void
GIB_Thread_Kill_f (void)
{
	if (GIB_Argc() != 2)
		Cbuf_Error ("syntax",
	  	"thread.kill: invalid syntax\n"
		  "usage: thread.kill id");
	else {
		gib_thread_t *thread;
		cbuf_t *cur;
		unsigned long int id = strtoul (GIB_Argv(1), 0, 10);
		thread = GIB_Thread_Find (id);
		if (!thread) {
			Cbuf_Error ("thread", "thread.kill: thread %ul does not exist.", id);
			return;
		}
		for (cur = thread->cbuf; cur; cur = cur->down) {
			GIB_DATA(cur)->type = GIB_BUFFER_NORMAL;
			dstring_clearstr (cur->line);
			dstring_clearstr (cur->buf);
		}
	}
}
			
void
GIB_Builtin_Init (void)
{
	gib_globals = Hash_NewTable (512, GIB_Var_Get_Key, GIB_Var_Free, 0);

	GIB_Builtin_Add ("function", GIB_Function_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("function.get", GIB_FunctionDotGet_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("export", GIB_Export_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("local", GIB_Local_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("global", GIB_Global_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("return", GIB_Return_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("if", GIB_If_f, GIB_BUILTIN_FIRSTONLY);
	GIB_Builtin_Add ("ifnot", GIB_If_f, GIB_BUILTIN_FIRSTONLY);
	GIB_Builtin_Add ("while", GIB_While_f, GIB_BUILTIN_NOPROCESS);
	GIB_Builtin_Add ("break", GIB_Break_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("string.length", GIB_String_Length_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("string.equal", GIB_String_Equal_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("thread.create", GIB_Thread_Create_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("thread.kill", GIB_Thread_Kill_f, GIB_BUILTIN_NORMAL);
}
