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

#include "qfcc.h"

int 			pr_source_line;

char			*pr_file_p;
char			*pr_line_start;		// start of current source line

int 			pr_bracelevel;

char			pr_token[2048];
int				pr_token_len;
token_type_t	pr_token_type;
type_t			*pr_immediate_type;
eval_t			pr_immediate;

char			pr_immediate_string[2048];

int 			pr_error_count;

// simple types.  function types are dynamically allocated
type_t	type_void = { ev_void, &def_void };
type_t	type_string = { ev_string, &def_string };
type_t	type_float = { ev_float, &def_float };
type_t	type_vector = { ev_vector, &def_vector };
type_t	type_entity = { ev_entity, &def_entity };
type_t	type_field = { ev_field, &def_field };
// type_function is a void() function used for state defs
type_t	type_function = { ev_func, &def_function, NULL, &type_void };
type_t	type_pointer = { ev_pointer, &def_pointer };
type_t	type_quaternion = { ev_quaternion, &def_quaternion };
type_t	type_integer = { ev_integer, &def_integer };

type_t	type_floatfield = { ev_field, &def_field, NULL, &type_float };

int 	type_size[ev_type_count] = { 1, 1, 1, 3, 1, 1, 1, 1, 4, 1 };

def_t	def_void = { &type_void, "temp" };
def_t	def_string = { &type_string, "temp" };
def_t	def_float = { &type_float, "temp" };
def_t	def_vector = { &type_vector, "temp" };
def_t	def_entity = { &type_entity, "temp" };
def_t	def_field = { &type_field, "temp" };
def_t	def_function = { &type_function, "temp" };
def_t	def_pointer = { &type_pointer, "temp" };
def_t	def_quaternion = { &type_quaternion, "temp"};
def_t	def_integer = { &type_integer, "temp"};

def_t	def_ret, def_parms[MAX_PARMS];

def_t	*def_for_type[8] = {
	&def_void, &def_string, &def_float, &def_vector,
	&def_entity, &def_field, &def_function, &def_pointer
};

void PR_LexWhitespace (void);
void PR_LexString (void);

/*
	PR_PrintNextLine
*/
void
PR_PrintNextLine (void)
{
	char	*t;

	printf ("%3i:", pr_source_line);
	for (t = pr_line_start; *t && *t != '\n'; t++)
		printf ("%c", *t);
	printf ("\n");
}

/*
	PR_NewLine

	Call at start of file and when *pr_file_p == '\n'
*/
void
PR_NewLine (void)
{
	qboolean    m;

	if (*pr_file_p == '\n') {
		pr_file_p++;
		m = true;
	} else
		m = false;

	if (*pr_file_p == '#') {
		char *p;
		int line;

		pr_file_p ++;			// skip over #
		line = strtol (pr_file_p, &p, 10);
		pr_file_p = p;
		while (isspace (*pr_file_p))
			pr_file_p++;
		if (!*pr_file_p)
			PR_ParseError ("Unexpected end of file");
		PR_LexString ();		// grab the filename
		if (!*pr_file_p)
			PR_ParseError ("Unexpected end of file");
		while (*pr_file_p && *pr_file_p != '\n')	// ignore flags
			pr_file_p++;
		if (!*pr_file_p)
			PR_ParseError ("Unexpected end of file");
		pr_source_line = line - (m != false);
		s_file = ReuseString (pr_immediate_string);
		if (!m)
			pr_file_p++;
		m = false;
	}

	pr_source_line++;
	pr_line_start = pr_file_p;

//	if (pr_dumpasm)
//		PR_PrintNextLine ();

	if (m)
		pr_file_p--;
}

/*
	PR_LexString

	Parse a quoted string
*/
void
PR_LexString (void)
{
	int         c;
	int			i;
	int			mask;
	int			boldnext;

	pr_token_len = 0;
	mask = 0x00;
	boldnext = 0;

	pr_file_p++;
	do {
		c = *pr_file_p++;
		if (!c)
			PR_ParseError ("EOF inside quote");
		if (c == '\n')
			PR_ParseError ("newline inside quote");
		if (c == '\\') {				// escape char
			c = *pr_file_p++;
			if (!c)
				PR_ParseError ("EOF inside quote");
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
									&& *pr_file_p <='7'; i++, pr_file_p++) {
						c *= 8;
						c += *pr_file_p - '0';
					}
					if (!*pr_file_p)
						PR_ParseError ("EOF inside quote");
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
						PR_ParseError ("EOF inside quote");
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
						PR_ParseError ("Unexpected end of string after \\^");
					boldnext = 1;
					continue;
				case '<':
					mask = 0x80;
					continue;
				case '>':
					mask = 0x00;
					continue;
				default:
					PR_ParseError ("Unknown escape char");
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

/*
	PR_LexNumber

	Parse a number
*/
float
PR_LexNumber (void)
{
	int 	c;
	int 	len;

	len = 0;
	c = *pr_file_p;
	do {
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	} while ((c >= '0' && c <= '9') || c == '.');
	pr_token[len] = 0;
	return atof (pr_token);
}

/*
	PR_LexVector

	Parses a single quoted vector
*/
void
PR_LexVector (void)
{
	int         i;

	pr_file_p++;
	pr_token_type = tt_immediate;
	pr_immediate_type = &type_vector;
	for (i = 0; i < 3; i++) {
		pr_immediate.vector[i] = PR_LexNumber ();
		PR_LexWhitespace ();
	}
	if (*pr_file_p != '\'')
		PR_ParseError ("Bad vector");
	pr_file_p++;
}

/*
	PR_LexName

	Parse an identifier
*/
void
PR_LexName (void)
{
	int 	c;
	int 	len;

	len = 0;
	c = *pr_file_p;
	do {
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	} while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
			|| (c >= '0' && c <= '9'));
	pr_token[len] = 0;
	pr_token_type = tt_name;
}

/*
	PR_LexPunctuation
*/
void
PR_LexPunctuation (void)
{
	char	*p = 0;
	int      len = 1;

	pr_token_type = tt_punct;

	switch (pr_file_p[0]) {
		case '&':
			if (pr_file_p[1] == '&') {
				p = "&&";
				len = 2;
			} else {
				p = "&";
			}
			break;
		case '|':
			if (pr_file_p[1] == '|') {
				p = "||";
				len = 2;
			} else {
				p = "|";
			}
			break;
		case '^':
			p = "^";
			break;
		case '<':
			if (pr_file_p[1] == '=') {
				p = "<=";
				len = 2;
			} else {
				p = "<";
			}
			break;
		case '>':
			if (pr_file_p[1] == '=') {
				p = ">=";
				len = 2;
			} else {
				p = ">";
			}
			break;
		case '=':
			if (pr_file_p[1] == '=') {
				p = "==";
				len = 2;
			} else {
				p = "=";
			}
			break;
		case '!':
			if (pr_file_p[1] == '=') {
				p = "!=";
				len = 2;
			} else {
				p = "!";
			}
			break;
		case ';':
			p = ";";
			break;
		case ',':
			p = ",";
			break;
		case '*':
			p = "*";
			break;
		case '/':
			p = "/";
			break;
		case '(':
			p = "(";
			break;
		case ')':
			p = ")";
			break;
		case '-':
			p = "-";
			break;
		case '+':
			p = "+";
			break;
		case '[':
			p = "[";
			break;
		case ']':
			p = "]";
			break;
		case '{':
			p = "{";
			break;
		case '}':
			p = "}";
			break;
		case '.':
			if (pr_file_p[1] == '.' && pr_file_p[2] == '.') {
				p = "...";
				len = 3;
			} else {
				p = ".";
			}
			break;
		case '#':
			p = "#";
			break;
	}

	if (p) {
		strcpy (pr_token, p);
		if (p[0] == '{')
			pr_bracelevel++;
		else if (p[0] == '}')
			pr_bracelevel--;
		pr_file_p += len;
		return;
	}

	PR_ParseError ("Unknown punctuation");
}


/*
	PR_LexWhitespace

	Skip whitespace and comments
*/
void
PR_LexWhitespace (void)
{
	int 	c;

	while (1) {
		// skip whitespace
		while ((c = *pr_file_p) <= ' ') {
			if (c == '\n')
				PR_NewLine ();
			if (!c)
				return;					// end of file
			pr_file_p++;
		}

		// skip // comments
		if (c == '/' && pr_file_p[1] == '/') {
			while (*pr_file_p && *pr_file_p != '\n')
				pr_file_p++;
			PR_NewLine ();
			pr_file_p++;
			continue;
		}

		// skip /* */ comments
		if (c == '/' && pr_file_p[1] == '*') {
			do {
				pr_file_p++;
				if (pr_file_p[0] == '\n')
					PR_NewLine ();
				if (pr_file_p[1] == 0)
					return;
			} while (pr_file_p[-1] != '*' || pr_file_p[0] != '/');
			pr_file_p++;
			continue;
		}

		break;							// a real character has been found
	}
}

//============================================================================

#define	MAX_FRAMES	256

char	pr_framemacros[MAX_FRAMES][16];
int 	pr_nummacros;

void
PR_ClearGrabMacros (void)
{
	pr_nummacros = 0;
}

void
PR_FindMacro (void)
{
	int 	i;

	for (i = 0; i < pr_nummacros; i++) {
		if (!strcmp (pr_token, pr_framemacros[i])) {
			sprintf (pr_token, "%d", i);
			pr_token_type = tt_immediate;
			pr_immediate_type = &type_float;
			pr_immediate._float = i;
			return;
		}
	}

	PR_ParseError ("Unknown frame macro $%s", pr_token);
}

/*
	PR_SimpleGetToken

	Just parses text, returning false if an eol is reached
*/
qboolean
PR_SimpleGetToken (void)
{
	int 	c;
	int 	i;

	// skip whitespace
	while ((c = *pr_file_p) <= ' ') {
		if (c == '\n' || c == 0)
			return false;
		pr_file_p++;
	}

	i = 0;
	while ((c = *pr_file_p) > ' ' && c != ',' && c != ';') {
		pr_token[i++] = c;
		pr_file_p++;
	}

	pr_token[i] = 0;
	return true;
}

void
PR_ParseFrame (void)
{
	while (PR_SimpleGetToken ()) {
		strcpy (pr_framemacros[pr_nummacros], pr_token);
		pr_nummacros++;
	}
}

/*
	PR_LexGrab

	Deals with counting sequence numbers and replacing frame macros
*/
void
PR_LexGrab (void)
{
	pr_file_p++;	// skip the $
	if (!PR_SimpleGetToken ())
		PR_ParseError ("hanging $");

	// check for $frame
	if (!strcmp (pr_token, "frame")) {
		PR_ParseFrame ();
		PR_Lex ();
	} else if (!strcmp (pr_token, "cd") // ignore other known $commands
			 || !strcmp (pr_token, "origin")
			 || !strcmp (pr_token, "base")
			 || !strcmp (pr_token, "flags")
			 || !strcmp (pr_token, "scale")
			 || !strcmp (pr_token, "skin")) {	// skip to end of line
		while (PR_SimpleGetToken ());
		PR_Lex ();
	} else {	// look for a frame name macro
		PR_FindMacro ();
	}
}

//============================================================================

/*
	PR_Lex

	Sets pr_token, pr_token_type, and possibly pr_immediate and pr_immediate_type
*/
void
PR_Lex (void)
{
	int 	c;

	pr_token[0] = 0;

	if (!pr_file_p) {
		pr_token_type = tt_eof;
		return;
	}

	PR_LexWhitespace ();

	c = *pr_file_p;

	if (!c) {
		pr_token_type = tt_eof;
		return;
	}

	if (c == '\"') {	// handle quoted strings as a unit
		PR_LexString ();
		return;
	}

	if (c == '\'') { // handle quoted vectors as a unit
		PR_LexVector ();
		return;
	}

	// if the first character is a valid identifier, parse until a non-id
	// character is reached
	if ((c >= '0' && c <= '9')
		|| (c == '-' && pr_file_p[1] >= '0' && pr_file_p[1] <= '9')) {
		pr_token_type = tt_immediate;
		pr_immediate_type = &type_float;
		pr_immediate._float = PR_LexNumber ();
		return;
	}

	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
		PR_LexName ();
		return;
	}

	if (c == '$') {
		PR_LexGrab ();
		return;
	}

	// parse symbol strings until a non-symbol is found
	PR_LexPunctuation ();
}

//=============================================================================

/*
	PR_ParseError

	Aborts the current file load
*/
void
PR_ParseError (const char *error, ...)
{
	va_list 	argptr;
	char		string[1024];

	va_start (argptr, error);
	vsprintf (string, error, argptr);
	va_end (argptr);

	printf ("%s:%i:%s\n", strings + s_file, pr_source_line, string);

#ifndef NEW_PARSER
	longjmp (pr_parse_abort, 1);
#endif
}


/*
	PR_Expect

	Gets the next token. Will issue an error if the current token isn't equal
	to string
*/
void
PR_Expect (token_type_t type, const char *string)
{
	if (type != pr_token_type || strcmp (string, pr_token))
		PR_ParseError ("expected %s, found %s", string, pr_token);
	PR_Lex ();
}


/*
	PR_Check

	If current token equals string, get next token and return true.
	Otherwise, return false and do nothing else.
*/
qboolean
PR_Check (token_type_t type, const char *string)
{
	if (type != pr_token_type || strcmp (string, pr_token))
		return false;

	PR_Lex ();
	return true;
}

/*
	PR_ParseName

	Checks to see if the current token is a valid name
*/
char *
PR_ParseName (void)
{
	static char ident[MAX_NAME];

	if (pr_token_type != tt_name)
		PR_ParseError ("not a name");

	if (strlen (pr_token) >= MAX_NAME - 1)
		PR_ParseError ("name too long");

	strcpy (ident, pr_token);
	PR_Lex ();

	return ident;
}

void
PR_PrintType (type_t *type)
{
	int i;
	if (!type) {
		printf("(null)");
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
	def_t		*def;
	type_t		*check;
	int 		i;

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
	*check = *type;
	check->next = pr.types;
	pr.types = check;

	// allocate a generic def for the type, so fields can reference it
	def = malloc (sizeof (def_t));
	def->name = "COMPLEX TYPE";
	def->type = check;
	check->def = def;
	return check;
}


/*
	PR_SkipToSemicolon

	For error recovery, also pops out of nested braces
*/
void
PR_SkipToSemicolon (void)
{
	do {
		if (!pr_bracelevel && PR_Check (tt_punct, ";"))
			return;
		PR_Lex ();
	} while (pr_token[0]);	// eof will return a null token
}


char	pr_parm_names[MAX_PARMS][MAX_NAME];

/*
	PR_ParseType

	Parses a variable type, including field and functions types
*/
type_t *
PR_ParseType (void)
{
	type_t		new;
	type_t		*type;
	char		*name;

	if (PR_Check (tt_punct, ".")) {
		memset (&new, 0, sizeof (new));
		new.type = ev_field;
		new.aux_type = PR_ParseType ();
		return PR_FindType (&new);
	}

	if (!strcmp (pr_token, "float"))
		type = &type_float;
	else if (!strcmp (pr_token, "vector"))
		type = &type_vector;
	else if (!strcmp (pr_token, "float"))
		type = &type_float;
	else if (!strcmp (pr_token, "entity"))
		type = &type_entity;
	else if (!strcmp (pr_token, "string"))
		type = &type_string;
	else if (!strcmp (pr_token, "void"))
		type = &type_void;
	else {
		PR_ParseError ("\"%s\" is not a type", pr_token);
		type = &type_float;				// shut up compiler warning
	}
	PR_Lex ();

	if (!PR_Check (tt_punct, "("))
		return type;

	// function type
	memset (&new, 0, sizeof (new));
	new.type = ev_func;
	new.aux_type = type;				// return type
	new.num_parms = 0;
	if (!PR_Check (tt_punct, ")")) {
		if (PR_Check (tt_punct, "...")) {
			new.num_parms = -1;			// variable args
		} else {
			do {
				type = PR_ParseType ();
				name = PR_ParseName ();
				strcpy (pr_parm_names[new.num_parms], name);
				new.parm_types[new.num_parms] = type;
				new.num_parms++;
			} while (PR_Check (tt_punct, ","));
		}

		PR_Expect (tt_punct, ")");
	}

	return PR_FindType (&new);
}
