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

*/

#ifndef __QF_dstring_h
#define __QF_dstring_h

/** \defgroup dstring Dynamic Strings
	\ingroup utils
*/
///@{

#include <stdarg.h>
#include <stdlib.h>

typedef struct dstring_mem_s {
	void     *(*alloc) (void *data, size_t size);
	void      (*free) (void *data, void *ptr);
	void     *(*realloc) (void *data, void *ptr, size_t size);
	void       *data;
} dstring_mem_t;

typedef struct dstring_s {
	dstring_mem_t *mem;
	size_t      size, truesize;
	char       *str;
} dstring_t;

extern dstring_mem_t dstring_default_mem;

// General buffer functions
///@{
/** Create a new dstring. size and truesize start at 0 and no string buffer
	is allocated.
*/
dstring_t *_dstring_new (dstring_mem_t *mem);
dstring_t *dstring_new (void);
///@}
/** Delete a dstring. Both the string buffer and dstring object are freed.
*/
void dstring_delete (dstring_t *dstr);
/** Resize the string buffer if necessary. The buffer is guaranteed to be
	large enough to hold size bytes (rounded up to the next 1kB boundary)
*/
void dstring_adjust (dstring_t *dstr);
/** Open up a hole in the string buffer. The contents of the opened hole
	are undefined.
	\param dstr		the dstring in which to open the hole.
	\param len		the size of the hole to open.
	\return			pointer to the beginning of the opened hole.
*/
char *dstring_open (dstring_t *dstr, size_t len);
/** Copy len bytes from data into the dstring, replacing any existing data.
*/
void dstring_copy (dstring_t *dstr, const char *data, size_t len);
/** Append len bytes from data onto the end of the dstring.
*/
void dstring_append (dstring_t *dstr, const char *data, size_t len);
/** Insert len bytes from data int the dstring at pos. If pos is past the
	end of the dstring, equivalent to dstring_append.
*/
void dstring_insert (dstring_t *dstr, size_t pos, const char *data,
					 size_t len);
/** Remove len bytes from the dstring starting at pos.
*/
void dstring_snip (dstring_t *dstr, size_t pos, size_t len);
/** Set the size of the dstring to 0 bytes. Does not free the string buffer
	anticipating reuse.
*/
void dstring_clear (dstring_t *dstr);
/** Replace rlen bytes in dstring at pos with len bytes from data. Moves
	trailing bytes as needed.
*/
void dstring_replace (dstring_t *dstr, size_t pos, size_t rlen,
						const char *data, size_t len);
/** Delete the dstring object retaining the string buffer. The string buffer
	will be just big enough to hold the data. Does NOT ensure the string is
	null terminated.
*/
char *dstring_freeze (dstring_t *dstr);

// String-specific functions
///@{
/** Allocate a new dstring pre-initialized as a null terminated string. size
	will be 1 and the first byte 0.
*/
dstring_t *_dstring_newstr (dstring_mem_t *mem);
dstring_t *dstring_newstr (void);
///@}
/** Create a new dstring from a string. Similar to strdup().
	\param str		the string to copy
	\return			inititialized dstring
*/
dstring_t *dstring_strdup (const char *str);
/** Open up a hole in the string buffer. The contents of the opened hole
	are undefined. The size of the opened hole will be 1 bigger than specified
	allowing for a null terminator.
	\param dstr		the dstring in which to open the hole.
	\param len		the size of the hole to open.
	\return			pointer to the current null terminator or beginning of the
					opened hole if there was no terminator.
*/
char *dstring_openstr (dstring_t *dstr, size_t len);
/** Copy the null terminated string into the dstring. Replaces any existing
	data.
	The dstring does not have to be null terminated but will become so.
*/
void dstring_copystr (dstring_t *dstr, const char *str);
/** Copy up to len bytes from the string into the dstring. Replaces any
	existing data.
	The dstring does not have to be null terminated but will become so.
*/
void dstring_copysubstr (dstring_t *dstr, const char *str, size_t len);
/** Append the null terminated string to the end of the dstring.
	The dstring does not have to be null terminated but will become so.
	However, any embedded nulls will be treated as the end of the dstring.
*/
void dstring_appendstr (dstring_t *dstr, const char *str);
/** Append up to len bytes from the string to the end of the dstring.
	The dstring does not have to be null terminated but will become so.
	However, any embedded nulls will be treated as the end of the dstring.
*/
void dstring_appendsubstr (dstring_t *dstr, const char *str, size_t len);
/** Insert the null terminated string into the dstring at pos. The dstring
	is NOT forced to be null terminated.
*/
void dstring_insertstr (dstring_t *dstr, size_t pos, const char *str);
/** Insert up to len bytes from the string into the dstring at pos. The
	dstring is NOT forced to be null terminated.
*/
void dstring_insertsubstr (dstring_t *dstr, size_t pos, const char *str,
						   size_t len);
/** Clear the dstring to be equivalent to "". Does not resize the string buffer
	but size is set to 1.
	dstr = dstring_new (); dstring_clearstr (dstr); is exactly equivalent to
	dstr = dstring_newstr ();
*/
void dstring_clearstr (dstring_t *dstr);

///@{
/** Formatted printing to dstrings. Existing data is replaced by the formatted
	string.
*/
char *dvsprintf (dstring_t *dstr, const char *fmt, va_list args) __attribute__((format(PRINTF,2,0)));
char *dsprintf (dstring_t *dstr, const char *fmt, ...) __attribute__((format(PRINTF,2,3)));
///@}
///@{
/** Formatted printing to dstrings. Formatted string is appened to the dstring.
	Embedded nulls in the dstring are ignored.
*/
char *davsprintf (dstring_t *dstr, const char *fmt, va_list args) __attribute__((format(PRINTF,2,0)));
char *dasprintf (dstring_t *dstr, const char *fmt, ...) __attribute__((format(PRINTF,2,3)));
///@}

///@}

#endif//__QF_dstring_h
