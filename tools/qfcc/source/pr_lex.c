/*
	pr_lex.c

	Lexical parser for GameC

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2001 Jeff Teunissen <deek@dusknet.dhs.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <QF/sys.h>

#include "qfcc.h"

int         pr_source_line;

char        pr_parm_names[MAX_PARMS][MAX_NAME];

char       *pr_file_p;
char       *pr_line_start;				// start of current source line

int         pr_bracelevel;

char        pr_token[2048];
int         pr_token_len;
token_type_t pr_token_type;
type_t     *pr_immediate_type;
eval_t      pr_immediate;

char        pr_immediate_string[2048];

int         pr_error_count;

// simple types.  function types are dynamically allocated
type_t      type_void = { ev_void, &def_void };
type_t      type_string = { ev_string, &def_string };
type_t      type_float = { ev_float, &def_float };
type_t      type_vector = { ev_vector, &def_vector };
type_t      type_entity = { ev_entity, &def_entity };
type_t      type_field = { ev_field, &def_field };

// type_function is a void() function used for state defs
type_t      type_function = { ev_func, &def_function, NULL, &type_void };
type_t      type_pointer = { ev_pointer, &def_pointer };
type_t      type_quaternion = { ev_quaternion, &def_quaternion };
type_t      type_integer = { ev_integer, &def_integer };
type_t      type_uinteger = { ev_uinteger, &def_uinteger };
type_t      type_short = { ev_short, &def_short };
type_t      type_struct = { ev_struct, &def_struct };

type_t      type_floatfield = { ev_field, &def_field, NULL, &type_float };

def_t       def_void = { &type_void, "temp" };
def_t       def_string = { &type_string, "temp" };
def_t       def_float = { &type_float, "temp" };
def_t       def_vector = { &type_vector, "temp" };
def_t       def_entity = { &type_entity, "temp" };
def_t       def_field = { &type_field, "temp" };
def_t       def_function = { &type_function, "temp" };
def_t       def_pointer = { &type_pointer, "temp" };
def_t       def_quaternion = { &type_quaternion, "temp" };
def_t       def_integer = { &type_integer, "temp" };
def_t       def_uinteger = { &type_uinteger, "temp" };
def_t       def_short = { &type_short, "temp" };
def_t       def_struct = { &type_struct, "temp" };

def_t       def_ret, def_parms[MAX_PARMS];

def_t      *def_for_type[8] = {
	&def_void, &def_string, &def_float, &def_vector,
	&def_entity, &def_field, &def_function, &def_pointer
};

/*
	PR_LexString

	Parse a quoted string
*/
void
PR_LexString (void)
{
	int         c;
	int         i;
	int         mask;
	int         boldnext;

	pr_token_len = 0;
	mask = 0x00;
	boldnext = 0;

	pr_file_p++;
	do {
		c = *pr_file_p++;
		if (!c)
			error (0, "EOF inside quote");
		if (c == '\n')
			error (0, "newline inside quote");
		if (c == '\\') {				// escape char
			c = *pr_file_p++;
			if (!c)
				error (0, "EOF inside quote");
			switch (c) {
				case 'n':
					c = '\n';
					break;
				case '"':
					c = '\"';
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					for (i = c = 0; i < 3
						 && *pr_file_p >= '0'
						 && *pr_file_p <= '7'; i++, pr_file_p++) {
						c *= 8;
						c += *pr_file_p - '0';
					}
					if (!*pr_file_p)
						error (0, "EOF inside quote");
					break;
				case 'x':
					c = 0;
					while (*pr_file_p && isxdigit (*pr_file_p)) {
						c *= 16;
						if (*pr_file_p <= '9')
							c += *pr_file_p - '0';
						else if (*pr_file_p <= 'F')
							c += *pr_file_p - 'A' + 10;
						else
							c += *pr_file_p - 'a' + 10;
						pr_file_p++;
					}
					if (!*pr_file_p)
						error (0, "EOF inside quote");
					break;
				case 'a':
					c = '\a';
					break;
				case 'b':
					c = '\b';
					break;
				case 'e':
					c = '\033';
					break;
				case 'f':
					c = '\f';
					break;
				case 'r':
					c = '\r';
					break;
				case 't':
					c = '\t';
					break;
				case 'v':
					c = '\v';
					break;
				case '^':
					if (*pr_file_p == '\"')
						error (0, "Unexpected end of string after \\^");
					boldnext = 1;
					continue;
				case '<':
					mask = 0x80;
					continue;
				case '>':
					mask = 0x00;
					continue;
				default:
					error (0, "Unknown escape char");
					break;
			}
		} else if (c == '\"') {
			pr_token[pr_token_len] = 0;
			pr_token_type = tt_immediate;
			pr_immediate_type = &type_string;
			strcpy (pr_immediate_string, pr_token);
			return;
		}
		if (boldnext)
			c = c ^ 0x80;
		boldnext = 0;
		c = c ^ mask;
		pr_token[pr_token_len] = c;
		pr_token_len++;
	} while (1);
}

void
PR_PrintType (type_t *type)
{
	int         i;

	if (!type) {
		printf ("(null)");
		return;
	}
	switch (type->type) {
		case ev_void:
			printf ("void");
			break;
		case ev_string:
			printf ("string");
			break;
		case ev_float:
			printf ("float");
			break;
		case ev_vector:
			printf ("vector");
			break;
		case ev_entity:
			printf ("entity");
			break;
		case ev_field:
			printf (".");
			PR_PrintType (type->aux_type);
			break;
		case ev_func:
			PR_PrintType (type->aux_type);
			printf ("(");
			for (i = 0; i < type->num_parms; i++) {
				PR_PrintType (type->parm_types[i]);
				if (i < type->num_parms - 1)
					printf (",");
			}
			if (type->num_parms == -1)
				printf ("...");
			printf (")");
			break;
		case ev_pointer:
			printf ("pointer to ");
			PR_PrintType (type->aux_type);
			break;
		default:
			printf ("unknown type %d", type->type);
			break;
	}
}

/*
	PR_FindType

	Returns a preexisting complex type that matches the parm, or allocates
	a new one and copies it out.
*/
type_t *
PR_FindType (type_t *type)
{
	def_t      *def;
	type_t     *check;
	int         i;

	for (check = pr.types; check; check = check->next) {
		if (check->type != type->type
			|| check->aux_type != type->aux_type
			|| check->num_parms != type->num_parms)
			continue;

		if (check->type != ev_func)
			return check;

		if (check->num_parms == -1)
			return check;

		for (i = 0; i < type->num_parms; i++)
			if (check->parm_types[i] != type->parm_types[i])
				break;

		if (i == type->num_parms)
			return check;
	}

	// allocate a new one
	check = malloc (sizeof (*check));
	if (!check)
		Sys_Error ("PR_FindType: Memory Allocation Failure\n");
	*check = *type;
	check->next = pr.types;
	pr.types = check;

	// allocate a generic def for the type, so fields can reference it
	def = malloc (sizeof (def_t));
	if (!def)
		Sys_Error ("PR_FindType: Memory Allocation Failure\n");
	def->name = "COMPLEX TYPE";
	def->type = check;
	check->def = def;
	return check;
}
