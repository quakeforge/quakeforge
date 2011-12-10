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
#include <math.h>

#include <QF/bspfile.h>
#include <QF/cmd.h>
#include <QF/cvar.h>
#include <QF/dstring.h>
#include <QF/pcx.h>
#include <QF/qtypes.h>
#include <QF/qendian.h>
#include <QF/quakeio.h>
#include <QF/wadfile.h>
#include <QF/sys.h>
#include <QF/zone.h>

#include "wad.h"

#define MEMSIZE (12 * 1024 * 1024)

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
	{"nomip", no_argument, 0, 256},
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
			"        --nomip               Do not output mipmaps with textures\n"
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
				printf ("wad version %s\n", PACKAGE_VERSION);
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
			case 256:
				options.nomip = 1;
				break;
			default:
				usage (1);
		}
	}

	if ((!options.wadfile) && argv[optind] && *(argv[optind]))
		options.wadfile = strdup (argv[optind++]);

	return optind;
}

byte default_palette[] = {
	0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x1F, 0x1F, 0x1F, 0x2F, 0x2F, 0x2F,
	0x3F, 0x3F, 0x3F, 0x4B, 0x4B, 0x4B, 0x5B, 0x5B, 0x5B, 0x6B, 0x6B, 0x6B,
	0x7B, 0x7B, 0x7B, 0x8B, 0x8B, 0x8B, 0x9B, 0x9B, 0x9B, 0xAB, 0xAB, 0xAB,
	0xBB, 0xBB, 0xBB, 0xCB, 0xCB, 0xCB, 0xDB, 0xDB, 0xDB, 0xEB, 0xEB, 0xEB,
	0x0F, 0x0B, 0x07, 0x17, 0x0F, 0x0B, 0x1F, 0x17, 0x0B, 0x27, 0x1B, 0x0F,
	0x2F, 0x23, 0x13, 0x37, 0x2B, 0x17, 0x3F, 0x2F, 0x17, 0x4B, 0x37, 0x1B,
	0x53, 0x3B, 0x1B, 0x5B, 0x43, 0x1F, 0x63, 0x4B, 0x1F, 0x6B, 0x53, 0x1F,
	0x73, 0x57, 0x1F, 0x7B, 0x5F, 0x23, 0x83, 0x67, 0x23, 0x8F, 0x6F, 0x23,
	0x0B, 0x0B, 0x0F, 0x13, 0x13, 0x1B, 0x1B, 0x1B, 0x27, 0x27, 0x27, 0x33,
	0x2F, 0x2F, 0x3F, 0x37, 0x37, 0x4B, 0x3F, 0x3F, 0x57, 0x47, 0x47, 0x67,
	0x4F, 0x4F, 0x73, 0x5B, 0x5B, 0x7F, 0x63, 0x63, 0x8B, 0x6B, 0x6B, 0x97,
	0x73, 0x73, 0xA3, 0x7B, 0x7B, 0xAF, 0x83, 0x83, 0xBB, 0x8B, 0x8B, 0xCB,
	0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x0B, 0x0B, 0x00, 0x13, 0x13, 0x00,
	0x1B, 0x1B, 0x00, 0x23, 0x23, 0x00, 0x2B, 0x2B, 0x07, 0x2F, 0x2F, 0x07,
	0x37, 0x37, 0x07, 0x3F, 0x3F, 0x07, 0x47, 0x47, 0x07, 0x4B, 0x4B, 0x0B,
	0x53, 0x53, 0x0B, 0x5B, 0x5B, 0x0B, 0x63, 0x63, 0x0B, 0x6B, 0x6B, 0x0F,
	0x07, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x17, 0x00, 0x00, 0x1F, 0x00, 0x00,
	0x27, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x37, 0x00, 0x00, 0x3F, 0x00, 0x00,
	0x47, 0x00, 0x00, 0x4F, 0x00, 0x00, 0x57, 0x00, 0x00, 0x5F, 0x00, 0x00,
	0x67, 0x00, 0x00, 0x6F, 0x00, 0x00, 0x77, 0x00, 0x00, 0x7F, 0x00, 0x00,
	0x13, 0x13, 0x00, 0x1B, 0x1B, 0x00, 0x23, 0x23, 0x00, 0x2F, 0x2B, 0x00,
	0x37, 0x2F, 0x00, 0x43, 0x37, 0x00, 0x4B, 0x3B, 0x07, 0x57, 0x43, 0x07,
	0x5F, 0x47, 0x07, 0x6B, 0x4B, 0x0B, 0x77, 0x53, 0x0F, 0x83, 0x57, 0x13,
	0x8B, 0x5B, 0x13, 0x97, 0x5F, 0x1B, 0xA3, 0x63, 0x1F, 0xAF, 0x67, 0x23,
	0x23, 0x13, 0x07, 0x2F, 0x17, 0x0B, 0x3B, 0x1F, 0x0F, 0x4B, 0x23, 0x13,
	0x57, 0x2B, 0x17, 0x63, 0x2F, 0x1F, 0x73, 0x37, 0x23, 0x7F, 0x3B, 0x2B,
	0x8F, 0x43, 0x33, 0x9F, 0x4F, 0x33, 0xAF, 0x63, 0x2F, 0xBF, 0x77, 0x2F,
	0xCF, 0x8F, 0x2B, 0xDF, 0xAB, 0x27, 0xEF, 0xCB, 0x1F, 0xFF, 0xF3, 0x1B,
	0x0B, 0x07, 0x00, 0x1B, 0x13, 0x00, 0x2B, 0x23, 0x0F, 0x37, 0x2B, 0x13,
	0x47, 0x33, 0x1B, 0x53, 0x37, 0x23, 0x63, 0x3F, 0x2B, 0x6F, 0x47, 0x33,
	0x7F, 0x53, 0x3F, 0x8B, 0x5F, 0x47, 0x9B, 0x6B, 0x53, 0xA7, 0x7B, 0x5F,
	0xB7, 0x87, 0x6B, 0xC3, 0x93, 0x7B, 0xD3, 0xA3, 0x8B, 0xE3, 0xB3, 0x97,
	0xAB, 0x8B, 0xA3, 0x9F, 0x7F, 0x97, 0x93, 0x73, 0x87, 0x8B, 0x67, 0x7B,
	0x7F, 0x5B, 0x6F, 0x77, 0x53, 0x63, 0x6B, 0x4B, 0x57, 0x5F, 0x3F, 0x4B,
	0x57, 0x37, 0x43, 0x4B, 0x2F, 0x37, 0x43, 0x27, 0x2F, 0x37, 0x1F, 0x23,
	0x2B, 0x17, 0x1B, 0x23, 0x13, 0x13, 0x17, 0x0B, 0x0B, 0x0F, 0x07, 0x07,
	0xBB, 0x73, 0x9F, 0xAF, 0x6B, 0x8F, 0xA3, 0x5F, 0x83, 0x97, 0x57, 0x77,
	0x8B, 0x4F, 0x6B, 0x7F, 0x4B, 0x5F, 0x73, 0x43, 0x53, 0x6B, 0x3B, 0x4B,
	0x5F, 0x33, 0x3F, 0x53, 0x2B, 0x37, 0x47, 0x23, 0x2B, 0x3B, 0x1F, 0x23,
	0x2F, 0x17, 0x1B, 0x23, 0x13, 0x13, 0x17, 0x0B, 0x0B, 0x0F, 0x07, 0x07,
	0xDB, 0xC3, 0xBB, 0xCB, 0xB3, 0xA7, 0xBF, 0xA3, 0x9B, 0xAF, 0x97, 0x8B,
	0xA3, 0x87, 0x7B, 0x97, 0x7B, 0x6F, 0x87, 0x6F, 0x5F, 0x7B, 0x63, 0x53,
	0x6B, 0x57, 0x47, 0x5F, 0x4B, 0x3B, 0x53, 0x3F, 0x33, 0x43, 0x33, 0x27,
	0x37, 0x2B, 0x1F, 0x27, 0x1F, 0x17, 0x1B, 0x13, 0x0F, 0x0F, 0x0B, 0x07,
	0x6F, 0x83, 0x7B, 0x67, 0x7B, 0x6F, 0x5F, 0x73, 0x67, 0x57, 0x6B, 0x5F,
	0x4F, 0x63, 0x57, 0x47, 0x5B, 0x4F, 0x3F, 0x53, 0x47, 0x37, 0x4B, 0x3F,
	0x2F, 0x43, 0x37, 0x2B, 0x3B, 0x2F, 0x23, 0x33, 0x27, 0x1F, 0x2B, 0x1F,
	0x17, 0x23, 0x17, 0x0F, 0x1B, 0x13, 0x0B, 0x13, 0x0B, 0x07, 0x0B, 0x07,
	0xFF, 0xF3, 0x1B, 0xEF, 0xDF, 0x17, 0xDB, 0xCB, 0x13, 0xCB, 0xB7, 0x0F,
	0xBB, 0xA7, 0x0F, 0xAB, 0x97, 0x0B, 0x9B, 0x83, 0x07, 0x8B, 0x73, 0x07,
	0x7B, 0x63, 0x07, 0x6B, 0x53, 0x00, 0x5B, 0x47, 0x00, 0x4B, 0x37, 0x00,
	0x3B, 0x2B, 0x00, 0x2B, 0x1F, 0x00, 0x1B, 0x0F, 0x00, 0x0B, 0x07, 0x00,
	0x00, 0x00, 0xFF, 0x0B, 0x0B, 0xEF, 0x13, 0x13, 0xDF, 0x1B, 0x1B, 0xCF,
	0x23, 0x23, 0xBF, 0x2B, 0x2B, 0xAF, 0x2F, 0x2F, 0x9F, 0x2F, 0x2F, 0x8F,
	0x2F, 0x2F, 0x7F, 0x2F, 0x2F, 0x6F, 0x2F, 0x2F, 0x5F, 0x2B, 0x2B, 0x4F,
	0x23, 0x23, 0x3F, 0x1B, 0x1B, 0x2F, 0x13, 0x13, 0x1F, 0x0B, 0x0B, 0x0F,
	0x2B, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x4B, 0x07, 0x00, 0x5F, 0x07, 0x00,
	0x6F, 0x0F, 0x00, 0x7F, 0x17, 0x07, 0x93, 0x1F, 0x07, 0xA3, 0x27, 0x0B,
	0xB7, 0x33, 0x0F, 0xC3, 0x4B, 0x1B, 0xCF, 0x63, 0x2B, 0xDB, 0x7F, 0x3B,
	0xE3, 0x97, 0x4F, 0xE7, 0xAB, 0x5F, 0xEF, 0xBF, 0x77, 0xF7, 0xD3, 0x8B,
	0xA7, 0x7B, 0x3B, 0xB7, 0x9B, 0x37, 0xC7, 0xC3, 0x37, 0xE7, 0xE3, 0x57,
	0x7F, 0xBF, 0xFF, 0xAB, 0xE7, 0xFF, 0xD7, 0xFF, 0xFF, 0x67, 0x00, 0x00,
	0x8B, 0x00, 0x00, 0xB3, 0x00, 0x00, 0xD7, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0xF3, 0x93, 0xFF, 0xF7, 0xC7, 0xFF, 0xFF, 0xFF, 0x9F, 0x5B, 0x53,
};

static int
wad_extract (wad_t *wad, lumpinfo_t *pf)
{
	static dstring_t name = {&dstring_default_mem, 0, 0, 0};
	size_t      count;
	int         len, i;
	unsigned    width, height;
	unsigned    u;
	QFile      *file;
	byte       *buffer;
	byte       *image;
	pcx_t      *pcx;
	qpic_t     *qpic;
	miptex_t   *miptex;

	switch (pf->type) {
		case TYP_PALETTE:
			dsprintf (&name, "%s.pcx", pf->name);
			break;
		case TYP_QPIC:
			dsprintf (&name, "%s.pcx", pf->name);
			break;
		case TYP_MIPTEX:
			dsprintf (&name, "%s.pcx", pf->name);
			break;
		case TYP_SOUND:
		case TYP_QTEX:
		default:
			dsprintf (&name, "%s", pf->name);
			break;
	}

	if (Sys_CreatePath (name.str) == -1)
		return -1;
	if (!(file = Qopen (name.str, "wb")))
		return -1;

	Qseek (wad->handle, pf->filepos, SEEK_SET);

	switch (pf->type) {
		case TYP_PALETTE:
			if (options.verbosity > 1)
				fprintf (stderr, "PALETTE: %d\n", pf->size);
			count = 768;
			buffer = calloc (count + 256, 1);
			if ((int) count > pf->size)
				count = pf->size;
			for (i = 0; i < 256; i++)
				buffer[i + 768] = i;
			Qread (wad->handle, buffer, count);
			pcx = EncodePCX (buffer + 768, 16, 16, 16, buffer, false, &len);
			if (Qwrite (file, pcx, len) != len) {
				fprintf (stderr, "Error writing to %s\n", name.str);
				return -1;
			}
			free (buffer);
			Qclose (file);
			return 0;
		case TYP_QPIC:
			if (options.verbosity > 1)
				fprintf (stderr, "QPIC: %d\n", pf->size);
			qpic = malloc (pf->size);
			Qread (wad->handle, qpic, pf->size);
			pcx = EncodePCX (qpic->data, qpic->width, qpic->height,
							 qpic->width, default_palette, false, &len);
			free (qpic);
			if (Qwrite (file, pcx, len) != len) {
				fprintf (stderr, "Error writing to %s\n", name.str);
				return -1;
			}
			Qclose (file);
			return 0;
		case TYP_MIPTEX:
			buffer = malloc (pf->size);
			Qread (wad->handle, buffer, pf->size);
			miptex = (miptex_t *) buffer;
			width = LittleLong (miptex->width);
			height = LittleLong (miptex->height);
			if (width > (unsigned) pf->size || height > (unsigned) pf->size
				|| (width * height * 85 / 64
					+ sizeof (miptex_t)) > (unsigned) pf->size) {
				if (options.verbosity)
					fprintf (stderr, "bogus MIPTEX. treating as raw data\n");
				if (Qwrite (file, buffer, pf->size) != pf->size) {
					fprintf (stderr, "Error writing to %s\n", name.str);
					return -1;
				}
				Qclose (file);
				return 0;
			}
			miptex->width = width;
			miptex->height = height;
			for (i = 0; i < 4; i++)
				miptex->offsets[i] = LittleLong (miptex->offsets[i]);
			if (options.verbosity > 1)
				fprintf (stderr, "MIPTEX: %.16s %d %d %d %d %d %d, %d\n",
						 miptex->name, miptex->width, miptex->height,
						 miptex->offsets[0], miptex->offsets[1],
						 miptex->offsets[2], miptex->offsets[3], pf->size);
			image = calloc (1, miptex->width * miptex->height * 3 / 2);
			memcpy (image, buffer + miptex->offsets[0],
					miptex->width * miptex->height);
			if (!options.nomip) {
				for (u = 0; u < miptex->height * 1 / 2; u++) {
					i = miptex->height + u;
					memcpy (image + i * miptex->width,
							buffer + miptex->offsets[1] + u * miptex->width / 2,
							miptex->width / 2);
					if (u >= miptex->height * 1 / 4)
						continue;
					memcpy (image + i * miptex->width + miptex->width / 2,
							buffer + miptex->offsets[2] + u * miptex->width / 4,
							miptex->width / 4);
					if (u >= miptex->height * 1 / 8)
						continue;
					memcpy (image + i * miptex->width + miptex->width * 3 / 4,
							buffer + miptex->offsets[2] + u * miptex->width / 8,
							miptex->width / 8);
				}
				pcx = EncodePCX (image, miptex->width, miptex->height * 3 / 2,
								 miptex->width, default_palette, false, &len);
			} else {
				pcx = EncodePCX (image, miptex->width, miptex->height,
								 miptex->width, default_palette, false, &len);
			}
			free (image);
			free (buffer);
			if (Qwrite (file, pcx, len) != len) {
				fprintf (stderr, "Error writing to %s\n", name.str);
				return -1;
			}
			Qclose (file);
			return 0;
		case TYP_SOUND:
			if (options.verbosity > 1)
				fprintf (stderr, "SOUND: %d\n", pf->size);
			break;
		case TYP_QTEX:
			if (options.verbosity > 1)
				fprintf (stderr, "QTEX: %d\n", pf->size);
			break;
		default:
			break;
	}

	buffer = malloc (16384);
	len = pf->size;
	while (len) {
		count = len;
		if (count > 16384)
			count = 16384;
		count = Qread (wad->handle, buffer, count);
		Qwrite (file, buffer, count);
		len -= count;
	}
	free (buffer);
	Qclose (file);
	return 0;
}

int
main (int argc, char **argv)
{
	wad_t      *wad;
	int         i;//, j, rehash = 0;
	lumpinfo_t *pf;

	Sys_Init ();
	Memory_Init (malloc (MEMSIZE), MEMSIZE);

	this_program = argv[0];

	decode_args (argc, argv);

	if (!options.wadfile) {
		fprintf (stderr, "%s: no archive/script file specified.\n",
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
				if (options.verbosity >= 2)
					printf ("%6d ", wad->lumps[i].filepos);
				if (options.verbosity >= 3)
					printf ("%6d ", wad->lumps[i].disksize);
				if (options.verbosity >= 1)
					printf ("%6d ", wad->lumps[i].size);
				if (options.verbosity >= 0)
					printf ("%3d ", wad->lumps[i].type);
				if (options.verbosity >= 3)
					printf ("%02x ", wad->lumps[i].compression);
				if (options.verbosity >= 0)
					printf ("%s\n", wad->lumps[i].name);
			}
			wad_close (wad);
			break;
		case mo_create:
			process_wad_script (options.wadfile);
			break;
		default:
			fprintf (stderr, "%s: No command given.\n",
							 this_program);
			usage (1);
	}
	return 0;
}
