#include <stdio.h>

#include <QF/hash.h>
#include <QF/pak.h>

typedef struct pack_s {
	char       *filename;
	FILE       *handle;
	int         numfiles;
	int         files_size;
	dpackfile_t *files;
	hashtab_t  *file_hash;
} pack_t;

pack_t *pack_new (const char *name);

void pack_del (pack_t *pack);

pack_t *pack_open (const char *name);

void pack_close (pack_t *pack);
