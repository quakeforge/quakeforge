/*
	maplist.c

	maplist command

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
static const char rcsid[] = 
	"$Id$";

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
# ifdef WIN32
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
#include "QF/console.h"	//FIXME maplist really shouldn't be in here
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vfs.h"
#include "QF/zone.h"

#include "compat.h"

#ifndef HAVE_FNMATCH_PROTO
int         fnmatch (const char *__pattern, const char *__string, int __flags);
#endif

struct maplist {
	char      **list;
	int         count;
	int         size;
};

static struct maplist *
maplist_new (void)
{
	return calloc (1, sizeof (struct maplist));
}

static void
maplist_free (struct maplist *maplist)
{
	int         i;

	for (i = 0; i < maplist->count; i++)
		free (maplist->list[i]);
	free (maplist->list);
	free (maplist);
}

static void
maplist_add_map (struct maplist *maplist, char *fname)
{
	char      **new_list;

	if (maplist->count == maplist->size) {
		maplist->size += 32;
		new_list = realloc (maplist->list, maplist->size * sizeof (char *));

		if (!new_list) {
			maplist->size -= 32;
			return;
		}
		maplist->list = new_list;
	}
	fname = strdup (fname);
	*strstr (fname, ".bsp") = 0;
	maplist->list[maplist->count++] = fname;
}

static int
maplist_cmp (const void *_a, const void *_b)
{
	char       *a = *(char **) _a;
	char       *b = *(char **) _b;

	return strcmp (a, b);
}

static void
maplist_print (struct maplist *maplist)
{
	int         i;
	const char **list;

	if (maplist->count) {
		qsort (maplist->list, maplist->count, sizeof (char *), maplist_cmp);

		list = (const char **)malloc (maplist->count + 1);
		list[maplist->count] = 0;
		for (i = 0; i < maplist->count; i++)
			list[i] = maplist->list[i];
		Con_DisplayList (list, con_linewidth);
		free (list);
	}
}

/*
	Con_Maplist_f

	List map files in gamepaths.
*/
void
Con_Maplist_f (void)
{
	searchpath_t *search;
	DIR        *dir_ptr;
	struct dirent *dirent;
	char        buf[MAX_OSPATH];

	for (search = com_searchpaths; search != NULL; search = search->next) {
		if (search->pack) {
			int         i;
			pack_t     *pak = search->pack;
			struct maplist *maplist = maplist_new ();

			Sys_Printf ("Looking in %s...\n", search->pack->filename);
			for (i = 0; i < pak->numfiles; i++) {
				char       *name = pak->files[i].name;

				if (!fnmatch ("maps/*.bsp", name, FNM_PATHNAME)
					|| !fnmatch ("maps/*.bsp.gz", name, FNM_PATHNAME))
					maplist_add_map (maplist, name + 5);
			}
			maplist_print (maplist);
			maplist_free (maplist);
		} else {
			struct maplist *maplist = maplist_new ();

			snprintf (buf, sizeof (buf), "%s/maps", search->filename);
			dir_ptr = opendir (buf);
			Sys_Printf ("Looking in %s...\n", buf);
			if (!dir_ptr)
				continue;
			while ((dirent = readdir (dir_ptr)))
				if (!fnmatch ("*.bsp", dirent->d_name, 0)
					|| !fnmatch ("*.bsp.gz", dirent->d_name, 0))
					maplist_add_map (maplist, dirent->d_name);
			closedir (dir_ptr);
			maplist_print (maplist);
			maplist_free (maplist);
		}
	}
}
