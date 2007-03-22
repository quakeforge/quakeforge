/*
	gib_semantics.c

	GIB tree handling functions

	Copyright (C) 2003 Brian Koropoff

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

#include <string.h>

#include "QF/qtypes.h"
#include "gib_tree.h"
#include "gib_parse.h"
#include "gib_semantics.h"

static int
GIB_Semantic_Validate_Class (gib_tree_t * tokens)
{
	gib_tree_t *a_class, *line, *cmd;
	
	if (!tokens->next || !tokens->next->next) {
		GIB_Parse_Error ("Malformed class definition; expected class "
				"name, optional 'extends' and parent class, and "
				"program block.", tokens->start);
		return -1;
	}

	if (tokens->next->next->delim == ' ' && !strcmp
			(tokens->next->next->str, "extends")) {
		if (!tokens->next->next->next) {
			GIB_Parse_Error ("Malformed class definition; "
					"expected parent class after "
					"\":\".", tokens->next->next->start);
			return -1;
		}
		a_class = tokens->next->next->next->next;
	} else
		a_class = tokens->next->next;

	if (!a_class || !a_class->children || a_class->delim != '{') {
		GIB_Parse_Error ("Malformed class definition; expected "
				"program block.", tokens->next->next->start);
		return -1;
	}

	for (line = a_class->children; line; line = line->next) {
		switch (line->type) {
			case TREE_T_LABEL:
				if (strcmp (line->str, "class") && strcmp
						(line->str, "instance")) {
					GIB_Parse_Error ("Malformed class "
							"definition; allowed "
							"labels are instance "
							"and class.",
							line->start);
					return -1;
				}
				break;
			case TREE_T_CMD:
				cmd = line->children;
				if (strcmp (cmd->str, "function")) {
					GIB_Parse_Error ("Malformed class "
							"definition; only "
							"allowed command is "
							"function.",
							cmd->start);
					return -1;
				} else {
					gib_tree_t *last;

					for (last = cmd; last->next; last = last->next);
					
					if (!cmd->next || !last || last->delim != '{' ||
							!last->children) {
						GIB_Parse_Error ("Malformed function "
								"definition; name, "
								"optional arg list, "
								"program block "
								"expected.",
								cmd->start);
						return -1;
					}
				}
				break;
			default:
				GIB_Parse_Error ("Malformed class "
						"definition; only commands "
						"and labels allowed.",
						line->start);
				return -1;
				break;
		}
	}
	return 0;
}
static gib_tree_t *
GIB_Semantic_Normal_To_Lines (gib_tree_t * tokens, const char *program, gib_tree_flags_t flags, unsigned int start, unsigned int end)
{
	gib_tree_t *mainline, *lines = 0, *embedded, *token, *temp;

	/* Normal tokens */
	// Create main line struct now as an anchor
	lines = mainline = GIB_Tree_New (TREE_T_CMD);

	for (token = tokens; token; token = token->next) {
		// Normal token?
		if (token->delim == ' ' || token->delim == '(') {
			// Check for embedded commands/variables
			embedded = GIB_Parse_Embedded (token);
			if (gib_parse_error) {
				GIB_Tree_Unref (&mainline);
				return 0;
			}
			if (embedded) {
				// Find end of embedded commands
				for (temp = embedded; temp->next; temp = temp->next);
				// Link it in
				temp->next = lines;
				lines = embedded;
				// Mark token for processing at runtime
				token->flags |= TREE_A_EMBED;
			} else if (token->children)
				// Mark token for processing at runtime
				token->flags |= TREE_A_EMBED;
		}
	}

	// Fill in info for primary line
	mainline->str = program;
	mainline->flags = flags;
	mainline->start = start;
	mainline->end = end;

	// Check for assignment
	if (tokens->next && tokens->next->delim == ' ' && !strcmp (tokens->next->str, "="))
		mainline->type = TREE_T_ASSIGN;
	// Check for message sending
	else if (tokens->next && tokens->next->delim == ' ' && !strcmp (tokens->next->str, "->")) {
		if (!tokens->next->next) {
			GIB_Tree_Unref (&mainline);
			GIB_Parse_Error ("Cannot send empty message.", token->next->start);
			return NULL;
		} else
			mainline->type = TREE_T_SEND;
	// Check for class definition, validate
	} else if (!strcmp (tokens->str, "class") &&
			GIB_Semantic_Validate_Class (tokens)) {
			GIB_Tree_Unref (&mainline);
			return NULL;
	}
	mainline->children = tokens;

	return lines;
}

static gib_tree_t *
GIB_Semantic_Process_Conditional (gib_tree_t *tokens)
{
	gib_tree_t *lines = 0, *temp, *cur, *embedded;

	// Check for embeddeds in first token
	lines = GIB_Parse_Embedded (tokens);
	if (gib_parse_error)
		return 0;
	if (lines || tokens->children)
		tokens->flags |= TREE_A_EMBED;

	// Iterate through all tokens concatenated to the first
	for (cur = tokens->next; cur && cur->flags & TREE_A_CONCAT; cur = cur->next) {
		embedded = GIB_Parse_Embedded (cur);
		if (gib_parse_error)
			return 0;
		if (embedded) {
			for (temp = embedded; temp->next; temp = temp->next);
			temp->next = lines;
			lines = embedded;
			cur->flags |= TREE_A_EMBED;
		} else if (cur->children)
			cur->flags |= TREE_A_EMBED;
	}

	return lines;
}

static gib_tree_t *
GIB_Semantic_If_To_Lines (gib_tree_t **tokens, const char *program, gib_tree_flags_t flags, gib_tree_t *end)
{
	gib_tree_t *token = *tokens, *lines, *temp, **next = &lines, *conditional;
	gib_tree_t *a_block;

	// Sanity checking
	if (flags & TREE_L_EMBED) {
		GIB_Parse_Error ("if statements may not be used in embedded commands.", token->start);
		return 0;
	}
	if (!token->next || !token->next->next) {
		GIB_Parse_Error ("malformed if statement; not enough arguments", token->start);
		return 0;
	}
	for (a_block = token->next->next; a_block->flags & TREE_A_CONCAT; a_block = a_block->next)
		if (!a_block->next) {
			GIB_Parse_Error ("malformed if statement; not enough arguments", token->start);
			return 0;
		}

	if (a_block->delim != '{') {
		GIB_Parse_Error ("malformed if statement; second argument is not a program block", token->next->next->start);
		return 0;
	}
	if (a_block->next) { // Else statement?
		if (a_block->next->delim != ' ' || strcmp (a_block->next->str, "else") || (a_block->next->next && a_block->next->next->flags & TREE_A_CONCAT)) {
			GIB_Parse_Error ("malformed if statement; expected \"else\" for third argument", token->next->next->next->start);
			return 0;
		}
		if (!a_block->next->next) {
			GIB_Parse_Error ("malformed if statement; expected arguments after \"else\"", token->next->next->next->start);
			return 0;
		}
	}

	// Process embedded stuff on conditional argument
	lines = GIB_Semantic_Process_Conditional (token->next);
	if (gib_parse_error)
		return 0;
	if (lines) {
		for (temp = lines; temp->next; temp = temp->next);
		next = &temp->next;
	}

	// Create conditional instruction
	conditional = GIB_Tree_New (TREE_T_COND);
	conditional->children = token;
	conditional->start = token->start;
	conditional->end = token->next->end;
	conditional->str = program;
	conditional->flags = token->str[2] ? flags | TREE_L_NOT : flags;

	// Link it in
	*next = conditional;
	next = &conditional->next;

	// Move program block inline
	*next = a_block->children;
	a_block->children = 0;

	// Find end of program block
	for (temp = *next; temp->next; temp = temp->next);
	next = &temp->next;

	// Add jump to end point
	*next = GIB_Tree_New (TREE_T_JUMP);
	(*next)->jump = end;
	conditional->jump = *next;
	next = &(*next)->next;

	// Is there an else statement?
	if (a_block->next) {
		temp = a_block->next;
		token = a_block->next->next;
		// Is this a program block?
		if (token->delim == '{') {
			// Move program block inline
			*next = token->children;
			token->children = 0;
			*tokens = 0;
		} else {
			// Cut off part we don't use
			temp->next = 0;
			// Indicate to caller that more tokens remain
			*tokens = token;
			// String will get used again, make a personal copy
			conditional->str = strdup (program);
		}
	} else
		// No tokens left
		*tokens = 0;
	return lines;
}

static gib_tree_t *
GIB_Semantic_While_To_Lines (gib_tree_t *tokens, const char *program, gib_tree_flags_t flags)
{
	gib_tree_t *lines, **next = &lines, *temp, *conditional, *endp;
	gib_tree_t *a_block;

	// Sanity checking
	if (flags & TREE_L_EMBED) {
		GIB_Parse_Error ("while statements may not be used in embedded commands", tokens->start);
		return 0;
	}
	if (!tokens->next || !tokens->next->next) {
		GIB_Parse_Error ("malformed while statement; incorrect number of arguments", tokens->start);
		return 0;
	}
	for (a_block = tokens->next->next; a_block->flags & TREE_A_CONCAT; a_block = a_block->next)
		if (!a_block->next) {
			GIB_Parse_Error ("malformed while statement; incorrect number of arguments", tokens->start);
			return 0;
		}
	if (a_block->delim != '{') {
		GIB_Parse_Error ("malformed while statement; expected program block for second argument", a_block->start);
		return 0;
	}

	// Process embedded stuff on conditional argument
	lines = GIB_Semantic_Process_Conditional (tokens->next);
	if (gib_parse_error)
		return 0;
	if (lines) {
		for (temp = lines; temp->next; temp = temp->next);
		next = &temp->next;
	}

	// Create conditional instruction
	conditional = GIB_Tree_New (TREE_T_COND);
	conditional->children = tokens;
	conditional->start = tokens->start;
	conditional->end = tokens->next->end;
	conditional->str = program;
	conditional->flags = flags;

	// Link it in
	*next = conditional;
	next = &conditional->next;

	// Create end point (for 'break' commands)
	endp = GIB_Tree_New (TREE_T_LABEL);

	// Move program block inline
	*next = a_block->children;
	a_block->children = 0;

	// Find end of program block
	for (temp = *next; temp->next; temp = temp->next) {
		// Check for break or continue
		if (!temp->jump && temp->children) {
			if (!strcmp (temp->children->str, "break")) {
				temp->type = TREE_T_JUMP;
				temp->jump = endp;
			} else if (!strcmp (temp->children->str, "continue")) {
				temp->type = TREE_T_JUMP;
				temp->jump = lines;
			}
		}
	}
	next = &temp->next;


	// Add jump back to top of loop
	temp = GIB_Tree_New (TREE_T_JUMP);
	temp->jump = lines;
	*next = temp;
	next = &temp->next;

	// Attach landing point for 'break' instructions
	*next = endp;

	// This is our last instruction, set it as escape for loop
	conditional->jump = endp;

	return lines;
}

static gib_tree_t *
GIB_Semantic_For_To_Lines (gib_tree_t *tokens, const char *program, gib_tree_flags_t flags)
{
	gib_tree_t *lines = 0, **next = &lines, *temp, *endp, *forinst, *embedded;
	gib_tree_t *a_in, *a_block;

	// Do sanity checks
	if (flags & TREE_L_EMBED) {
		GIB_Parse_Error ("for statements may not be used in embedded commands.", tokens->start);
		return 0;
	}

	if (!tokens->next) {
		GIB_Parse_Error ("malformed for statement; not enough arguments.", tokens->start);
		return 0;
	}

	for (a_in = tokens->next; a_in->delim != ' ' || strcmp (a_in->str, "in"); a_in = a_in->next) {
		if (!a_in->next) {
			GIB_Parse_Error ("malformed for statement; expected \"in\".", tokens->start);
			return 0;
		}
		if (a_in->flags & TREE_A_EMBED || a_in->flags & TREE_A_EXPAND)
			a_in->flags &= ~(TREE_A_EMBED | TREE_A_EXPAND);
	}
	if (!a_in->next) {
		GIB_Parse_Error ("malformed for statement; expected arguments after \"in\".", a_in->start);
		return 0;
	}

	for (a_block = a_in->next; a_block->next; a_block = a_block->next);
	if (a_block->delim != '{') {
		GIB_Parse_Error ("malformed for statement; expected program block as last argument.", a_block->start);
		return 0;
	}

	for (a_in = a_in->next; a_in != a_block; a_in = a_in->next) {
		embedded = GIB_Parse_Embedded (a_in);
		if (gib_parse_error)
			return 0;
		if (embedded) {
			for (temp = embedded; temp->next; temp = temp->next);
			temp->next = lines;
			lines = embedded;
			a_in->flags |= TREE_A_EMBED;
		} else if (a_in->children)
			a_in->flags |= TREE_A_EMBED;
	}

	if (lines) {
		for (temp = lines; temp->next; temp = temp->next);
		next = &temp->next;
	}

	forinst = GIB_Tree_New (TREE_T_CMD);
	forinst->children = tokens;
	forinst->start = tokens->start;
	forinst->end = a_block->start;
	forinst->str = program;
	forinst->flags = flags;

	*next = forinst;
	forinst->next = GIB_Tree_New (TREE_T_FORNEXT);
	forinst = forinst->next;
	next = &forinst->next;

	*next = a_block->children;
	a_block->children = 0;

	endp = GIB_Tree_New (TREE_T_LABEL);

	// Find end of program block
	for (temp = *next; temp->next; temp = temp->next) {
		// Check for break or continue
		if (!temp->jump && temp->children) {
			if (!strcmp (temp->children->str, "break")) {
				temp->type = TREE_T_JUMP;
				temp->jump = endp;
			} else if (!strcmp (temp->children->str, "continue")) {
				temp->type = TREE_T_JUMP;
				temp->jump = forinst;
			}
		}
	}
	next = &temp->next;

	*next = GIB_Tree_New (TREE_T_JUMP);
	(*next)->jump = forinst;
	(*next)->next = endp;

	forinst->jump = endp;

	return lines;
}

static gib_tree_t *
GIB_Semantic_Label_To_Lines (gib_tree_t *tokens, const char *program,
		gib_tree_flags_t flags)
{
	gib_tree_t *line;
	char *name;

	line = GIB_Tree_New (TREE_T_LABEL);
	
	name = strdup (tokens->str);
	name[strlen(name)-1] = '\0';
	line->str = name;
	line->flags = flags;
	
	GIB_Tree_Unref (&tokens);

	return line;
}
	
gib_tree_t *
GIB_Semantic_Tokens_To_Lines (gib_tree_t *tokens, const char *program, gib_tree_flags_t flags, unsigned int start, unsigned int end)
{
	gib_tree_t *lines, **next = &lines, *temp, *endp = 0;

	// If we are being weird, don't even bother trying to match special statements
	if (tokens->next && tokens->next->flags & TREE_A_CONCAT)
		return GIB_Semantic_Normal_To_Lines (tokens, program, flags, start, end);

	// Handle if statements
	if (!strcmp (tokens->str, "if") || !strcmp (tokens->str, "ifnot")) {
		// Create landing pad where all program blocks in
		// a chained if/else structure will jump to after
		// completing so that only one block is executed.
		endp = GIB_Tree_New (TREE_T_LABEL);
		do {
			// Link in output from if statement handler
			*next = GIB_Semantic_If_To_Lines (&tokens, program, flags, endp);
			if (gib_parse_error)
				goto ERROR;

			// Find end of output
			for (temp = *next; temp->next; temp = temp->next);
			next = &temp->next;
		} while (tokens && (!strcmp (tokens->str, "if") || !strcmp (tokens->str, "ifnot")));

	}
	if (!tokens)
		// Yes, goto is evil.  See if I care.
		goto DONE;
	if (!strcmp (tokens->str, "while"))
		*next = GIB_Semantic_While_To_Lines (tokens, program, flags);
	else if (!strcmp (tokens->str, "for"))
		*next = GIB_Semantic_For_To_Lines (tokens, program, flags);
	else if (tokens->str[strlen(tokens->str)-1] == ':' && !tokens->next)
		*next = GIB_Semantic_Label_To_Lines (tokens, program, flags);
	else
		*next = GIB_Semantic_Normal_To_Lines (tokens, program, flags, start, end);
	next = &(*next)->next;
	if (gib_parse_error)
		goto ERROR;
DONE:
	// Connect landing pad, if one exists
	if (endp)
		*next = endp;
	return lines;
ERROR:
	if (endp)
		GIB_Tree_Unref (&endp);
	return 0;
}
