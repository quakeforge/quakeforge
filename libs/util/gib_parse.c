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

#include <ctype.h>
#include <string.h>

#include "QF/sys.h"
#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/gib_buffer.h"
#include "QF/gib_process.h"

/* Co-recursive parsing functions */

inline char
GIB_Parse_Match_Dquote (const char *str, unsigned int *i)
{
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '\"')
			return 0;
	}
	return '\"';
}

inline char
GIB_Parse_Match_Brace (const char *str, unsigned int *i)
{
	char c;
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '\"') {
			if ((c = GIB_Parse_Match_Dquote (str, i)))
				return c;
		}
		if (str[*i] == '{') {
			if ((c = GIB_Parse_Match_Brace (str, i)))
				return c;
		}
		if (str[*i] == '}')
				return 0;
	}
	return '{';
}

void
GIB_Parse_Extract_Line (struct cbuf_s *cbuf)
{
	int i;
	char c;
	dstring_t *dstr = cbuf->buf;
	
	dstring_clearstr (cbuf->line);
	
	for (i = 0; dstr->str[i]; i++) {
		if (dstr->str[i] == '{') {
			if ((c = GIB_Parse_Match_Brace (dstr->str, &i))) {
				Cbuf_Error ("parse", "Could not find match for character %c", c);
				return;
			}
		}
		else if (dstr->str[i] == '\n' || dstr->str[i] == ';')
			break;
	}
	
	if (i) {
		dstring_insert (cbuf->line, dstr->str, i, 0);
		Sys_DPrintf ("extracted line: %s\n", cbuf->line->str);
		if (dstr->str[0])
			dstring_snip (dstr, 0, i);
	}
	else
		dstring_clearstr (dstr);
	return;

}

inline static char
GIB_Parse_Get_Token (const char *str, unsigned int *i, dstring_t *dstr)
{
	int n;
	char c;
	
	n = *i; // Save start position
	if (str[*i] == '\"') {
		if ((c = GIB_Parse_Match_Dquote (str, i))) {
			Cbuf_Error ("parse", "Could not find match for character %c", c);
			return 0; // Parse error
		} else {
			dstring_insert (dstr, str+n+1, *i-n-1, 0);
			return '\"';
		}
	} else if (str[*i] == '{') {
		if ((c = GIB_Parse_Match_Brace (str, i))) {
			Cbuf_Error ("parse", "Could not find match for character %c", c);
			return 0; // Parse error
		} else {
			dstring_insert (dstr, str+n+1, *i-n-1, 0);
			return '{';
		}
	} else {
		while (str[*i] && !isspace(str[*i]) && str[*i] != ',') // find end of token
			(*i)++;
		dstring_insert (dstr, str+n, *i-n, 0);
		return ' ';
	}
	return 0; // We should never get here
}

void
GIB_Parse_Generate_Composite (struct cbuf_s *cbuf)
{
	cbuf_args_t *args = cbuf->args;
	int i;
	
	dstring_clearstr (GIB_DATA (cbuf)->arg_composite);
	for (i = 0; i < args->argc; i++) {
		args->args[i] = (const char *) strlen (GIB_DATA (cbuf)->arg_composite->str);
		dstring_appendstr (GIB_DATA (cbuf)->arg_composite, args->argv[i]->str);
		dstring_appendstr (GIB_DATA (cbuf)->arg_composite, " ");
	}
	GIB_DATA (cbuf)->arg_composite->str[strlen(GIB_DATA (cbuf)->arg_composite->str)-1] = 0;
	for (i = 0; i < args->argc; i++)
		args->args[i] += (unsigned long int) GIB_DATA (cbuf)->arg_composite->str;
}		

void
GIB_Parse_Tokenize_Line (struct cbuf_s *cbuf)
{
	dstring_t *arg = dstring_newstr ();
	const char *str = cbuf->line->str;
	cbuf_args_t *args = cbuf->args;
	qboolean cat = false;
	char delim;
	int i;
	
	cbuf->args->argc = 0;
	
	for (i = 0;str[i];) {
		while (isspace(str[i])) // Eliminate whitespace
			i++;
		if (!str[i]) // Blank token
			break;
		if (str[i] == ',') { // Concatenation
			cat = true;
			i++;
			continue;
		}
		delim = GIB_Parse_Get_Token (str, &i, arg);
		if (!delim)
			break;
		Sys_DPrintf("Got token: %s\n", arg->str);
		if (*arg->str == '$' && delim == ' ')
			GIB_Process_Variable (arg);
		if (cat) {
			dstring_appendstr (args->argv[args->argc-1], arg->str);
			cat = false;
		} else
			Cbuf_ArgsAdd (args, arg->str);
		if (delim != ' ')
			i++;
		dstring_clearstr (arg);
	}
	dstring_delete (arg);
	GIB_Parse_Generate_Composite (cbuf);
}
