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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

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
int         fnmatch (const char *__pattern, const char *__string, int __flags);
#endif

struct filelist {
	char      **list;
	int         count;
	int         size;
};

static struct filelist *
filelist_new (void)
{
	return calloc (1, sizeof (struct filelist));
}

static void
filelist_free (struct filelist *filelist)
{
	int         i;

	for (i = 0; i < filelist->count; i++)
		free (filelist->list[i]);
	free (filelist->list);
	free (filelist);
}

static void
filelist_add_file (struct filelist *filelist, char *fname, const char *ext)
{
	char      **new_list;
	char      *s;

	while ((s = strchr(fname, '/')))
		fname = s+1;

	if (filelist->count == filelist->size) {
		filelist->size += 32;
		new_list = realloc (filelist->list, filelist->size * sizeof (char *));

		if (!new_list) {
			filelist->size -= 32;
			return;
		}
		filelist->list = new_list;
	}
	fname = strdup (fname);
	
	if (ext && (s = strstr(fname, va(".%s", ext))))
		*s = 0;
	filelist->list[filelist->count++] = fname;
}

static int
filelist_cmp (const void *_a, const void *_b)
{
	char       *a = *(char **) _a;
	char       *b = *(char **) _b;

	return strcmp (a, b);
}

static void
filelist_print (struct filelist *filelist)
{
	int         i;
	const char **list;

	if (filelist->count) {
		qsort (filelist->list, filelist->count, sizeof (char *), filelist_cmp);

		//if (0) filelist_cmp (0, 0);

		list = (const char **)malloc ((filelist->count + 1)*sizeof(char **));
		list[filelist->count] = 0;
		for (i = 0; i < filelist->count; i++)
			list[i] = filelist->list[i];
		Con_DisplayList (list, con_linewidth);
		free (list);
	}
}

/*
	filelist_fill

	Fills a list with files of a specific extension.
*/
static void
filelist_fill (struct filelist *filelist, const char *path, const char *ext)
{
	searchpath_t *search;
	DIR        *dir_ptr;
	struct dirent *dirent;
	char        buf[MAX_OSPATH];

	for (search = qfs_searchpaths; search != NULL; search = search->next) {
		if (search->pack) {
			int         i;
			pack_t     *pak = search->pack;

			for (i = 0; i < pak->numfiles; i++) {
				char       *name = pak->files[i].name;

				if (!fnmatch (va("%s*.%s", path, ext), name, FNM_PATHNAME)
					|| !fnmatch (va("%s*.%s.gz", path, ext), name, FNM_PATHNAME))
					filelist_add_file (filelist, name, ext);
			}
		} else {
			snprintf (buf, sizeof (buf), "%s/%s", search->filename, path);
			dir_ptr = opendir (buf);
			if (!dir_ptr)
				continue;
			while ((dirent = readdir (dir_ptr)))
				if (!fnmatch (va("*.%s", ext), dirent->d_name, 0)
					|| !fnmatch (va("*.%s.gz", ext), dirent->d_name, 0))
					filelist_add_file (filelist, dirent->d_name, ext);
			closedir (dir_ptr);
		}
	}
}

void
Con_Maplist_f (void)
{
	struct filelist *maplist = filelist_new ();
	
	filelist_fill (maplist, "maps/", "bsp");
	
	filelist_print (maplist);
	filelist_free (maplist);
}

void
Con_Skinlist_f (void)
{
	struct filelist *skinlist = filelist_new ();
	
	filelist_fill (skinlist, "skins/", "pcx");

	filelist_print (skinlist);
	filelist_free (skinlist);
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

void
Con_Skyboxlist_f (void)
{
	int i, j, k, c, b;
	char basename[256];

	struct filelist *skyboxlist = filelist_new ();
	struct filelist *cutlist = filelist_new ();

	filelist_fill(skyboxlist, "env/", "tga");
	filelist_fill(skyboxlist, "env/", "pcx");

	for (i = 0; i < skyboxlist->count; i++) {
		if (strlen(skyboxlist->list[i]) > strlen(sb_endings[0]) && strcmp(skyboxlist->list[i] + strlen(skyboxlist->list[i]) - strlen(sb_endings[0]), sb_endings[0]) == 0) {
			strncpy(basename, skyboxlist->list[i], sizeof(basename));
			basename[strlen(skyboxlist->list[i]) - strlen(sb_endings[0])] = 0;
			c = 0;
			for (j = 1; sb_endings[j]; j++) {
				b = 0;
				for (k = 0; k < skyboxlist->count; k++) {
					if (strcmp(va("%s%s", basename, sb_endings[j]), skyboxlist->list[k]) == 0) {
						b = 1;
						*skyboxlist->list[k] = 0;
					}
				}
				c += b;
				
			}
			if (c == 5)
				filelist_add_file(cutlist, basename, 0);
		}
	}
	filelist_print(cutlist);
	filelist_free(cutlist);
	filelist_free(skyboxlist);
}

void
Con_Demolist_QWD_f (void)
{
	struct filelist *demolist = filelist_new ();
	
	filelist_fill(demolist, "", "qwd");
	
	filelist_print(demolist);
	filelist_free(demolist);
	
	return;
}

void
Con_Demolist_DEM_f (void)
{
	struct filelist *demolist = filelist_new ();
	
	filelist_fill(demolist, "", "dem");

	filelist_print(demolist);
	filelist_free(demolist);

	return;
}
