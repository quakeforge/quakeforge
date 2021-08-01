/*

bsp2bp - converts Quake I BSP's to a bitmap (map!) of the level
Copyright (C) 1999  Matthew Wong <mkyw@iname.com>

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "QF/bspfile.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/pcx.h"
#include "QF/png.h"
#include "QF/quakefs.h"
#include "QF/quakeio.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"

#define PROGNAME  "bsp2img"
#define V_MAJOR   0
#define V_MINOR   0
#define V_REV     12

#define Z_PAD_HACK    16
#define MAX_REF_FACES 4

#define MEMSIZE (12 * 1024 * 1024)

#define X point[0]
#define Y point[1]
#define Z point[2]

#define SUB(a,b,c) ((c).X = (a).X - (b).X, \
					(c).Y = (a).Y - (b).Y, \
					(c).Z = (a).Z - (b).Z)
#define DOT(a,b) ((a).X * (b).X + (a).Y * (b).Y + (a).Z * (b).Z)
#define CROSS(a,b,c) ((c).X = (a).Y * (b).Z - (a).Z * (b).Y, \
					  (c).Y = (a).Z * (b).X - (a).X * (b).Z, \
					  (c).Z = (a).X * (b).Y - (a).Y * (b).X)

/*
Thanks fly to Id for a hackable game! :)
*/

/* MW */
typedef struct edge_extra_t {
	long        num_face_ref;
	long        ref_faces[MAX_REF_FACES];	// which faces are referenced
	dvertex_t   ref_faces_normal[MAX_REF_FACES];	// normal of referenced
													// faces
	int         ref_faces_area[MAX_REF_FACES];	// area of the referenced faces
} edge_extra_t;

typedef unsigned char eightbit;

typedef struct options_t {
	char       *bspf_name;
	char       *outf_name;
	int         outf_type;

	float       scaledown;
	float       z_pad;

	float       image_pad;

	int         z_direction;
	int         camera_axis;			// 1 - X, 2 - Y, 3 - Z, negatives will
										// come from negative side of axis

	int         edgeremove;
	float       flat_threshold;
	int         area_threshold;
	int         linelen_threshold;

	int         negative_image;

	int         write_raw;
	int         write_nocomp;
} options_t;

typedef struct {
	eightbit   *image;
	long        width;
	long        height;
} image_t;

struct options_t options;

static void
show_help (void)
{
	printf ("BSP->bitmap, version %d.%d.%d\n\n", V_MAJOR, V_MINOR, V_REV);
	printf ("Usage:\n");
	printf ("  %s [options] <bspfile> <outfile>\n\n", PROGNAME);
	printf ("Options:\n");
	printf ("    -s<scaledown>     default: 4, ie 1/4 scale\n");
	printf ("    -z<z_scaling>     default: 0 for flat map, >0 for iso 3d, -1 for auto\n");
	printf ("    -p<padding>       default: 16-pixel border around final image\n");
	printf ("    -d<direction>     iso 3d direction: 7  0  1\n");
	printf ("                                         \\ | /\n");
	printf ("                                        6--+--2\n");
	printf ("                                         / | \\\n");
	printf ("                                        5  4  3\n");
	printf ("                      default: 7\n");
	printf ("    -c<camera_axis>   default: +Z (+/- X/Y/Z axis)\n");
	printf ("    -t<flatness>      threshold of dot product for edge removal;\n");
	printf ("                      default is 0.90\n");
	printf ("    -e                disable extraneous edges removal\n");
	printf ("    -a<area>          minimum area for a polygon to be drawn\n");
	printf ("                      default is 0\n");
	printf ("    -l<length>        minimum length for an edge to be drawn\n");
	printf ("                      default is 0\n");
	printf ("    -n                negative image (black on white\n");
	printf ("    -r                write raw data, rather than bmp file\n");

	return;
}

static void
plotpoint (image_t *image, long xco, long yco, unsigned int color)
{
	unsigned int bigcol = 0;

	if (image->width <= 0 || image->height <= 0
		|| xco < 0 || xco >= image->width
		|| yco < 0 || yco >= image->height)
		return;

	bigcol = (unsigned int) image->image[yco * image->width + xco];

	bigcol += color;

	bigcol = min (bigcol, 255);

	image->image[yco * image->width + xco] = (eightbit) bigcol;

	return;
}

static void
bresline (image_t * image, long x1, long y1, long x2, long y2,
		  unsigned int color)
{
	long        x = 0, y = 0;
	long        deltax = 0, deltay = 0;
	long        xchange = 0, ychange = 0;
	long        error, length, i;

	x = x1;
	y = y1;

	deltax = x2 - x1;
	deltay = y2 - y1;

	if (deltax < 0) {
		xchange = -1;
		deltax = -deltax;
	} else {
		xchange = 1;
	}

	if (deltay < 0) {
		ychange = -1;
		deltay = -deltay;
	} else {
		ychange = 1;
	}

	/* Main seq */
	error = 0;
	i = 0;

	if (deltax < deltay) {
		length = deltay + 1;
		while (i < length) {
			y = y + ychange;
			error = error + deltax;
			if (error > deltay) {
				x = x + xchange;
				error = error - deltay;
			}

			i++;

			plotpoint (image, x, y, color);
		}
	} else {
		length = deltax + 1;
		while (i < length) {
			x = x + xchange;
			error = error + deltay;
			if (error > deltax) {
				y = y + ychange;
				error = error - deltax;
			}

			i++;

			plotpoint (image, x, y, color);
		}
	}
}

static void
def_options (struct options_t *opt)
{
	static struct options_t locopt;

	locopt.bspf_name = NULL;
	locopt.outf_name = NULL;
	locopt.outf_type = 0;

	locopt.scaledown = 4.0;
	locopt.image_pad = 16.0;
	locopt.z_pad = 0.0;

	locopt.z_direction = 1;
	locopt.camera_axis = 3;				// default is from +Z

	locopt.edgeremove = 1;
	locopt.flat_threshold = 0.90;
	locopt.area_threshold = 0;
	locopt.linelen_threshold = 0;

	locopt.negative_image = 0;

	locopt.write_raw = 0;
	locopt.write_nocomp = 1;

	memcpy (opt, &locopt, sizeof (struct options_t));
	return;
}

static void
get_options (struct options_t *opt, int argc, char *argv[])
{
	static struct options_t locopt;
	int         i = 0;
	char       *arg;
	long        lnum = 0;
	float       fnum = 0.0;
	char        pm = '+', axis = 'Z';

	/* Copy curr options */
	memcpy (&locopt, opt, sizeof (struct options_t));

	/* Go through command line */
	for (i = 1; i < argc; i++) {
		arg = argv[i];
		if (arg[0] == '-') {
			/* Okay, dash-something */
			switch (arg[1]) {
				case 's':
					if (sscanf (&arg[2], "%ld", &lnum) == 1)
						if (lnum > 0)
							locopt.scaledown = (float) lnum;
					break;

				case 'z':
					if (sscanf (&arg[2], "%ld", &lnum) == 1)
						if (lnum >= -1)
							locopt.z_pad = (float) lnum;
					break;

				case 'p':
					if (sscanf (&arg[2], "%ld", &lnum) == 1)
						if (lnum >= 0)
							locopt.image_pad = (float) lnum;
					break;

				case 'd':
					if (sscanf (&arg[2], "%ld", &lnum) == 1)
						if (lnum >= 0 && lnum <= 7)
							locopt.z_direction = (int) lnum;
					break;

				case 'c':
					if (strlen (&arg[2]) == 2) {
						pm = arg[2];
						axis = arg[3];
						printf ("-c%c%c\n", pm, axis);
						switch (axis) {
							case 'x':
							case 'X':
								locopt.camera_axis = 1;
								break;

							case 'y':
							case 'Y':
								locopt.camera_axis = 2;
								break;

							case 'z':
							case 'Z':
								locopt.camera_axis = 3;
								break;

							default:
								printf ("Must specify a valid axis.\n");
								show_help ();
								exit (1);
								break;
						}

						switch (pm) {
							case '+':
								break;
							case '-':
								locopt.camera_axis = -locopt.camera_axis;
								break;
							default:
								printf ("Must specify +/-\n");
								show_help ();
								exit (1);
								break;
						}
					} else {
						printf ("Unknown option: -%s\n", &arg[1]);
						show_help ();
						exit (1);
					}
					break;
				case 't':
					if (sscanf (&arg[2], "%f", &fnum) == 1)
						if (fnum >= 0.0 && fnum <= 1.0)
							locopt.flat_threshold = (float) fnum;
					break;

				case 'e':
					locopt.edgeremove = 0;
					break;

				case 'n':
					locopt.negative_image = 1;
					break;

				case 'a':
					if (sscanf (&arg[2], "%ld", &lnum) == 1)
						if (lnum >= 0)
							locopt.area_threshold = (int) lnum;
					break;

				case 'l':
					if (sscanf (&arg[2], "%ld", &lnum) == 1)
						if (lnum >= 0)
							locopt.linelen_threshold = (int) lnum;
					break;

				case 'r':
					locopt.write_raw = 1;
					break;

				case 'u':
					locopt.write_nocomp = 1;
					break;

				default:
					printf ("Unknown option: -%s\n", &arg[1]);
					show_help ();
					exit (1);
					break;
			}							/* switch */
		} else {
			if (locopt.bspf_name == NULL) {
				locopt.bspf_name = arg;
			} else if (locopt.outf_name == NULL) {
				const char *ext;
				locopt.outf_name = arg;
				ext = QFS_FileExtension (locopt.outf_name);
				if (strcmp (ext, ".pcx") == 0)
					locopt.outf_type = 0;
				else if (strcmp (ext, ".png") == 0)
					locopt.outf_type = 1;
				else {
					printf ("Unknown output format: %s\n", ext);
					exit (1);
				}
			} else {
				printf ("Unknown option: %s\n", arg);
				show_help ();
				exit (1);
			}
		}								/* if */
	}									/* for */

	memcpy (opt, &locopt, sizeof (struct options_t));
	return;
}

static void
show_options (struct options_t *opt)
{
	char        dirstr[80];

	printf ("Options:\n");
	printf ("  Scale down by: %.0f\n", opt->scaledown);
	printf ("  Z scale: %.0f\n", opt->z_pad);
	printf ("  Border: %.0f\n", opt->image_pad);
	/* Zoffset calculations */
	switch (opt->z_direction) {
		case 0:
			sprintf (dirstr, "up");
			break;
		case 1:
			sprintf (dirstr, "up & right");
			break;
		case 2:
			sprintf (dirstr, "right");
			break;
		case 3:
			sprintf (dirstr, "down & right");
			break;
		case 4:
			sprintf (dirstr, "down");
			break;
		case 5:
			sprintf (dirstr, "down & left");
			break;
		case 6:
			sprintf (dirstr, "left");
			break;
		case 7:
			sprintf (dirstr, "up & left");
			break;
		default:
			sprintf (dirstr, "unknown!");
			break;
	}

	printf ("  Z direction: %d [%s]\n", opt->z_direction, dirstr);
	if (opt->z_pad == 0) {
		printf ("    Warning: direction option has no effect with Z scale set to 0.\n");
	}

	/* Camera axis */
	switch (opt->camera_axis) {
		case 1:
			sprintf (dirstr, "+X");
			break;
		case -1:
			sprintf (dirstr, "-X");
			break;
		case 2:
			sprintf (dirstr, "+Y");
			break;
		case -2:
			sprintf (dirstr, "-Y");
			break;
		case 3:
			sprintf (dirstr, "+Z");
			break;
		case -3:
			sprintf (dirstr, "-Z");
			break;
		default:
			sprintf (dirstr, "unknown!");
			break;
	}

	printf ("  Camera axis: %s\n", dirstr);
	printf ("  Remove extraneous edges: %s\n",
			(opt->edgeremove == 1) ? "yes" : "no");
	printf ("  Edge removal dot product theshold: %f\n", opt->flat_threshold);
	printf ("  Minimum polygon area threshold (approximate): %d\n",
			opt->area_threshold);
	printf ("  Minimum line length threshold: %d\n", opt->linelen_threshold);
	printf ("  Creating %s image.\n",
			(opt->negative_image == 1) ? "negative" : "positive");

	printf ("\n");
	printf ("  Input (bsp) file: %s\n", opt->bspf_name);
	if (opt->write_raw)
		printf ("  Output (raw) file: %s\n\n", opt->outf_name);
	else
		printf ("  Output (%s bmp) file: %s\n\n",
				opt->write_nocomp ? "uncompressed" : "RLE compressed",
				opt->outf_name);

	return;
}

static image_t *
create_image (long width, long height)
{
	long        size;
	image_t    *image;

	image = malloc (sizeof (image_t));
	image->width = width;
	image->height = height;
	size = sizeof (eightbit) * width * height;
	if (!(image->image = malloc (size))) {
		fprintf (stderr, "Error allocating image buffer %ldx%ld.\n",
				 width, height);
		exit (2);
	}
	printf ("Allocated buffer %ldx%ld for image.\n", width, height);
	memset (image->image, 0, size);
	return image;
}

static image_t *
render_map (bsp_t *bsp)
{
	long        j = 0, k = 0, x = 0;

	dvertex_t  *vertexlist, *vert1, *vert2;
	dedge_t    *edgelist;
	dface_t    *facelist;
	int        *ledges;

	/* edge removal stuff */
	struct edge_extra_t *edge_extra = NULL;
	dvertex_t   v0, v1, vect;
	int         area = 0, usearea;

	float       minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0, minZ = 0.0;
	float       maxZ = 0.0, midZ = 0.0, tempf = 0.0;
	long        Zoffset0 = 0, Zoffset1 = 0;
	long        Z_Xdir = 1, Z_Ydir = -1;

	image_t    *image;
	int         drawcol;

	vertexlist = bsp->vertexes;
	edgelist = bsp->edges;
	facelist = bsp->faces;
	ledges = bsp->surfedges;

	/* Precalc stuff if we're removing edges - - - - - - - - - - - */

	if (options.edgeremove) {
		printf ("Precalc edge removal stuff...\n");
		edge_extra = malloc (sizeof (struct edge_extra_t) * bsp->numedges);
		if (edge_extra == NULL) {
			fprintf (stderr, "Error allocating %ld bytes for extra edge info.",
					 (long) sizeof (struct edge_extra_t) * bsp->numedges);
			exit (2);
		}
		/* initialize the array */
		for (unsigned i = 0; i < bsp->numedges; i++) {
			edge_extra[i].num_face_ref = 0;
			for (int j = 0; j < MAX_REF_FACES; j++) {
				edge_extra[i].ref_faces[j] = -1;
			}
		}

		for (unsigned i = 0; i < bsp->numfaces; i++) {
			/* calculate the normal (cross product) */
			/* starting edge: edgelist[ledges[facelist[i].firstedge]] */
			/* number of edges: facelist[i].numedges; */

			/* quick hack - just take the first 2 edges */
			j = facelist[i].firstedge;
			k = j;
			vect.X = 0.0;
			vect.Y = 0.0;
			vect.Z = 0.0;
			while (vect.X == 0.0 && vect.Y == 0.0 && vect.Z == 0.0
				   && k < (facelist[i].numedges + j)) {
				dedge_t    *e1, *e2;
				/* If the first 2 are parallel edges, go with the next one */
				k++;

				e1 = &edgelist[abs (ledges[j])];
				e2 = &edgelist[abs (ledges[k])];
				//FIXME verify directions
				if (ledges[j] > 0) {
					SUB (vertexlist[e1->v[0]], vertexlist[e1->v[1]], v0);
					SUB (vertexlist[e2->v[0]], vertexlist[e2->v[1]], v1);
				} else {
					/* negative index, therefore walk in reverse order */
					SUB (vertexlist[e1->v[1]], vertexlist[e1->v[0]], v0);
					SUB (vertexlist[e2->v[1]], vertexlist[e2->v[0]], v1);
				}

				/* cross product */
				CROSS (v0, v1, vect);

				/* Okay, it's not the REAL area, but i'm lazy, and since a lot
				   of mapmakers use rectangles anyways... */
				area = sqrt (DOT (v0, v0)) * sqrt (DOT (v1, v1));
			}							/* while */

			/* reduce cross product to a unit vector */
			tempf = sqrt (DOT (vect, vect));
			if (tempf > 0.0) {
				vect.X = vect.X / tempf;
				vect.Y = vect.Y / tempf;
				vect.Z = vect.Z / tempf;
			} else {
				vect.X = 0.0;
				vect.Y = 0.0;
				vect.Z = 0.0;
			}

			/* Now go put ref in all edges... */
			for (j = 0; j < facelist[i].numedges; j++) {
				edge_extra_t *e;
				k = j + facelist[i].firstedge;
				e = &edge_extra[abs (ledges[k])];
				x = e->num_face_ref;
				if (e->num_face_ref < MAX_REF_FACES) {
					x++;
					e->num_face_ref = x;
					e->ref_faces[x - 1] = i;
					e->ref_faces_normal[x - 1].X = vect.X;
					e->ref_faces_normal[x - 1].Y = vect.Y;
					e->ref_faces_normal[x - 1].Z = vect.Z;
					e->ref_faces_area[x - 1] = area;
				}

			}							/* for */
		}
	}

	/* . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . */

	printf ("Collecting min/max\n");
	/* Collect min and max */
	for (unsigned i = 0; i < bsp->numvertexes; i++) {

		/* Ugly hack - flip stuff around for different camera angles */
		switch (options.camera_axis) {
			case -1:					/* -X -- (-y <--> +y, +x into screen,
										   -x out of screen; -z down, +z up) */
				tempf = vertexlist[i].X;
				vertexlist[i].X = vertexlist[i].Y;
				vertexlist[i].Y = vertexlist[i].Z;
				vertexlist[i].Z = -tempf;
				break;

			case 1:					/* +X -- (+y <--> -y; -x into screen,
										   +x out of screen; -z down, +z up) */
				tempf = vertexlist[i].X;
				vertexlist[i].X = -vertexlist[i].Y;
				vertexlist[i].Y = vertexlist[i].Z;
				vertexlist[i].Z = tempf;
				break;

			case -2:					/* -Y -- (+x <--> -x; -y out of screen,
										   +z up) */
				vertexlist[i].X = -vertexlist[i].X;
				tempf = vertexlist[i].Z;
				vertexlist[i].Z = vertexlist[i].Y;
				vertexlist[i].Y = tempf;
				break;

			case 2:					/* +Y -- (-x <--> +x; +y out of screen,
										   +z up) */
				tempf = vertexlist[i].Z;
				vertexlist[i].Z = -vertexlist[i].Y;
				vertexlist[i].Y = tempf;
				break;

			case -3:					/* -Z -- negate X and Z (ie. 180 rotate
										   along Y axis) */
				vertexlist[i].X = -vertexlist[i].X;
				vertexlist[i].Z = -vertexlist[i].Z;
				break;

			case 3:					/* +Z -- do nothing! */
			default:					/* do nothing! */
				break;
		}

		/* flip Y for proper screen cords */
		vertexlist[i].Y = -vertexlist[i].Y;

		/* max and min */
		if (i == 0) {
			minX = vertexlist[i].X;
			maxX = vertexlist[i].X;

			minY = vertexlist[i].Y;
			maxY = vertexlist[i].Y;

			minZ = vertexlist[i].Z;
			maxZ = vertexlist[i].Z;
		} else {
			minX = min (vertexlist[i].X, minX);
			maxX = max (vertexlist[i].X, maxX);

			minY = min (vertexlist[i].Y, minY);
			maxY = max (vertexlist[i].Y, maxY);

			minZ = min (vertexlist[i].Z, minZ);
			maxZ = max (vertexlist[i].Z, maxZ);
		}
	}
	if (options.z_pad == -1)
		options.z_pad = (long) (maxZ - minZ) / (options.scaledown * Z_PAD_HACK);

	midZ = (maxZ + minZ) / 2.0;
	printf ("\n");
	printf ("Bounds: X [%8.4f .. %8.4f] %8.4f\n", minX, maxX, (maxX - minX));
	printf ("        Y [%8.4f .. %8.4f] %8.4f\n", minY, maxY, (maxY - minY));
	printf ("        Z [%8.4f .. %8.4f] %8.4f - mid: %8.4f\n", minZ, maxZ,
			(maxZ - minZ), midZ);

	/* image array */
	{
		long        width, height;

		width = (maxX - minX + 1) / options.scaledown;
		width += (options.image_pad + options.z_pad) * 2;

		height = (maxY - minY + 1) / options.scaledown;
		height += (options.image_pad + options.z_pad) * 2;

		image = create_image (width, height);
	}

	/* Zoffset calculations */
	switch (options.z_direction) {
		case 0:
			Z_Xdir = 0;					/* up */
			Z_Ydir = 1;
			break;

		default:						/* unknown */
		case 1:							/* up & right */
			Z_Xdir = 1;
			Z_Ydir = -1;
			break;

		case 2:							/* right */
			Z_Xdir = 1;
			Z_Ydir = 0;
			break;

		case 3:							/* down & right */
			Z_Xdir = 1;
			Z_Ydir = 1;
			break;

		case 4:							/* down */
			Z_Xdir = 0;
			Z_Ydir = 1;
			break;

		case 5:							/* down & left */
			Z_Xdir = -1;
			Z_Ydir = 1;
			break;

		case 6:							/* left */
			Z_Xdir = -1;
			Z_Ydir = 0;
			break;

		case 7:							/* up & left */
			Z_Xdir = -1;
			Z_Ydir = -1;
			break;
	}

	/* Plot edges on image */
	fprintf (stderr, "Plotting edges...");
	k = 0;
	drawcol = (options.edgeremove) ? 64 : 32;
	for (unsigned i = 0; i < bsp->numedges; i++) {
		/* Do a check on this line ... see if we keep this line or not */

		/* run through all referenced faces */

		/* ICK ... do I want to check area of all faces? */
		usearea = INT_MAX;
		if (options.edgeremove) {
			if (edge_extra[i].num_face_ref > 1) {
				tempf = 1.0;
				/* dot products of all referenced faces */
				for (j = 0; j < edge_extra[i].num_face_ref - 1; j = j + 2) {
					tempf =
						tempf * (DOT (edge_extra[i].ref_faces_normal[j],
									  edge_extra[i].ref_faces_normal[j + 1]));

					/* What is the smallest area this edge references? */
					if (usearea > edge_extra[i].ref_faces_area[j])
						usearea = edge_extra[i].ref_faces_area[j];
					if (usearea > edge_extra[i].ref_faces_area[j + 1])
						usearea = edge_extra[i].ref_faces_area[j + 1];
				}
			} else {
				tempf = 0.0;
			}
		} else {
			tempf = 0.0;
		}

		vert1 = &vertexlist[edgelist[i].v[0]];
		vert2 = &vertexlist[edgelist[i].v[1]];
		SUB (*vert1, *vert2, vect);
		if (abs (tempf) < options.flat_threshold
			&& usearea > options.area_threshold
			&& sqrt (DOT (vect, vect)) > options.linelen_threshold) {
			float       offs0, offs1;
			Zoffset0 = (options.z_pad * (vert1->Z - midZ) / (maxZ - minZ));
			Zoffset1 = (options.z_pad * (vert2->Z - midZ) / (maxZ - minZ));

			offs0 = options.image_pad + options.z_pad + (Zoffset0 * Z_Xdir);
			offs1 = options.image_pad + options.z_pad + (Zoffset1 * Z_Ydir);

			bresline (image, (vert1->X - minX) / options.scaledown + offs0,
							 (vert1->Y - minY) / options.scaledown + offs0,
							 (vert2->X - minX) / options.scaledown + offs1,
							 (vert2->Y - minY) / options.scaledown + offs1,
					  drawcol);
		} else {
			k++;
		}

	}
	printf ("%zd edges plotted", bsp->numedges);
	if (options.edgeremove) {
		printf (" (%ld edges removed)\n", k);
	} else {
		printf ("\n");
	}

	/* Little gradient */
	for (unsigned i = 0; i <= 255; i++) {
		// across from top left
		plotpoint (image, i, 0, 255 - i);
		// down from top left
		plotpoint (image, 0, i, 255 - i);

		// back from top right
		plotpoint (image, image->width - i - 1, 0, 255 - i);
		// down from top right
		plotpoint (image, image->width - 1, i, 255 - i);

		// back from bottom right
		plotpoint (image, image->width - i - 1, image->height - 1, 255 - i);
		// up from bottom right
		plotpoint (image, image->width - 1, image->height - i - 1, 255 - i);

		// across from bottom left
		plotpoint (image, i, image->height - 1, 255 - i);
		// up from bottom left
		plotpoint (image, 0, image->height - i - 1, 255 - i);
	}

	/* Negate image if necessary */
	if (options.negative_image) {
		for (int i = 0; i < image->height; i++) {
			for (int j = 0; j < image->width; j++) {
				image->image[i * image->width + j] =
					255 - image->image[i * image->width + j];
			}
		}
	}
	if (options.edgeremove) {
		free (edge_extra);
	}
	return image;
}

static void
write_png (image_t *image)
{
	byte       *data, *in, *out, b;
	int         size = image->width * image->height;

	out = data = malloc (size * 3);
	for (in = image->image; in - image->image < size; in++) {
		b = *in;
		*out++ = b;
		*out++ = b;
		*out++ = b;
	}
	WritePNG (options.outf_name, data, image->width, image->height);
}

static void
write_pcx (image_t *image)
{
	pcx_t      *pcx;
	int         pcx_len, i;
	byte        palette[768];
	QFile      *outfile;

	outfile = Qopen (options.outf_name, "wb");
	if (outfile == NULL) {
		fprintf (stderr, "Error opening output file %s.\n", options.outf_name);

		exit (1);
	}

	// quick and dirty greyscale palette
	for (i = 0; i < 256; i++) {
		palette[i * 3 + 0] = i;
		palette[i * 3 + 1] = i;
		palette[i * 3 + 2] = i;
	}

	Sys_Init ();

	Memory_Init (Sys_Alloc (MEMSIZE), MEMSIZE);
	pcx = EncodePCX (image->image, image->width, image->height,
					 image->width, palette, false, &pcx_len);
	if (Qwrite (outfile, pcx, pcx_len) != pcx_len) {
		fprintf (stderr, "Error writing to %s\n", options.outf_name);
		exit (1);
	}
	Qclose (outfile);
}

int
main (int argc, char *argv[])
{
	QFile      *bspfile;
	bsp_t      *bsp;
	image_t    *image;

	/* Enough args? */
	if (argc < 3) {
		show_help ();
		return 1;
	}

	/* Setup options */
	def_options (&options);
	get_options (&options, argc, argv);
	show_options (&options);

	bspfile = Qopen (options.bspf_name, "rbz");
	if (bspfile == NULL) {
		fprintf (stderr, "Error opening bsp file %s.\n", options.bspf_name);
		return 1;
	}
	bsp = LoadBSPFile (bspfile, Qfilesize (bspfile));
	Qclose (bspfile);

	image = render_map (bsp);
	BSP_Free (bsp);

	/* Write image */

	switch (options.outf_type) {
		case 0:
			write_pcx (image);
			break;
		case 1:
			write_png (image);
			break;
	}

	printf ("File written to %s.\n", options.outf_name);

	/* Close, done! */

	free (image->image);
	free (image);

	return 0;
}
