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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/msg.h"

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/strpool.h"

static hashtab_t *saved_strings;

static const char *
strpool_get_key (const void *_str, void *_strpool)
{
	long        str = (intptr_t) _str;
	strpool_t  *strpool = (strpool_t *) _strpool;

	return strpool->strings + str;
}

strpool_t *
strpool_new (void)
{
	strpool_t  *strpool = calloc (1, sizeof (strpool_t));

	strpool->str_tab = Hash_NewTable (16381, strpool_get_key, 0, strpool, 0);
	strpool->size = 1;
	strpool->max_size = 16384;
	strpool->strings = malloc (strpool->max_size);
	strpool->strings[0] = 0;
	return strpool;
}

strpool_t *
strpool_build (const char *strings, int size)
{
	intptr_t    s;

	strpool_t  *strpool = malloc (sizeof (strpool_t));
	strpool->str_tab = Hash_NewTable (16381, strpool_get_key, 0, strpool, 0);
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
	intptr_t    s;
	int         len;

	if (!str)
		return 0;
	s = (intptr_t) Hash_Find (strpool->str_tab, str);
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

int
strpool_findstr (strpool_t *strpool, const char *str)
{
	if (!str)
		return 0;
	return (intptr_t) Hash_Find (strpool->str_tab, str);
}

static const char *
ss_get_key (const void *s, void *unused)
{
	return (const char *)s;
}

const char *
save_string (const char *str)
{
	char       *s;
	if (!saved_strings)
		saved_strings = Hash_NewTable (16381, ss_get_key, 0, 0, 0);
	s = Hash_Find (saved_strings, str);
	if (s)
		return s;
	s = strdup (str);
	Hash_Add (saved_strings, s);
	return s;
}

const char *
save_substring (const char *str, int len)
{
	char        substr[len + 1];
	strncpy (substr, str, len);
	substr[len] = 0;
	return save_string (substr);
}

const char *
save_cwd (void)
{
	char       *cwd = getcwd (0, 0);
	const char *str = save_string (cwd);
	free (cwd);
	return str;
}

const char *
make_string (const char *token, char **end)
{
	char        s[7];	// utf8 needs 6 + nul
	sizebuf_t   utf8str = {
		.maxsize = sizeof (s),
		.data = (byte *) s,
	};
	int         c;
	int         i;
	int         mask;
	int         boldnext;
	int         unicount;
	int         quote;
	static dstring_t *str;

	if (!str)
		str = dstring_newstr ();
	dstring_clearstr (str);

	s[1] = 0;

	mask = 0x00;
	boldnext = 0;
	unicount = 0;

	quote = *token++;
	if (quote == '<') {
		quote = '>';		// #include <file>
	}
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
					boldnext = 0;
					c = '\\' ^ mask;
					break;
				case 'n':
					boldnext = 0;
					c = '\n' ^ mask;
					break;
				case '"':
					boldnext = 0;
					c = '\"' ^ mask;
					break;
				case '\'':
					boldnext = 0;
					c = '\'' ^ mask;
					break;
				case '?':
					boldnext = 0;
					c = '?' ^ mask;
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
						c ^= mask;
						boldnext = 0;
						break;
					}
				case '8':
				case '9':
					boldnext = 0;
					c = 18 + c - '0';
					c ^= mask;
					break;
				case 'x':
					boldnext = 0;
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
					c ^= mask;	// cancel mask below
					break;
				case 'U':
					unicount += 4;
				case 'u':
					unicount += 4;
					boldnext = 0;
					c = 0;
					while (unicount && *token
						   && isxdigit ((unsigned char)*token)) {
						c *= 16;
						if (*token <= '9')
							c += *token - '0';
						else if (*token <= 'F')
							c += *token - 'A' + 10;
						else
							c += *token - 'a' + 10;
						token++;
						--unicount;
					}
					if (!*token) {
						error (0, "EOF inside quote");
					} else if (unicount) {
						error (0, "incomplete unicode sequence: %x %d", c, unicount);
					}
					unicount = 1;	// signal need to encode to utf8
					c ^= mask;	// cancel mask below
					break;
				case 'a':
					boldnext = 0;
					c = '\a' ^ mask;
					break;
				case 'b':
					boldnext = 0;
					if (options.qccx_escapes) {
						mask ^= 0x80;
						continue;
					} else {
						c = '\b' ^ mask;
						break;
					}
				case 'e':
					boldnext = 0;
					c = '\033' ^ mask;
					break;
				case 'f':
					boldnext = 0;
					c = '\f' ^ mask;
					break;
				case 'r':
					boldnext = 0;
					c = '\r' ^ mask;
					break;
				case 's':
					boldnext = 0;
					mask ^= 0x80;
					continue;
				case 't':
					boldnext = 0;
					c = '\t' ^ mask;
					break;
				case 'v':
					boldnext = 0;
					c = '\v' ^ mask;
					break;
				case '^':
					if (*token == '\"')
						error (0, "Unexpected end of string after \\^");
					boldnext = 1;
					continue;
				case '[':
					boldnext = 0;
					c = 0x90;		// gold [
					break;
				case ']':
					boldnext = 0;
					c = 0x91;		// gold ]
					break;
				case '.':
					boldnext = 0;
					c = 28;			// center dot
					break;
				case '<':
					boldnext = 0;
					if (options.qccx_escapes) {
						c = 29 ^ mask;			// brown left end
						break;
					} else {
						mask = 0x80;
						continue;
					}
				case '-':
					boldnext = 0;
					c = 30;			// brown center bit
					break;
				case '>':
					boldnext = 0;
					if (options.qccx_escapes) {
						c = 31 ^ mask;			// brown right end
						break;
					} else {
						mask = 0x00;
						continue;
					}
				case '(':
					boldnext = 0;
					c = 128 ^ mask;		// left slider end
					break;
				case '=':
					boldnext = 0;
					c = 129 ^ mask;		// slider center
					break;
				case ')':
					boldnext = 0;
					c = 130 ^ mask;		// right slider end
					break;
				case '{':
					boldnext = 0;
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
					c ^= mask;
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
		if (unicount) {
			SZ_Clear (&utf8str);
			MSG_WriteUTF8 (&utf8str, c);
			MSG_WriteByte (&utf8str, 0);	// nul-terminate string
			unicount = 0;
		} else {
			s[0] = c;
			s[1] = 0;
		}
		dstring_appendstr (str, s);
	} while (1);

	if (end)
		*end = (char *) token;

	return save_string (str->str);
}

const char *
html_string (const char *str)
{
	static dstring_t *q;
	char        c[2] = {0, 0};

	if (!str)
		return "(null)";
	if (!q)
		q = dstring_new ();
	dstring_clearstr (q);
	while ((c[0] = *str++)) {
		switch (c[0]) {
			case '<':
				dstring_appendstr (q, "&lt;");
				break;
			case '>':
				dstring_appendstr (q, "&gt;");
				break;
			case '&':
				dstring_appendstr (q, "&amp;");
				break;
			case '"':
				dstring_appendstr (q, "&quot;");
				break;
			default:
				dstring_appendstr (q, c);
				break;
		}
	}
	return q->str;
}

const char *
quote_string (const char *str)
{
	static dstring_t *q;
	char        c[2] = {0, 0};

	if (!str)
		return "(null)";
	if (!q)
		q = dstring_new ();
	dstring_clearstr (q);
	while ((c[0] = *str++)) {
		switch (c[0]) {
			case '\a':
				dstring_appendstr (q, "\\a");
				break;
			case '\b':
				dstring_appendstr (q, "\\b");
				break;
			case '\f':
				dstring_appendstr (q, "\\f");
				break;
			case '\n':
				dstring_appendstr (q, "\\n");
				break;
			case '\r':
				dstring_appendstr (q, "\\r");
				break;
			case '\t':
				dstring_appendstr (q, "\\t");
				break;
			case '\\':
				dstring_appendstr (q, "\\\\");
				break;
			case '\'':
				dstring_appendstr (q, "\\'");
				break;
			case '\"':
				dstring_appendstr (q, "\\\"");
				break;
			default:
				if (c[0] >= 127 || c[0] < 32)
					dasprintf (q, "\\\\x%02d", (byte) c[0]);
				else
					dstring_appendstr (q, c);
				break;
		}
	}
	return q->str;
}
