/* 
	wad.c

	wadfile tool

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

static __attribute__ ((unused)) const char rcsid[] =
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
#include <QF/wadfile.h>

#include "wad.h"

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
	options.wadfile = NULL;
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
				printf ("wad version %s\n", VERSION);
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
				options.wadfile = strdup (optarg);
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

	if ((!options.wadfile) && argv[optind] && *(argv[optind]))
		options.wadfile = strdup (argv[optind++]);

	return optind;
}

int
main (int argc, char **argv)
{
	wad_t      *wad;
	int         i;//, j, rehash = 0;
	lumpinfo_t *pf;

	this_program = argv[0];

	decode_args (argc, argv);

	if (!options.wadfile) {
		fprintf (stderr, "%s: no archive file specified.\n",
						 this_program);
		usage (1);
	}

	switch (options.mode) {
		case mo_extract:
			if (!(wad = wad_open (options.wadfile))) {
				fprintf (stderr, "%s: error opening %s: %s\n", this_program,
								 options.wadfile,
								 strerror (errno));
				return 1;
			}
			for (i = 0; i < wad->numlumps; i++) {
				pf = &wad->lumps[i];
				if (optind == argc) {
					if (options.verbosity > 0)
						printf ("%s\n", pf->name);
					wad_extract (wad, pf);
				}
			}
			if (optind < argc) {
				while (optind < argc) {
					pf = wad_find_lump (wad, argv[optind]);
					if (!pf) {
						fprintf (stderr, "could not find %s\n", argv[optind]);
						continue;
					}
					if (options.verbosity > 0)
						printf ("%s\n", pf->name);
					wad_extract (wad, pf);
					optind++;
				}
			}
			wad_close (wad);
			break;
		case mo_test:
			if (!(wad = wad_open (options.wadfile))) {
				fprintf (stderr, "%s: error opening %s: %s\n", this_program,
								 options.wadfile,
								 strerror (errno));
				return 1;
			}
			for (i = 0; i < wad->numlumps; i++) {
				if (options.verbosity >= 1)
					printf ("%6d ", wad->lumps[i].size);
				if (options.verbosity >= 0)
					printf ("%s\n", wad->lumps[i].name);
			}
			wad_close (wad);
			break;
		case mo_create:
/*			wad = wad_create (options.wadfile);
			if (!wad) {
				fprintf (stderr, "%s: error creating %s: %s\n", this_program,
								 options.wadfile,
								 strerror (errno));
				return 1;
			}
			wad->pad = options.pad;
			while (optind < argc) {
				if (options.verbosity > 0)
					printf ("%s\n", argv[optind]);
				wad_add (wad, argv[optind++]);
			}
			wad_close (wad);*/
			break;
		default:
			fprintf (stderr, "%s: No command given.\n",
							 this_program);
			usage (1);
	}
	return 0;
}
