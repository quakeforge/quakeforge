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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>

#include "QF/cvar.h"
#include "QF/quakeio.h"
#include "QF/quakefs.h"
#include "QF/zone.h"
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
#include "QF/gib_regex.h"
#include "QF/gib_thread.h"
#include "regex.h"

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

dstring_t *
GIB_Return (const char *str)
{
	if (GIB_DATA(cbuf_active)->type != GIB_BUFFER_PROXY)
		return 0;
	dstring_clearstr (GIB_DATA(cbuf_active->up)->ret.retval);
	GIB_DATA(cbuf_active->up)->ret.available = true;
	if (!str)
		return GIB_DATA(cbuf_active->up)->ret.retval;
	else
		dstring_appendstr (GIB_DATA(cbuf_active->up)->ret.retval, str);
	return 0;
}

/*
	GIB Builtin functions
	
	See GIB docs for information.
*/
void
GIB_Function_f (void)
{
	if (GIB_Argc () != 3)
		GIB_USAGE ("name program");
	else
		GIB_Function_Define (GIB_Argv(1), GIB_Argv(2));
}			

void
GIB_Function_Get_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("name");
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
	int i;
	
	if (GIB_Argc () < 2)
		GIB_USAGE ("var1 [var2 var3 ...]");
	else
		for (i = 1; i < GIB_Argc(); i++)
			GIB_Var_Set_Local (cbuf_active, GIB_Argv(i), "");
}

void
GIB_Global_f (void)
{
	int i;
	
	if (GIB_Argc () < 2)
		GIB_USAGE ("var1 [var2 var3 ...]");
	else
		for (i = 1; i < GIB_Argc(); i++)
			GIB_Var_Set_Global (GIB_Argv(i), "");
}

void
GIB_Global_Delete_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("var");
	if (GIB_Var_Get_Global (GIB_Argv(1)))
		GIB_Var_Free_Global (GIB_Argv(1));
}

void
GIB_Return_f (void)
{
	cbuf_t *sp;
	
	if (GIB_Argc () > 2)
		GIB_USAGE ("[value]");
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
		  sp->up->interpreter != &gib_interp || // Not a GIB buffer
		  GIB_DATA(sp->up)->type != GIB_BUFFER_PROXY || // Not a proxy buffer
		  !sp->up->up ||  // Nothing above proxy buffer on the stack
		  sp->up->up->interpreter != &gib_interp || // Not a GIB buffer to return to
		  !GIB_DATA(sp->up->up)->ret.waiting) // Buffer doesn't want a return value
			Sys_Printf("GIB warning: unwanted return value discarded.\n"); // Not a serious error
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
	    	if (GIB_Argv(0)[2])
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
		GIB_USAGE ("condition program [else ...]");
}

void
GIB_While_f (void)
{
	if (GIB_Argc() != 3) {
		GIB_USAGE ("condition program");
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
GIB_Field_Get_f (void)
{
	unsigned int field;
	char *list, *end;
	const char *ifs;
	if (GIB_Argc() < 3 || GIB_Argc() > 4) {
		GIB_USAGE ("list element [fs]");
		return;
	}
	field = atoi (GIB_Argv(2));
	
	if (GIB_Argc() == 4)
		ifs = GIB_Argv (3);
	else if (!(ifs = GIB_Var_Get_Local (cbuf_active, "ifs")))
		ifs = " \t\n\r";
	if (!*ifs) {
		if (field < strlen(GIB_Argv(1))) {
			GIB_Argv(1)[field+1] = 0;
			GIB_Return (GIB_Argv(1)+field);
		} else
			GIB_Return ("");
		return;
	}
	for (list = GIB_Argv(1); *list && strchr(ifs, *list); list++);
	while (field) {
		while (!strchr(ifs, *list))
			list++;
		while (*list && strchr(ifs, *list))
			list++;
		if (!*list) {
			GIB_Return ("");
			return;
		}
		field--;
	}
	for (end = list; !strchr(ifs, *end); end++);
	*end = 0;
	GIB_Return (list);
}
		
		
void
GIB___For_f (void)
{
	char *end = 0, old = 0;
	if (!GIB_DATA(cbuf_active)->loop_list_p[0]) {
		Cbuf_InsertText (cbuf_active, "break;");
		return;
	}
	if (!GIB_DATA(cbuf_active)->loop_ifs_p[0]) {
		end = GIB_DATA(cbuf_active)->loop_list_p;
		old = end[1];
		end[1] = 0;
		GIB_Var_Set_Local (cbuf_active, GIB_DATA(cbuf_active)->loop_var_p, GIB_DATA(cbuf_active)->loop_list_p++);
		end[1] = old;
		return;
	}
	for (end = GIB_DATA(cbuf_active)->loop_list_p; !strchr(GIB_DATA(cbuf_active)->loop_ifs_p, *end); end++);
	if (*end) {
		old = *end;
		*end = 0;
	}
	GIB_Var_Set_Local (cbuf_active, GIB_DATA(cbuf_active)->loop_var_p, GIB_DATA(cbuf_active)->loop_list_p);
	if (old)
		*end = old;
	while (*end && strchr(GIB_DATA(cbuf_active)->loop_ifs_p, *end))
		end++;
	GIB_DATA(cbuf_active)->loop_list_p = end;
}

void
GIB_For_f (void)
{
	if (strcmp ("in", GIB_Argv (2)) ||
	   (GIB_Argc() == 7 && strcmp ("by", GIB_Argv(4))) ||
	   (GIB_Argc() != 5 && GIB_Argc() != 7))
		GIB_USAGE ("variable in list [by fs] program");
	else if (GIB_Argv (3)[0]) {
		char *ll;
		const char *ifs;
		cbuf_t *sub = Cbuf_New (&gib_interp);
		// Create loop buffer
		GIB_DATA(sub)->type = GIB_BUFFER_LOOP;
		GIB_DATA(sub)->locals = GIB_DATA(cbuf_active)->locals;
		GIB_DATA(sub)->loop_program = dstring_newstr ();
		GIB_DATA(sub)->loop_data = dstring_newstr ();
		if (cbuf_active->down)
			Cbuf_DeleteStack (cbuf_active->down);
		cbuf_active->down = sub;
		sub->up = cbuf_active;
		// Store all for-loop data in one big dstring (easy to clean up)
		dstring_appendstr (GIB_DATA(sub)->loop_data, GIB_Argv(3));
		dstring_append (GIB_DATA(sub)->loop_data, GIB_Argv(1), strlen(GIB_Argv(1))+1);
		if (GIB_Argc() == 7)
			ifs = GIB_Argv (5);
		else if (!(ifs = GIB_Var_Get_Local (cbuf_active, "ifs")))
			ifs = " \n\r\t";
		dstring_append (GIB_DATA(sub)->loop_data, ifs, strlen(ifs)+1);
		// Store pointers to data
		for (ll = GIB_DATA(sub)->loop_data->str; *ll && strchr (ifs, *ll); ll++);
		GIB_DATA(sub)->loop_list_p = ll; // List to iterate through
		GIB_DATA(sub)->loop_var_p = GIB_DATA(sub)->loop_data->str + strlen(GIB_Argv(3))+1; // Var to use
		GIB_DATA(sub)->loop_ifs_p = GIB_DATA(sub)->loop_var_p + strlen(GIB_Argv(1))+1; // Internal field separator
		dstring_appendstr (GIB_DATA(sub)->loop_program, "__for;");
		dstring_appendstr (GIB_DATA(sub)->loop_program, GIB_Argc() == 7 ? GIB_Argv (6) : GIB_Argv(4));
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
		GIB_DATA(cbuf_active)->type = GIB_BUFFER_PROXY; // If we set it to normal, locals will get double freed
		dstring_clearstr (cbuf_active->buf);
	}					
}

void
GIB_Continue_f (void)
{
	if (GIB_DATA(cbuf_active)->type != GIB_BUFFER_LOOP)
		Cbuf_Error ("syntax",
					"continue attempted outside of a loop"
					);
	else
		dstring_clearstr (cbuf_active->buf);
}

// Note: this is a standard console command, not a GIB builtin
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
GIB_Function_Export_f (void)
{
	gib_function_t *f;
	int i;
	
	if (GIB_Argc() < 2)
		GIB_USAGE ("function1 [function2 function3 ...]");
	for (i = 1; i < GIB_Argc(); i++) {
		if (!(f = GIB_Function_Find (GIB_Argv (i))))
			Cbuf_Error ("function", "function::export: function '%s' not found", GIB_Argv (i));
		else if (!f->exported) {
			Cmd_AddCommand (f->name->str, GIB_Runexported_f, "Exported GIB function.");
			f->exported = true;
		}
	}
}

void
GIB_String_Length_f (void)
{
	dstring_t *ret;
	if (GIB_Argc() != 2)
		GIB_USAGE ("string");
	else if ((ret = GIB_Return (0)))
		dsprintf (ret, "%i", (int) strlen(GIB_Argv(1)));
}

void
GIB_String_Equal_f (void)
{
	if (GIB_Argc() != 3)
		GIB_USAGE ("string1 string2");
	else if (strcmp(GIB_Argv(1), GIB_Argv(2)))
		GIB_Return ("0");
	else
		GIB_Return ("1");
}

void
GIB_String_Findsub_f (void)
{
	dstring_t *ret;
	char *haystack, *res;
	if (GIB_Argc() != 3) {
		GIB_USAGE ("string substr");
		return;
	}
	haystack = GIB_Argv(1);
	if ((res = strstr(haystack, GIB_Argv(2)))) {
		if ((ret = GIB_Return (0)))
		dsprintf (ret, "%lu", (unsigned long int)(res - haystack));
	} else
		GIB_Return ("-1");
}
	
void
GIB_Regex_Match_f (void)
{
	regex_t *reg;
	
	if (GIB_Argc() != 4) {
		GIB_USAGE ("string regex options");
		return;
	}
	
	if (!(reg = GIB_Regex_Compile (GIB_Argv(2), REG_EXTENDED | GIB_Regex_Translate_Options (GIB_Argv(3)))))
		Cbuf_Error ("regex", "%s: %s", GIB_Argv(0), GIB_Regex_Error ());
	else if (regexec(reg, GIB_Argv(1), 0, 0, 0))
		GIB_Return ("0");
	else
		GIB_Return ("1");
}

void
GIB_Regex_Replace_f (void)
{
	regex_t *reg;
	int ofs, len;
	regmatch_t match[10];
	
	if (GIB_Argc() != 5) {
		GIB_USAGE ("string regex options replacement");
		return;
	}
	
	ofs = 0;
	len = strlen (GIB_Argv(4));
	
	if (!(reg = GIB_Regex_Compile (GIB_Argv(2), REG_EXTENDED | GIB_Regex_Translate_Options (GIB_Argv(3)))))
		Cbuf_Error ("regex", "%s: %s", GIB_Argv(0), GIB_Regex_Error ());
	else if (strchr(GIB_Argv(3), 'g'))
		while (!regexec(reg, GIB_Argv(1)+ofs, 10, match, ofs > 0 ? REG_NOTBOL : 0) && match[0].rm_eo)
			ofs += GIB_Regex_Apply_Match (match, GIB_Argd(1), ofs, GIB_Argv(4));
	else if (!regexec(reg, GIB_Argv(1), 10, match, 0) && match[0].rm_eo)
		GIB_Regex_Apply_Match (match, GIB_Argd(1), 0, GIB_Argv(4));
	GIB_Return (GIB_Argv(1));
}

void
GIB_Regex_Extract_f (void)
{
	regex_t *reg;
	regmatch_t *match;
	dstring_t *ret;
	int i;
	char o;
	
	if (GIB_Argc() < 4) {
		GIB_USAGE ("string regex options [var1 var2 var3 ...]");
		return;
	}
	match = calloc (GIB_Argc() - 4, sizeof(regmatch_t));
	
	if (!(reg = GIB_Regex_Compile (GIB_Argv(2), REG_EXTENDED | GIB_Regex_Translate_Options (GIB_Argv(3)))))
		Cbuf_Error ("regex", "%s: %s", GIB_Argv(0), GIB_Regex_Error ());
	else if (!regexec(reg, GIB_Argv(1), GIB_Argc() - 4, match, 0) && match[0].rm_eo) {
		for (i = 0; i < GIB_Argc() - 4; i++) {
			if (match[i].rm_so != -1 && *GIB_Argv(i+4)) {
				o = GIB_Argv(1)[match[i].rm_eo];
				GIB_Argv(1)[match[i].rm_eo] = 0;
				GIB_Var_Set_Local (cbuf_active, GIB_Argv(i+4), GIB_Argv(1)+match[i].rm_so);
				GIB_Argv(1)[match[i].rm_eo] = o;
			}
		}
		if ((ret = GIB_Return (0)))
			dsprintf (ret, "%lu", (unsigned long) match[0].rm_eo);
	} else
		GIB_Return ("-1");
	free (match);
}

void
GIB_Thread_Create_f (void)
{
	dstring_t *ret;
	if (GIB_Argc() != 2)
		GIB_USAGE ("program");
	else {
		gib_thread_t *thread = GIB_Thread_New ();
		Cbuf_AddText (thread->cbuf, GIB_Argv(1));
		GIB_Thread_Add (thread);
		if ((ret = GIB_Return (0)))
			dsprintf (ret, "%lu", thread->id);
	}
}

void
GIB_Thread_Kill_f (void)
{
	if (GIB_Argc() != 2)
		GIB_USAGE ("id");
	else {
		gib_thread_t *thread;
		cbuf_t *cur;
		unsigned long int id = strtoul (GIB_Argv(1), 0, 10);
		thread = GIB_Thread_Find (id);
		if (!thread) {
			Cbuf_Error ("thread", "thread.kill: thread %lu does not exist.", id);
			return;
		}
		for (cur = thread->cbuf; cur; cur = cur->down) {
			// Kill all loops
			if (GIB_DATA(cur)->type == GIB_BUFFER_LOOP)
				GIB_DATA(cur)->type = GIB_BUFFER_PROXY; // Proxy to prevent shared locals being freed
			dstring_clearstr (cur->line);
			dstring_clearstr (cur->buf);
		}
	}
}

/* File access */

int (*GIB_File_Transform_Path) (dstring_t *path) = NULL;

int
GIB_File_Transform_Path_Null (dstring_t *path)
{
	char *s;
	
	// Convert backslash to forward slash
	for (s = strchr(path->str, '\\'); s; s = strchr (s, '\\'))
		*s = '/';
	return 0;
}

int
GIB_File_Transform_Path_Secure (dstring_t *path)
{
	char *s /* , e_dir[MAX_OSPATH] */;
	
	for (s = strchr(path->str, '\\'); s; s = strchr (s, '\\'))
		*s = '/';
	if (Sys_PathType (path->str) != PATHTYPE_RELATIVE_BELOW)
		return -1;
	/* Qexpand_squiggle (fs_userpath->string, e_dir); */
	dstring_insertstr (path, 0, "/");
	dstring_insertstr (path, 0, /* e_dir */ com_gamedir);
	return 0;
}
		
void
GIB_File_Read_f (void)
{
	QFile      *file;
	char       *path;
	int        len;
	dstring_t  *ret;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("file");
		return;
	}
	if (!*GIB_Argv (1)) {
		Cbuf_Error ("file",
		  "file::read: null filename provided");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access",
		  "file::read: access to %s denied", GIB_Argv(1));
		return;
	}
	if (!(ret = GIB_Return (0)))
		return;
	path = GIB_Argv (1);
	file = Qopen (path, "r");
	if (file) {
		len = Qfilesize (file);
		ret->size = len+1;
		dstring_adjust (ret);
		Qread (file, ret->str, len);
		ret->str[len] = 0;
		Qclose (file);
	}
	if (!file) {
		Cbuf_Error ("file",
		  "file::read: could not read %s: %s", path, strerror (errno));
		return;
	}
}

void
GIB_File_Write_f (void)
{
	QFile      *file;
	char       *path;
	
	if (GIB_Argc () != 3) {
		GIB_USAGE ("file data");
		return;
	}
	if (!*GIB_Argv(1)) {
		Cbuf_Error ("file",
		  "file::write: null filename provided");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access",
		  "file::write: access to %s denied", GIB_Argv(1));
		return;
	}
	path = GIB_Argv(1);
	if (!(file = Qopen (path, "w"))) {
		Cbuf_Error ("file",
		  "file::write: could not open %s for writing: %s", path, strerror (errno));
		return;
	}
	Qprintf (file, "%s", GIB_Argv (2));
	Qclose (file);
}

void
GIB_File_Find_f (void)
{
	DIR        *directory;
	struct dirent *entry;
	char       *path, *glob, *s;
	const char *ifs;
	dstring_t  *list;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("glob");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access", 
		  "file::find: access to %s denied", GIB_Argv(1));
		return;
	}
	path = GIB_Argv(1);
	s = strrchr (path, '/');
	if (!s) { // No slash in path
		glob = path; // The glob is the entire argument
		path = "."; // The path is the current directory
	} else if (s == path) // Unix filesystem root (carne only)
		path = "/";
	else {
		*s = 0; // Split the string at the final slash
		glob = s+1;
	}
	directory = opendir (path);
	if (!directory) {
		Cbuf_Error ("file",
		  "file.find: could not open directory %s: %s", path, strerror (errno));
		return;
	}
	if ((list = GIB_Return (0))) {
		if (!(ifs = GIB_Var_Get_Local (cbuf_active, "ifs")))
			ifs = "\n"; // Newlines don't appear in filenames and are part of the default ifs
		while ((entry = readdir (directory))) {
			if (strcmp (entry->d_name, ".") &&
			  strcmp (entry->d_name, "..") &&
			  !fnmatch (glob, entry->d_name, 0)) {
				dstring_appendsubstr (list, ifs, 1);
				dstring_appendstr (list, entry->d_name);
			}
		}
	}
	closedir (directory);
}

void
GIB_File_Move_f (void)
{
	char *path1, *path2;

	if (GIB_Argc () != 3) {
		GIB_USAGE ("from_file to_file");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access",
		  "file::move: access to %s denied", GIB_Argv(1));
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(2))) {
		Cbuf_Error ("access",
		  "file::move: access to %s denied", GIB_Argv(2));
		return;
	}
	path1 = GIB_Argv(1);
	path2 = GIB_Argv(2);
	if (Qrename (path1, path2))
		Cbuf_Error ("file",
		  "file::move: could not move %s to %s: %s",
	      path1, path2, strerror(errno));
}

void
GIB_File_Delete_f (void)
{
	char *path;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("file");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access",
		  "file::delete: access to %s denied", GIB_Argv(1));
		return;
	}
	path = GIB_Argv (1);
	if (Qremove(path))
		Cbuf_Error ("file",
		  "file::delete: could not delete %s: %s",
	      path, strerror(errno));
}

void
GIB_Range_f (void)
{
	double i, inc, start, limit;
	dstring_t *dstr;
	const char *ifs;
	if (GIB_Argc () < 3 || GIB_Argc () > 4) {
		GIB_USAGE ("lower upper [step]");
		return;
	}

	limit = atof(GIB_Argv(2));
	start = atof(GIB_Argv(1));
	if (GIB_Argc () == 4)
		inc = atof(GIB_Argv(3));
	else
		inc = limit < start ? -1.0 : 1.0;
	if (inc == 0.0) {
		GIB_Return ("");
		return;
	}
	if (!(ifs = GIB_Var_Get_Local (cbuf_active, "ifs")))
			ifs = " ";
	dstr = dstring_newstr ();
	for (i = atof(GIB_Argv(1)); inc < 0 ? i >= limit : i <= limit; i += inc)
		dasprintf(dstr, "%.1s%.10g", ifs, i);
	GIB_Return (dstr->str[0] ? dstr->str+1 : "");
	dstring_delete (dstr);
}

void
GIB_Print_f (void)
{
	if (GIB_Argc() != 2) {
		GIB_USAGE ("text");
		return;
	}
	Sys_Printf ("%s", GIB_Argv(1));
}

void
GIB_Builtin_Init (qboolean sandbox)
{
	gib_globals = Hash_NewTable (512, GIB_Var_Get_Key, GIB_Var_Free, 0);

	if (sandbox)
		GIB_File_Transform_Path = GIB_File_Transform_Path_Secure;
	else
		GIB_File_Transform_Path = GIB_File_Transform_Path_Null;
	
	GIB_Builtin_Add ("function", GIB_Function_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("function::get", GIB_Function_Get_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("function::export", GIB_Function_Export_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("local", GIB_Local_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("global", GIB_Global_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("global::delete", GIB_Global_Delete_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("return", GIB_Return_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("if", GIB_If_f, GIB_BUILTIN_FIRSTONLY);
	GIB_Builtin_Add ("ifnot", GIB_If_f, GIB_BUILTIN_FIRSTONLY);
	GIB_Builtin_Add ("while", GIB_While_f, GIB_BUILTIN_NOPROCESS);
	GIB_Builtin_Add ("field::get", GIB_Field_Get_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("for", GIB_For_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("__for", GIB___For_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("break", GIB_Break_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("continue", GIB_Continue_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("string::length", GIB_String_Length_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("string::equal", GIB_String_Equal_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("string::findsub", GIB_String_Findsub_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("regex::match", GIB_Regex_Match_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("regex::replace", GIB_Regex_Replace_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("regex::extract", GIB_Regex_Extract_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("thread::create", GIB_Thread_Create_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("thread::kill", GIB_Thread_Kill_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("file::read", GIB_File_Read_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("file::write", GIB_File_Write_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("file::find", GIB_File_Find_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("file::move", GIB_File_Move_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("file::delete", GIB_File_Delete_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("range", GIB_Range_f, GIB_BUILTIN_NORMAL);
	GIB_Builtin_Add ("print", GIB_Print_f, GIB_BUILTIN_NORMAL);
}
