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
#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/gib_buffer.h"
#include "QF/gib_process.h"
#include "QF/gib_builtin.h"
#include "QF/gib_function.h"

// Interpreter structure and prototypes

void GIB_Parse_Extract_Line (cbuf_t *cbuf);
void GIB_Parse_Tokenize_Line (cbuf_t *cbuf);
void GIB_Parse_Execute_Line (cbuf_t *cbuf);

cbuf_interpreter_t gib_interp = {
	GIB_Parse_Extract_Line,
	GIB_Parse_Tokenize_Line,
	GIB_Parse_Execute_Line,
	GIB_Buffer_Construct,
	GIB_Buffer_Destruct,
};


/* Co-recursive parsing functions */

char
GIB_Parse_Match_Dquote (const char *str, unsigned int *i)
{
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '\n')
			return '\"'; // Newlines should never occur inside quotes, EVER
		if (str[*i] == '\"')
			return 0;
	}
	return '\"';
}

char
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

char
GIB_Parse_Match_Paren (const char *str, unsigned int *i)
{
	char c;
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '(') {
			if ((c = GIB_Parse_Match_Paren (str, i)))
				return c;
		}
		if (str[*i] == ')')
			return 0;
	}
	return '(';
}

char
GIB_Parse_Match_Angle (const char *str, unsigned int *i)
{
	char c;
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '>')
			return 0;
		else if (str[*i] == '<') {
			if ((c = GIB_Parse_Match_Angle (str, i))) // Embedded inside embedded
				return c;
		} else if (str[*i] == '(') { // Skip over math expressions to avoid < and >
			if ((c = GIB_Parse_Match_Paren (str, i)))
				return c;
		} else if (str[*i] == '\"') { // Skip over strings
			if ((c = GIB_Parse_Match_Dquote (str, i)))
				return c;
		}
	}
	return '<';
}

void
GIB_Parse_Extract_Line (struct cbuf_s *cbuf)
{
	int i;
	char c;
	dstring_t *dstr = cbuf->buf;
	
	if (GIB_DATA(cbuf)->ret.waiting ) // Not ready for the next line
		return;
		
	dstring_clearstr (cbuf->line);
	
		
	for (i = 0; dstr->str[i]; i++) {
		if (dstr->str[i] == '{') {
			if ((c = GIB_Parse_Match_Brace (dstr->str, &i))) {
				Cbuf_Error ("parse", "Could not find matching %c", c);
				return;
			}
		} else if (dstr->str[i] == '\"') {
			if ((c = GIB_Parse_Match_Dquote (dstr->str, &i))) {
				Cbuf_Error ("parse", "Could not find matching %c", c);
				return;
			}
		} else if (dstr->str[i] == '<') {
			if ((c = GIB_Parse_Match_Angle (dstr->str, &i))) {
				Cbuf_Error ("parse", "Could not find matching %c", c);
				return;
			}
		} else if (dstr->str[i] == '\n' || dstr->str[i] == ';')
			break;
		else if (dstr->str[i] == '/' && dstr->str[i+1] == '/') {
			char *n;
			if ((n = strchr (dstr->str+i, '\n')))
				dstring_snip (dstr, i, n-dstr->str);
			else {
				dstring_snip (dstr, i, strlen(dstr->str+i));
				i--; // So we don't go past null
			}
		}
	}
	
	if (dstr->str[0]) {
		if (i) {
			dstring_insert (cbuf->line, 0, dstr->str, i);
			Sys_DPrintf ("extracted line: %s\n", cbuf->line->str);
		}
		dstring_snip (dstr, 0, i + (dstr->str[i] == '\n' || dstr->str[i] == ';'));
	}

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
			Cbuf_Error ("parse", "Could not find matching %c", c);
			return 0; // Parse error
		} else {
			dstring_insert (dstr, 0, str+n+1, *i-n-1);
			return '\"';
		}
	} else if (str[*i] == '{') {
		if ((c = GIB_Parse_Match_Brace (str, i))) {
			Cbuf_Error ("parse", "Could not find matching %c", c);
			return 0; // Parse error
		} else {
			dstring_insert (dstr, 0, str+n+1, *i-n-1);
			return '{';
		}
	} else if (str[*i] == '(') {
		if ((c = GIB_Parse_Match_Paren (str, i))) {
			Cbuf_Error ("parse", "Could not find matching %c", c);
			return 0; // Parse error
		} else {
			dstring_insert (dstr, 0, str+n+1, *i-n-1);
			return '\(';
		}
	} else {
		while (str[*i] && !isspace(str[*i]) && str[*i] != ',') { // find end of token
			if (str[*i] == '<') {
				if ((c = GIB_Parse_Match_Angle (str, i))) {
				Cbuf_Error ("parse", "Could not find matching %c", c);
				return 0; // Parse error
				}
			}
			(*i)++;
		}
		dstring_insert (dstr, 0, str+n, *i-n);
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
		// ->str could be moved by realloc when a dstring is resized
		// so save the offset instead of the pointer
		args->args[i] = (const char *) strlen (GIB_DATA (cbuf)->arg_composite->str);
		dstring_appendstr (GIB_DATA (cbuf)->arg_composite, args->argv[i]->str);
		dstring_appendstr (GIB_DATA (cbuf)->arg_composite, " ");
	}
	
	// Get rid of trailing space
	GIB_DATA (cbuf)->arg_composite->str[strlen(GIB_DATA (cbuf)->arg_composite->str)-1] = 0;
	
	for (i = 0; i < args->argc; i++)
		// now that arg_composite is done we can add the pointer to the stored
		// offsets and get valid pointers. This *should* be portable.
		args->args[i] += (unsigned long int) GIB_DATA (cbuf)->arg_composite->str;
}		

inline void
GIB_Parse_Add_Token (struct cbuf_args_s *args, qboolean cat, dstring_t *token)
{
	if (cat) {
			dstring_appendstr (args->argv[args->argc-1], token->str);
	} else
			Cbuf_ArgsAdd (args, token->str);
}

void
GIB_Parse_Tokenize_Line (struct cbuf_s *cbuf)
{
	dstring_t *arg = GIB_DATA(cbuf)->current_token;
	const char *str = cbuf->line->str;
	cbuf_args_t *args = cbuf->args;
	static qboolean cat = false;
	char delim;
	int i = 0;
	
	// This function can be interrupted to call a GIB
	// subroutine.  First we need to clean up anything
	// we left unfinished
	
	// Do we have a left-over token that needs processing?
	
	if (GIB_DATA(cbuf)->ret.waiting) {
		// FIXME: Call processing function here
		if (GIB_DATA(cbuf)->ret.waiting) // Still not done?
			return;
		i = GIB_DATA(cbuf)->ret.line_pos; // Start where we left off
	} else {
		args->argc = 0; // Start from scratch
		i = 0;
		cat = false;
	}
	
	// Get just the first token so we can look up any
	// parsing options that a builtin requests
	
	while (str[i]) {
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
		
		// FIXME:  Command sub goes here with subroutine handling
		
		if (GIB_Process_Token (arg, delim))
			goto FILTER_ERROR;
		
		GIB_Parse_Add_Token (cbuf->args, cat, arg);
		if (cat)
			cat = false;
		
		if (delim != ' ') // Move into whitespace if we haven't already
			i++;
		dstring_clearstr (arg);
	}
	GIB_Parse_Generate_Composite (cbuf);
	return;
FILTER_ERROR: // Error during filtering, clean up mess
	dstring_clearstr (arg);
	args->argc = 0;
	return;
}

void GIB_Parse_Execute_Line (cbuf_t *cbuf)
{
	gib_builtin_t *b;
	gib_function_t *f;
	
	if ((b = GIB_Builtin_Find (cbuf->args->argv[0]->str)))
		b->func ();
	else if ((f = GIB_Function_Find (cbuf->args->argv[0]->str))) {
		GIB_Function_Execute (f);
	} else	
		Cmd_Command (cbuf->args);
}

