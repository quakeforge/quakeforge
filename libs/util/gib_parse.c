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
		else if (str[*i] == '\"')
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
		} else if (str[*i] == '{') {
			if ((c = GIB_Parse_Match_Brace (str, i)))
				return c;
		} else if (str[*i] == '}')
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
		} else if (str[*i] == ')')
			return 0;
		else if (str[*i] == '\n') // Newlines in math == bad
			return '(';
	}
	return '(';
}

char
GIB_Parse_Match_Backtick (const char *str, unsigned int *i)
{
	char c;
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '`')
			return 0;
		else if (str[*i] == '\"') { // Skip over strings
			if ((c = GIB_Parse_Match_Dquote (str, i)))
				return c;
		}
	}
	return '`';
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

	if (GIB_DATA(cbuf)->type == GIB_BUFFER_LOOP && !dstr->str[0])
		Cbuf_AddText (cbuf, GIB_DATA(cbuf)->loop_program->str);

	return;

}

inline static char
GIB_Parse_Get_Token (const char *str, unsigned int *i, dstring_t *dstr, qboolean include_delim)
{
	int n;
	char c;
	
	n = *i; // Save start position
	if (str[*i] == '\"') {
		if ((c = GIB_Parse_Match_Dquote (str, i))) {
			Cbuf_Error ("parse", "Could not find matching %c", c);
			return 0; // Parse error
		} else {
			dstring_insert (dstr, 0, str+n+!include_delim, *i-n+1-!include_delim-!include_delim);
			return '\"';
		}
	} else if (str[*i] == '{') {
		if ((c = GIB_Parse_Match_Brace (str, i))) {
			Cbuf_Error ("parse", "Could not find matching %c", c);
			return 0; // Parse error
		} else {
			dstring_insert (dstr, 0, str+n+!include_delim, *i-n+1-!include_delim-!include_delim);
			return '{';
		}
	} else if (str[*i] == '(') {
		if ((c = GIB_Parse_Match_Paren (str, i))) {
			Cbuf_Error ("parse", "Could not find matching %c", c);
			return 0; // Parse error
		} else {
			dstring_insert (dstr, 0, str+n+!include_delim, *i-n+1-!include_delim-!include_delim);
			return '\(';
		}
	} else {
		while (str[*i] && !isspace(str[*i]) && str[*i] != ',') { // find end of token
			if (str[*i] == '`') {
				if ((c = GIB_Parse_Match_Backtick (str, i))) {
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
	qboolean cat = false;
	int noprocess;
	char delim;
	int i;
	
	// This function can be interrupted to call a GIB
	// subroutine.  First we need to clean up anything
	// we left unfinished
	
	// Do we have a left-over token that needs processing?
	
	if (GIB_DATA(cbuf)->ret.waiting) {
		if (GIB_Process_Token (arg, GIB_DATA(cbuf)->ret.delim))
			return; // Still not done, or error
		GIB_Parse_Add_Token (cbuf->args, GIB_DATA(cbuf)->ret.cat, arg);
		i = GIB_DATA(cbuf)->ret.line_pos; // Start tokenizing where we left off
		noprocess = GIB_DATA(cbuf)->ret.noprocess ? 1 : 0; // Past second token, no sense in having it be 2
	} else {
		args->argc = 0; // Start from scratch
		i = 0;
		noprocess = 0;
	}

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
		dstring_clearstr (arg);
		delim = GIB_Parse_Get_Token (str, &i, arg, noprocess == 1);
		if (!delim)
			break;
		Sys_DPrintf("Got token: %s\n", arg->str);
		if (delim != ' ') // Move into whitespace if we haven't already
			i++;

		if (noprocess != 1)
			if (GIB_Process_Token (arg, delim))
				goto FILTER_ERROR; // Error or GIB subroutine needs to be called
		
		if (noprocess > 1)
			noprocess--;
		
		GIB_Parse_Add_Token (cbuf->args, cat, arg);
		if (cat)
			cat = false;
		

		if (cbuf->args->argc == 1) {
			gib_builtin_t *b;
			if ((b = GIB_Builtin_Find (cbuf->args->argv[0]->str)))
				noprocess = b->type;
		}
	}
	GIB_Parse_Generate_Composite (cbuf);
	return;
FILTER_ERROR:
	GIB_DATA(cbuf)->ret.line_pos = i; // save our information in case
	GIB_DATA(cbuf)->ret.cat = cat;    // error is not fatal
	GIB_DATA(cbuf)->ret.delim = delim;
	GIB_DATA(cbuf)->ret.noprocess = noprocess;
	return;
}

void GIB_Parse_Execute_Line (cbuf_t *cbuf)
{
	cbuf_args_t *args = cbuf->args;
	gib_builtin_t *b;
	gib_function_t *f;
	
	if ((b = GIB_Builtin_Find (args->argv[0]->str)))
		b->func ();
	else if ((f = GIB_Function_Find (args->argv[0]->str)))
		GIB_Function_Execute (f);
	else if (args->argc == 3 && !strcmp (args->argv[1]->str, "="))
		GIB_Local_Set (cbuf, args->argv[0]->str, args->argv[2]->str);
	else	
		Cmd_Command (cbuf->args);
	dstring_clearstr (cbuf->line);
}

