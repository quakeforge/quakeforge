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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "QF/sys.h"
#include "QF/dstring.h"
#include "QF/va.h"
#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/gib.h"

#include "gib_parse.h"
#include "gib_process.h"
#include "gib_semantics.h"

/*
	GIB_Escaped

	Returns true if character i in str is
	escaped with a backslash (and the backslash
	is not itself escaped).
*/
inline bool
GIB_Escaped (const char *str, int i)
{
	int         n, c;

	if (!i)
		return 0;
	for (n = i - 1, c = 0; n >= 0 && str[n] == '\\'; n--, c++);
	return c & 1;
}

/*
	GIB_Parse_Match_*

	These are the workhorses of the GIB parser.  They iterate
	an index variable through a string until an appropriate
	matching character is found, calling themselves and their
	neighbors recursively to handle sections of string that they
	are uninterested in.

	FIXME: Make sure everything is calling everything else it might
	need to.  Make appropriate functions intolerant of newlines.
*/

static char
GIB_Parse_Match_Dquote (const char *str, unsigned int *i)
{
	unsigned int n = *i;

	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '\n')
			break;
		else if (str[*i] == '\"' && !GIB_Escaped (str, *i))
			return 0;
	}
	*i = n;
	return '\"';
}

char
GIB_Parse_Match_Brace (const char *str, unsigned int *i)
{
	char        c;
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

char
GIB_Parse_Match_Paren (const char *str, unsigned int *i)
{
	char        c;
	unsigned int n = *i;

	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '(') {
			if ((c = GIB_Parse_Match_Paren (str, i)))
				return c;
		} else if (str[*i] == '\"') {
			if ((c = GIB_Parse_Match_Dquote (str, i)))
				return c;
		} else if (str[*i] == ')')
			return 0;
	}
	*i = n;
	return '(';
}

char
GIB_Parse_Match_Backtick (const char *str, unsigned int *i)
{
	char        c;
	unsigned int n = *i;

	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '`')
			return 0;
		else if (str[*i] == '\"') {		// Skip over strings as usual
			if ((c = GIB_Parse_Match_Dquote (str, i)))
				return c;
		}
	}
	*i = n;
	return '`';
}

char
GIB_Parse_Match_Index (const char *str, unsigned int *i)
{
	char        c;
	unsigned int n = *i;

	for ((*i)++; str[*i]; (*i)++) {
		if (str[*i] == '[' && (c = GIB_Parse_Match_Index (str, i)))
			return c;
		else if (str[*i] == ']')
			return 0;
	}
	*i = n;
	return '[';
}

char
GIB_Parse_Match_Var (const char *str, unsigned int *i)
{
	char        c;

	(*i)++;
	if (str[*i] == '{' && (c = GIB_Parse_Match_Brace (str, i)))
		return c;
	else {
		for (; isalnum ((byte) str[*i]) || str[*i] == '_'; (*i)++);
		if (str[*i] == '[') {
			if ((c = GIB_Parse_Match_Index (str, i)))
				return c;
			(*i)++;
		}
	}
	return 0;
}

VISIBLE bool        gib_parse_error;
unsigned int gib_parse_error_pos;
const char *gib_parse_error_msg;

void
GIB_Parse_Error (const char *msg, unsigned int pos)
{
	gib_parse_error = true;
	gib_parse_error_msg = msg;
	gib_parse_error_pos = pos;
}

const char *
GIB_Parse_ErrorMsg (void)
{
	return gib_parse_error_msg;
}

unsigned int
GIB_Parse_ErrorPos (void)
{
	return gib_parse_error_pos;
}

static gib_tree_t *
GIB_Parse_Tokens (const char *program, unsigned int *i, unsigned int pofs)
{
	char        c = 0, delim, *str;
	unsigned int tstart, start;
	gib_tree_t *nodes = 0, *cur, *new;
	gib_tree_t **node = &nodes;
	enum {CAT_NORMAL = 0, CAT_DISALLOW, CAT_CONCAT} cat = CAT_DISALLOW;
	const char *catestr = "Comma found before first argument, nothing to concatenate to.";

	gib_parse_error = false;

	while (1) {
		// Skip whitespace
		while (program[*i] != '\n' && isspace ((byte) program[*i]))
			(*i)++;
		// Check for concatenation, skip comma and any more whitespace
		if (program[*i] == ',') {
			if (cat == CAT_DISALLOW) {
				GIB_Parse_Error(catestr, *i + pofs);
				goto ERROR;
			}
			cat = CAT_CONCAT;
			(*i)++;
			while (program[*i] != '\n' && isspace ((byte) program[*i]))
				(*i)++;
			if (program[*i] == ',') {
				GIB_Parse_Error("Double comma error.", *i+pofs);
				goto ERROR;
			}
		} else
			cat = CAT_NORMAL;
		// New line/command?
		if (!program[*i] || program[*i] == '\n' || program[*i] == ';')
			break;
		// Save our start position
		start = *i;
		tstart = start + 1;
		delim = program[*i];
		switch (delim) {
			case '{':
				if ((c = GIB_Parse_Match_Brace (program, i)))
					goto ERROR;
				break;
			case '\"':
				if ((c = GIB_Parse_Match_Dquote (program, i)))
					goto ERROR;
				break;
			case '(':
				if ((c = GIB_Parse_Match_Paren (program, i)))
					goto ERROR;
				break;
			default:
				// Find the end of a "normal" token
				delim = ' ';
				tstart = *i;
				for (;
					 program[*i] && !isspace ((byte) program[*i])
					 && program[*i] != ',' && program[*i] != ';'; (*i)++) {
					if (program[*i] == '{') {
						if ((c = GIB_Parse_Match_Brace (program, i)))
							goto ERROR;
					} else if (program[*i] == '(') {
						if ((c = GIB_Parse_Match_Paren (program, i)))
							goto ERROR;
					} else if (program[*i] == '`') {
						if ((c = GIB_Parse_Match_Backtick (program, i)))
							goto ERROR;
						// Handle comments
					} else if (program[*i] == '/' && program[*i + 1] == '/') {
						for ((*i) += 2; program[*i] && program[*i] != '\n';
							 (*i)++);
						goto DONE;
					}
				}
		}
		c = 0;
		cur = *node = GIB_Tree_New (TREE_T_ARG);
		cur->start = start + pofs;
		cur->end = *i + pofs;
		cur->delim = delim;
		str = calloc (*i - tstart + 1, sizeof (char));
		cur->str = str;
		memcpy (str, program + tstart, *i - tstart);
		if (cur->delim == '{') {
			if (cat == CAT_CONCAT) {
				GIB_Parse_Error ("Program blocks may not be concatenated with other arguments.", start + pofs);
				goto ERROR;
			}
			catestr = "Program blocks may not be concatenated with other arguments.";
			cat = CAT_DISALLOW;
			// Try to parse sub-program
			if (!(new = GIB_Parse_Lines (str, tstart + pofs)))
				goto ERROR;
			cur->children = new;
			// Check for embedded commands/variables
		} else if (cur->delim == ' ' || cur->delim == '(') {
			if (cur->delim == ' ' && (str[0] == '@' || str[0] == '%')) {
				if (cat == CAT_CONCAT) {
					GIB_Parse_Error ("Variable expansions may not be concatenated with other arguments.", start + pofs);
					goto ERROR;
				}
				catestr = "Variable expansions may not be concatenated with other arguments.";
				cat = CAT_DISALLOW;
				cur->flags |= TREE_A_EXPAND;
			}
		// We can handle escape characters now
		} else if (cur->delim == '\"')
			GIB_Process_Escapes (str);
		if (cat == CAT_CONCAT)
			cur->flags |= TREE_A_CONCAT;
		// Nothing left to parse?
		if (!program[*i])
			break;
		// On non-normal tokens, move past the delimeter
		if (cur->delim != ' ')
			(*i)++;
		node = &cur->next;
	}
  DONE:
	return nodes;
  ERROR:
	if (c)
		GIB_Parse_Error (va (0, "Could not find match for '%c'.", c),
							 *i + pofs);
	if (nodes)
		GIB_Tree_Unref (&nodes);
	return 0;
}

gib_tree_t *
GIB_Parse_Lines (const char *program, unsigned int pofs)
{
	unsigned int i = 0, lstart;
	gib_tree_t *lines = 0, *cur, *tokens, **line = &lines, *temp;
	char       *str;

	while (1) {
		while (isspace ((byte) program[i]) || program[i] == ';')
			i++;
		if (!program[i])
			break;
		lstart = i;
		// If we parse something useful...
		if ((tokens = GIB_Parse_Tokens (program, &i, pofs))) {
			str = calloc (i - lstart + 1, sizeof(char));
			memcpy (str, program + lstart, i - lstart);

			// All settings for the line must be passed here
			cur = GIB_Semantic_Tokens_To_Lines (
				tokens, str, 0,
				lstart+pofs,
				i+pofs
			);
			if (gib_parse_error) {
				free (str);
				goto ERROR;
			}
			// Find last line
			for (temp = cur; temp->next; temp = temp->next);

			// Link it to lines we already have
			*line = cur;
			line = &temp->next;
		}
		if (gib_parse_error)
			goto ERROR;
	}
	return lines;
  ERROR:
	if (lines)
		GIB_Tree_Unref (&lines);
	return 0;
}

gib_tree_t *
GIB_Parse_Embedded (gib_tree_t *token)
{
	unsigned int i, n, t, start, end;
	char        c, d, *str;
	const char *program = token->str;
	gib_tree_t *lines = 0, *cur, *tokens, **metanext, *temp;

	gib_parse_error = false;

	metanext = &token->children;

	for (i = 0; program[i]; i++) {
		if (program[i] == '`' || (program[i] == '$' && program[i + 1] == '(')) {
			// Extract the embedded command
			start = i;
			if (program[i] == '`') {
				n = i + 1;
				if ((c = GIB_Parse_Match_Backtick (program, &i)))
					goto ERROR;
			} else {
				n = ++i + 1;
				if ((c = GIB_Parse_Match_Paren (program, &i)))
					goto ERROR;
			}
			end = i + 1;

			// Construct the actual line to be executed

			// Reset error condition
			c = 0;
			// Location to begin parsing from
			t = 0;

			// Extract embedded command into a new string
			str = calloc (end - start + 1, sizeof(char));
			memcpy (str, program + n, i - n);

			// Attempt to parse tokens from it
			tokens = GIB_Parse_Tokens (str, &t, start + token->start);
			if (!tokens) {
				free (str);
				goto ERROR;
			}

			// All settings for the line must be passed here
			cur = GIB_Semantic_Tokens_To_Lines (
				tokens, str, TREE_L_EMBED,
				start + token->start,
				end + token->start
			);
			if (gib_parse_error) {
				free (str);
				goto ERROR;
			}
			// Find last line
			for (temp = cur; temp->next; temp = temp->next);

			// Link it to lines we already have
			// (new lines are added to the start)
			temp->next = lines;
			lines = cur;

			// Create a representative child node for
			// GIB_Process_Embedded to use
			cur = GIB_Tree_New (TREE_T_META);
			cur->delim = '`';

			// Save start/end indices
			cur->start = start;
			cur->end = end;

			// Link it to end of children list for token we are processing
			*metanext = cur;
			metanext = &cur->next;
		// Check for variable substitution
		} else if (program[i] == '$' || program[i] == '#') {
			// Extract variable name
			start = i;
			end = 0;
			d = program[i];
			if (program[i + 1] == '{') {
				n = i + 2;
				end++;
			} else
				n = i + 1;
			if ((c = GIB_Parse_Match_Var (program, &i)))
				goto ERROR;
			end += i;

			cur = GIB_Tree_New (TREE_T_META);
			cur->delim = d;
			str = calloc (i - n + 1, sizeof (char));
			memcpy (str, program + n, i - n);
			cur->str = str;
			// Can we use the name as is, or must processing be done at
			// runtime?
			if (strchr (str, '$') || strchr (str, '#'))
				cur->flags |= TREE_A_EMBED;
			// Save start/end indices
			cur->start = start;
			cur->end = end;
			*metanext = cur;
			metanext = &cur->next;
			// Don't skip anything important
			if (program[n - 1] != '{')
				i--;
		}
	}
	return lines;
  ERROR:
	if (c)
		GIB_Parse_Error (va (0, "Could not find match for '%c'.", c),
							 i + token->start);
	if (lines)
		GIB_Tree_Unref (&lines);
	return 0;
}
