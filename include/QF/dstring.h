/*
	dstring.h

	Dynamic string buffer functions

	Copyright (C) 1996-1997  Id Software, Inc.

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

	$Id$
*/

#ifndef __dstring_h
#define __dstring_h

typedef struct dstring_s {
	unsigned long int size, truesize;
	char *str;
} dstring_t;


// General buffer functions
dstring_t *dstring_new(void);
void dstring_delete (dstring_t *dstr);
void dstring_adjust(dstring_t *dstr);
void dstring_append (dstring_t *dstr, const char *data, unsigned int len);
void dstring_insert(dstring_t *dstr, const char *data, unsigned int len, unsigned int pos);
void dstring_snip (dstring_t *dstr, unsigned int pos, unsigned int len);
void dstring_clear (dstring_t *dstr);

// String-specific functions
dstring_t *dstring_newstr (void);
void dstring_appendstr (dstring_t *dstr, const char *str);
void dstring_insertstr (dstring_t *dstr, const char *str, unsigned int pos);
void dstring_clearstr (dstring_t *dstr);
#endif // __dstring_h
