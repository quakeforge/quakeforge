/*
	rua_qfs.c

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "rua_internal.h"

typedef struct {
	int         count;
	pr_ptr_t    list;
} qfslist_t;

static void
check_buffer (progs_t *pr, pr_type_t *buf, int count, const char *name)
{
	qfZoneScoped (true);
	int         len;

	len = (count + sizeof (pr_type_t) - 1) / sizeof (pr_type_t);
	if (buf < pr->pr_globals || buf + len > pr->pr_globals + pr->globals_size)
		PR_RunError (pr, "%s: bad buffer", name);
}


static void
bi_QFS_Open (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	QFile      *file;
	const char *path = P_GSTRING (pr, 0);
	const char *mode = P_GSTRING (pr, 1);

	if (!(file = QFS_Open (path, mode))) {
		R_INT (pr) = 0;
		return;
	}
	if (!(R_INT (pr) = QFile_AllocHandle (pr, file)))
		Qclose (file);
}

static void
bi_QFS_WOpen (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	QFile      *file;
	const char *path = P_GSTRING (pr, 0);
	int         zip = P_INT (pr, 1);

	if (!(file = QFS_WOpen (path, zip))) {
		R_INT (pr) = 0;
		return;
	}
	if (!(R_INT (pr) = QFile_AllocHandle (pr, file)))
		Qclose (file);
}

static void
bi_QFS_Rename (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *old = P_GSTRING (pr, 0);
	const char *new = P_GSTRING (pr, 1);

	R_INT (pr) = QFS_Rename (old, new);
}

static void
bi_QFS_LoadFile (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *filename = P_GSTRING (pr, 0);
	QFile      *file;
	int         size;
	void       *buffer;

	file = QFS_FOpenFile (filename);
	if (!file) {
		RETURN_POINTER (pr, 0);
		return;
	}
	size = Qfilesize (file);
	buffer = PR_Zone_Malloc (pr, (size + 3) & ~3);
	if (!buffer) {
		Qclose (file);
		RETURN_POINTER (pr, 0);
		return;
	}
	Qread (file, buffer, size);
	Qclose (file);
	RETURN_POINTER (pr, buffer);
}

static void
bi_QFS_OpenFile (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	QFile      *file;
	const char *filename = P_GSTRING (pr, 0);

	file = QFS_FOpenFile (filename);
	if (!file) {
		R_INT (pr) = 0;
		return;
	}
	if (!(R_INT (pr) = QFile_AllocHandle (pr, file)))
		Qclose (file);
}

static void
bi_QFS_WriteFile (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *filename = P_GSTRING (pr, 0);
	pr_type_t  *buf = P_GPOINTER (pr, 1);
	int         count = P_INT (pr, 2);

	check_buffer (pr, buf, count, "QFS_WriteFile");
	QFS_WriteFile (va (0, "%s/%s", qfs_gamedir->dir.def, filename), buf,
					   count);
}

static void
bi_QFS_Filelist (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	filelist_t *filelist = QFS_FilelistNew ();
	qfslist_t  *list;
	pr_string_t *strings;
	int         i;

	QFS_FilelistFill (filelist, P_GSTRING (pr, 0), P_GSTRING (pr, 1),
					  P_INT (pr, 2));

	list = PR_Zone_Malloc (pr, sizeof (list) + filelist->count * 4);
	list->count = filelist->count;
	strings = (pr_string_t *) (list + 1);
	list->list = PR_SetPointer (pr, strings);
	for (i = 0; i < filelist->count; i++)
		strings[i] = PR_SetDynamicString (pr, filelist->list[i]);
	RETURN_POINTER (pr, list);
}

static void
bi_QFS_FilelistFree (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	qfslist_t  *list = &P_STRUCT (pr, qfslist_t, 0);
	pr_string_t *strings = &G_STRUCT (pr, pr_string_t, list->list);
	int         i;

	for (i = 0; i < list->count; i++)
		PR_FreeString (pr, strings[i]);
	PR_Zone_Free (pr, list);
}

static void
bi_QFS_GetDirectory (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	RETURN_STRING (pr, qfs_gamedir->dir.def);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(QFS_Open,         2, p(string), p(string)),
	bi(QFS_WOpen,        2, p(string), p(int)),
	bi(QFS_Rename,       2, p(string), p(string)),
	bi(QFS_LoadFile,     1, p(string)),
	bi(QFS_OpenFile,     1, p(string)),
	bi(QFS_WriteFile,    3, p(string), p(ptr), p(int)),
	bi(QFS_Filelist,     3, p(string), p(string), p(int)),
	bi(QFS_FilelistFree, 1, p(ptr)),
	bi(QFS_GetDirectory, 0),
	{0}
};

void
RUA_QFS_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	PR_RegisterBuiltins (pr, builtins, 0);
}
