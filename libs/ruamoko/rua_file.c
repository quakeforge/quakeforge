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
# ifdef _WIN32
# include "fnmatch.h"
# endif
#endif

#ifndef HAVE_FNMATCH_PROTO
int         fnmatch (const char *__pattern, const char *__string, int __flags);
#endif

#include "QF/cvar.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "rua_internal.h"

static const char *file_ban_list[] = {
	"default.cfg{,.gz}",
	"demo1.dem{,.gz}",
	"demo2.dem{,.gz}",
	"demo3.dem{,.gz}",
	"end1.bin{,.gz}",
	"end2.bin{,.gz}",
	"gfx.wad{,.gz}",
	"progs.dat{,.gz}",
	"quake.rc{,.gz}",
	0,
};

static const char *dir_ban_list[] = {
	"gfx",
	"maps",
	"progs",
	"skins",
	"sound",
	0,
};

static int
file_readable (char *path)
{
	char        t;
	char       *p = strchr (path, '/');
	const char **match;

	if (p) {
		t = *p;
		*p = 0;
		for (match = dir_ban_list; *match; match++) {
			if (fnmatch (*match, path, FNM_PATHNAME) == 0) {
				*p = t;
				return 0;
			}
		}
	} else {
		for (match = file_ban_list; *match; match++) {
			if (fnmatch (*match, path, FNM_PATHNAME) == 0) {
				return 0;
			}
		}
	}
	return 1;
}

static int
file_writeable (char *path)
{
	return file_readable (path);
}

static void
bi_File_Open (progs_t *pr)
{
	qfile_resources_t *res = PR_Resources_Find (pr, "QFile");
	QFile     **file = QFile_AllocHandle (pr, res);
	const char *pth = P_GSTRING (pr, 0);
	const char *mode = P_GSTRING (pr, 1);
	char       *path;
	char       *p;
	int         do_write = 0;
	int         do_read = 0;

	p = strchr (mode, 'r');
	if (p) {
		do_read |= 1;
		if (p[1] == '+')
			do_write |= 1;
	}

	p = strchr (mode, 'w');
	if (p) {
		do_write |= 1;
		if (p[1] == '+')
			do_read |= 1;
	}

	p = strchr (mode, 'a');
	if (p) {
		do_write |= 1;
		if (p[1] == '+')
			do_read |= 1;
	}

	path = QFS_CompressPath (pth);
	//printf ("'%s'  '%s'\n", P_GSTRING (pr, 0), path);
	if (!path[0])
		goto error;
	if (path[0] == '.' && path[1] == '.' && (path[2] == '/' || path [2] == 0))
		goto error;
	if (path[strlen (path) - 1] =='/')
		goto error;
	if (!do_read && !do_write)
		goto error;
	if (do_read && !file_readable (path))
		goto error;
	if (do_write && !file_writeable (path))
		goto error;

	*file = QFS_Open (va ("%s/%s", qfs_gamedir->dir.def, path), mode);
	if (!*file)
		goto error;
	R_INT (pr) = (file - res->handles) + 1;
	free (path);
	return;
error:
	free (path);
	R_INT (pr) = 0;
}

static builtin_t builtins[] = {
	{"File_Open",	bi_File_Open,	-1},
	{0}
};

void
RUA_File_Init (progs_t *pr, int secure)
{
	PR_RegisterBuiltins (pr, builtins);
}
