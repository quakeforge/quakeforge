/*
	trace.c

	(description)

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

#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/dstring.h"
#include "QF/vfile.h"
#include "QF/vfs.h"
#include "QF/bspfile.h"
#include "QF/mathlib.h"
#include "light.h"
#include "threads.h"
#include "entities.h"
#include "options.h"

options_t	options;

char *bspfile;

float scalecos = 0.5;

byte *filebase, *file_p, *file_end;

dmodel_t *bspmodel;
int bspfileface;		// next surface to dispatch

vec3_t bsp_origin;

qboolean extrasamples;

float minlights[MAX_MAP_FACES];


byte	*
GetFileSpace (int size)
{
    byte *buf;

    LOCK;
    file_p = (byte *) (((long) file_p + 3) & ~3);
    buf = file_p;
    file_p += size;
    UNLOCK;
    if (file_p > file_end)
		fprintf (stderr, "GetFileSpace: overrun");
    return buf;
}


void
LightThread (void *junk)
{
    int i;

    while (1) {
		LOCK;
		i = bspfileface++;
		UNLOCK;
		if (i >= numfaces)
			return;

		LightFace (i);
    }
}

void
LightWorld (void)
{
    filebase = file_p = dlightdata;
    file_end = filebase + MAX_MAP_LIGHTING;

    RunThreadsOn (LightThread);

    lightdatasize = file_p - filebase;

	if (options.verbosity >= 0)
		printf ("lightdatasize: %i\n", lightdatasize);
}

int
main (int argc, char **argv)
{
    double      start, stop;
	
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
	
    LoadBSPFile (bspfile);
    LoadEntities ();

    MakeTnodes (&dmodels[0]);

    LightWorld ();

    WriteEntitiesToString ();
    WriteBSPFile (bspfile);

	stop = Sys_DoubleTime ();
	
	if (options.verbosity >= 0)
		printf ("%5.1f seconds elapsed\n", stop - start);

    return 0;
}
