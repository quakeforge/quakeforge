/*
	packfile.c

	pak file support

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

#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>

#include "QF/pakfile.h"
#include "QF/qendian.h"
#include "QF/quakeio.h"
#include "QF/sys.h"

static const char *
pack_get_key (const void *p, void *unused)
{
	return ((dpackfile_t *) p)->name;
}

VISIBLE pack_t *
pack_new (const char *name)
{
	pack_t *pack = calloc (sizeof (*pack), 1);

	if (!pack)
		return 0;
	pack->filename = strdup (name);
	if (!pack->filename) {
		free (pack);
		return 0;
	}
	pack->file_hash = Hash_NewTable (1021, pack_get_key, 0, 0, 0);
	if (!pack->file_hash) {
		free (pack->filename);
		free (pack);
		return 0;
	}
	return pack;
}

VISIBLE void
pack_del (pack_t *pack)
{
	if (pack->files)
		free (pack->files);
	if (pack->handle)
		Qclose (pack->handle);
	if (pack->filename)
		free (pack->filename);
	if (pack->file_hash)
		free (pack->file_hash);
	free (pack);
}

VISIBLE void
pack_rehash (pack_t *pack)
{
	int         i;

	for (i = 0; i < pack->numfiles; i++) {
		Hash_Add (pack->file_hash, &pack->files[i]);
	}
}

VISIBLE pack_t *
pack_open (const char *name)
{
	pack_t     *pack = pack_new (name);
	int         i;

	if (!pack)
		return 0;
	pack->handle = Qopen (name, "rb");
	if (!pack->handle) {
		goto error;
	}
	if (Qread (pack->handle, &pack->header, sizeof (pack->header))
		!= sizeof (pack->header)) {
		fprintf (stderr, "%s: not a pack file\n", name);
		errno = 0;
		goto error;
	}
	if (strncmp (pack->header.id, "PACK", 4)) {
		fprintf (stderr, "%s: not a pack file\n", name);
		errno = 0;
		goto error;
	}

	pack->header.dirofs = LittleLong (pack->header.dirofs);
	pack->header.dirlen = LittleLong (pack->header.dirlen);

	pack->numfiles = pack->header.dirlen / sizeof (dpackfile_t);
	pack->old_numfiles = pack->files_size = pack->numfiles;

	pack->files = malloc (pack->files_size * sizeof (dpackfile_t));
	if (!pack->files) {
		//fprintf (stderr, "out of memory\n");
		goto error;
	}
	Qseek (pack->handle, pack->header.dirofs, SEEK_SET);
	Qread (pack->handle, pack->files, pack->numfiles * sizeof (pack->files[0]));

	for (i = 0; i < pack->numfiles; i++) {
		pack->files[i].filepos = LittleLong (pack->files[i].filepos);
		pack->files[i].filelen = LittleLong (pack->files[i].filelen);
		Hash_Add (pack->file_hash, &pack->files[i]);
	}
	return pack;
error:
	pack_del (pack);
	return 0;
}

VISIBLE pack_t *
pack_create (const char *name)
{
	pack_t     *pack = pack_new (name);

	if (!pack)
		return 0;

	pack->handle = Qopen (name, "wb");
	if (!pack->handle) {
		pack_del (pack);
		return 0;
	}
	memcpy (pack->header.id, "PACK", sizeof (pack->header.id));

	Qwrite (pack->handle, &pack->header, sizeof (pack->header));

	return pack;
}

VISIBLE void
pack_close (pack_t *pack)
{
	int         i;

	if (pack->modified) {
		if (pack->numfiles > pack->old_numfiles) {
			Qseek (pack->handle, 0, SEEK_END);
			pack->header.dirofs = Qtell (pack->handle);
		}
		for (i = 0; i < pack->numfiles; i++) {
			pack->files[i].filepos = LittleLong (pack->files[i].filepos);
			pack->files[i].filelen = LittleLong (pack->files[i].filelen);
		}
		Qseek (pack->handle, pack->header.dirofs, SEEK_SET);
		Qwrite (pack->handle, pack->files,
				pack->numfiles * sizeof (pack->files[0]));
		pack->header.dirlen = pack->numfiles * sizeof (pack->files[0]);
		pack->header.dirofs = LittleLong (pack->header.dirofs);
		pack->header.dirlen = LittleLong (pack->header.dirlen);
		Qseek (pack->handle, 0, SEEK_SET);
		Qwrite (pack->handle, &pack->header, sizeof (pack->header));

		Qseek (pack->handle, 0, SEEK_END);
	}
	pack_del (pack);
}

VISIBLE int
pack_add (pack_t *pack, const char *filename)
{
	dpackfile_t *pf;
	QFile      *file;
	char        buffer[16384];
	int         bytes;

	pf = Hash_Find (pack->file_hash, filename);
	if (pf)
		return -1;
	if (pack->numfiles == pack->files_size) {
		dpackfile_t *f;

		pack->files_size += 64;

		f = realloc (pack->files, pack->files_size * sizeof (dpackfile_t));
		if (!f)
			return -1;
		pack->files = f;
	}

	file = Qopen (filename, "rb");
	if (!file)
		return -1;

	pack->modified = 1;

	pf = &pack->files[pack->numfiles++];

	if (filename[0] == '/') {
		fprintf (stderr, "removing leading /");
		filename++;
	}
	strncpy (pf->name, filename, sizeof (pf->name));
	pf->name[sizeof (pf->name) - 1] = 0;

	Qseek (pack->handle, 0, SEEK_END);
	pf->filepos = Qtell (pack->handle);
	pf->filelen = 0;
	while ((bytes = Qread (file, buffer, sizeof (buffer)))) {
		Qwrite (pack->handle, buffer, bytes);
		pf->filelen += bytes;
	}
	Qclose (file);
	if (pack->pad && pf->filelen & 3) {
		static char buf[4];
		Qwrite (pack->handle, buf, 4 - (pf->filelen & 3));
	}
	Hash_Add (pack->file_hash, pf);
	return 0;
}

VISIBLE int
pack_extract (pack_t *pack, dpackfile_t *pf)
{
	const char *name = pf->name;
	size_t      count;
	int         len;
	QFile      *file;
	char        buffer[16384];

	if (Sys_CreatePath (name) == -1)
		return -1;
	if (!(file = Qopen (name, "wb")))
		return -1;
	Qseek (pack->handle, pf->filepos, SEEK_SET);
	len = pf->filelen;
	while (len) {
		count = len;
		if (count > sizeof (buffer))
			count = sizeof (buffer);
		count = Qread (pack->handle, buffer, count);
		Qwrite (file, buffer, count);
		len -= count;
	}
	Qclose (file);
	return 0;
}

VISIBLE dpackfile_t *
pack_find_file (pack_t *pack, const char *filename)
{
	return Hash_Find (pack->file_hash, filename);
}
