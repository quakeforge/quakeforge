/*
	filelist.c

	filelist commands

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef HAVE_FNMATCH_H
# define model_t sunmodel_t
# include <fnmatch.h>
# undef model_t
#else
# ifdef _WIN32
# include "fnmatch.h"
# endif
#endif

#ifdef HAVE_IO_H
# include <io.h>
#endif

#ifdef _MSC_VER
# define _POSIX_
#endif

#include <limits.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/pakfile.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "compat.h"

#ifndef HAVE_FNMATCH_PROTO
int fnmatch (const char *__pattern, const char *__string, int __flags);
#endif

static int
filelist_cmp (const void *_a, const void *_b)
{
	char       *a = *(char **) _a;
	char       *b = *(char **) _b;

	return strcmp (a, b);
}

static void
filelist_print (filelist_t *filelist)
{
	int         i;
	const char **list;

	if (filelist->count) {
		qsort (filelist->list, filelist->count, sizeof (char *), filelist_cmp);

		list = malloc ((filelist->count + 1) * sizeof (char *));
		list[filelist->count] = 0;
		for (i = 0; i < filelist->count; i++)
			list[i] = filelist->list[i];
		Con_DisplayList (list, con_linewidth);
		free ((void *) list);
	}
}

VISIBLE void
Con_Maplist_f (void)
{
	filelist_t *maplist = QFS_FilelistNew ();

	QFS_FilelistFill (maplist, "maps/", "bsp", 1);

	filelist_print (maplist);
	QFS_FilelistFree (maplist);
}

VISIBLE void
Con_Skinlist_f (void)
{
	filelist_t *skinlist = QFS_FilelistNew ();

	QFS_FilelistFill (skinlist, "skins/", "pcx", 1);

	filelist_print (skinlist);
	QFS_FilelistFree (skinlist);
}

const char *sb_endings[] = {
"bk",
"dn",
"ft",
"lf",
"rt",
"up",
0
};

VISIBLE void
Con_Skyboxlist_f (void)
{
	int         i, j, k, c, b;
	size_t      ending_len = strlen (sb_endings[0]);
	dstring_t  *basename = dstring_new ();

	filelist_t *skyboxlist = QFS_FilelistNew ();
	filelist_t *cutlist = QFS_FilelistNew ();

	QFS_FilelistFill (skyboxlist, "env/", "tga", 1);
	QFS_FilelistFill (skyboxlist, "env/", "pcx", 1);

	for (i = 0; i < skyboxlist->count; i++) {
		if (strlen(skyboxlist->list[i]) > ending_len
			&& strcmp((skyboxlist->list[i] + strlen (skyboxlist->list[i])
					   - ending_len),
					  sb_endings[0]) == 0) {
			dstring_copysubstr (basename, skyboxlist->list[i],
								strlen (skyboxlist->list[i]) - ending_len);
			c = 0;
			for (j = 1; sb_endings[j]; j++) {
				b = 0;
				for (k = 0; k < skyboxlist->count; k++) {
					if (strcmp(va (0, "%s%s", basename->str, sb_endings[j]),
							   skyboxlist->list[k]) == 0) {
						b = 1;
						*skyboxlist->list[k] = 0;
					}
				}
				c += b;
			}
			if (c == 5)
				QFS_FilelistAdd (cutlist, basename->str, 0);
		}
	}
	filelist_print (cutlist);
	QFS_FilelistFree (cutlist);
	QFS_FilelistFree (skyboxlist);
	dstring_delete (basename);
}

VISIBLE void
Con_Demolist_QWD_f (void)
{
	filelist_t *demolist = QFS_FilelistNew ();

	QFS_FilelistFill (demolist, "", "qwd", 1);

	filelist_print (demolist);
	QFS_FilelistFree (demolist);

	return;
}

VISIBLE void
Con_Demolist_DEM_f (void)
{
	filelist_t *demolist = QFS_FilelistNew ();

	QFS_FilelistFill (demolist, "", "dem", 1);

	filelist_print (demolist);
	QFS_FilelistFree (demolist);

	return;
}
