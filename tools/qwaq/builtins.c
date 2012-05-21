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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <QF/dstring.h>
#include <QF/progs.h>
#include <QF/zone.h>

#include "qwaq.h"

static void
bi_print (progs_t *pr)
{
	const char *str;

	str = P_GSTRING (pr, 0);
	fprintf (stdout, "%s", str);
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
	const char *path = P_GSTRING (pr, 0);
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
	int *read_result = (int *)P_GPOINTER (pr, 2);
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
	const char *buffer = P_GSTRING (pr, 1);
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

/*
static void
bi_traceon (progs_t *pr)
{
	pr->pr_trace = true;
	pr->pr_trace_depth = pr->pr_depth;
}

static void
bi_traceoff (progs_t *pr)
{
	pr->pr_trace = false;
}
*/
static void
bi_printf (progs_t *pr)
{
	const char *fmt = P_GSTRING (pr, 0);
	int         count = pr->pr_argc - 1;
	pr_type_t **args = pr->pr_params + 1;
	dstring_t  *dstr = dstring_new ();

	PR_Sprintf (pr, dstr, "bi_printf", fmt, count, args);
	if (dstr->str)
		fputs (dstr->str, stdout);
	dstring_delete (dstr);
}

static builtin_t builtins[] = {
	{"print",		bi_print,		-1},
	{"errno",		bi_errno,		-1},
	{"strerror",	bi_strerror,	-1},
	{"open",		bi_open,		-1},
	{"close",		bi_close,		-1},
	{"read",		bi_read,		-1},
	{"write",		bi_write,		-1},
	{"seek",		bi_seek,		-1},
//	{"traceon",		bi_traceon,		-1},
//	{"traceoff",	bi_traceoff,	-1},
	{"printf",		bi_printf,		-1},
	{0}
};

void
BI_Init (progs_t *pr)
{
	PR_RegisterBuiltins (pr, builtins);
}
