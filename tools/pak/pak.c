#include <getopt.h>

#include "pakfile.h"

static const struct option long_options[] = {
	{NULL, 0, NULL, 0},
};

typedef enum {
	mo_none,
	mo_test,
	mo_create,
	mo_extract,
} mode_t;

int
main (int argc, char **argv)
{
	pack_t     *pack;
	mode_t      mode = mo_none;
	int         i;
	int         c;
	const char *pack_file = 0;
	int         verbose = 0;

	while ((c = getopt_long (argc, argv, "f:tv", long_options, 0)) != -1) {
		switch (c) {
			case 'f':
				pack_file = optarg;
				break;
			case 't':
				mode = mo_test;
				break;
			case 'v':
				verbose = 1;
				break;
		}
	}

	if (!pack_file) {
		fprintf (stderr, "no pak file specified\n");
		return 1;
	}

	switch (mode) {
		case mo_test:
			pack = pack_open (pack_file);
			if (!pack) {
				fprintf (stderr, "error opening %s\n", pack_file);
				return 1;
			}
			for (i = 0; i < pack->numfiles; i++)
				printf ("%6d %s\n", pack->files[i].filelen,
						pack->files[i].name);
			pack_close (pack);
			break;
		default:
			fprintf (stderr, "no operation specified\n");
			return 1;
	}
	return 0;
}
