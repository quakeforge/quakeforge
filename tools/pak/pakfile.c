#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <QF/qendian.h>

#include "pakfile.h"

static const char *
pack_get_key (void *p, void *unused)
{
	return ((dpackfile_t *) p)->name;
}

pack_t *
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
	pack->file_hash = Hash_NewTable (1021, pack_get_key, 0, 0);
	if (!pack->file_hash) {
		free (pack->filename);
		free (pack);
		return 0;
	}
	return pack;
}

void
pack_del (pack_t *pack)
{
	if (pack->files)
		free (pack->files);
	if (pack->handle)
		fclose (pack->handle);
	if (pack->filename)
		free (pack->filename);
	if (pack->file_hash)
		free (pack->file_hash);
	free (pack);
}

pack_t *
pack_open (const char *name)
{
	dpackheader_t header;
	pack_t     *pack = pack_new (name);
	int         i;

	if (!pack)
		return 0;
	pack->handle = fopen (name, "rb");
	if (!pack->handle) {
		goto error;
	}
	if (fread (&header, 1, sizeof (header), pack->handle) != sizeof (header)) {
		fprintf (stderr, "%s: not a pack file", name);
		goto error;
	}
	if (strncmp (header.id, "PACK", 4)) {
		fprintf (stderr, "%s: not a pack file", name);
		goto error;
	}

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	pack->numfiles = header.dirlen / sizeof (dpackfile_t);
	pack->files_size = pack->numfiles;
	if (pack->numfiles > MAX_FILES_IN_PACK) {
		fprintf (stderr, "%s: too many files in pack: %d", name, pack->numfiles);
		goto error;
	}
	pack->files = malloc (pack->files_size * sizeof (dpackfile_t));
	if (!pack->files) {
		fprintf (stderr, "out of memory\n");
		goto error;
	}
	fseek (pack->handle, header.dirofs, SEEK_SET);
	fread (pack->files, pack->numfiles, sizeof (pack->files[0]), pack->handle);

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

void
pack_close (pack_t *pack)
{
	pack_del (pack);
}
