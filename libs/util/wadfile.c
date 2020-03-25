/*
	wadfile.c

	wad file support

	Copyright (C) 2003 #AUTHOR#

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "QF/hash.h"
#include "QF/qendian.h"
#include "QF/wad.h"

// case insensitive hash and compare
static uintptr_t
wad_get_hash (const void *l, void *unused)
{
	char        name[17];
	int         i;

	for (i = 0; i < 16; i++)
		name[i] = tolower ((byte) ((lumpinfo_t *) l)->name[i]);
	name[16] = 0;
	return Hash_String (name);
}

static int
wad_compare (const void *la, const void *lb, void *unused)
{
	return strncasecmp (((lumpinfo_t *) la)->name,
						((lumpinfo_t *) lb)->name, 16) == 0;
}

VISIBLE wad_t *
wad_new (const char *name)
{
	wad_t *wad = calloc (sizeof (*wad), 1);

	if (!wad)
		return 0;
	wad->filename = strdup (name);
	if (!wad->filename) {
		free (wad);
		return 0;
	}
	wad->lump_hash = Hash_NewTable (1021, 0, 0, 0, 0);
	if (!wad->lump_hash) {
		free (wad->filename);
		free (wad);
		return 0;
	}
	Hash_SetHashCompare (wad->lump_hash, wad_get_hash, wad_compare);
	return wad;
}

VISIBLE void
wad_del (wad_t *wad)
{
	if (wad->lumps)
		free (wad->lumps);
	if (wad->handle)
		Qclose (wad->handle);
	if (wad->filename)
		free (wad->filename);
	if (wad->lump_hash)
		free (wad->lump_hash);
	free (wad);
}

VISIBLE void
wad_rehash (wad_t *wad)
{
	int         i;

	for (i = 0; i < wad->numlumps; i++) {
		Hash_AddElement (wad->lump_hash, &wad->lumps[i]);
	}
}

VISIBLE wad_t *
wad_open (const char *name)
{
	wad_t     *wad = wad_new (name);
	int         i;

	if (!wad)
		return 0;
	wad->handle = Qopen (name, "rbz");
	if (!wad->handle) {
		goto error;
	}
	if (Qread (wad->handle, &wad->header, sizeof (wad->header))
		!= sizeof (wad->header)) {
		fprintf (stderr, "%s: not a wad file\n", name);
		errno = 0;
		goto error;
	}
	if (strncmp (wad->header.id, "WAD2", 4)) {
		fprintf (stderr, "%s: not a wad file\n", name);
		errno = 0;
		goto error;
	}

	wad->header.infotableofs = LittleLong (wad->header.infotableofs);
	wad->header.numlumps = LittleLong (wad->header.numlumps);

	wad->numlumps = wad->header.numlumps;
	wad->old_numlumps = wad->lumps_size = wad->numlumps;

	wad->lumps = malloc (wad->lumps_size * sizeof (lumpinfo_t));
	if (!wad->lumps) {
		//fprintf (stderr, "out of memory\n");
		goto error;
	}
	Qseek (wad->handle, wad->header.infotableofs, SEEK_SET);
	Qread (wad->handle, wad->lumps, wad->numlumps * sizeof (wad->lumps[0]));

	for (i = 0; i < wad->numlumps; i++) {
		wad->lumps[i].filepos = LittleLong (wad->lumps[i].filepos);
		wad->lumps[i].size = LittleLong (wad->lumps[i].size);
		Hash_AddElement (wad->lump_hash, &wad->lumps[i]);
	}
	return wad;
error:
	wad_del (wad);
	return 0;
}

VISIBLE wad_t *
wad_create (const char *name)
{
	wad_t     *wad = wad_new (name);

	if (!wad)
		return 0;

	wad->handle = Qopen (name, "wb");
	if (!wad->handle) {
		wad_del (wad);
		return 0;
	}
	memcpy (wad->header.id, "WAD2", sizeof (wad->header.id));

	Qwrite (wad->handle, &wad->header, sizeof (wad->header));

	return wad;
}

VISIBLE void
wad_close (wad_t *wad)
{
	int         i;

	if (wad->modified) {
		if (wad->numlumps > wad->old_numlumps) {
			Qseek (wad->handle, 0, SEEK_END);
			wad->header.infotableofs = Qtell (wad->handle);
		}
		for (i = 0; i < wad->numlumps; i++) {
			wad->lumps[i].filepos = LittleLong (wad->lumps[i].filepos);
			wad->lumps[i].size = LittleLong (wad->lumps[i].size);
		}
		Qseek (wad->handle, wad->header.infotableofs, SEEK_SET);
		Qwrite (wad->handle, wad->lumps,
				wad->numlumps * sizeof (wad->lumps[0]));
		wad->header.infotableofs = LittleLong (wad->header.infotableofs);
		wad->header.numlumps = LittleLong (wad->numlumps);
		Qseek (wad->handle, 0, SEEK_SET);
		Qwrite (wad->handle, &wad->header, sizeof (wad->header));

		Qseek (wad->handle, 0, SEEK_END);
	}
	wad_del (wad);
}

VISIBLE int
wad_add (wad_t *wad, const char *filename, const char *lumpname, byte type)
{
	lumpinfo_t *pf;
	lumpinfo_t  dummy;
	QFile      *file;
	char        buffer[16384];
	int         bytes;

	strncpy (dummy.name, lumpname, 16);
	dummy.name[15] = 0;

	pf = Hash_FindElement (wad->lump_hash, &dummy);
	if (pf)
		return -1;
	if (wad->numlumps == wad->lumps_size) {
		lumpinfo_t *f;

		wad->lumps_size += 64;

		f = realloc (wad->lumps, wad->lumps_size * sizeof (lumpinfo_t));
		if (!f)
			return -1;
		wad->lumps = f;
	}

	file = Qopen (filename, "rb");
	if (!file)
		return -1;

	wad->modified = 1;

	pf = &wad->lumps[wad->numlumps++];

	strncpy (pf->name, lumpname, sizeof (pf->name));
	pf->name[sizeof (pf->name) - 1] = 0;

	Qseek (wad->handle, 0, SEEK_END);
	pf->filepos = Qtell (wad->handle);
	pf->type = type;
	pf->size = 0;
	while ((bytes = Qread (file, buffer, sizeof (buffer)))) {
		Qwrite (wad->handle, buffer, bytes);
		pf->size += bytes;
	}
	Qclose (file);
	if (wad->pad && pf->size & 3) {
		static char buf[4];
		Qwrite (wad->handle, buf, 4 - (pf->size & 3));
	}
	pf->disksize = pf->size;
	Hash_AddElement (wad->lump_hash, pf);
	return 0;
}

VISIBLE int
wad_add_data (wad_t *wad, const char *lumpname, byte type, const void *data,
			  int bytes)
{
	lumpinfo_t *pf;
	lumpinfo_t  dummy;

	strncpy (dummy.name, lumpname, 16);
	dummy.name[15] = 0;

	pf = Hash_FindElement (wad->lump_hash, &dummy);
	if (pf)
		return -1;
	if (wad->numlumps == wad->lumps_size) {
		lumpinfo_t *f;

		wad->lumps_size += 64;

		f = realloc (wad->lumps, wad->lumps_size * sizeof (lumpinfo_t));
		if (!f)
			return -1;
		wad->lumps = f;
	}

	wad->modified = 1;

	pf = &wad->lumps[wad->numlumps++];

	strncpy (pf->name, lumpname, sizeof (pf->name));
	pf->name[sizeof (pf->name) - 1] = 0;

	Qseek (wad->handle, 0, SEEK_END);
	pf->filepos = Qtell (wad->handle);
	pf->type = type;
	pf->size = bytes;

	Qwrite (wad->handle, data, bytes);

	if (wad->pad && pf->size & 3) {
		static char buf[4];
		Qwrite (wad->handle, buf, 4 - (pf->size & 3));
	}
	pf->disksize = pf->size;
	Hash_AddElement (wad->lump_hash, pf);
	return 0;
}

VISIBLE lumpinfo_t *
wad_find_lump (wad_t *wad, const char *lumpname)
{
	lumpinfo_t  dummy;
	strncpy (dummy.name, lumpname, 16);
	dummy.name[15] = 0;
	return Hash_FindElement (wad->lump_hash, &dummy);
}
