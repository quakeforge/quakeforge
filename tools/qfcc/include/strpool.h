/*
	strpool.h

	unique strings support

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

#ifndef __strpool_h
#define __strpool_h

typedef struct strpool_s {
	char       *strings;
	struct hashtab_s *str_tab;
	int         size, max_size;
	int         qfo_space;
} strpool_t;

strpool_t *strpool_new (void);
strpool_t *strpool_build (const char *strings, int size);
void strpool_delete (strpool_t *strpool);
int strpool_addstr (strpool_t *strpool, const char *str);

/**	Smart strdup.

	Create a unique copy of a string. If the same string has been seen
	before, does not create a new copy but rather returns the previously
	seen string.
	\param str		The string to copy.
	\return			The unique copy of the string.
*/
const char *save_string (const char *str);

const char *save_cwd (void);

const char *make_string (char *token, char **end);

const char *html_string (const char *str);
const char *quote_string (const char *str);

#endif//__strpool_h
