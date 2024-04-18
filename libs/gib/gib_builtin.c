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
#include "QF/quakefs.h"
#include "QF/zone.h"
#include "QF/va.h"
#include "QF/sys.h"
#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/hash.h"
#include "QF/llist.h"
#include "QF/dstring.h"
#include "QF/gib.h"

#include "regex.h"
#include "gib_buffer.h"
#include "gib_parse.h"
#include "gib_function.h"
#include "gib_vars.h"
#include "gib_regex.h"
#include "gib_thread.h"
#include "gib_handle.h"
#include "gib_builtin.h"
#include "gib_classes.h"

static char _gib_null_string[] = "";
VISIBLE char * const gib_null_string = _gib_null_string;

hashtab_t  *gib_builtins;

/*
	Hashtable callbacks
*/
static const char *
GIB_Builtin_Get_Key (const void *ele, void *ptr)
{
	return ((gib_builtin_t *) ele)->name;
}
static void
GIB_Builtin_Free (void *ele, void *ptr)
{
	gib_builtin_t *b;

	b = (gib_builtin_t *) ele;
	free ((void *)b->name);
	free (b);
}

/*
	GIB_Builtin_Add

	Registers a new builtin GIB command.
*/

VISIBLE void
GIB_Builtin_Add (const char *name, void (*func) (void))
{
	gib_builtin_t *new;

	if (!gib_builtins)
		gib_builtins =
			Hash_NewTable (1024, GIB_Builtin_Get_Key, GIB_Builtin_Free, 0, 0);

	new = calloc (1, sizeof (gib_builtin_t));
	new->func = func;
	new->name = strdup (name);
	Hash_Add (gib_builtins, new);
}

VISIBLE void
GIB_Builtin_Remove (const char *name)
{
	gib_builtin_t *del;

	if ((del = Hash_Del (gib_builtins, name)))
		Hash_Free (gib_builtins, del);
}

VISIBLE bool
GIB_Builtin_Exists (const char *name)
{
	return Hash_Find (gib_builtins, name) ? true : false;
}

/*
	GIB_Builtin_Find

	Looks up the builtin name in the builtin hash,
	returning a pointer to the struct on success,
	zero otherwise.
*/
VISIBLE gib_builtin_t *
GIB_Builtin_Find (const char *name)
{
	if (!gib_builtins)
		return 0;
	return (gib_builtin_t *) Hash_Find (gib_builtins, name);
}

VISIBLE dstring_t *
GIB_Return (const char *str)
{
	dstring_t  *dstr;

	if (GIB_DATA (cbuf_active)->waitret) {
		dstr = GIB_Buffer_Dsarray_Get (cbuf_active);
		dstring_clearstr (dstr);
		if (!str)
			return dstr;
		else
			dstring_appendstr (dstr, str);
	}
	return 0;
}

VISIBLE void
GIB_Error (const char *type, const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	GIB_Buffer_Error (cbuf_active, type, fmt, args);
	va_end (args);
}

/*
	GIB Builtin functions

	See GIB docs for information.
*/
static void
GIB_Function_f (void)
{
	gib_tree_t *program;
	gib_function_t *func;
	int i;
	int argc = GIB_Argc ();

	if (argc < 3) {
		GIB_USAGE ("name [arg1 arg2 ...] program");
	} else {
		// Is the function program already tokenized?
		if (GIB_Argm (argc-1)->delim != '{') {
			// Parse on the fly
			if (!(program = GIB_Parse_Lines (GIB_Argv
							(argc-1), 0))) {
				// Error!
				GIB_Error ("ParseError", "Parse error while defining function '%s'.",
						   GIB_Argv (1));
				return;
			}
		} else
			program = GIB_Argm (argc-1)->children;
		func = GIB_Function_Define (GIB_Argv (1), GIB_Argv (argc-1), program,
							 GIB_DATA (cbuf_active)->script,
							 GIB_DATA (cbuf_active)->globals);
		llist_flush (func->arglist);
		for (i = 2; i < argc-1; i++)
			llist_append (func->arglist, strdup (GIB_Argv(i)));
		func->minargs = argc-2;
	}
}

static void
GIB_Function_Get_f (void)
{
	if (GIB_Argc () != 2) {
		GIB_USAGE ("name");
	} else {
		gib_function_t *f;

		if ((f = GIB_Function_Find (GIB_Argv (1))))
			GIB_Return (f->text->str);
		else
			GIB_Return ("");
	}
}

static void
GIB_Local_f (void)
{
	gib_var_t  *var;
	unsigned int index;
	int i;
	static hashtab_t  *zero = 0;

	if (GIB_Argc () < 2) {
		GIB_USAGE ("var [= value1 value2 ...] || var [var2 var3 ...]");
	} else if (!strcmp (GIB_Argv(2), "=")) {
		var = GIB_Var_Get_Complex (&GIB_DATA (cbuf_active)->locals, &zero,
								 GIB_Argv (1), &index, true);
		if (GIB_Argc () >= 3)
			GIB_Var_Assign (var, index, cbuf_active->args->argv + 3,
							GIB_Argc () - 3, GIB_Argv (1)[strlen (GIB_Argv(1)) - 1] != ']');
		if (GIB_CanReturn ())
			for (i = 3; i < GIB_Argc(); i++)
				GIB_Return (GIB_Argv(i));
	} else for (i = 1; i < GIB_Argc(); i++)
		GIB_Var_Get_Complex (&GIB_DATA (cbuf_active)->locals, &zero,
							 GIB_Argv (i), &index, true);
}


static void
GIB_Shared_f (void)
{
	gib_var_t  *var;
	unsigned int index;
	int i;
	static hashtab_t  *zero = 0;

	if (GIB_Argc () < 2) {
		GIB_USAGE ("var [= value1 value2 ...] || var [var2 var3 ...]");
	} else if (!strcmp (GIB_Argv(2), "=")) {
		var = GIB_Var_Get_Complex (&GIB_DATA (cbuf_active)->globals, &zero,
								 GIB_Argv (1), &index, true);
		if (GIB_Argc () >= 3)
			GIB_Var_Assign (var, index, cbuf_active->args->argv + 3,
							GIB_Argc () - 3, GIB_Argv (1)[strlen (GIB_Argv(1)) - 1] != ']');
		if (GIB_CanReturn ())
			for (i = 3; i < GIB_Argc(); i++)
				GIB_Return (GIB_Argv(i));
	} else for (i = 1; i < GIB_Argc(); i++)
		GIB_Var_Get_Complex (&GIB_DATA (cbuf_active)->globals, &zero,
							 GIB_Argv (i), &index, true);
}

static void
GIB_Delete_f (void)
{
	gib_var_t *var;
	unsigned int index;
	int i;
	hashtab_t *source;
	char *c;

	if (GIB_Argc () < 2) {
		GIB_USAGE ("var [var2 var2 ...]");
	} else for (i = 1; i < GIB_Argc(); i++) {
		if ((c = strrchr (GIB_Argv(i), '.'))) {
			*(c++) = 0;
			if (!(var = GIB_Var_Get_Complex (&GIB_DATA (cbuf_active)->locals,
								&GIB_DATA(cbuf_active)->globals,
								 GIB_Argv (i), &index, false)))
				continue;
			source = var->array[index].leaves;
		} else {
			c = GIB_Argv(i);
			source = GIB_DATA(cbuf_active)->globals;
		}
		Hash_Free (source, Hash_Del (source, c));
	}
}

static void
GIB_Domain_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("domain");
	else
		GIB_DATA (cbuf_active)->globals = GIB_Domain_Get (GIB_Argv (1));
}

static void
GIB_Domain_Clear_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("domain");
	else
		Hash_FlushTable (GIB_Domain_Get (GIB_Argv (2)));
}

static gib_tree_t fakeip = {0,0,0,0,0,0,0,0};

static void
GIB_Return_f (void)
{
	cbuf_t     *sp = cbuf_active->up;

	GIB_DATA (cbuf_active)->ip = &fakeip;

	if (GIB_DATA (cbuf_active)->reply.obj) {
		gib_buffer_data_t *g = GIB_DATA (cbuf_active);
		const char **argv = malloc (sizeof (char *) * GIB_Argc() -1);
		int i;

		for (i = 1; i < GIB_Argc(); i++)
			argv[i-1] = GIB_Argv(i);

		GIB_Reply (g->reply.obj, g->reply.mesg, GIB_Argc()-1, argv);
		free ((void*)argv);
		g->dnotify = NULL;
	} else if (GIB_Argc () > 1 && sp && sp->interpreter == &gib_interp
		&& GIB_DATA (sp)->waitret) {
		int i;
		dstring_t  *dstr;

		for (i = 1; i < GIB_Argc (); i++) {
			dstr = GIB_Buffer_Dsarray_Get (sp);
			dstring_clearstr (dstr);
			dstring_appendstr (dstr, GIB_Argv (i));
		}
	}
}

static void
GIB_For_f (void)
{
	dstring_t  *dstr;
	int i;

	GIB_Buffer_Push_Sstack (cbuf_active);
	dstr = GIB_Buffer_Dsarray_Get (cbuf_active);
	dstring_clearstr (dstr);
	dstring_appendstr (dstr, GIB_Argv (1));
	for (i = GIB_Argc () - 2; i > 2; i--) {
		dstr = GIB_Buffer_Dsarray_Get (cbuf_active);
		dstring_appendstr (dstr, GIB_Argv (i));
	}
}

// Note: this is a standard console command, not a GIB builtin
static void
GIB_Runexported_f (void)
{
	gib_function_t *f;
	const char **args;

	if (!(f = GIB_Function_Find (Cmd_Argv (0)))) {
		Sys_Printf ("Error:  No function found for exported command \"%s\".\n"
					"This is most likely a bug, please report it to"
					"The QuakeForge developers.", Cmd_Argv (0));
	} else {
		cbuf_t     *sub = Cbuf_PushStack (&gib_interp);
		int i;

		args = malloc (sizeof (char *) * Cmd_Argc());
		for (i = 0; i < Cmd_Argc(); i++)
			args[i] = Cmd_Argv(i);
		GIB_Function_Execute (sub, f, args, Cmd_Argc());
		free ((void*)args);
	}
}

static void
GIB_Function_Export_f (void)
{
	gib_function_t *f;
	int         i;

	if (GIB_Argc () < 2)
		GIB_USAGE ("function1 [function2 function3 ...]");
	for (i = 1; i < GIB_Argc (); i++) {
		if (!(f = GIB_Function_Find (GIB_Argv (i))))
			GIB_Error ("UnknownFunctionError", "%s: function '%s' not found.", GIB_Argv (0),
					   GIB_Argv (i));
		else if (!f->exported) {
			if (Cmd_Exists (f->name)) {
				GIB_Error ("NameConflictError",
						   "%s: A console command with the name '%s' already exists.",
						   GIB_Argv (0), GIB_Argv (i));
				return;
			} else {
				Cmd_AddCommand (f->name, GIB_Runexported_f,
								"Exported GIB function.");
				f->exported = true;
			}
		}
	}
}

static void
GIB_Length_f (void)
{
	dstring_t  *ret;

	if (GIB_Argc () != 2)
		GIB_USAGE ("string");
	else if ((ret = GIB_Return (0)))
		dsprintf (ret, "%i", (int) strlen (GIB_Argv (1)));
}

static void
GIB_Equal_f (void)
{
	if (GIB_Argc () != 3)
		GIB_USAGE ("string1 string2");
	else if (strcmp (GIB_Argv (1), GIB_Argv (2)))
		GIB_Return ("0");
	else
		GIB_Return ("1");
}

static void
GIB_Count_f (void)
{
	if (GIB_CanReturn())
		dsprintf (GIB_Return(0), "%u", GIB_Argc() - 1);
}


static void
GIB_Contains_f (void)
{
	int i;
	if (GIB_Argc () < 2)
		GIB_USAGE ("needle [straw1 straw2 ...]");
	else if (GIB_CanReturn ())
		for (i = 2; i < GIB_Argc(); i++)
			if (!strcmp(GIB_Argv(1), GIB_Argv(i))) {
				GIB_Return("1");
				return;
			}
	GIB_Return ("0");
}

static void
GIB_Slice_f (void)
{
	dstring_t  *ret;
	int         start, end, len;

	if (GIB_Argc () < 3 || GIB_Argc () > 4) {
		GIB_USAGE ("string start [end]");
	} else {
		len = strlen (GIB_Argv (1));
		start = atoi (GIB_Argv (2));
		end = *GIB_Argv (3) ? atoi (GIB_Argv (3)) : len;
		if (end < 0)
			end += len;
		else if (end > len)
			end = len;
		if (start < 0) {
			start += len;
			if (start < 0)
				start = 0;
		} else if (start >= len || start >= end)
			return;
		if ((ret = GIB_Return (0)))
			dstring_appendsubstr (ret, GIB_Argv (1) + start, end - start);
	}
}


static void
GIB_Slice_Find_f (void)
{
	char       *res;

	if (GIB_Argc () != 3) {
		GIB_USAGE ("haystack needle");
		return;
	} else if (!GIB_CanReturn ()) {
		return;
	} else if ((res = strstr (GIB_Argv (1), GIB_Argv (2)))) {
		dsprintf (GIB_Return (0), "%lu",
				  (unsigned long int) (res - GIB_Argv (1)));
		dsprintf (GIB_Return (0), "%lu",
				  (unsigned long int) (res - GIB_Argv (1) +
									   strlen (GIB_Argv (2))));
	}
}


static void
GIB_Split_f (void)
{
	char       *end, *start;
	const char *ifs;

	if (GIB_Argc () < 2 || GIB_Argc () > 3) {
		GIB_USAGE ("string [fs]");
		return;
	}

	ifs = GIB_Argc () == 3 ? GIB_Argv (2) : " \t\r\n";

	end = GIB_Argv (1);
	while (*end) {
		for (; strchr (ifs, *end); end++)
			if (!*end)
				return;
		start = end;
		while (!strchr (ifs, *end))
			end++;
		if (*end)
			*(end++) = 0;
		GIB_Return (start);
	}
}

static void
GIB_Chomp_f (void)
{
	char *str;
	const char *junk;
	unsigned int i;

	if (GIB_Argc () < 2 || GIB_Argc () > 3) {
		GIB_USAGE ("string [junk]");
		return;
	}

	str = GIB_Argv (1);
	if (GIB_Argc () == 2)
		junk = " \t\n\r";
	else
		junk = GIB_Argv (2);

	for (; *str && strchr (junk, *str); str++);
	for (i = strlen(str) - 1; i && strchr (junk, str[i]); i--);
	str[i+1] = 0;
	GIB_Return (str);
}

static void
GIB_Regex_Match_f (void)
{
	regex_t    *reg;

	if (GIB_Argc () != 4) {
		GIB_USAGE ("string regex options");
		return;
	}

	if (!(reg =  GIB_Regex_Compile (GIB_Argv (2), REG_EXTENDED |
					GIB_Regex_Translate_Options (GIB_Argv (3)))))
		GIB_Error ("RegexError", "%s: %s", GIB_Argv (0), GIB_Regex_Error ());
	else if (regexec (reg, GIB_Argv (1), 0, 0, GIB_Regex_Translate_Runtime_Options (GIB_Argv (3))))
		GIB_Return ("0");
	else
		GIB_Return ("1");
}

static void
GIB_Regex_Replace_f (void)
{
	regex_t    *reg;
	int         ofs;
	regmatch_t  match[10];

	if (GIB_Argc () != 5) {
		GIB_USAGE ("string regex options replacement");
		return;
	}

	ofs = 0;

	if (!
		(reg =
		 GIB_Regex_Compile (GIB_Argv (2),
							REG_EXTENDED |
							GIB_Regex_Translate_Options (GIB_Argv (3)))))
		GIB_Error ("RegexError", "%s: %s", GIB_Argv (0), GIB_Regex_Error ());
	else if (strchr (GIB_Argv (3), 'g'))
		while (!regexec
			   (reg, GIB_Argv (1) + ofs, 10, match, ofs > 0 ? REG_NOTBOL : 0)
			   && match[0].rm_eo)
			ofs +=
				GIB_Regex_Apply_Match (match, GIB_Argd (1), ofs, GIB_Argv (4));
	else if (!regexec (reg, GIB_Argv (1), 10, match, GIB_Regex_Translate_Runtime_Options (GIB_Argv (3))) && match[0].rm_eo)
		GIB_Regex_Apply_Match (match, GIB_Argd (1), 0, GIB_Argv (4));
	GIB_Return (GIB_Argv (1));
}

static void
GIB_Regex_Extract_f (void)
{
	regex_t    *reg;
	regmatch_t *match;
	int         i;
	char        o;

	if (GIB_Argc () != 4) {
		GIB_USAGE ("string regex options");
		return;
	} else if (!GIB_CanReturn ())
		return;
	match = calloc (32, sizeof (regmatch_t));

	if (!
		(reg =
		 GIB_Regex_Compile (GIB_Argv (2),
							REG_EXTENDED |
							GIB_Regex_Translate_Options (GIB_Argv (3)))))
		GIB_Error ("RegexError", "%s: %s", GIB_Argv (0), GIB_Regex_Error ());
	else if (!regexec (reg, GIB_Argv (1), 32, match, GIB_Regex_Translate_Runtime_Options (GIB_Argv (3))) && match[0].rm_eo) {
		dsprintf (GIB_Return (0), "%lu", (unsigned long) match[0].rm_eo);
		for (i = 0; i < 32; i++) {
			if (match[i].rm_so != -1) {
				o = GIB_Argv (1)[match[i].rm_eo];
				GIB_Argv (1)[match[i].rm_eo] = 0;
				GIB_Return (GIB_Argv (1) + match[i].rm_so);
				GIB_Argv (1)[match[i].rm_eo] = o;
			}
		}
	}
	free (match);
}

static void
GIB_Text_White_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("text");
	else if (GIB_CanReturn ()) {
		unsigned int i;
		dstring_t *dstr;
		char *str;

		dstr = GIB_Return (0);
		dstring_appendstr (dstr, GIB_Argv(1));
		str = dstr->str;

		for (i = 0; i < dstr->size-1; i++)
			str[i] = str[i] & ~0x80;
	}
}

static void
GIB_Text_Brown_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("text");
	else if (GIB_CanReturn ()) {
		unsigned int i;
		dstring_t *dstr;
		char *str;

		dstr = GIB_Return (0);
		dstring_appendstr (dstr, GIB_Argv(1));
		str = dstr->str;

		for (i = 0; i < dstr->size-1; i++)
			str[i] = str[i] | 0x80;
	}
}

/*
static void
GIB_Text_To_Gold_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("text");
	else if (GIB_CanReturn ()) {
		dstring_t *dstr;
		char *str;

		dstr = GIB_Return (0);
		dstring_copystr (dstr, GIB_Argv(1));

		for (str = dstr->str; *str; str++) {
			switch (*str) {
*/

static void
GIB_Text_To_Decimal_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("text");
	else if (GIB_CanReturn ()) {
		char *str;

		for (str = GIB_Argv(1); *str; str++)
			dsprintf (GIB_Return (0), "%i", (int) *str);
	}
}

static void
GIB_Text_From_Decimal_f (void)
{
	if (GIB_Argc () < 2)
		GIB_USAGE ("num1 [...]");
	else if (GIB_CanReturn ()) {
		int i;
		dstring_t *dstr;
		char *str;

		dstr = GIB_Return (0);
		dstr->size = GIB_Argc();
		dstring_adjust (dstr);

		str = dstr->str;

		for (i = 1; i < GIB_Argc(); i++, str++)
			*str = (char) atoi (GIB_Argv(i));
		*str = 0;
	}
}

static void
GIB_Event_Register_f (void)
{
	gib_function_t *func;

	if (GIB_Argc () != 3)
		GIB_USAGE ("event function");
	else if (!(func = GIB_Function_Find (GIB_Argv (2))) && GIB_Argv (2)[0])
		GIB_Error ("UnknownFunctionError", "Function %s not found.", GIB_Argv (2));
	else if (GIB_Event_Register (GIB_Argv (1), func))
		GIB_Error ("UnknownEventError", "Event %s not found.", GIB_Argv (1));
}

/* File access */

static int   (*GIB_File_Transform_Path) (dstring_t * path) = NULL;

static int
GIB_File_Transform_Path_Null (dstring_t * path)
{
	char       *s;

	// Convert backslash to forward slash
	for (s = strchr (path->str, '\\'); s; s = strchr (s, '\\'))
		*s = '/';
	return 0;
}

static int
GIB_File_Transform_Path_Secure (dstring_t * path)
{
	char       *s;

	for (s = strchr (path->str, '\\'); s; s = strchr (s, '\\'))
		*s = '/';

	if (path->str[0] != '/')
		dstring_insertstr (path, 0, "/");
	dstring_insertstr (path, 0, qfs_gamedir->dir.def);
	dstring_insertstr (path, 0, "/");
	dstring_insertstr (path, 0, qfs_userpath);
	return 0;
}

static void
GIB_File_Read_f (void)
{
	QFile      *file;
	char       *path;
	int         len;
	dstring_t  *ret;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("file");
		return;
	}
	if (!*GIB_Argv (1)) {
		GIB_Error ("FileAccessError", "%s: null filename provided", GIB_Argv (0));
		return;
	}

	if (!(ret = GIB_Return (0)))
		return;
	path = GIB_Argv (1);
	file = QFS_FOpenFile (path);
	if (file) {
		len = Qfilesize (file);
		ret->size = len + 1;
		dstring_adjust (ret);
		Qread (file, ret->str, len);
		ret->str[len] = 0;
		Qclose (file);
	} else {
		GIB_Error ("FileAccessError",
				   "%s: could not read %s: %s", GIB_Argv (0), path,
				   strerror (errno));
		return;
	}
}

static void
GIB_File_Write_f (void)
{
	char       *path;

	if (GIB_Argc () != 3) {
		GIB_USAGE ("file data");
		return;
	}
	if (!*GIB_Argv (1)) {
		GIB_Error ("InvalidArgumentError", "%s: null filename provided", GIB_Argv (0));
		return;
	}

	path = GIB_Argv (1);
	QFS_WriteFile (va (0, "%s/%s", qfs_gamedir->dir.def, path),
				   GIB_Argv(2), GIB_Argd(2)->size-1);
}

static void
GIB_File_Find_f (void)
{
	DIR        *directory;
	struct dirent *entry;
	const char *path, *glob = 0;
	char       *s;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("glob");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd (1))) {
		GIB_Error ("FileAccessError",
				   "%s: access to %s denied", GIB_Argv (0), GIB_Argv (1));
		return;
	}
	path = GIB_Argv (1);
	s = strrchr (path, '/');
	if (!s) {							// No slash in path
		glob = path;					// The glob is the entire argument
		path = ".";						// The path is the current directory
	} else if (s == path)				// Unix filesystem root (carne only)
		path = "/";
	else {
		*s = 0;							// Split the string at the final slash
		glob = s + 1;
	}
	directory = opendir (path);
	if (!directory)
		return;
	while ((entry = readdir (directory)))
		if (strcmp (entry->d_name, ".") && strcmp (entry->d_name, "..")
			&& !fnmatch (glob, entry->d_name, 0))
			GIB_Return (entry->d_name);
	closedir (directory);
}

static void
GIB_File_Move_f (void)
{
	char       *path1, *path2;

	if (GIB_Argc () != 3) {
		GIB_USAGE ("from_file to_file");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd (1))) {
		GIB_Error ("FileAccessError",
				   "%s: access to %s denied", GIB_Argv (0), GIB_Argv (1));
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd (2))) {
		GIB_Error ("FileAccessError",
				   "%s: access to %s denied", GIB_Argv (0), GIB_Argv (2));
		return;
	}
	path1 = GIB_Argv (1);
	path2 = GIB_Argv (2);
	if (QFS_Rename (path1, path2))
		GIB_Error ("FileAccessError", "%s: could not move %s to %s: %s", GIB_Argv (0),
				   path1, path2, strerror (errno));
}

static void
GIB_File_Delete_f (void)
{
	char       *path;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("file");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd (1))) {
		GIB_Error ("FileAccessError",
				   "%s: access to %s denied", GIB_Argv (0), GIB_Argv (1));
		return;
	}
	path = GIB_Argv (1);
	if (QFS_Remove (path))
		GIB_Error ("FileAccessError", "%s: could not delete %s: %s", GIB_Argv (0), path,
				   strerror (errno));
}

static void
GIB_Range_f (void)
{
	double      i, inc, start, limit;
	dstring_t  *dstr;

	if (GIB_Argc () < 3 || GIB_Argc () > 4) {
		GIB_USAGE ("lower upper [step]");
		return;
	}
	limit = atof (GIB_Argv (2));
	start = atof (GIB_Argv (1));
	if (GIB_Argc () == 4) {
		if ((inc = atof (GIB_Argv (3))) == 0.0)
			return;
	} else
		inc = limit < start ? -1.0 : 1.0;
	for (i = atof (GIB_Argv (1)); inc < 0 ? i >= limit : i <= limit; i += inc) {
		if (!(dstr = GIB_Return (0)))
			return;
		dsprintf (dstr, "%.10g", i);
	}
}

static void
GIB_Print_f (void)
{
	if (GIB_Argc () != 2) {
		GIB_USAGE ("text");
		return;
	}
	Sys_Printf ("%s", GIB_Argv (1));
}

static void
GIB_Class_f (void)
{
	if (GIB_Object_Get (GIB_Argv(1))) {
		GIB_Error ("ClassRedefinitionError",
				"Class '%s' already exists", GIB_Argv(1));
	} else if (GIB_Argc () == 5)
		GIB_Classes_Build_Scripted (GIB_Argv(1), GIB_Argv(3),
				GIB_Argm (4)->children,
				GIB_DATA(cbuf_active)->script);
	else
		GIB_Classes_Build_Scripted (GIB_Argv(1), "Object",
				GIB_Argm (2)->children,
				GIB_DATA(cbuf_active)->script);
}

static void
GIB_Emit_f (void)
{
	if (GIB_Argc () < 2) {
		GIB_USAGE ("signal [arg1 arg2 ...]");
		return;
	} else if (!GIB_DATA(cbuf_active)->reply.obj) {
		GIB_Error ("InvalidContextError", "Cannot emit signal in this context.");
		return;
	} else {
		int i;
		const char **argv = malloc (GIB_Argc () - 1);

		for (i = 1; i < GIB_Argc (); i ++)
			argv[i-1] = GIB_Argv (1);

		GIB_Object_Signal_Emit (GIB_DATA(cbuf_active)->reply.obj,
				GIB_Argc () - 1, argv);

		free ((void*)argv);
	}
}

static void
GIB_Exists_f (void)
{
	if (GIB_Object_Get (GIB_Argv (1)))
		GIB_Return ("1");
	else
		GIB_Return ("0");
}

static void
GIB_Error_f (void)
{
	if (GIB_Argc() < 3) {
		GIB_USAGE ("error_type explanation");
		return;
	} else
		GIB_Error (GIB_Argv(1), "%s", GIB_Argv(2));
}

/*
static void
GIB_New_f (void)
{
	GIB_Object_t *classobj;
	if (GIB_Argc() < 2) {
		GIB_USAGE ("classname");
	} else if (
			   !(class = GIB_Object_Get(GIB_Argv(1)))
			|| classobj->class->classobj != classobj) {
		GIB_Error ("UnknownClassError", "Class '%s' does not exist",
				GIB_Argv(1));
	} else {
		GIB_Send (classobj,
*/

static void
GIB_bp1_f (void)
{
}

static void
GIB_bp2_f (void)
{
}

static void
GIB_bp3_f (void)
{
}

static void
GIB_bp4_f (void)
{
}

static void
gib_builtin_shutdown (void *data)
{
	Hash_DelTable (gib_builtins);
}

void
GIB_Builtin_Init (bool sandbox)
{
	qfZoneScoped (true);

	if (sandbox)
		GIB_File_Transform_Path = GIB_File_Transform_Path_Secure;
	else
		GIB_File_Transform_Path = GIB_File_Transform_Path_Null;

	GIB_Builtin_Add ("function", GIB_Function_f);
	GIB_Builtin_Add ("function::get", GIB_Function_Get_f);
	GIB_Builtin_Add ("function::export", GIB_Function_Export_f);
	GIB_Builtin_Add ("local", GIB_Local_f);
	GIB_Builtin_Add ("shared", GIB_Shared_f);
	GIB_Builtin_Add ("global", GIB_Shared_f);
	GIB_Builtin_Add ("delete", GIB_Delete_f);
	GIB_Builtin_Add ("domain", GIB_Domain_f);
	GIB_Builtin_Add ("domain::clear", GIB_Domain_Clear_f);
	GIB_Builtin_Add ("return", GIB_Return_f);
	GIB_Builtin_Add ("for", GIB_For_f);
	GIB_Builtin_Add ("length", GIB_Length_f);
	GIB_Builtin_Add ("equal", GIB_Equal_f);
	GIB_Builtin_Add ("count", GIB_Count_f);
	GIB_Builtin_Add ("contains", GIB_Contains_f);
	GIB_Builtin_Add ("slice", GIB_Slice_f);
	GIB_Builtin_Add ("slice::find", GIB_Slice_Find_f);
	GIB_Builtin_Add ("split", GIB_Split_f);
	GIB_Builtin_Add ("chomp", GIB_Chomp_f);
	GIB_Builtin_Add ("regex::match", GIB_Regex_Match_f);
	GIB_Builtin_Add ("regex::replace", GIB_Regex_Replace_f);
	GIB_Builtin_Add ("regex::extract", GIB_Regex_Extract_f);
	GIB_Builtin_Add ("text::toWhite", GIB_Text_White_f);
	GIB_Builtin_Add ("text::toBrown", GIB_Text_Brown_f);
	GIB_Builtin_Add ("text::toDecimal", GIB_Text_To_Decimal_f);
	GIB_Builtin_Add ("text::fromDecimal", GIB_Text_From_Decimal_f);
	GIB_Builtin_Add ("event::register", GIB_Event_Register_f);
	GIB_Builtin_Add ("file::read", GIB_File_Read_f);
	GIB_Builtin_Add ("file::write", GIB_File_Write_f);
	GIB_Builtin_Add ("file::find", GIB_File_Find_f);
	GIB_Builtin_Add ("file::move", GIB_File_Move_f);
	GIB_Builtin_Add ("file::delete", GIB_File_Delete_f);
	GIB_Builtin_Add ("range", GIB_Range_f);
	GIB_Builtin_Add ("print", GIB_Print_f);
	GIB_Builtin_Add ("class", GIB_Class_f);
	GIB_Builtin_Add ("emit", GIB_Emit_f);
	GIB_Builtin_Add ("exists", GIB_Exists_f);
	GIB_Builtin_Add ("error", GIB_Error_f);
	GIB_Builtin_Add ("bp1", GIB_bp1_f);
	GIB_Builtin_Add ("bp2", GIB_bp2_f);
	GIB_Builtin_Add ("bp3", GIB_bp3_f);
	GIB_Builtin_Add ("bp4", GIB_bp4_f);

	Sys_RegisterShutdown (gib_builtin_shutdown, 0);
}
