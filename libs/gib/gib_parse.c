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

static __attribute__ ((unused)) const char rcsid[] =
        "$Id$";

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "QF/sys.h"
#include "QF/dstring.h"
#include "QF/va.h"
#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/gib_buffer.h"
#include "QF/gib_process.h"
#include "QF/gib_builtin.h"
#include "QF/gib_function.h"
#include "QF/gib_vars.h"
#include "QF/gib_parse.h"


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

char
GIB_Parse_Match_Paren (const char *str, unsigned int *i)
{
	char c;
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

char
GIB_Parse_Match_Index (const char *str, unsigned int *i)
{
	char c;
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
	char c;
	(*i)++;
	if (str[*i] == '{' && (c = GIB_Parse_Match_Brace (str, i)))
		return c;
	else {
		for (; isalnum((byte) str[*i]) || str[*i] == '_'; (*i)++);
		if (str[*i] == '[') {
			if ((c = GIB_Parse_Match_Index (str, i)))
				return c;
			(*i)++;
		}
	}
	return 0;
}

qboolean gib_parse_error;

// FIXME: Concatenation in stupid circumstances should generate errors

static gib_tree_t *
GIB_Parse_Tokens (const char *program, unsigned int *i, unsigned int flags, gib_tree_t **embedded)
{
	char c, delim, *str;
	unsigned int tstart;
	gib_tree_t *nodes = 0, *cur, *new, *embs = 0, *tmp;
	gib_tree_t **node = &nodes;
	qboolean cat = false;

	gib_parse_error = false;

	while (1) {
		// Skip whitespace
		while (program[*i] != '\n' && isspace((byte)program[*i]))
			(*i)++;
		// Check for concatenation, skip comma and any more whitespace
		if (program[*i] == ',') {
			cat = true;
			(*i)++;
			continue;
		}
		// New line/command?
		if (!program[*i] || program[*i] == '\n' || program[*i] == ';')
			break;
		// Save our start position
		tstart = *i + 1;
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
				for (;program[*i] && !isspace((byte)program[*i]) && program[*i] != ',' && program[*i] != ';'; (*i)++) {
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
					} else if (program[*i] == '/' && program[*i+1] == '/') {
						for((*i) += 2; program[*i] && program[*i] != '\n'; (*i)++);
						goto DONE;
					}
				}
		}
		
		cur = *node = GIB_Tree_New (flags);
		cur->delim = delim;
		str = calloc (*i - tstart + 1, sizeof(char));
		memcpy (str, program+tstart, *i - tstart);
		
		// Don't bother parsing further if we are concatenating, as the resulting
		// string would differ from the parsed program.
		if (cur->delim == '{' && !cat) {
			// Try to parse sub-program
			if (!(new = GIB_Parse_Lines (str, flags)))
				goto ERROR;
			new->parent = cur;
			cur->children = new;
		// Check for embedded commands/variables
		} else if (cur->delim == ' ' || cur->delim == '(') {
			if (!(cur->children = GIB_Parse_Embedded (str, flags, &new))) {
				// There could be no embedded elements, so check for a real error
				if (gib_parse_error)
					goto ERROR;
			} else {
				// Link/set flags
				cur->children->parent = cur;
				cur->flags |= TREE_P_EMBED;
				// Add any embedded commands to top of chain
				if (new) {
					for (tmp = new; tmp->next; tmp = tmp->next);
					tmp->next = embs;
					embs = new;
				}
			}
			// Check for array splitting
			// Concatenating this onto something else is non-sensical
			if (cur->delim == ' ' && str[0] == '@' && !cat) {
				cur->flags |= TREE_ASPLIT;
			}
		// We can handle escape characters now
		} else if (cur->delim == '\"')
			GIB_Process_Escapes (str);
		cur->str = str;

		if (cat) {
			cur->flags |= TREE_CONCAT;
			cat = false;
		}
		// Nothing left to parse?
		if (!program[*i])
			break;
		// On non-normal tokens, move past the delimeter
		if (cur->delim != ' ')
			(*i)++;
		node = &cur->next;
	}
DONE:
	*embedded = embs;
	return nodes;
ERROR:
	if (nodes)
		GIB_Tree_Free_Recursive (nodes);
	gib_parse_error = true;
	return 0;
}

static gib_tree_t *
GIB_Parse_Semantic_Preprocess (gib_tree_t *line)
{
	gib_tree_t *p, *start = line;
	unsigned int flags = line->flags;
	while (!strcmp (line->children->str, "if") || !strcmp (line->children->str, "ifnot")) {
		// Sanity checking
		if (!line->children->next || !line->children->next->next || !line->children->next->next->children || line->flags & TREE_EMBED) {
			gib_parse_error = true;
			return line;
		}
		// Set conditional flag
		line->flags |= TREE_COND;
		if (line->children->str[2])
			line->flags |= TREE_NOT;
		// Save our spot
		p = line;
		// Move subprogram inline
		line->next = line->children->next->next->children;
		line->next->parent = 0;
		line->children->next->next->children = 0;
		// Find end of subprogram
		while (line->next) line = line->next;
		line->next = GIB_Tree_New (flags | TREE_END);
		line = line->next;
		// Mark jump point
		p->jump = line;
		line->flags |= TREE_END; // Last instruction of subprogram
		// Handle "else"
		if (p->children->next->next->next && !strcmp (p->children->next->next->next->str, "else")) {
			// Sanity checking
			if (!p->children->next->next->next->next) {
				gib_parse_error = true;
				return line;
			}
			// Is "else" followed by a subprogram?
			if (p->children->next->next->next->next->delim == '{') {
				// Move subprogram inline
				line->next = p->children->next->next->next->next->children;
				line->next->parent = 0;
				p->children->next->next->next->next->children = 0;
				while (line->next) line = line->next;
			} else {
				// Push rest of tokens into a new line
				line->next = GIB_Tree_New (flags);
				line->next->children = p->children->next->next->next->next;
				p->children->next->next->next->next = 0;
				line = line->next;
			}
		} else break; // Don't touch if statements in the sub program
	}
	// Now we know our exit point, set it on all our ending instructions
	while (start) {
		if (start->flags & TREE_END && !start->jump)
			start->jump = line;
		start = start->next;
	}
	// Nothing expanded from a line remains, exit now
	if (!line->children)
		return line;
	// If we have a while loop, handle that
	if (!strcmp (line->children->str, "while")) {
		// Sanity checks
		if (!line->children->next ||
		   !line->children->next->next ||
		   line->children->next->next->delim != '{' ||
		   !line->children->next->next->children ||
		   line->flags & TREE_EMBED) {
			gib_parse_error = true;
			return line;
		}
		// Set conditional flag
		line->flags |= TREE_COND;
		// Save our spot
		p = line;
		// Move subprogram inline
		line->next = line->children->next->next->children;
		line->next->parent = 0;
		line->children->next->next->children = 0;
		// Find end of subprogram, set jump point back to top of loop as we go
		for (; line->next; line = line->next)
			if (!line->jump)
				line->jump = p;
		line->next = GIB_Tree_New (flags | TREE_END);
		line->next->jump = p;
		line = line->next;
		// Mark jump point out of loop
		p->jump = line;
	} else if (!strcmp (line->children->str, "for")) {
		gib_tree_t *tmp;
		// Sanity checks
		if (!line->children->next || !line->children->next->next || strcmp (line->children->next->next->str, "in") || !line->children->next->next->next || !line->children->next->next->next->next) {
			gib_parse_error = true;
			return line;
		}
		// Find last token in line (contains program block)
		for (tmp = line->children->next->next->next->next; tmp->next; tmp = tmp->next);
		// More sanity
		if (tmp->delim != '{' || !tmp->children) {
			gib_parse_error = true;
			return line;
		}
		p = line;
		// Move subprogram inline
		line->next = tmp->children;
		line->next->parent = 0;
		tmp->children = 0;
		// Find end of subprogram, set jump point back to top of loop as we go
		for (; line->next; line = line->next)
			if (!line->jump)
				line->jump = p;
		line->next = GIB_Tree_New (flags | TREE_FORNEXT);
		line->next->jump = p;
		line = line->next;
		// Mark jump point out of loop
		p->jump = line;
	}	
	return line;
}		

gib_tree_t *
GIB_Parse_Lines (const char *program, unsigned int flags)
{
	unsigned int i = 0, lstart;
	gib_tree_t *lines = 0, *cur, *tokens, **line = &lines, *embs;
	char *str;

	while (1) {
		while (isspace((byte)program[i]) || program[i] == ';')
			i++;
		if (!program[i])
			break;
		lstart = i;
		// If we parse something useful...
		if ((tokens = GIB_Parse_Tokens (program, &i, flags, &embs))) {
			// Link it in
			cur = GIB_Tree_New (flags);
			cur->delim = '\n';
			str = calloc (i - lstart + 1, sizeof(char));
			memcpy (str, program+lstart, i - lstart);
			cur->str = str;
			cur->children = tokens;
			tokens->parent = cur;
			// Line contains embedded commands?
			if (embs) {
				// Add them to chain before actual line
				*line = embs;
				for (; embs->next; embs = embs->next);
				embs->next = cur;
			} else
				*line = cur;
			// Do preprocessing
			line = &(GIB_Parse_Semantic_Preprocess (cur))->next;
		}
		if (gib_parse_error)
			goto ERROR;
	}
	return lines;
ERROR:
	if (lines)
		GIB_Tree_Free_Recursive (lines);
	return 0;
}

gib_tree_t *
GIB_Parse_Embedded (const char *program, unsigned int flags, gib_tree_t **embedded)
{
	unsigned int i, n, t;
	char c, d, *str;
	gib_tree_t *lines = 0, **line = &lines, *cur, *tokens, *emb, *tmp;
	unsigned int start, end;
	
	gib_parse_error = false;
	*embedded = 0;
	
	for (i = 0; program[i]; i++) {
		if (program[i] == '`' || (program[i] == '$' && program[i+1] == '(')) {
			// Extract the embedded command
			start = i;
			if (program[i] == '`') {
				n = i+1;
				if ((c = GIB_Parse_Match_Backtick (program, &i)))
					goto ERROR;
			} else {
				n = ++i+1;
				if ((c = GIB_Parse_Match_Paren (program, &i)))
					goto ERROR;
			}
			end = i+1;
			// Construct the actual line to be executed
			cur = GIB_Tree_New (flags | TREE_EMBED);
			cur->delim = '`';
			str = calloc (i - n + 1, sizeof (char));
			memcpy (str, program+n, i - n);
			cur->str = str;
			
			t = 0;
			if (!(tokens = GIB_Parse_Tokens (cur->str, &t, flags, &emb))) {
				c = 0;
				goto ERROR;
			}
			cur->children = tokens;
			tokens->parent = cur;
			GIB_Parse_Semantic_Preprocess (cur)->next = *embedded;
			if (gib_parse_error)
				goto ERROR;
			// Did this have embedded commands of it's own?
			if (emb) {
				// Link them in first
				for (tmp = emb; tmp->next; tmp = tmp->next);
				tmp->next = cur;
				*embedded = emb;
			} else
				*embedded = cur;
			// Create a representative child node for GIB_Process_Embedded to use
			cur = GIB_Tree_New (flags | TREE_EMBED);
			cur->delim = '`';
			// Save start/end indices
			cur->start = start;
			cur->end = end;
			*line = cur;
			line = &cur->next;
		// Check for variable substitution
		} else if (program[i] == '$' || program[i] == '#') {
			// Extract variable name
			start = i;
			end = 0;
			d = program[i];
			if (program[i+1] == '{') {
				n = i+2;
				end++;
			} else
				n = i+1;
			if ((c = GIB_Parse_Match_Var (program, &i)))
				goto ERROR;
			end += i;
			
			cur = GIB_Tree_New (flags | TREE_EMBED);
			cur->delim = d;
			str = calloc (i - n + 1, sizeof(char));
			memcpy (str, program+n, i - n);
			cur->str = str;
			// Can we use the name as is, or must processing be done at runtime?
			if (strchr (str, '$') || strchr (str, '#'))
				cur->flags |= TREE_P_EMBED;
			// Save start/end indices
			cur->start = start;
			cur->end = end;
			*line = cur;
			line = &cur->next;
			// Don't skip anything important
			if (program[n-1] != '{')
				i--;
		}
	}
	return lines;
ERROR:
	gib_parse_error = true;
	if (lines)
		GIB_Tree_Free_Recursive (lines);
	return 0;
}
