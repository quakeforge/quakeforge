/* 
	cmdlib.c

	Command library

	Copyright (C) 1996-1997 id Software, Inc.
	Copyright (C) 2001 Jeff Teunissen <deek@dusknet.dhs.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>

#ifdef NeXT
#include <libc.h>
#endif

#include <QF/sys.h>

#include "cmdlib.h"

char        qfcc_com_token[1024];

/*
	Error

	For abnormal program terminations
*/
void
Error (char *error, ...)
{
	va_list     argptr;

	printf ("************ ERROR ************\n");

	va_start (argptr, error);
	vprintf (error, argptr);
	va_end (argptr);
	printf ("\n");
	exit (1);
}


/*
	Parse

	Parse a token out of a string
*/
char *
Parse (char *data)
{
	int         c;
	int         len = 0;

	qfcc_com_token[0] = 0;

	if (!data)
		return NULL;

	// skip whitespace
  skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0) {
			return NULL;				// end of file;
		}
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	// handle quoted strings specially
	if (c == '\"') {
		data++;
		do {
			c = *data++;
			if (c == '\"') {
				qfcc_com_token[len] = 0;
				return data;
			}
			qfcc_com_token[len] = c;
			len++;
		} while (1);
	}
	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':') {
		qfcc_com_token[len] = c;
		len++;
		qfcc_com_token[len] = 0;
		return data + 1;
	}
	// parse a regular word
	do {
		qfcc_com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\''
			|| c == ':')
			break;
	} while (c > 32);

	qfcc_com_token[len] = 0;
	return data;
}


/*
=============================================================================

						MISC FUNCTIONS

=============================================================================
*/


/*
	FileLength
*/
int
FileLength (FILE *f)
{
	int         pos;
	int         end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}


FILE *
SafeOpenWrite (const char *filename)
{
	FILE       *f;

	f = fopen (filename, "wb");

	if (!f)
		Error ("Error opening %s: %s", filename, strerror (errno));

	return f;
}

FILE *
SafeOpenRead (const char *filename)
{
	FILE       *f;

	f = fopen (filename, "rb");

	if (!f)
		Error ("Error opening %s: %s", filename, strerror (errno));

	return f;
}


void
SafeRead (FILE *f, void *buffer, int count)
{
	if (fread (buffer, 1, count, f) != (size_t) count)
		Error ("File read failure");
}


void
SafeWrite (FILE *f, void *buffer, int count)
{
	if (fwrite (buffer, 1, count, f) != (size_t) count)
		Error ("File read failure");
}


/*
	LoadFile
*/
int
LoadFile (const char *filename, void **bufferptr)
{
	FILE       *f;
	int         length;
	void       *buffer;

	f = SafeOpenRead (filename);
	length = FileLength (f);
	buffer = malloc (length + 1);
	SYS_CHECKMEM (buffer);
	((char *) buffer)[length] = 0;
	SafeRead (f, buffer, length);
	fclose (f);

	*bufferptr = buffer;
	return length;
}
