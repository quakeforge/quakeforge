/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <QF/progs.h>
#include <QF/zone.h>

#include "qwaq.h"

static void
bi_print (progs_t *pr)
{
	char *str;

	str = G_STRING (pr, (OFS_PARM0));
	fprintf (stdout, "%s", str);
}

static void
bi_GarbageCollect (progs_t *pr)
{
	PR_GarbageCollect (pr);
}

static void
bi_errno (progs_t *pr)
{
	R_INT (pr) = errno;
}

static void
bi_strerror (progs_t *pr)
{
	int err = P_INT (pr, 0);
	RETURN_STRING (pr, strerror (err));
}

static void
bi_open (progs_t *pr)
{
	char *path = P_STRING (pr, 0);
	int flags = P_INT (pr, 1);
	int mode = P_INT (pr, 2);
	R_INT (pr) = open (path, flags, mode);
}

static void
bi_close (progs_t *pr)
{
	int handle = P_INT (pr, 0);
	R_INT (pr) = close (handle);
}

static void
bi_read (progs_t *pr)
{
	int handle = P_INT (pr, 0);
	int count = P_INT (pr, 1);
	int *read_result = (int *)P_POINTER (pr, 2);
	int res;
	char *buffer;

	buffer = Hunk_TempAlloc (count);
	if (!buffer)
		PR_Error (pr, "%s: couldn't allocate %d bytes", "bi_read", count);
	res = read (handle, buffer, count);
	if (res != -1)
		RETURN_STRING (pr, buffer);
	*read_result = res;
}

static void
bi_write (progs_t *pr)
{
	int handle = P_INT (pr, 0);
	char *buffer = P_STRING (pr, 1);
	int count = P_INT (pr, 2);

	R_INT (pr) = write (handle, buffer, count);
}

static void
bi_seek (progs_t *pr)
{
	int handle = P_INT (pr, 0);
	int pos = P_INT (pr, 1);
	int whence = P_INT (pr, 2);

	R_INT (pr) = lseek (handle, pos, whence);
}

static void
bi_traceon (progs_t *pr)
{
	    pr->pr_trace = true;
}

static void
bi_traceoff (progs_t *pr)
{   
	    pr->pr_trace = false;
}

static void
bi_printf (progs_t *pr)
{
	const char *fmt = P_STRING (pr, 0);
	char c;
	int count = 0;
	float *v;

	while ((c = *fmt++)) {
		if (c == '%' && count < 7) {
			switch (c = *fmt++) {
				case 'i':
					fprintf (stdout, "%i", P_INT (pr, 1 + count++ * 3));
					break;
				case 'f':
					fprintf (stdout, "%f", P_FLOAT (pr, 1 + count++ * 3));
					break;
				case 's':
					fputs (P_STRING (pr, 1 + count++ * 3), stdout);
					break;
				case 'v':
					v = P_VECTOR (pr, 1 + count++ * 3);
					fprintf (stdout, "'%f %f %f'", v[0], v[1], v[2]);
					break;
				default:
					fputc ('%', stdout);
					fputc (c, stdout);
					count = 7;
					break;
			}
		} else {
			fputc (c, stdout);
		}
	}
}

void
BI_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "print", bi_print, 1);
	PR_AddBuiltin (pr, "GarbageCollect", bi_GarbageCollect, 2);
	PR_AddBuiltin (pr, "errno", bi_errno, 3);
	PR_AddBuiltin (pr, "strerror", bi_strerror, 4);
	PR_AddBuiltin (pr, "open", bi_open, 5);
	PR_AddBuiltin (pr, "close", bi_close, 6);
	PR_AddBuiltin (pr, "read", bi_read, 7);
	PR_AddBuiltin (pr, "write", bi_write, 8);
	PR_AddBuiltin (pr, "seek", bi_seek, 9);
	PR_AddBuiltin (pr, "traceon", bi_traceon, 10);
	PR_AddBuiltin (pr, "traceoff", bi_traceoff, 11);
	PR_AddBuiltin (pr, "printf", bi_printf, 12);
}
