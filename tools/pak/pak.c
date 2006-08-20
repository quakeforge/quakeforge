/* 
	pak.c

	Pakfile tool

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2002 Bill Currie <bill@taniwha.org>
	Copyright (C) 2002 Jeff Teunissen <deek@quakeforge.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <QF/qtypes.h>
#include <QF/pakfile.h>

#include "pak.h"

const char	*this_program;
options_t	options;

static const struct option long_options[] = {
	{"create", no_argument, 0, 'c'},
	{"test", no_argument, 0, 't'},
	{"extract", no_argument, 0, 'x'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"pad", no_argument, 0, 'p'},
	{"quiet", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{NULL, 0, NULL, 0},
};

static void
usage (int status)
{
	printf ("%s - QuakeForge Packfile tool\n", this_program);
	printf ("Usage: %s <command> [options] ARCHIVE [FILE...]\n",
			this_program);

	printf ("Commands:\n"
			"    -c, --create              Create archive\n"
			"    -t, --test                Test archive\n"
			"    -x, --extract             Extract archive contents\n"
			"    -h, --help                Display this help and exit\n"
			"    -V, --version             Output version information and exit\n\n");

	printf ("Options:\n"
			"    -f, --file ARCHIVE        Use ARCHIVE for archive filename\n"
			"    -p, --pad                 Pad file space to a 32-bit boundary\n"
			"    -q, --quiet               Inhibit usual output\n"
			"    -v, --verbose             Display more output than usual\n");
	exit (status);
}

static int
decode_args (int argc, char **argv)
{
	int		c;

	options.mode = mo_none;
	options.pad = false;
	options.packfile = NULL;
	options.verbosity = 0;

	while ((c = getopt_long (argc, argv, "c"	// create archive
										 "t"	// test archive
										 "x"	// extract archive contents
										 "h"	// show help
										 "V"	// show version
										 "f:"	// filename
										 "p"	// pad
										 "q"	// quiet
										 "v"	// verbose
							 , long_options, (int *) 0)) != EOF) {
		switch (c) {
			case 'h':					// help
				usage (0);
				break;
			case 'V':					// version
				printf ("pak version %s\n", VERSION);
				exit (0);
				break;
			case 'c':					// create
				options.mode = mo_create;
				break;
			case 't':					// test
				options.mode = mo_test;
				break;
			case 'x':					// extract
				options.mode = mo_extract;
				break;
			case 'f':					// set filename
				options.packfile = strdup (optarg);
				break;
			case 'q':					// lower verbosity
				options.verbosity--;
				break;
			case 'p':					// pad
				options.pad = true;
				break;
			case 'v':					// increase verbosity
				options.verbosity++;
				break;
			default:
				usage (1);
		}
	}

	if ((!options.packfile) && argv[optind] && *(argv[optind]))
		options.packfile = strdup (argv[optind++]);

	return optind;
}

int
main (int argc, char **argv)
{
	pack_t	*pack;
	int		i, j, rehash = 0;
	dpackfile_t *pf;

	this_program = argv[0];

	decode_args (argc, argv);

	if (!options.packfile) {
		fprintf (stderr, "%s: no archive file specified.\n",
						 this_program);
		usage (1);
	}

	switch (options.mode) {
		case mo_extract:
			if (!(pack = pack_open (options.packfile))) {
				fprintf (stderr, "%s: error opening %s: %s\n", this_program,
								 options.packfile,
								 strerror (errno));
				return 1;
			}
			for (i = 0; i < pack->numfiles; i++) {
				pf = &pack->files[i];
				for (j = 0; j < PAK_PATH_LENGTH; j++) {
					if (!pf->name[j])
						break;
					if (pf->name[j] == '\\') {
						pf->name[j] = '/';
						rehash = 1;
					}
				}
				if (optind == argc) {
					if (options.verbosity > 0)
						printf ("%s\n", pf->name);
					pack_extract (pack, pf);
				}
			}
			if (optind < argc) {
				if (rehash)
					pack_rehash (pack);
				while (optind < argc) {
					pf = pack_find_file (pack, argv[optind]);
					if (!pf) {
						fprintf (stderr, "could not find %s\n", argv[optind]);
						optind++;
						continue;
					}
					if (options.verbosity > 0)
						printf ("%s\n", pf->name);
					pack_extract (pack, pf);
					optind++;
				}
			}
			pack_close (pack);
			break;
		case mo_test:
			if (!(pack = pack_open (options.packfile))) {
				fprintf (stderr, "%s: error opening %s: %s\n", this_program,
								 options.packfile,
								 strerror (errno));
				return 1;
			}
			for (i = 0; i < pack->numfiles; i++) {
				if (options.verbosity >= 1)
					printf ("%6d ", pack->files[i].filelen);
				if (options.verbosity >= 0)
					printf ("%s\n", pack->files[i].name);
			}
			pack_close (pack);
			break;
		case mo_create:
			pack = pack_create (options.packfile);
			if (!pack) {
				fprintf (stderr, "%s: error creating %s: %s\n", this_program,
								 options.packfile,
								 strerror (errno));
				return 1;
			}
			pack->pad = options.pad;
			while (optind < argc) {
				if (options.verbosity > 0)
					printf ("%s\n", argv[optind]);
				pack_add (pack, argv[optind++]);
			}
			pack_close (pack);
			break;
		default:
			fprintf (stderr, "%s: No command given.\n",
							 this_program);
			usage (1);
	}
	return 0;
}
