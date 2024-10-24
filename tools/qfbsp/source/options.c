/*
	options.c

	command line options handling

	Copyright (C) 2002 Bill Currie

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
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "QF/quakefs.h"

#include "tools/qfbsp/include/options.h"

/**	\addtogroup qfbsp_options
*/
//@{

const char *this_program;

enum {
	long_opt_base = 255,

	fix_point_off_plane,

	extract_textures,
	extract_entities,
	extract_hull,
	extract_model,
	smart_leak,
};

static struct option const long_options[] = {
	{"quiet",				no_argument,		0, 'q'},
	{"verbose",				no_argument,		0, 'v'},
	{"help",				no_argument,		0, 'h'},
	{"version",				no_argument,		0, 'V'},
	{"draw",				no_argument,		0, 'd'},
	{"fix-off-plane",		no_argument,		0, fix_point_off_plane},
	{"notjunc",				no_argument,		0, 't'},
	{"nofill",				no_argument,		0, 'f'},
	{"noclip",				no_argument,		0, 'c'},
	{"onlyents",			no_argument,		0, 'e'},
	{"portal",				no_argument,		0, 'p'},
	{"info",				no_argument,		0, 'i'},
	{"extract-textures",	no_argument,		0, extract_textures},
	{"extract-entities",	no_argument,		0, extract_entities},
	{"extract-hull",		no_argument,		0, extract_hull},
	{"extract-model",		no_argument,		0, extract_model},
	{"smart-leak",			no_argument,		0, smart_leak},
	{"usehulls",			no_argument,		0, 'u'},
	{"hullnum",				required_argument,	0, 'H'},
	{"subdivide",			required_argument,	0, 's'},
	{"wadpath",				required_argument,	0, 'w'},
	{"watervis",			no_argument,		0, 'W'},
	{NULL, 0, NULL, 0}
};

static const char *short_options =
	"q"		// quiet
	"v"		// verbose
	"h"		// help
	"V"		// version
	"d"		// draw
	"t"		// notjunc
	"f"		// nofill
	"c"		// noclip
	"e"		// onlyents
	"i"		// info
	"o:"	// outputfile
	"p"		// portal
	"u"		// usehulls
	"H:"	// hullnum
	"s:"	// subdivide
	"w:"	// wadpath
	"W"		// watervis
	;


static void
usage (int status)
{
	printf ("%s - QuakeForge PVS/PHS generation tool\n", this_program);
	printf ("Usage: %s [options]\n", this_program);
	printf (
		"Options:\n"
		"    -q, --quiet               Inhibit usual output\n"
		"    -v, --verbose             Display more output than usual\n"
		"    -h, --help                Display this help and exit\n"
		"    -V, --version             Output version information and exit\n"
		"    -i, --info                Display info about the bsp\n"
		"    -d, --draw\n"
		"    -t, --notjunc\n"
		"    -c, --noclip\n"
		"    -e, --onlyents\n"
		"    -p, --portal\n"
		"        --extract-hull\n"
		"        --extract-textures\n"
		"        --extract-entities\n"
		"    -u, --usehulls            Use the existing hull files\n"
		"    -H, --hullnum [num]\n"
		"    -s, --subdivide [size]\n"
		"    -w, --wadpath [path]      semicolon delimited set of dirs to\n"
		"                              search for texture wads\n"
		"    -W, --watervis            allow transparent liquid surfaces\n"
	);
	exit (status);
}

int
DecodeArgs (int argc, char **argv)
{
	int         c;

	memset (&options, 0, sizeof (options));
	options.verbosity = 1;
	options.subdivide_size = 240;

	while ((c = getopt_long (argc, argv, short_options, long_options, 0))
		   != EOF) {
		switch (c) {
			case 'q':					// quiet
				options.verbosity -= 1;
				break;
			case 'v':					// verbose
				options.verbosity += 1;
				break;
			case 'h':					// help
				usage (0);
				break;
			case 'V':					// version
				printf ("%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
				exit (0);
				break;
			case 'd':					// draw
				options.drawflag = true;
				break;
			case 't':					// notjunc
				options.notjunc = true;
				break;
			case 'f':					// nofill
				options.nofill = true;
				break;
			case 'c':					// noclip
				options.noclip = true;
				break;
			case 'e':					// onlyents
				options.onlyents = true;
				break;
			case fix_point_off_plane:
				options.fix_point_off_plane = true;
				break;
			case 'o':
				options.output_file = strdup (optarg);
				break;
			case 'i':
				options.extract = true;
				options.info = true;
				break;
			case 'p':					// portal
				options.extract = true;
				options.portal = true;
				break;
			case extract_textures:
				options.extract = true;
				options.extract_textures = true;
				break;
			case extract_entities:
				options.extract = true;
				options.extract_entities = true;
				break;
			case extract_hull:
				options.extract = true;
				options.extract_hull = true;
				break;
			case extract_model:
				options.extract = true;
				options.extract_model = true;
				break;
			case smart_leak:
				options.smart_leak = true;
				break;
			case 'u':					// usehulls
				options.usehulls = true;
				break;
			case 'H':					// hullnum
				options.hullnum = atoi (optarg);
				break;
			case 's':					// subdivide
				options.subdivide_size = atoi (optarg);
				break;
			case 'w':					// wadpath
				options.wadpath = optarg;
				break;
			case 'W':					// watervis
				options.watervis = true;
				break;
			default:
				usage (1);
		}
	}

	if (options.extract) {
		options.bspfile = strdup (argv[optind++]);
	} else {
		if (argv[optind] && *(argv[optind])) {
			options.mapfile = malloc (strlen(argv[optind]) + 5);
			strcpy (options.mapfile, argv[optind++]);
		} else {
			usage (1);
		}
		if (argv[optind] && *(argv[optind])) {
			options.bspfile = strdup (argv[optind++]);
		} else {
			options.bspfile = malloc (strlen (options.mapfile) + 5);
			QFS_StripExtension (options.mapfile, options.bspfile);
			strcat (options.bspfile, ".bsp");
		}
		options.hullfile = malloc (strlen (options.bspfile) + 5);
		options.portfile = malloc (strlen (options.bspfile) + 5);
		options.pointfile = malloc (strlen (options.bspfile) + 5);

		// create filenames
		QFS_StripExtension (options.bspfile, options.hullfile);
		strcat (options.hullfile, ".h0");

		QFS_StripExtension (options.bspfile, options.portfile);
		strcat (options.portfile, ".prt");

		QFS_StripExtension (options.bspfile, options.pointfile);
		strcat (options.pointfile, ".pts");
	}

	if (options.wadpath) {
		char       *t = malloc (strlen (options.wadpath) + 2);
		strcpy (t, ";");
		strcat (t, options.wadpath);
		options.wadpath = t;
	} else {
		options.wadpath = "";
	}

	return optind;
}

//@}
