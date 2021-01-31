/*
	locs.c

	Parsing and handling of location files.

	Copyright (C) 2000       Anton Gavrilov (tonik@quake.ru)

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

#ifdef _MSC_VER
# define _POSIX_
#endif

#include <limits.h>

#include "QF/locs.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"

#include "qw/include/client.h"

#define LOCATION_BLOCK	128				// 128 locations per block.

location_t **locations = NULL;
int          locations_alloced = 0;
int          locations_count = 0;
int          location_blocks = 0;

int
locs_nearest (const vec3_t loc)
{
	float       best_distance = 9999999, distance;
	int         i, j = -1;
	location_t *cur;

	for (i = 0; i < locations_count; i++) {
		cur = locations[i];
		distance = VectorDistance_fast (loc, cur->loc);
		if ((distance < best_distance)) {
			best_distance = distance;
			j = i;
		}
	}
	return (j);
}

location_t *
locs_find (const vec3_t target)
{
	int i;

	i = locs_nearest (target);
	if (i == -1)
		return NULL;
	return locations[i];
}

static void
locs_more (void)
{
	size_t      size;

	location_blocks++;
	locations_alloced += LOCATION_BLOCK;
	size = (sizeof (location_t *) * LOCATION_BLOCK * location_blocks);

	if (locations)
		locations = realloc (locations, size);
	else
		locations = malloc (size);

	if (!locations)
		Sys_Error ("ERROR! Can not alloc memory for location block!");
}

void
locs_add (const vec3_t location, const char *name)
{
	int         num;

	locations_count++;
	if (locations_count >= locations_alloced)
		locs_more ();

	num = locations_count - 1;

	locations[num] = malloc (sizeof (location_t));
	SYS_CHECKMEM (locations[num]);

	locations[num]->loc[0] = location[0];
	locations[num]->loc[1] = location[1];
	locations[num]->loc[2] = location[2];
	locations[num]->name = strdup (name);
	if (!locations[num]->name)
		Sys_Error ("locs_add: Can't strdup name!");
}

void
locs_load (const char *filename)
{
	const char *tmp;
	char       *t1, *t2;
	const char *line;
	vec3_t      loc;
	QFile      *file;

	tmp = va (0, "maps/%s", filename);
	file = QFS_FOpenFile (tmp);
	if (!file) {
		Sys_Printf ("Couldn't load %s\n", tmp);
		return;
	}
	while ((line = Qgetline (file))) {
		if (line[0] == '#')
			continue;

		loc[0] = strtol (line, &t1, 0) * (1.0 / 8);
		if (line == t1)
			continue;
		loc[1] = strtol (t1, &t2, 0) * (1.0 / 8);
		if (t2 == t1)
			continue;
		loc[2] = strtol (t2, &t1, 0) * (1.0 / 8);
		if ((t1 == t2) || (strlen (t1) < 2))
			continue;
		t1++;
		t2 = strrchr (t1, '\n');
		if (t2) {
			t2[0] = '\0';
			// handle dos format lines (QFS_FOpenFile is binary-only)
			// and unix is effectively binary-only anyway
			while (t2 > t1 && t2[-1] == '\r') {
				t2[-1] = '\0';
				t2--;
			}
		}
		locs_add (loc, t1);
	}
	Qclose (file);
}

void
locs_reset (void)
{
	int         i;

	for (i = 0; i < locations_count; i++) {
		free ((void *) locations[i]->name);
		free ((void *) locations[i]);
		locations[i] = NULL;
	}

	free (locations);
	locations_alloced = 0;
	locations_count = 0;
	locations = NULL;
}

void
locs_save (const char *filename, qboolean gz)
{
	int i;
	QFile *locfd;

	if (gz) {
		if (strcmp (QFS_FileExtension (filename), ".gz") != 0)
			filename = va (0, "%s.gz", filename);
		locfd = QFS_Open (filename, "z9w+");
	} else
		locfd = QFS_Open (filename, "w+");
	if (locfd == 0) {
		Sys_Printf ("ERROR: Unable to open %s\n", filename);
		return;
	}
	for (i = 0; i < locations_count; i++)
		Qprintf (locfd,"%.0f %.0f %.0f %s\n", locations[i]->loc[0] * 8,
				 locations[i]->loc[1] * 8, locations[i]->loc[2] * 8,
				 locations[i]->name);
	Qclose (locfd);
}

void
locs_mark (const vec3_t loc, const char *desc)
{
	locs_add (loc, desc);
	Sys_Printf ("Marked current location: %s\n", desc);
}

/*
    locs_edit

    call with description to modify location description
    call with NULL description to modify location vectors
*/
void
locs_edit (const vec3_t loc, const char *desc)
{
	int i;

	if (locations_count) {
		i = locs_nearest (loc);
		if (!desc) {
			VectorCopy (loc, locations[i]->loc);
			Sys_Printf ("Moving location marker for %s\n",
					locations[i]->name);
		} else {
			free ((void *) locations[i]->name);
			locations[i]->name = strdup (desc);
			Sys_Printf ("Changing location description to %s\n",
					locations[i]->name);
		}
	} else
		Sys_Printf ("Error: No location markers to modify!\n");
}

void
locs_del (const vec3_t loc)
{
	int i;

	if (locations_count) {
		i = locs_nearest (loc);
		Sys_Printf ("Removing location marker for %s\n",
				locations[i]->name);
		free ((void *) locations[i]->name);
		free ((void *) locations[i]);
		locations_count--;
		for (; i < locations_count; i++)
			locations[i] = locations[i+1];
		locations[locations_count] = NULL;
	} else
		Sys_Printf ("Error: No location markers to remove\n");
}

void
map_to_loc (const char *mapname, char *filename)
{
	char *t1;

	strcpy (filename, mapname);
	t1 = strrchr (filename, '.');
	if (!t1)
		Sys_Error ("Can't find .!");
	t1++;
	strcpy (t1, "loc");
}
