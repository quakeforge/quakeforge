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

#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/qtypes.h"

#include "compat.h"

#include "tools/qflight/include/entities.h"
#include "tools/qflight/include/options.h"
#include "tools/qflight/include/properties.h"

const char *this_program;

static struct option const long_options[] = {
	{"quiet", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"threads", required_argument, 0, 't'},
	{"properties", required_argument, 0, 'P'},
	{"attenuation", required_argument, 0, 'a'},
	{"noise", required_argument, 0, 'n'},
	{"cutoff", required_argument, 0, 'c'},
	{"extra", required_argument, 0, 'e'},
	{"novis", no_argument, 0, 'N'},
	{"distance", required_argument, 0, 'd'},
	{"range", required_argument, 0, 'r'},
	{"file", required_argument, 0, 'f'},
	{"solid-sky", no_argument, 0, 'S'},
	{NULL, 0, NULL, 0}
};

static const char *short_options =
	"q"		// quiet
	"v"		// verbose
	"h"		// help
	"V"		// version
	"N"		// novis
	"t:"	// threads
	"a:"	// attenuation
	"n:"	// noise
	"c:"	// cutoff
	"e:"	// extra sampling
	"d:"	// scale distance
	"r:"	// scale range
	"P:"	// properties file
	"f:"
	"S"		// solid-sky
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
		"    -N, --novis               Don't use vis data\n"
		"    -t, --threads [num]       Number of threads to use\n"
		"    -e, --extra [num]         Apply extra sampling\n"
		"    -d, --distance [scale]    Scale distance\n"
		"    -r, --range [scale]       Scale range\n"
		"    -a, --attenuation [type]  linear,radius,inverse,realistic,none,\n"
		"                              havoc\n"
		"    -n, --noise [factor]      Scale noise. 0 (default) to disable\n"
		"    -c, --cutoff [scale]      Scale cutoff. 0 to disable\n"
		"    -P, --properties [file]   Properties file\n"
		"    -f, --file [bspfile]      BSP file\n"
		"    -S, --solid-sky           Enable solid sky brushes\n"
		"\n");
	exit (status);
}

int
DecodeArgs (int argc, char **argv)
{
	int         c;
	char       *eptr;

	memset (&options, 0, sizeof (options));
	options.verbosity = 0;
	options.threads = 1;
	options.distance = 1.0;
	options.range = 0.5;
	options.globallightscale = 1.0;
	options.cutoff = 1.0;
	options.attenuation = LIGHT_LINEAR;
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
			case 'N':					// novis
				options.novis = 1;
				break;
			case 'V':					// version
				printf ("%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
				exit (0);
				break;
			case 't':					// threads
				options.threads = strtol (optarg, &eptr, 10);
				if (eptr == optarg || *eptr)
					usage (1);
				break;
			case 'a':
				if ((options.attenuation = parse_attenuation (optarg)) == -1)
					usage (1);
				break;
			case 'n':					// noise
				options.noise = strtod (optarg, &eptr);
				if (eptr == optarg || *eptr)
					usage (1);
				options.noise = bound (0, options.extrabit, 2);
				break;
			case 'c':					// cutoff
				options.cutoff = strtod (optarg, &eptr);
				if (eptr == optarg || *eptr)
					usage (1);
				options.cutoff = bound (0, options.extrabit, 2);
				break;
			case 'e':					// extra
				options.extrabit = strtol (optarg, &eptr, 10);
				if (eptr == optarg || *eptr)
					usage (1);
				options.extrabit = bound (0, options.extrabit, 2);
				break;
			case 'd':					// scale distance
				options.distance = strtod (optarg, &eptr);
				if (eptr == optarg || *eptr)
					usage (1);
				break;
			case 'r':					// scale range
				options.range = strtod (optarg, &eptr);
				if (eptr == optarg || *eptr)
					usage (1);
				break;
			case 'f':
				bspfile = dstring_strdup (optarg);
				break;
			case 'P':
				options.properties_filename = strdup (optarg);
				break;
			case 'S':
				options.solid_sky = true;
			default:
				usage (1);
		}
	}
	options.extrascale = 1.0 / (1 << (options.extrabit * 2));
	if ((!bspfile) && argv[optind] && *(argv[optind]))
		bspfile = dstring_strdup (argv[optind++]);

	return optind;
}
