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

	$Id$
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
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#include <direct.h>
#endif

#ifdef NeXT
#include <libc.h>
#endif

#include "cmdlib.h"

#define PATHSEPERATOR   '/'

// set these before calling CheckParm
int 		myargc;
char		**myargv;

char		com_token[1024];
qboolean	com_eof;

/*
	Error

	For abnormal program terminations
*/ 
void
Error (char *error, ...) 
{
	va_list argptr;
	
	printf ("************ ERROR ************\n");
	
	va_start (argptr, error);
	vprintf (error, argptr);
	va_end (argptr);
	printf ("\n");
	exit (1);
} 


/*
	COM_Parse

	Parse a token out of a string
*/
char *
COM_Parse (char *data)
{
	int 	c;
	int 	len = 0;

	com_token[0] = 0;

	if (!data)
		return NULL;

	// skip whitespace
skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0) {
			com_eof = true;
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
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		} while (1);
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':') {
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\''
			|| c == ':')
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}


/*
=============================================================================

						MISC FUNCTIONS

=============================================================================
*/


/*
	CheckParm

	Checks for the given parameter in the program's command line arguments
	Returns the argument number (1 to argc-1) or 0 if not present
*/
int
CheckParm (char *check)
{
	int 	i;

	for (i = 1; i < myargc; i++) {
		if (!strcasecmp (check, myargv[i]))
			return i;
	}

	return 0;
}



/*
	filelength
*/
int
filelength (FILE *f)
{
	int 	pos;
	int 	end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}


FILE *
SafeOpenWrite (char *filename)
{
	FILE	*f;

	f = fopen (filename, "wb");

	if (!f)
		Error ("Error opening %s: %s", filename, strerror (errno));

	return f;
}

FILE *
SafeOpenRead (char *filename)
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
LoadFile (char *filename, void **bufferptr)
{
	FILE	*f;
	int 	length;
	void	*buffer;

	f = SafeOpenRead (filename);
	length = filelength (f);
	buffer = malloc (length + 1);
	((char *) buffer)[length] = 0;
	SafeRead (f, buffer, length);
	fclose (f);

	*bufferptr = buffer;
	return length;
}
