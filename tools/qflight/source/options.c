/*
	options.c

	command line options handling

	Copyright (C) 2002 Colin Thompson

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
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <getopt.h>
#include <errno.h>
#include <stdlib.h>

#include "QF/qtypes.h"

#include "options.h"

const char *this_program;

static struct option const long_options[] = {
	{"quiet", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"threads", required_argument, 0, 't'},
	{"extra", no_argument, 0, 'e'},
	{"distance", required_argument, 0, 'd'},	
	{"range", required_argument, 0, 'r'},
	{"file", required_argument, 0, 'f'},
	{NULL, 0, NULL, 0}
};

static const char *short_options =
	"q"		// quiet
	"v"		// verbose
	"h"		// help
	"V"		// version
	"t:"	// threads
	"e"		// extra sampling
	"d:"	// scale distance
	"r:"	// scale range
	"f:"
	;

void
usage (int status)
{
	printf ("%s - QuakeForge light tool\n", this_program);
	printf ("Usage: %s [options] bspfile\n", this_program);
	printf (
		"Options:\n"
		"    -q, --quiet               Inhibit usual output\n"
		"    -v, --verbose             Display more output than usual\n"
		"    -h, --help                Display this help and exit\n"
		"    -V, --version             Output version information and exit\n"
		"    -t, --threads [num]       Number of threads to use\n"
		"    -e, --extra               Apply extra sampling\n"
		"    -d, --distance [scale]    Scale distance\n"
		"    -r, --range [scale]       Scale range\n"
		"    -f, --file [bspfile]      BSP file\n\n");
	exit (status);
}

int
DecodeArgs (int argc, char **argv)
{
	int		c;

	options.verbosity = 0;
	options.threads = 1;
	options.extra = false;
	options.distance = 1.0;
	options.range = 0.5;
	bspfile = NULL;

	while ((c = getopt_long (argc, argv, short_options, long_options, 0))
		   != EOF) {
		switch (c) {
			case 'q':				// quiet
				options.verbosity -= 1;
				break;
			case 'v':					// verbose
				options.verbosity += 1;
				break;
			case 'h':					// help
				usage (0);
				break;
			case 'V':					// version
				printf ("%s version %s\n", PACKAGE, VERSION);
				exit (0);
				break;
			case 't':					// threads
				options.threads = atoi (optarg);
				break;
			case 'e':					// extra
				options.extra = true;
				break;
			case 'd':					// scale distance
				options.distance = atof (optarg);
				break;
			case 'r':					// scale range
				options.range = atof (optarg);
				break;
			case 'f':
				bspfile = strdup (optarg);
				break;
			default:
				usage (1);
		}
	}
	if ((!bspfile) && argv[optind] && *(argv[optind]))
		bspfile = strdup (argv[optind++]);

	return optind;
}
