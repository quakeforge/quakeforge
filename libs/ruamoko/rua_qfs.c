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
	pointer_t   list;
} qfslist_t;

static void
check_buffer (progs_t *pr, pr_type_t *buf, int count, const char *name)
{
	int         len;

	len = (count + sizeof (pr_type_t) - 1) / sizeof (pr_type_t);
	if (buf < pr->pr_globals || buf + len > pr->pr_globals + pr->globals_size)
		PR_RunError (pr, "%s: bad buffer", name);
}


static void
bi_QFS_Open (progs_t *pr)
{
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
bi_QFS_WOpen (progs_t *pr)
{
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
bi_QFS_Rename (progs_t *pr)
{
	const char *old = P_GSTRING (pr, 0);
	const char *new = P_GSTRING (pr, 1);

	R_INT (pr) = QFS_Rename (old, new);
}

static void
bi_QFS_LoadFile (progs_t *pr)
{
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
bi_QFS_OpenFile (progs_t *pr)
{
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
bi_QFS_WriteFile (progs_t *pr)
{
	const char *filename = P_GSTRING (pr, 0);
	pr_type_t  *buf = P_GPOINTER (pr, 1);
	int         count = P_INT (pr, 2);

	check_buffer (pr, buf, count, "QFS_WriteFile");
	QFS_WriteFile (va ("%s/%s", qfs_gamedir->dir.def, filename), buf, count);
}

static void
bi_QFS_Filelist (progs_t *pr)
{
	filelist_t *filelist = QFS_FilelistNew ();
	qfslist_t  *list;
	string_t   *strings;
	int         i;

	QFS_FilelistFill (filelist, P_GSTRING (pr, 0), P_GSTRING (pr, 1),
					  P_INT (pr, 2));

	list = PR_Zone_Malloc (pr, sizeof (list) + filelist->count * 4);
	list->count = filelist->count;
	strings = (string_t *) (list + 1);
	list->list = PR_SetPointer (pr, strings);
	for (i = 0; i < filelist->count; i++)
		strings[i] = PR_SetDynamicString (pr, filelist->list[i]);
	RETURN_POINTER (pr, list);
}

static void
bi_QFS_FilelistFree (progs_t *pr)
{
	qfslist_t  *list = &P_STRUCT (pr, qfslist_t, 0);
	string_t   *strings = &G_STRUCT (pr, string_t, list->list);
	int         i;

	for (i = 0; i < list->count; i++)
		PR_FreeString (pr, strings[i]);
	PR_Zone_Free (pr, list);
}

static void
bi_QFS_GetDirectory (progs_t *pr)
{
	RETURN_STRING (pr, qfs_gamedir->dir.def);
}

static builtin_t builtins[] = {
	{"QFS_Open",			bi_QFS_Open,			-1},
	{"QFS_WOpen",			bi_QFS_WOpen,			-1},
	{"QFS_Rename",			bi_QFS_Rename,			-1},
	{"QFS_LoadFile",		bi_QFS_LoadFile,		-1},
	{"QFS_OpenFile",		bi_QFS_OpenFile,		-1},
	{"QFS_WriteFile",		bi_QFS_WriteFile,		-1},
	{"QFS_Filelist",		bi_QFS_Filelist,		-1},
	{"QFS_FilelistFree",	bi_QFS_FilelistFree,	-1},
	{"QFS_GetDirectory",	bi_QFS_GetDirectory,	-1},
	{0}
};

void
RUA_QFS_Init (progs_t *pr, int secure)
{
	PR_RegisterBuiltins (pr, builtins);
}
