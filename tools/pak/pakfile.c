#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <QF/qendian.h>

#include "pakfile.h"

#ifdef _WIN32
void *alloca(size_t size);
#endif

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
	pack_t     *pack = pack_new (name);
	int         i;

	if (!pack)
		return 0;
	pack->handle = fopen (name, "rb");
	if (!pack->handle) {
		goto error;
	}
	if (fread (&pack->header, 1, sizeof (pack->header), pack->handle)
		!= sizeof (pack->header)) {
		fprintf (stderr, "%s: not a pack file", name);
		goto error;
	}
	if (strncmp (pack->header.id, "PACK", 4)) {
		fprintf (stderr, "%s: not a pack file", name);
		goto error;
	}

	pack->header.dirofs = LittleLong (pack->header.dirofs);
	pack->header.dirlen = LittleLong (pack->header.dirlen);

	pack->numfiles = pack->header.dirlen / sizeof (dpackfile_t);
	pack->old_numfiles = pack->files_size = pack->numfiles;

	pack->files = malloc (pack->files_size * sizeof (dpackfile_t));
	if (!pack->files) {
		fprintf (stderr, "out of memory\n");
		goto error;
	}
	fseek (pack->handle, pack->header.dirofs, SEEK_SET);
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

pack_t *
pack_create (const char *name)
{
	pack_t     *pack = pack_new (name);

	if (!pack)
		return 0;

	pack->handle = fopen (name, "wb");
	if (!pack->handle) {
		pack_del (pack);
		return 0;
	}
	strncpy (pack->header.id, "PACK", sizeof (pack->header.id));

	fwrite (&pack->header, 1, sizeof (pack->header), pack->handle);

	return pack;
}

void
pack_close (pack_t *pack)
{
	int         i;

	if (pack->modified) {
		if (pack->numfiles > pack->old_numfiles) {
			fseek (pack->handle, 0, SEEK_END);
			pack->header.dirofs = ftell (pack->handle);
		}
		for (i = 0; i < pack->numfiles; i++) {
			pack->files[i].filepos = LittleLong (pack->files[i].filepos);
			pack->files[i].filelen = LittleLong (pack->files[i].filelen);
		}
		fseek (pack->handle, pack->header.dirofs, SEEK_SET);
		fwrite (pack->files, pack->numfiles,
				sizeof (pack->files[0]), pack->handle);
		pack->header.dirlen = pack->numfiles * sizeof (pack->files[0]);
		pack->header.dirofs = LittleLong (pack->header.dirofs);
		pack->header.dirlen = LittleLong (pack->numfiles
										  * sizeof (pack->files[0]));
		fseek (pack->handle, 0, SEEK_SET);
		fwrite (&pack->header, 1, sizeof (pack->header), pack->handle);

		fseek (pack->handle, 0, SEEK_END);
	}
	pack_del (pack);
}

int
pack_add (pack_t *pack, const char *filename)
{
	dpackfile_t *pf;
	FILE       *file;
	char        buffer[16384];
	int         bytes;

	pf = Hash_Find (pack->file_hash, filename);
	if (pf)
		return -1;
	if (pack->numfiles == pack->files_size) {
		dpackfile_t *f;
		
		if (pack->files_size == MAX_FILES_IN_PACK)
			return -1;
		pack->files_size += 64;
		if (pack->files_size > MAX_FILES_IN_PACK)
			pack->files_size = MAX_FILES_IN_PACK;

		f = realloc (pack->files, pack->files_size * sizeof (dpackfile_t));
		if (!f)
			return -1;
		pack->files = f;
	}

	file = fopen (filename, "rb");
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

	fseek (pack->handle, 0, SEEK_END);
	pf->filepos = ftell (pack->handle);
	pf->filelen = 0;
	while ((bytes = fread (buffer, 1, sizeof (buffer), file))) {
		fwrite (buffer, 1, bytes, pack->handle);
		pf->filelen += bytes;
	}
	fclose (file);
	if (pack->pad && pf->filelen & 3) {
		static char buf[4];
		fwrite (buf, 1, 4 - (pf->filelen & 3), pack->handle);
	}
	Hash_Add (pack->file_hash, pf);
	return 0;
}

static int
make_parents (const char *_path)
{
	char       *path;
	char       *d, *p, t;

	path = (char *) alloca (strlen (_path) + 1);
	strcpy (path, _path);
	for (p = path; *p && (d = strchr (p, '/')); p = d + 1) {
		t = *d;
		*d = 0;
#ifdef WIN32
		if (mkdir (path) < 0)
#else
		if (mkdir (path, 0777) < 0)
#endif
			if (errno != EEXIST)
				return -1;
		*d = t;
	}
	return 0;
}

int
pack_extract (pack_t *pack, dpackfile_t *pf)
{
	const char *name = pf->name;
	int         count;
	int         len;
	FILE       *file;
	char        buffer[16384];

	if (make_parents (name) == -1)
		return -1;
	if (!(file = fopen (name, "wb")))
		return -1;
	fseek (pack->handle, pf->filepos, SEEK_SET);
	len = pf->filelen;
	while (len) {
		count = len;
		if (count > sizeof (buffer))
			count = sizeof (buffer);
		count = fread (buffer, 1, count, pack->handle);
		fwrite (buffer, 1, count, file);
		len -= count;
	}
	fclose (file);
	return 0;
}
