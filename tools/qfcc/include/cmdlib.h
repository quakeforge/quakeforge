/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.

	$Id$
*/

// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#ifndef _DEF_BYTE_
# define _DEF_BYTE_
typedef unsigned char byte;
#endif

#ifndef _DEF_BOOL_
# define _DEF_BOOL_
typedef enum    {false, true} qboolean;
#endif

// the dec offsetof macro doesn't work very well...
#define myoffsetof(type,identifier) ((size_t)&((type *)0)->identifier)

// set these before calling CheckParm
extern int myargc;
extern char **myargv;

int FileLength (FILE *f);

void	Error (char *error, ...)__attribute__((format(printf, 1,2)));
int		CheckParm (char *check);

FILE	*SafeOpenWrite (char *filename);
FILE	*SafeOpenRead (char *filename);
void	SafeRead (FILE *f, void *buffer, int count);
void	SafeWrite (FILE *f, void *buffer, int count);

int		LoadFile (char *filename, void **bufferptr);

char *Parse (char *data);

extern	char		qfcc_com_token[1024];
extern	qboolean	com_eof;

#endif
