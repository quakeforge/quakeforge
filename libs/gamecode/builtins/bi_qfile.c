/*
	bi_file.c

	CSQC file builtins

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef HAVE_FNMATCH_H
# define model_t sunmodel_t
# include <fnmatch.h>
# undef model_t
#else
# ifdef WIN32
# include "fnmatch.h"
# endif
#endif

#ifndef HAVE_FNMATCH_PROTO
int         fnmatch (const char *__pattern, const char *__string, int __flags);
#endif

#include "QF/csqc.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/va.h"
#include "QF/zone.h"

static void
bi_qfile_clear (progs_t *pr, void *data)
{
	qfile_resources_t *res = (qfile_resources_t *) data;
	int         i;
	
	for (i = 0; i < QFILE_MAX_HANDLES; i++)
		if (res->handles[i]) {
			Qclose (res->handles[i]);
			res->handles[i] = 0;
		}
}

QFile **
QFile_AllocHandle (struct progs_s *pr, qfile_resources_t *res)
{
	int         h;

	for (h = 0; h < QFILE_MAX_HANDLES && res->handles[h]; h++)
		;
	if (h == QFILE_MAX_HANDLES)
		goto error;
	res->handles[h] = (QFile *) 1;
	return res->handles + h;
error:
	return 0;
}

static void
secured (progs_t *pr)
{
	PR_RunError (pr, "Secured function called");
}

static void
bi_Qrename (progs_t *pr)
{
	const char *old = P_STRING (pr, 0);
	const char *new = P_STRING (pr, 1);

	R_INT (pr) = Qrename (old, new);
}

static void
bi_Qremove (progs_t *pr)
{
	const char *path = P_STRING (pr, 0);

	R_INT (pr) = Qremove (path);
}

static void
bi_Qopen (progs_t *pr)
{
	qfile_resources_t *res = PR_Resources_Find (pr, "QFile");
	const char *path = P_STRING (pr, 0);
	const char *mode = P_STRING (pr, 1);
	QFile     **h = QFile_AllocHandle (pr, res);

	if (!h) {
		R_INT (pr) = 0;
		return;
	}
	*h = Qopen (path, mode);
	R_INT (pr) = (h - res->handles) + 1;
}

static QFile **
get_qfile (progs_t *pr, int handle, const char *func)
{
	qfile_resources_t *res = PR_Resources_Find (pr, "QFile");

	if (handle < 1 || handle > QFILE_MAX_HANDLES || !res->handles[handle - 1])
		PR_RunError (pr, "%s: Invalid QFile", func);
	return res->handles + handle - 1;
}

static void
bi_Qclose (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qclose");

	Qclose (*h);
	*h = 0;
}

static void
bi_Qgetline (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qgetline");
	const char *s;

	s = Qgetline (*h);
	RETURN_STRING (pr, s);
}

static void
check_buffer (progs_t *pr, pr_type_t *buf, int count, const char *name)
{
	int         len;

	len = (count + sizeof (pr_type_t) - 1) / sizeof (pr_type_t);
	if (buf < pr->pr_globals || buf + len > pr->pr_globals + pr->globals_size)
		PR_RunError (pr, "%s: bad buffer", name);
}

static void
bi_Qread (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qread");
	pr_type_t  *buf = P_POINTER (pr, 1);
	int         count = P_INT (pr, 2);

	check_buffer (pr, buf, count, "Qread");
	R_INT (pr) = Qread (*h, buf, count);
}

static void
bi_Qwrite (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qwrite");
	pr_type_t  *buf = P_POINTER (pr, 1);
	int         count = P_INT (pr, 2);

	check_buffer (pr, buf, count, "Qwrite");
	R_INT (pr) = Qwrite (*h, buf, count);
}

static void
bi_Qputs (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qputs");
	const char *str = P_STRING (pr, 1);

	R_INT (pr) = Qputs (*h, str);
}
#if 0
static void
bi_Qgets (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qgets");
	pr_type_t  *buf = P_POINTER (pr, 1);
	int         count = P_INT (pr, 2);

	check_buffer (pr, buf, count, "Qgets");
	R_INT (pr) = POINTER_TO_PROG (pr, Qgets (*h, (char *) buf, count));
}
#endif
static void
bi_Qgetc (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qgetc");

	R_INT (pr) = Qgetc (*h);
}

static void
bi_Qputc (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qputc");
	int         c = P_INT (pr, 1);

	R_INT (pr) = Qputc (*h, c);
}

static void
bi_Qseek (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qseek");
	int         offset = P_INT (pr, 1);
	int         whence = P_INT (pr, 2);

	R_INT (pr) = Qseek (*h, offset, whence);
}

static void
bi_Qtell (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qtell");

	R_INT (pr) = Qtell (*h);
}

static void
bi_Qflush (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qflush");

	R_INT (pr) = Qflush (*h);
}

static void
bi_Qeof (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qeof");

	R_INT (pr) = Qeof (*h);
}

static void
bi_Qfilesize (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	QFile     **h = get_qfile (pr, handle, "Qfilesize");

	R_INT (pr) = Qfilesize (*h);
}

void
QFile_Progs_Init (progs_t *pr, int secure)
{
	qfile_resources_t *res = calloc (sizeof (qfile_resources_t), 1);

	PR_Resources_Register (pr, "QFile", res, bi_qfile_clear);
	if (secure) {
		PR_AddBuiltin (pr, "Qrename", secured, -1);
		PR_AddBuiltin (pr, "Qremove", secured, -1);
		PR_AddBuiltin (pr, "Qopen", secured, -1);
	} else {
		PR_AddBuiltin (pr, "Qrename", bi_Qrename, -1);
		PR_AddBuiltin (pr, "Qremove", bi_Qremove, -1);
		PR_AddBuiltin (pr, "Qopen", bi_Qopen, -1);
	}
	PR_AddBuiltin (pr, "Qclose", bi_Qclose, -1);
	PR_AddBuiltin (pr, "Qgetline", bi_Qgetline, -1);
	PR_AddBuiltin (pr, "Qread", bi_Qread, -1);
	PR_AddBuiltin (pr, "Qwrite", bi_Qwrite, -1);
	PR_AddBuiltin (pr, "Qputs", bi_Qputs, -1);
//	PR_AddBuiltin (pr, "Qgets", bi_Qgets, -1);
	PR_AddBuiltin (pr, "Qgetc", bi_Qgetc, -1);
	PR_AddBuiltin (pr, "Qputc", bi_Qputc, -1);
	PR_AddBuiltin (pr, "Qseek", bi_Qseek, -1);
	PR_AddBuiltin (pr, "Qtell", bi_Qtell, -1);
	PR_AddBuiltin (pr, "Qflush", bi_Qflush, -1);
	PR_AddBuiltin (pr, "Qeof", bi_Qeof, -1);
	PR_AddBuiltin (pr, "Qfilesize", bi_Qfilesize, -1);
}
