/*
	trace.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
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

#include "QF/bspfile.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "light.h"
#include "threads.h"
#include "entities.h"
#include "options.h"

options_t	options;
bsp_t *bsp;

char *bspfile;

float scalecos = 0.5;

dstring_t *lightdata;

dmodel_t *bspmodel;
int bspfileface;		// next surface to dispatch

vec3_t bsp_origin;

qboolean extrasamples;

float minlights[MAX_MAP_FACES];


int
GetFileSpace (int size)
{
	int ofs;

	LOCK;
	lightdata->size = (lightdata->size + 3) & ~3;
	ofs = lightdata->size;
	lightdata->size += size;
	dstring_adjust (lightdata);
	UNLOCK;
	return ofs;
}

static void *
LightThread (void *junk)
{
	int i;

	while (1) {
		LOCK;
		i = bspfileface++;
		UNLOCK;
		if (i >= bsp->numfaces)
			return 0;

		LightFace (i);
	}
}

static void
LightWorld (void)
{
	lightdata = dstring_new ();

	RunThreadsOn (LightThread);

	BSP_AddLighting (bsp, lightdata->str, lightdata->size);

	if (options.verbosity >= 0)
		printf ("lightdatasize: %i\n", bsp->lightdatasize);
}

int
main (int argc, char **argv)
{
	double      start, stop;
	QFile      *f;

	start = Sys_DoubleTime ();

	this_program = argv[0];

	DecodeArgs (argc, argv);	

	if (!bspfile) {
		fprintf (stderr, "%s: no bsp file specified.\n", this_program);
		usage (1);
	}

    InitThreads ();

	COM_StripExtension (bspfile, bspfile);
	COM_DefaultExtension (bspfile, ".bsp");

	f = Qopen (bspfile, "rb");
	bsp = LoadBSPFile (f, Qfilesize (f));
	Qclose (f);
	LoadEntities ();

	MakeTnodes (&bsp->models[0]);

	LightWorld ();

	WriteEntitiesToString ();

	f = Qopen (bspfile, "wb");
	WriteBSPFile (bsp, f);
	Qclose (f);

	stop = Sys_DoubleTime ();
	
	if (options.verbosity >= 0)
		printf ("%5.1f seconds elapsed\n", stop - start);

	return 0;
}
