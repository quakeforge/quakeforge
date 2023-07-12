/*
	lmp.c

	lump file tool

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2002 Bill Currie <bill@taniwha.org>
	Copyright (C) 2002,2011 Jeff Teunissen <deek@quakeforge.net>

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
/*
	read 4 bytes -> width
	read 4 bytes -> height
	width * height bytes
	no attached palette
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <string.h>

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

#include <QF/cmd.h>
#include <QF/cvar.h>
#include <QF/image.h>
#include <QF/pcx.h>
#include <QF/qtypes.h>
#include <QF/qendian.h>
#include <QF/quakeio.h>
#include <QF/sys.h>
#include <QF/va.h>
#include <QF/zone.h>

#include "lmp.h"

#define MEMSIZE (8 * 1024 * 1024)

const char	*this_program;

options_t	options;
Pixel		*palette;

static const struct option long_options[] = {
	{"export", no_argument, 0, 'e'},
	{"import", no_argument, 0, 'i'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"geometry", required_argument, 0, 'g'},
	{"geometry", required_argument, 0, 'g'},
	{"palette", required_argument, 0, 'p'},
	{"raw", no_argument, 0, 'r'},
	{"quiet", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{NULL, 0, NULL, 0},
};


static char *
replaceExtension (const char *oldstr, const char *extension)
{
	char *tmp = strdup (oldstr);
	char *blank = strrchr (tmp, '.');
	char *newstr;

	*blank = 0;
	newstr = nva ("%s.%s", tmp, extension);
	free (tmp);
	return newstr;
}


static void
usage (int status)
{
	Sys_Printf ("%s - QuakeForge Lump tool\n", this_program);
	Sys_Printf ("Usage: %s <command> [options] [FILE...]\n", this_program);

	Sys_Printf ("Commands:\n"
			"    -e, --export              Export lump to PCX\n"
			"    -i, --import              Import lump from PCX\n"
			"    -h, --help                Display this help and exit\n"
			"    -V, --version             Output version information and exit\n\n");

	Sys_Printf ("Options:\n"
			"    -g, --geometry WIDTHxHEIGHT Ignore geometry for image data\n"
			"                              (Warning: does not check validity of input)\n"
			"    -p, --palette FILE        Use palette FILE for conversion\n"
			"    -r, --raw                 File(s) are raw (no lump header)\n"
			"                              (must use --geometry to be useful)\n"
			"    -q, --quiet               Inhibit usual output\n"
			"    -v, --verbose             Display more output than usual\n");
	exit (status);
}


static bool
exportFile (const char *inpath)
{
	char 		*outpath = replaceExtension (inpath, "pcx");
	QFile		*infile = Qopen (inpath, "rb");
	QFile		*outfile = Qopen (outpath, "wb");
	int			fsize = Qfilesize (infile);
	pcx_t		*pcx;
	int			pcx_size;
	int32_t		width;
	int32_t		height;
	void		*data;
	bool		ret = false;

	if (options.verbosity > 1)
		Sys_Printf ("file size: %d\n", fsize);

	if (options.raw) {
		width = options.width;
		height = options.height;
		if (width <= 0 || height <= 0) {
			Sys_Printf ("%s: cowardly refusing to write a raw image with no geometry\n",
						this_program);
			goto die;
		}
	} else {
		Qread (infile, &width, sizeof (width));
		Qread (infile, &height, sizeof (height));
		fsize -= 8;
		if (options.width > 0 && options.height > 0) {
			width = options.width;
			height = options.height;
		} else {
			width = LittleLong (width);
			height = LittleLong (height);
		}
	}

	if (options.verbosity > 1)
		Sys_Printf ("dimensions: %dx%d\n", width, height);

	if (options.verbosity > 1)
		Sys_Printf ("data size: %d\n", fsize);

	if ((width * height) > fsize) {
		Sys_Printf ("%s: %s: not enough image data for a %dx%d image.\n",
					this_program, inpath, width, height);
		goto die;
	}

	data = malloc (fsize);
	Qread (infile, data, fsize);

	pcx = EncodePCX (data, width, height, width, (byte *) palette, false, &pcx_size);
	free (data);

	if (options.verbosity > 1)
		Sys_Printf ("PCX data size: %d\n", pcx_size);

	if (Qwrite (outfile, pcx, pcx_size) != pcx_size) {
		Sys_Printf ("%s: Error writing to %s\n", this_program, outpath);
		goto die;
	}
	ret = true;

die:
	if (outpath)
		free (outpath);

	Qclose (infile);
	Qclose (outfile);

	return ret;
}


static bool
importFile (const char *inpath)
{
	char 		*outpath = replaceExtension (inpath, "lmp");
	QFile		*infile = Qopen (inpath, "rb");
	QFile		*outfile = Qopen (outpath, "wb");
	int			fsize = Qfilesize (infile);
	tex_t		*lmp;
	bool		ret = false;

	if (options.verbosity > 1)
		Sys_Printf ("PCX file size: %d\n", fsize);

	lmp = LoadPCX (infile, false, NULL, 1);

	if (!lmp) {
		Sys_Printf ("%s: Failed to load %s as texture.\n",
					this_program, inpath);
		goto die;
	}

	if (lmp->format != tex_palette) {
		Sys_Printf ("%s: %s is not a paletted image.\n", this_program, inpath);
		goto die;
	} else {	// got a paletted image
		int32_t		width = lmp->width;
		int32_t		height = lmp->height;
		int			lmp_size = width * height;

		if (options.verbosity > 1)
			Sys_Printf ("geometry: %dx%d\ndata size: %d\n",
						width, height, lmp_size);

		if (!options.raw) {	// write lump header
			Qwrite (outfile, &width, sizeof (width));
			Qwrite (outfile, &height, sizeof (height));
		}

		if (Qwrite (outfile, lmp->data, lmp_size) != lmp_size) {
			Sys_Printf ("%s: Error writing to %s\n", this_program, outpath);
			goto die;
		}
		ret = true;
	}

die:
	if (outpath)
		free (outpath);

	Qclose (infile);
	Qclose (outfile);

	return ret;
}


static int
decode_args (int argc, char **argv)
{
	int		c;

	options.mode = mo_none;
	options.verbosity = 0;
	options.raw = false;
	options.width = options.height = -1;

	while ((c = getopt_long (argc, argv, "e"	// export pcx
										 "i"	// import pcx
										 "h"	// show help
										 "V"	// show version
										 "g:"	// geometry
										 "p:"	// palette
										 "r"	// raw
										 "q"	// quiet
										 "v"	// verbose
							 , long_options, (int *) 0)) != EOF) {
		switch (c) {
			case 'h':	// help
				usage (0);
				break;
			case 'V':	// version
				printf ("lmp version %s\n", PACKAGE_VERSION);
				exit (0);
				break;
			case 'e':	// export
				options.mode = mo_export;
				break;
			case 'i':	// import
				options.mode = mo_import;
				break;
			case 'g':
				{	// set geometry
					char *tmp = strdup (optarg);
					char *blank = strrchr (tmp, 'x');
					if (blank && *blank) {
						char *first, *second;

						first = tmp;
						second = blank + 1;

						options.width = atoi (first);
						options.height = atoi (second);
					}
				}
				break;
			case 'p':	// palette
				options.palette = strdup (optarg);
				break;
			case 'r':	// raw
				options.raw = 1;
				break;
			case 'q':	// lower verbosity
				options.verbosity--;
				break;
			case 'v':					// increase verbosity
				options.verbosity++;
				break;
			default:
				usage (1);
		}
	}

	return optind;
}

int
main (int argc, char **argv)
{
	QFile	*infile;
	size_t	ret;

	this_program = argv[0];

	Sys_Init ();
	Memory_Init (Sys_Alloc (MEMSIZE), MEMSIZE);

	decode_args (argc, argv);

	if (options.palette) {
		if ((infile = Qopen (options.palette, "rb"))) {
			// read in the palette
			palette = malloc (sizeof (quake_palette));
			if (!(ret = Qread (infile, palette, sizeof (quake_palette)))) {
				Sys_Printf ("%s: failed to read palette from file \"%s\"\n",
							this_program, options.palette);
				Qclose (infile);
				exit (1);
			}
		} else {
			Sys_Printf ("%s: Couldn't open palette file \"%s\"\n",
						this_program, options.palette);
		}
		Qclose (infile);
	} else {
		palette = quake_palette;
	}

	switch (options.mode) {
		case mo_export:
			while (optind < argc) {
				if (options.verbosity > 0)
					Sys_Printf ("Exporting %s to PCX...\n", argv[optind]);
				exportFile (argv[optind]);
				optind++;
			}
			break;

		case mo_import:
			while (optind < argc) {
				if (options.verbosity > 0)
					Sys_Printf ("Converting %s to lump...\n", argv[optind]);
				importFile (argv[optind]);
				optind++;
			}
			break;

		default:
			Sys_Printf ("%s: No command given.\n", this_program);
			usage (1);
	}

#if 0
	for (i = 1; i < argc; i++) {
		int32_t	width = 0;
		int32_t	height = 0;
		QFile	*outfile;
		char	*in = argv[i];
		char	*out;
		unsigned char	*line;
		int		j;

		Sys_Printf ("%s\n", in);
		asprintf (&out, "%s.ppm", in);

		infile = Qopen (in, "rb");
		outfile = Qopen (out, "wb");

		// We use ASCII mode because we can put comments into it.
		Qprintf (outfile, "P3\n# %s\n", out);
		if (strcasestr (in, "conchars")) {	// don't read w/h from conchars
			width = height = 128;
		} else {
			Qread (infile, &width, sizeof (width));
			Qread (infile, &height, sizeof (height));
		}
		Qprintf (outfile, "%d\n", width);
		Qprintf (outfile, "%d\n", height);
		Qprintf (outfile, "255\n");

		for (j = 0; j < height; j++) {
			int k;

			line = malloc (width);	// doing a line at a time
			ret = Qread (infile, line, width);
			for (k = 0; k < width; k++) {
				Sys_Printf ("%u %d %d\n", (unsigned) ret, k, line[k]);
				if (k > 239)
					Qprintf (outfile, "# next pixel fullbright\n");

				Qprintf (outfile, "%3d %3d %3d\n",
							palette[line[k]].color.red,
							palette[line[k]].color.green,
							palette[line[k]].color.blue);
			}
			Qprintf (outfile, "# next line\n");
			free (line);
		}
		Qclose (infile);
		Qclose (outfile);
		free (out);
	}
#endif

	return 0;
}
