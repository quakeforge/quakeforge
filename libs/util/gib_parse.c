/*
	gib_parse.c

	GIB parser functions

	Copyright (C) 2002 Brian Koropoff

	Author: Brian Koropoff
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
#include <string.h>

#include "QF/sys.h"
#include "QF/dstring.h"
#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/gib_buffer.h"
#include "QF/gib_process.h"
#include "QF/gib_builtin.h"
#include "QF/gib_function.h"
#include "QF/gib_vars.h"
#include "QF/gib_parse.h"

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

/* 
	GIB_Escaped

	Returns true if character i in str is
	escaped with a backslash (and the backslash
	is not itself escaped).
*/
inline qboolean
GIB_Escaped (const char *str, int i)
{
	int         n, c;

	if (!i)
		return 0;
	for (n = i - 1, c = 0; n >= 0 && str[n] == '\\'; n--, c++);
	return c & 1;
}


/*
	GIB_Parse_Match_Dquote
	
	Progresses an index variable through a string
	until a double quote is reached.  Returns
	0 on success, or the character it could not
	match otherwise
*/
char
GIB_Parse_Match_Dquote (const char *str, unsigned int *i)
{
	unsigned int n = *i;
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '\n')
			return '\"'; // Newlines should never occur inside quotes, EVER
		else if (str[*i] == '\"' && !GIB_Escaped (str, *i))
			return 0;
	}
	*i = n;
	return '\"';
}

/*
	GIB_Parse_Match_Brace
	
	Progresses an index variable through a string
	until a closing brace (}) is reached.  Calls
	other matching functions to skip areas of a
	string it does not want to handle.  Returns
	0 on success, or the character it could not
	match otherwise.
*/
char
GIB_Parse_Match_Brace (const char *str, unsigned int *i)
{
	char c;
	unsigned int n = *i;
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
	*i = n;
	return '{';
}

/*
	GIB_Parse_Match_Paren
	
	Progresses an index variable through a string
	until a closing parenthesis is reached.  Calls
	other matching functions to skip areas of a
	string it does not want to handle.  Returns
	0 on success, or the character it could not
	match otherwise.
*/
char
GIB_Parse_Match_Paren (const char *str, unsigned int *i)
{
	char c;
	unsigned int n = *i;
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '(') {
			if ((c = GIB_Parse_Match_Paren (str, i)))
				return c;
		} else if (str[*i] == ')')
			return 0;
		else if (str[*i] == '\n') // Newlines in math == bad
			break;
	}
	*i = n;
	return '(';
}

/*
	GIB_Parse_Match_Backtick
	
	Progresses an index variable through a string
	until a backtick (`) is reached.  Calls
	other matching functions to skip areas of a
	string it does not want to handle.  Returns
	0 on success, or the character it could not
	match otherwise.
*/
char
GIB_Parse_Match_Backtick (const char *str, unsigned int *i)
{
	char c;
	unsigned int n = *i;
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '`')
			return 0;
		else if (str[*i] == '\"') { // Skip over strings as usual
			if ((c = GIB_Parse_Match_Dquote (str, i)))
				return c;
		}
	}
	*i = n;
	return '`';
}

/*
	GIB_Parse_Match_Index

	Progresses an index variable through a string
	until a normal brace (]) is reached.  Calls
	other matching functions to skip areas of a
	string it does not want to handle.  Returns
	0 on success, or the character it could not
	match otherwise.
*/
char
GIB_Parse_Match_Index (const char *str, unsigned int *i)
{
	unsigned int n = *i;
	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == ']')
			return 0;
	}
	*i = n;
	return '[';
}

/*
	GIB_Parse_Strip_Comments

	Finds instances of // comments in a cbuf
	outside of double quotes and snips between
	the // and the next newline or the end of
	the string.
*/
void
GIB_Parse_Strip_Comments (struct cbuf_s *cbuf)
{
	unsigned int i;
	dstring_t *dstr = cbuf->buf;
	char c, *n;
	
	for (i = 0; dstr->str[i]; i++) {
		if (dstr->str[i] == '\"') {
			if ((c = GIB_Parse_Match_Dquote (dstr->str, &i)))
				// We don't care about parse errors here.
				// Let the parser sort it out later.
				return;
		} else if (dstr->str[i] == '/' && dstr->str[i+1] == '/') {
			if ((n = strchr (dstr->str+i, '\n'))) {
				dstring_snip (dstr, i, n-dstr->str-i);
				i--;
			} else {
				dstring_snip (dstr, i, strlen(dstr->str+i));
				break;
			}
		}
	}
}

/*
	GIB_Parse_Extract_Line
	
	Extracts the next command from a cbuf and
	deposits it in cbuf->line, removing the
	portion used from the buffer
*/
void
GIB_Parse_Extract_Line (struct cbuf_s *cbuf)
{
	unsigned int i;
	char c;
	dstring_t *dstr = cbuf->buf;
	
	if (GIB_DATA(cbuf)->ret.waiting ) // Not ready for the next line
		return;
		
	dstring_clearstr (cbuf->line);
	
	for (i = 0; dstr->str[i]; i++) {
		if (dstr->str[i] == '{') {
			if ((c = GIB_Parse_Match_Brace (dstr->str, &i)))
				goto PARSE_ERROR;
		} else if (dstr->str[i] == '\"') {
			if ((c = GIB_Parse_Match_Dquote (dstr->str, &i)))
				goto PARSE_ERROR;
		} else if (dstr->str[i] == '`') {
			if ((c = GIB_Parse_Match_Backtick (dstr->str, &i)))
				goto PARSE_ERROR;
		} else if (dstr->str[i] == '\n' || dstr->str[i] == ';')
			break;
	}
	
	if (dstr->str[0]) { // If something is left in the buffer
		if (i)// If we used any of it
			dstring_insert (cbuf->line, 0, dstr->str, i);
		// Clip out what we used or any leftover newlines or ;s
		dstring_snip (dstr, 0, i + (dstr->str[i] == '\n' || dstr->str[i] == ';'));
	}

	// If this is a looping buffer and it is now empty,
	// copy the loop program back in
	if (GIB_DATA(cbuf)->type == GIB_BUFFER_LOOP && !cbuf->buf->str[0])
		Cbuf_AddText (cbuf, GIB_DATA(cbuf)->loop_program->str);

	return;

PARSE_ERROR: // Extract out the line where the parse error occurred

	for (; i && dstr->str[i] != '\n'; i--);
	if (dstr->str[i] == '\n')
		i++;
	dstring_clearstr (cbuf->line);
	dstring_appendstr (cbuf->line, dstr->str+i);
	Cbuf_Error ("parse", "Could not find match for %c.", c);
}

/*
	GIB_Parse_Get_Token
	
	Progresses an index variable through a string
	until the end of the next token is found.
	Stores the token in dstr with or without
	wrapping characters as specified by include_delim.
	Returns 0 on error or the wrapping character of
	the token otherwise
*/
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
		while (str[*i] && !isspace((byte)str[*i]) && str[*i] != ',') { // find end of token
			if (str[*i] == '`') {
				if ((c = GIB_Parse_Match_Backtick (str, i))) {
				Cbuf_Error ("parse", "Could not find matching %c", c);
				return 0; // Parse error
				}
			}
			if (str[*i] == '{') {
				if ((c = GIB_Parse_Match_Brace (str, i))) {
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

/*
	GIB_Parse_Generate_Composite
	
	Concatenates all parsed arguments in cbuf
	into a single string and creates an array
	of pointers to the beginnings of tokens
	within it.
*/
inline static void
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

/*
	GIB_Parse_Add_Token
	
	Adds a new token to the argument list args
	or concatenates it to the end of the last
	according to cat.
*/
inline static void
GIB_Parse_Add_Token (struct cbuf_args_s *args, qboolean cat, dstring_t *token)
{
	if (cat) {
			dstring_appendstr (args->argv[args->argc-1], token->str);
	} else
			Cbuf_ArgsAdd (args, token->str);
}

/*
	GIB_Parse_Tokenize_Line
	
	Tokenizes and processes an extracted command,
	producing an argument list suitable for executing
	the command.  This function can be interrupted
	to call a GIB subroutine, in which case it will
	save important variables and start where it left
	off when called again.
*/
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
		while (isspace((byte)str[i])) // Eliminate whitespace
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

/*
	GIB_Parse_Execute_Line
	
	After an argument list has been created,
	this function executes the appropriate command,
	searching in this order:
	
	GIB builtins
	GIB functions
	Assignment to a local/global variable
	Normal quake console commands
*/
void GIB_Parse_Execute_Line (cbuf_t *cbuf)
{
	cbuf_args_t *args = cbuf->args;
	gib_builtin_t *b;
	gib_function_t *f;
	
	if ((b = GIB_Builtin_Find (args->argv[0]->str)))
		b->func ();
	else if ((f = GIB_Function_Find (args->argv[0]->str))) {
		cbuf_t *sub = Cbuf_New (&gib_interp);
		GIB_Function_Execute (sub, f, cbuf_active->args);
		cbuf_active->down = sub;
		sub->up = cbuf_active;
		cbuf_active->state = CBUF_STATE_STACK;
	} else if (args->argc == 3 && !strcmp (args->argv[1]->str, "=")) {
		// First, determine global versus local
		int glob = 0;
		char *c = 0;
		if ((c = strchr (args->argv[0]->str, '.'))) // Only check stem
			*c = 0;
		glob = (!GIB_Var_Get_Local (cbuf, args->argv[0]->str) && GIB_Var_Get_Global (args->argv[0]->str));
		if (c)
			*c = '.';
		if (glob)
			GIB_Var_Set_Global (args->argv[0]->str, args->argv[2]->str); // Set the global
		else
			GIB_Var_Set_Local (cbuf, args->argv[0]->str, args->argv[2]->str); // Set the local
	} else	
		Cmd_Command (cbuf->args);
	dstring_clearstr (cbuf->line);
}

