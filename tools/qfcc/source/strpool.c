/*
	strpool.c

	unique string support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/7/5

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include "QF/dstring.h"
#include "QF/hash.h"

#include "expr.h"
#include "options.h"
#include "strpool.h"

static hashtab_t *saved_strings;

static const char *
strpool_get_key (void *_str, void *_strpool)
{
	long        str = (long) _str;
	strpool_t  *strpool = (strpool_t *) _strpool;

	return strpool->strings + str;
}

strpool_t *
strpool_new (void)
{
	strpool_t  *strpool = malloc (sizeof (strpool_t));

	strpool->str_tab = Hash_NewTable (16381, strpool_get_key, 0, strpool);
	strpool->size = 1;
	strpool->max_size = 16384;
	strpool->strings = malloc (strpool->max_size);
	strpool->strings[0] = 0;
	return strpool;
}

strpool_t *
strpool_build (const char *strings, int size)
{
	long        s;

	strpool_t  *strpool = malloc (sizeof (strpool_t));
	strpool->str_tab = Hash_NewTable (16381, strpool_get_key, 0, strpool);
	strpool->size = size + (*strings != 0);
	strpool->max_size = (strpool->size + 16383) & ~16383;
	strpool->strings = malloc (strpool->max_size);
	memcpy (strpool->strings + (*strings != 0), strings, strpool->size);
	strpool->strings[0] = 0;
	for (s = 1; s < strpool->size; s += strlen (strpool->strings + s) + 1) {
		Hash_Add (strpool->str_tab, (void *) s);
	}
	return strpool;
}

void
strpool_delete (strpool_t *strpool)
{
	Hash_DelTable (strpool->str_tab);
	free (strpool->strings);
	free (strpool);
}

int
strpool_addstr (strpool_t *strpool, const char *str)
{
	long        s;
	int         len;

	if (!str || !*str)
		return 0;
	s = (long) Hash_Find (strpool->str_tab, str);
	if (s)
		return s;
	len = strlen (str) + 1;
	if (strpool->size + len > strpool->max_size) {
		strpool->max_size += (len + 16383) & ~16383;
		strpool->strings = realloc (strpool->strings, strpool->max_size);
	}
	s = strpool->size;
	strpool->size += len;
	strcpy (strpool->strings + s, str);
	Hash_Add (strpool->str_tab, (void *) s);
	return s;
}

static const char *
ss_get_key (void *s, void *unused)
{
	return (const char *)s;
}

const char *
save_string (const char *str)
{
	char       *s;
	if (!saved_strings)
		saved_strings = Hash_NewTable (16381, ss_get_key, 0, 0);
	s = Hash_Find (saved_strings, str);
	if (s)
		return s;
	s = strdup (str);
	Hash_Add (saved_strings, s);
	return s;
}

const char *
make_string (char *token, char **end)
{
	char        s[2];
	int         c;
	int         i;
	int         mask;
	int         boldnext;
	int         quote;
	static dstring_t *str;

	if (!str)
		str = dstring_newstr ();
	dstring_clearstr (str);

	s[1] = 0;

	mask = 0x00;
	boldnext = 0;

	quote = *token++;
	do {
		c = *token++;
		if (!c)
			error (0, "EOF inside quote");
		if (c == '\n')
			error (0, "newline inside quote");
		if (c == '\\') {				// escape char
			c = *token++;
			if (!c)
				error (0, "EOF inside quote");
			switch (c) {
				case '\\':
					c = '\\';
					break;
				case 'n':
					c = '\n';
					break;
				case '"':
					c = '\"';
					break;
				case '\'':
					c = '\'';
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					if (!options.qccx_escapes) {
						for (i = c = 0; i < 3
							 && *token >= '0'
							 && *token <= '7'; i++, token++) {
							c *= 8;
							c += *token - '0';
						}
						if (!*token)
							error (0, "EOF inside quote");
						break;
					}
				case '8':
				case '9':
					c = 18 + c - '0';
					break;
				case 'x':
					c = 0;
					while (*token && isxdigit ((unsigned char)*token)) {
						c *= 16;
						if (*token <= '9')
							c += *token - '0';
						else if (*token <= 'F')
							c += *token - 'A' + 10;
						else
							c += *token - 'a' + 10;
						token++;
					}
					if (!*token)
						error (0, "EOF inside quote");
					break;
				case 'a':
					c = '\a';
					break;
				case 'b':
					if (options.qccx_escapes)
						mask ^= 0x80;
					else
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
					if (*token == '\"')
						error (0, "Unexpected end of string after \\^");
					boldnext = 1;
					continue;
				case '[':
					c = 0x90;		// gold [
					break;
				case ']':
					c = 0x91;		// gold ]
					break;
				case '.':
					c = 28;			// center dot
					break;
				case '<':
					if (options.qccx_escapes)
						c = 29;			// brown left end
					else
						mask = 0x80;
					continue;
				case '-':
					c = 30;			// brown center bit
					break;
				case '>':
					if (options.qccx_escapes)
						c = 31;			// broun right end
					else
						mask = 0x00;
					continue;
				case '(':
					c = 128;		// left slider end
					break;
				case '=':
					c = 129;		// slider center
					break;
				case ')':
					c = 130;		// right slider end
					break;
				case '{':
					c = 0;
					while (*token && *token != '}'
						   && isdigit ((unsigned char)*token)) {
						c *= 10;
						c += *token++ - '0';
					}
					if (!*token)
						error (0, "EOF inside quote");
					if (*token != '}')
						error (0, "non-digit inside \\{}");
					else
						token++;
					if (c > 255)
						warning (0, "\\{%d} > 255", c);
					break;
				default:
					error (0, "Unknown escape char");
					break;
			}
		} else if (c == quote) {
			break;
		}
		if (boldnext)
			c = c ^ 0x80;
		boldnext = 0;
		c = c ^ mask;
		s[0] = c;
		dstring_appendstr (str, s);
	} while (1);

	if (end)
		*end = token;

	return save_string (str->str);
}
