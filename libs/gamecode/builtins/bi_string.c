/*
	bi_string.c

	CSQC string builtins

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/csqc.h"
#include "QF/progs.h"
#include "QF/zone.h"

/*
    bi_String_ReplaceChar
    
    Repalces a special character in a string with another
*/
static void
bi_String_ReplaceChar (progs_t *pr)
{
	char        old = P_INT (pr, 0);
	char        new = P_INT (pr, 1);
	const char *src = P_STRING (pr, 2);
	const char *s;
	char       *dst = Hunk_TempAlloc (strlen (src) + 1);
	char       *d;

	for (d = dst, s = src; *s; d++, s++) {
		if (*s == old)
			*d = new;
		else
			*d = *s;
	}
	RETURN_STRING (pr, dst);
}

/*
    bi_String_Cut
    
    Cuts a specified part from a string
*/
static void
bi_String_Cut (progs_t *pr)
{
	char        pos = P_INT (pr, 0);
	char        len = P_INT (pr, 1);
	const char *str = P_STRING (pr, 2);
	char       *dst = Hunk_TempAlloc ((strlen (str) - len) + 1);
	int			cnt;

	memset (dst, 0, (strlen (str) - len) + 1);
	strncpy(dst, str, pos);
	str += pos;
	for (cnt = 0; cnt < len; cnt++) 
		str++;
	strcpy(dst, str);
	RETURN_STRING (pr, dst);
}

/*
    bi_String_Len
    
    Gives back the length of the string
*/
static void
bi_String_Len (progs_t *pr)
{
	const char *str = P_STRING (pr, 0);

    R_INT (pr) = strlen(str);
}

/*
	bi_String_GetChar

	Gives the intervalue of a character in
	a string
*/
static void
bi_String_GetChar (progs_t *pr)
{   
    const char *str = P_STRING (pr, 0);
	int         pos = P_INT (pr, 1);
	int         ret = 0; 

	if(pos > 0 && pos < strlen(str)) {
		ret = (int)str[pos];
	}
	R_INT (pr) = ret;
}


void
String_Progs_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "String_ReplaceChar", bi_String_ReplaceChar, -1);
	PR_AddBuiltin (pr, "String_Cut", bi_String_Cut, -1);
	PR_AddBuiltin (pr, "String_Len", bi_String_Len, -1);
	PR_AddBuiltin (pr, "String_GetChar", bi_String_GetChar, -1);
}
