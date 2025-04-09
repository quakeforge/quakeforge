/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

// trilib.c: library for loading triangles from an Alias triangle file

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "QF/mathlib.h"
#include "QF/qendian.h"
#include "QF/quakeio.h"
#include "QF/sys.h"

#include "tools/qfmodelgen/include/trilib.h"

// on disk representation of a face

#define	FLOAT_START	99999.0
#define	FLOAT_END	-FLOAT_START
#define MAGIC       123322

//#define NOISY 1

typedef struct {
	float v[3];
} vector;

typedef struct
{
	vector n;    /* normal */
	vector p;    /* point */
	vector c;    /* color */
	float  u;    /* u */
	float  v;    /* v */
} aliaspoint_t;

typedef struct {
	aliaspoint_t	pt[3];
} tf_triangle;


static void
ByteSwapTri (tf_triangle *tri)
{
	unsigned int i;

	for (i = 0; i < sizeof (tf_triangle) / 4; i++) {
		((int *) tri)[i] = BigLong (((int *) tri)[i]);
	}
}

void
LoadTriangleList (char *filename, triangle_t **pptri, int *numtriangles)
{
	QFile       *input;
	char        name[256], tex[256];
	float       start, exitpattern;
	int         count, magic, i;
	tf_triangle	tri;
	triangle_t	*ptri;

	// FIXME not sure if this should be BigFloat or FloatSwap
	exitpattern = BigFloat (FLOAT_END);
	//*((unsigned char *) &exitpattern + 0) = *((unsigned char *) &t + 3);
	//*((unsigned char *) &exitpattern + 1) = *((unsigned char *) &t + 2);
	//*((unsigned char *) &exitpattern + 2) = *((unsigned char *) &t + 1);
	//*((unsigned char *) &exitpattern + 3) = *((unsigned char *) &t + 0);

	if ((input = Qopen(filename, "rb")) == 0) {
		fprintf (stderr,"reader: could not open file '%s'\n", filename);
		exit (0);
	}

	Qread(input, &magic, sizeof(int));
	if (BigLong (magic) != MAGIC) {
		fprintf (stderr,"File is not a Alias object separated triangle file, "
				 "magic number is wrong.\n");
		exit (0);
	}

	ptri = malloc (MAXTRIANGLES * sizeof (triangle_t));

	*pptri = ptri;

	while (Qeof(input) == 0) {
		Qread(input, &start,  sizeof (float));
		start = BigFloat (start);
		if (start != exitpattern) {
			if (start == FLOAT_START) {
				// Start of an object or group of objects.
				i = -1;
				do {
					// There are probably better ways to read a string from
					// a file, but this does allow you to do error checking
					// (which I'm not doing) on a per character basis.
					i++;
					Qread(input, &(name[i]), sizeof (char));
				} while (name[i] != '\0');

//				indent ();
//				fprintf(stdout,"OBJECT START: %s\n",name);
				Qread (input, &count, sizeof (int));
				count = BigLong (count);
				if (count != 0) {
//					indent();
//					fprintf (stdout, "NUMBER OF TRIANGLES: %d\n", count);
					i = -1;
					do {
						i++;
						Qread (input, &(tex[i]), sizeof (char));
					} while (tex[i] != '\0');

//					indent();
//					fprintf(stdout,"  Object texture name: '%s'\n",tex);
				}

				/* Else (count == 0) this is the start of a group, and */
				/* no texture name is present. */
			} else if (start == FLOAT_END) {
				/* End of an object or group. Yes, the name should be */
				/* obvious from context, but it is in here just to be */
				/* safe and to provide a little extra information for */
				/* those who do not wish to write a recursive reader. */
				/* Mia culpa. */
				i = -1;
				do {
					i++;
					Qread (input, &(name[i]), sizeof (char));
				} while (name[i] != '\0');

	//			indent();
	//			fprintf(stdout,"OBJECT END: %s\n",name);
				continue;
			}
		}

// read the triangles
		for (i = 0; i < count; ++i) {
			int		j;

			Qread (input, &tri, sizeof (tf_triangle));
			ByteSwapTri (&tri);
			for (j = 0; j < 3; j++) {
				int		k;

				for (k = 0; k < 3; k++) {
					ptri->verts[j][k] = tri.pt[j].p.v[k];
				}
			}

			ptri++;

			if ((ptri - *pptri) >= MAXTRIANGLES)
				Sys_Error ("Error: too many triangles; increase MAXTRIANGLES\n");
		}
	}

	*numtriangles = ptri - *pptri;

	Qclose (input);
}
