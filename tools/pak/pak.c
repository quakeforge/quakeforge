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
	int         pad = 0;

	while ((c = getopt_long (argc, argv, "cf:ptvx", long_options, 0)) != -1) {
		switch (c) {
			case 'f':
				pack_file = optarg;
				break;
			case 't':
				mode = mo_test;
				break;
			case 'c':
				mode = mo_create;
				break;
			case 'x':
				mode = mo_extract;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'p':
				pad = 1;
				break;
		}
	}

	if (!pack_file) {
		fprintf (stderr, "no pak file specified\n");
		return 1;
	}

	switch (mode) {
		case mo_extract:
			pack = pack_open (pack_file);
			if (!pack) {
				fprintf (stderr, "error opening %s\n", pack_file);
				return 1;
			}
			for (i = 0; i < pack->numfiles; i++) {
				if (verbose)
					fprintf (stderr, "%s\n", pack->files[i].name);
				pack_extract (pack, &pack->files[i]);
			}
			pack_close (pack);
			break;
		case mo_test:
			pack = pack_open (pack_file);
			if (!pack) {
				fprintf (stderr, "error opening %s\n", pack_file);
				return 1;
			}
			for (i = 0; i < pack->numfiles; i++) {
				if (verbose)
					printf ("%6d %s\n", pack->files[i].filelen,
							pack->files[i].name);
				else
					printf ("%s\n", pack->files[i].name);
			}
			pack_close (pack);
			break;
		case mo_create:
			pack = pack_create (pack_file);
			if (!pack) {
				fprintf (stderr, "error creating %s\n", pack_file);
				return 1;
			}
			pack->pad = pad;
			while (optind < argc) {
				if (verbose)
					fprintf (stderr, "%s\n", argv[optind]);
				pack_add (pack, argv[optind++]);
			}
			pack_close (pack);
			break;
		default:
			fprintf (stderr, "no operation specified\n");
			return 1;
	}
	return 0;
}
