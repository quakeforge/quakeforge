#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
	Structs for pack files on disk
*/

#define PAK_PATH_LENGTH 56

typedef struct {
	char        name[PAK_PATH_LENGTH];
	int         filepos, filelen;
} dpackfile_t;

typedef struct {
	char        id[4];
	int         dirofs;
	int         dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK   2048

typedef struct pack_s {
	char        filename[MAX_PATH];
	FILE       *handle;
	int         numfiles;
	int         files_size;
	dpackfile_t *files;
} pack_t;

pack_t *
new_pack (const char *name)
{
	pack_t *pack = calloc (sizeof (*pack), 1);

	if (!pack)
		return 0;
	strncpy (pack->filename, name, sizeof (pack->filename));
	pack->filename[sizeof (pack->filename) - 1] = 0;
	return pack;
}

void
del_pack (pack_t *pack)
{
	if (pack->files)
		free (pack->files);
	if (pack->handle)
		fclose (pack->handle);
	free (pack);
}

pack_t *
open_pack (const char *name)
{
	dpackheader_t header;
	pack_t *pack = new_pack (name);

	if (!pack)
		return 0;
	pack->handle = fopen (name, "rb");
	if (!pack->handle) {
		goto error;
	}
	if (fread (&header, sizeof (header), 1, pack->handle) != sizeof (header)) {
		fprintf (stderr, "%s: not a pack file", name);
		goto error;
	}
	if (strncmp (header.id, "PACK", 4)) {
		fprintf (stderr, "%s: not a pack file", name);
		goto error;
	}
	pack->numfiles = header.dirlen / sizeof (dpackfile_t);
	pack->files_size = pack->numfiles;
	if (pack->numfiles > MAX_FILES_IN_PACK) {
		fprintf (stderr, "%s: too many files in pack: %d", name, pack->numfiles);
		goto error;
	}
	pack->files = malloc (numpackfiles * sizeof (packfile_t));
	if (!pack->files) {
		fprintf (stderr, "out of memory\n");
		goto error;
	}
	fseek (pack->handle, header.diroffs, SEEK_POS);
	fread (pack->files, pack->numfiles, sizeof (pack->files[0]), pack->handle);
error:
	del_pack (pack);
	return 0;
}
