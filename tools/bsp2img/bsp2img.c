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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <values.h>
#include <stdint.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/pcx.h"
#include "QF/quakeio.h"
#include "QF/sys.h"
#include "QF/zone.h"

#define PROGNAME  "bsp2img"
#define V_MAJOR   0
#define V_MINOR   0
#define V_REV     12

#define Z_PAD_HACK    16
#define MAX_REF_FACES 4

#define MEMSIZE (12 * 1024 * 1024)

/*
Thanks fly to Id for a hackable game! :)

Types are copied from the BSP section of the Quake specs,
thanks to Olivier Montanuy et al.
*/

/* Data structs */
typedef struct dentry_t {
	long        offset;
	long        size;
} dentry_t;

typedef struct dheader_t {
	long        version;
	dentry_t    entities;
	dentry_t    planes;

	dentry_t    miptex;
	dentry_t    vertices;

	dentry_t    visilist;
	dentry_t    nodes;

	dentry_t    texinfo;
	dentry_t    faces;

	dentry_t    lightmaps;
	dentry_t    clipnodes;

	dentry_t    leaves;

	dentry_t    iface;
	dentry_t    edges;

	dentry_t    ledges;
	dentry_t    models;

} dheader_t;

/* vertices */
typedef struct vertex_t {
	float       X;
	float       Y;
	float       Z;
} vertex_t;

/* edges */
typedef struct edge_t {
	unsigned short vertex0;				// index of start vertex,
										// 0..numvertices
	unsigned short vertex1;				// index of end vertex, 0..numvertices
} edge_t;

/* faces */
typedef struct face_t {
	unsigned short plane_id;

	unsigned short side;
	long        ledge_id;

	unsigned short ledge_num;
	unsigned short texinfo_id;

	unsigned char typelight;
	unsigned char baselight;
	unsigned char light[2];
	long        lightmap;
} face_t;

/* MW */
typedef struct edge_extra_t {
	long        num_face_ref;
	long        ref_faces[MAX_REF_FACES];	// which faces are referenced
	vertex_t    ref_faces_normal[MAX_REF_FACES];	// normal of referenced
													// faces
	int         ref_faces_area[MAX_REF_FACES];	// area of the referenced faces 
} edge_extra_t;

typedef unsigned char eightbit;

typedef struct options_t {
	char       *bspf_name;
	char       *outf_name;

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

void
show_help ()
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
	// printf(" -u write uncompressed bmp\n");

	return;
}

void
plotpoint (image_t * image, long xco, long yco, unsigned int color)
{
	unsigned int bigcol = 0;

	if (xco < 0 || xco > image->width || yco < 0 || yco > image->height)
		return;

	bigcol = (unsigned int) image->image[yco * image->width + xco];

	bigcol = bigcol + color;

	if (bigcol < 0)
		bigcol = 0;
	if (bigcol > 255)
		bigcol = 255;

	image->image[yco * image->width + xco] = (eightbit) bigcol;

	return;
}

void
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

void
def_options (struct options_t *opt)
{
	static struct options_t locopt;

	locopt.bspf_name = NULL;
	locopt.outf_name = NULL;

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

void
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
				locopt.outf_name = arg;
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

void
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

int
main (int argc, char *argv[])
{
	QFile      *bspfile = NULL;
	QFile      *outfile = NULL;
	long        i = 0, j = 0, k = 0, x = 0;
	struct dheader_t bsp_header;

	struct vertex_t *vertexlist = NULL;
	struct edge_t *edgelist = NULL;
	struct face_t *facelist = NULL;
	int        *ledges = NULL;

	/* edge removal stuff */
	struct edge_extra_t *edge_extra = NULL;
	struct vertex_t v0, v1, vect;
	int         area = 0, usearea;

	long        numedges = 0;
	long        numlistedges = 0;
	long        numvertices = 0;
	long        numfaces = 0;

	float       minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0, minZ = 0.0;
	float       maxZ = 0.0, midZ = 0.0, tempf = 0.0;
	long        Zoffset0 = 0, Zoffset1 = 0;
	long        Z_Xdir = 1, Z_Ydir = -1;

	image_t    *image;
	struct options_t options;
	int         drawcol;


	/* Enough args? */
	if (argc < 3) {
		show_help ();
		return 1;
	}

	/* Setup options */
	def_options (&options);
	get_options (&options, argc, argv);
	show_options (&options);

	bspfile = Qopen (options.bspf_name, "r");
	if (bspfile == NULL) {
		fprintf (stderr, "Error opening bsp file %s.\n", options.bspf_name);
		return 1;
	}

	/* Read header */
	printf ("Reading header...");
	i = Qread (bspfile, &bsp_header, sizeof (struct dheader_t));
	if (i != sizeof (struct dheader_t)) {
		printf ("error %s!\n", strerror (errno));
		return 1;
	} else {
		printf ("done.\n");
	}

	numvertices = (bsp_header.vertices.size / sizeof (struct vertex_t));
	numedges = (bsp_header.edges.size / sizeof (struct edge_t));
	numlistedges = (bsp_header.ledges.size / sizeof (short));
	numfaces = (bsp_header.faces.size / sizeof (struct face_t));

	/* display header */
	printf ("Header info:\n\n");
	printf (" version %ld\n", bsp_header.version);
	printf (" vertices - offset %ld\n", bsp_header.vertices.offset);
	printf ("          - size %ld", bsp_header.vertices.size);
	printf (" [numvertices = %ld]\n", numvertices);
	printf ("\n");

	printf ("    edges - offset %ld\n", bsp_header.edges.offset);
	printf ("          - size %ld", bsp_header.edges.size);
	printf (" [numedges = %ld]\n", numedges);
	printf ("\n");

	printf ("   ledges - offset %ld\n", bsp_header.ledges.offset);
	printf ("          - size %ld", bsp_header.ledges.size);
	printf (" [numledges = %ld]\n", numlistedges);
	printf ("\n");

	printf ("    faces - offset %ld\n", bsp_header.faces.offset);
	printf ("          - size %ld", bsp_header.faces.size);
	printf (" [numfaces = %ld]\n", numfaces);
	printf ("\n");

	/* Read vertices - - - - - - - - - - - - - - - - - - - */
	vertexlist = malloc (sizeof (struct vertex_t) * numvertices);
	if (vertexlist == NULL) {
		fprintf (stderr, "Error allocating %ld bytes for vertices.",
				 sizeof (struct vertex_t) * numvertices);
		return 2;
	}

	printf ("Reading %ld vertices...", numvertices);
	if (Qseek (bspfile, bsp_header.vertices.offset, SEEK_SET) == -1) {
		fprintf (stderr, "error seeking to %ld\n", bsp_header.vertices.offset);
		return 1;
	} else {
		printf ("seek to %ld...", Qtell (bspfile));
	}
	i = Qread (bspfile, vertexlist, sizeof (struct vertex_t) * numvertices);
	if (i != sizeof (struct vertex_t) * numvertices) {
		fprintf (stderr, "error %s! only %ld read.\n", strerror (errno), i);
		return 1;
	} else {
		printf ("successfully read %ld vertices.\n", i);
	}

	/* Read edges - - - - - - - - - - - - - - - - - - - - */
	edgelist = malloc (sizeof (struct edge_t) * numedges);
	if (edgelist == NULL) {
		fprintf (stderr, "Error allocating %ld bytes for vertices.",
				 sizeof (struct edge_t) * numedges);
		return 2;
	}

	printf ("Reading %ld edges...", numedges);
	if (Qseek (bspfile, bsp_header.edges.offset, SEEK_SET) == -1) {
		fprintf (stderr, "error seeking to %ld\n", bsp_header.vertices.offset);
		return 1;
	} else {
		printf ("seek to %ld...", Qtell (bspfile));
	}
	i = Qread (bspfile, edgelist, sizeof (struct edge_t) * numedges);
	if (i != sizeof (struct edge_t) * numedges) {
		fprintf (stderr, "error %s! only %ld read.\n", strerror (errno), i);
		return 1;
	} else {
		printf ("successfully read %ld edges.\n", i);
	}

	/* Read ledges - - - - - - - - - - - - - - - - - - - */
	ledges = malloc (sizeof (short) * numlistedges);
	if (ledges == NULL) {
		fprintf (stderr, "Error allocating %ld bytes for ledges.",
				 sizeof (short) * numlistedges);
		return 2;
	}

	printf ("Reading ledges...");
	if (Qseek (bspfile, bsp_header.ledges.offset, SEEK_SET) == -1) {
		fprintf (stderr, "error seeking to %ld\n", bsp_header.ledges.offset);
		return 1;
	} else {
		printf ("seek to %ld...", Qtell (bspfile));
	}
	i = Qread (bspfile, ledges, sizeof (short) * numlistedges);
	if (i != sizeof (short) * numlistedges) {
		fprintf (stderr, "error %s! only %ld read.\n", strerror (errno), i);
		return 1;
	} else {
		printf ("successfully read %ld ledges.\n", i);
	}

	/* Read faces - - - - - - - - - - - - - - - - - - - - */
	facelist = malloc (sizeof (struct face_t) * numfaces);
	if (facelist == NULL) {
		fprintf (stderr, "Error allocating %ld bytes for faces.",
				 sizeof (short) * numfaces);
		return 2;
	}

	printf ("Reading faces...");
	if (Qseek (bspfile, bsp_header.faces.offset, SEEK_SET) == -1) {
		fprintf (stderr, "error seeking to %ld\n", bsp_header.faces.offset);
		return 1;
	} else {
		printf ("seek to %ld...", Qtell (bspfile));
	}
	i = Qread (bspfile, facelist, sizeof (struct face_t) * numfaces);
	if (i != sizeof (struct face_t) * numfaces) {
		fprintf (stderr, "error %s! only %ld read.\n", strerror (errno), i);
		return 1;
	} else {
		printf ("successfully read %ld faces.\n", i);
	}

	/* Should be done reading stuff - - - - - - - - - - - - - - */
	Qclose (bspfile);

	/* Precalc stuff if we're removing edges - - - - - - - - - - - */
	/* 
	   typedef struct edge_extra_t { int num_face_ref; int
	   ref_faces[MAX_REF_FACES]; vertex_t ref_faces_normal[MAX_REF_FACES]; }
	   edge_extra_t; */

	if (options.edgeremove) {
		printf ("Precalc edge removal stuff...\n");
		edge_extra = malloc (sizeof (struct edge_extra_t) * numedges);
		if (edge_extra == NULL) {
			fprintf (stderr, "Error allocating %ld bytes for extra edge info.",
					 sizeof (struct edge_extra_t) * numedges);
			return 2;
		}
		/* initialize the array */
		for (i = 0; i < numedges; i++) {
			edge_extra[i].num_face_ref = 0;
			for (j = 0; j < MAX_REF_FACES; j++) {
				edge_extra[i].ref_faces[j] = -1;
			}
		}

		for (i = 0; i < numfaces; i++) {
			/* calculate the normal (cross product) */
			/* starting edge: edgelist[ledges[facelist[i].ledge_id]] */
			/* number of edges: facelist[i].ledge_num; */

			/* quick hack - just take the first 2 edges */
			j = facelist[i].ledge_id;
			k = j;
			vect.X = 0.0;
			vect.Y = 0.0;
			vect.Z = 0.0;
			while (vect.X == 0.0 && vect.Y == 0.0 && vect.Z == 0.0
				   && k < (facelist[i].ledge_num + j)) {
				/* If the first 2 are parællel edges, go with the next one */
				k++;
				/* 
				   if (i == (numfaces-1)) k=0; */

				if (ledges[j] > 0) {
					v0.X =
						vertexlist[edgelist[abs ((int) ledges[j])].vertex0].X -
						vertexlist[edgelist[abs ((int) ledges[j])].vertex1].X;
					v0.Y =
						vertexlist[edgelist[abs ((int) ledges[j])].vertex0].Y -
						vertexlist[edgelist[abs ((int) ledges[j])].vertex1].Y;
					v0.Z =
						vertexlist[edgelist[abs ((int) ledges[j])].vertex0].Z -
						vertexlist[edgelist[abs ((int) ledges[j])].vertex1].Z;

					v1.X =
						vertexlist[edgelist[abs ((int) ledges[k])].vertex0].X -
						vertexlist[edgelist[abs ((int) ledges[k])].vertex1].X;
					v1.Y =
						vertexlist[edgelist[abs ((int) ledges[k])].vertex0].Y -
						vertexlist[edgelist[abs ((int) ledges[k])].vertex1].Y;
					v1.Z =
						vertexlist[edgelist[abs ((int) ledges[k])].vertex0].Z -
						vertexlist[edgelist[abs ((int) ledges[k])].vertex1].Z;
				} else {
					/* negative index, therefore walk in reverse order */
					v0.X =
						vertexlist[edgelist[abs ((int) ledges[j])].vertex1].X -
						vertexlist[edgelist[abs ((int) ledges[j])].vertex0].X;
					v0.Y =
						vertexlist[edgelist[abs ((int) ledges[j])].vertex1].Y -
						vertexlist[edgelist[abs ((int) ledges[j])].vertex0].Y;
					v0.Z =
						vertexlist[edgelist[abs ((int) ledges[j])].vertex1].Z -
						vertexlist[edgelist[abs ((int) ledges[j])].vertex0].Z;

					v1.X =
						vertexlist[edgelist[abs ((int) ledges[k])].vertex1].X -
						vertexlist[edgelist[abs ((int) ledges[k])].vertex0].X;
					v1.Y =
						vertexlist[edgelist[abs ((int) ledges[k])].vertex1].Y -
						vertexlist[edgelist[abs ((int) ledges[k])].vertex0].Y;
					v1.Z =
						vertexlist[edgelist[abs ((int) ledges[k])].vertex1].Z -
						vertexlist[edgelist[abs ((int) ledges[k])].vertex0].Z;
				}

				/* cross product */
				vect.X = (v0.Y * v1.Z) - (v0.Z * v1.Y);
				vect.Y = (v0.Z * v1.X) - (v0.X * v1.Z);
				vect.Z = (v0.X * v1.Y) - (v0.Y * v1.X);

				/* Okay, it's not the REAL area, but i'm lazy, and since a lot
				   of mapmakers use rectangles anyways... */
				area =
					(int) (sqrt (v0.X * v0.X + v0.Y * v0.Y + v0.Z * v0.Z) *
						   sqrt (v1.X * v1.X + v1.Y * v1.Y + v1.Z * v1.Z));
			}							/* while */

			/* reduce cross product to a unit vector */
			tempf =
				(float)
				sqrt ((double)
					  (vect.X * vect.X + vect.Y * vect.Y + vect.Z * vect.Z));
			// printf("%4ld - (%8.3f, %8.3f, %8.3f) X (%8.3f, %8.3f, %8.3f) =
			// (%8.3f, %8.3f, %8.3f) -> ",i,v0.X, v0.Y, v0.Z, v1.X, v1.Y, v1.Z, 
			// vect.X, vect.Y, vect.Z);
			if (tempf > 0.0) {
				vect.X = vect.X / tempf;
				vect.Y = vect.Y / tempf;
				vect.Z = vect.Z / tempf;
			} /* if tempf */
			else {
				vect.X = 0.0;
				vect.Y = 0.0;
				vect.Z = 0.0;
			}

			// printf("(%8.3f, %8.3f, %8.3f)\n",vect.X, vect.Y, vect.Z);

			/* Now go put ref in all edges... */
			/* printf("<id=%ld|num=%ld>",facelist[i].ledge_id,
			   facelist[i].ledge_num); */
			for (j = 0; j < facelist[i].ledge_num; j++) {
				k = j + facelist[i].ledge_id;
				x = edge_extra[abs ((int) ledges[k])].num_face_ref;
				/* 
				   printf("e%d(le%ld)",abs((int)ledges[k]),k); */
				if (edge_extra[abs ((int) ledges[k])].num_face_ref <
					MAX_REF_FACES) {
					x++;
					edge_extra[abs ((int) ledges[k])].num_face_ref = x;
					edge_extra[abs ((int) ledges[k])].ref_faces[x - 1] = i;
					edge_extra[abs ((int) ledges[k])].ref_faces_normal[x -
																	   1].X =
						vect.X;
					edge_extra[abs ((int) ledges[k])].ref_faces_normal[x -
																	   1].Y =
						vect.Y;
					edge_extra[abs ((int) ledges[k])].ref_faces_normal[x -
																	   1].Z =
						vect.Z;
					edge_extra[abs ((int) ledges[k])].ref_faces_area[x - 1] =
						area;
				}

			}							/* for */
		}
	}

	/* . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . */

	printf ("Collecting min/max\n");
	/* Collect min and max */
	for (i = 0; i < numvertices; i++) {
		/* DEBUG - print vertices */
		// printf("vertex %ld: (%f, %f, %f)\n", i, vertexlist[i].X,
		// vertexlist[i].Y, vertexlist[i].Z);

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
				vertexlist[i].Y = tempf;;
				break;

			case 2:					/* +Y -- (-x <--> +x; +y out of screen, 
										   +z up) */
				tempf = vertexlist[i].Z;
				vertexlist[i].Z = -vertexlist[i].Y;
				vertexlist[i].Y = tempf;;
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
			if (vertexlist[i].X < minX)
				minX = vertexlist[i].X;
			if (vertexlist[i].X > maxX)
				maxX = vertexlist[i].X;

			if (vertexlist[i].Y < minY)
				minY = vertexlist[i].Y;
			if (vertexlist[i].Y > maxY)
				maxY = vertexlist[i].Y;

			if (vertexlist[i].Z < minZ)
				minZ = vertexlist[i].Z;
			if (vertexlist[i].Z > maxZ)
				maxZ = vertexlist[i].Z;
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
	image = malloc (sizeof (image_t));
	image->width =
		(long) ((maxX - minX) / options.scaledown) + (options.image_pad * 2) +
		(options.z_pad * 2);
	image->height =
		(long) ((maxY - minY) / options.scaledown) + (options.image_pad * 2) +
		(options.z_pad * 2);
	if (!
		(image->image =
		 malloc (sizeof (eightbit) * image->width * image->height))) {
		fprintf (stderr, "Error allocating image buffer %ldx%ld.\n",
				 image->width, image->height);
		return 0;
	} else {
		printf ("Allocated buffer %ldx%ld for image.\n", image->width,
				image->height);
		memset (image->image, 0,
				sizeof (eightbit) * image->width * image->height);
	}

	/* Zoffset calculations */
	switch (options.z_direction) {
		case 0:
			Z_Xdir = 0;					/* up */
			Z_Ydir = 1;
			break;

		case 1:
			Z_Xdir = 1;					/* up & right */
			Z_Ydir = -1;
			break;

		case 2:
			Z_Xdir = 1;					/* right */
			Z_Ydir = 0;
			break;

		case 3:
			Z_Xdir = 1;					/* down & right */
			Z_Ydir = 1;
			break;

		case 4:
			Z_Xdir = 0;					/* down */
			Z_Ydir = 1;
			break;

		case 5:
			Z_Xdir = -1;				/* down & left */
			Z_Ydir = 1;
			break;

		case 6:
			Z_Xdir = -1;				/* left */
			Z_Ydir = 0;
			break;

		case 7:
			Z_Xdir = -1;				/* up & left */
			Z_Ydir = -1;
			break;

		default:
			Z_Xdir = 1;					/* unknown - go with case 1 */
			Z_Ydir = -1;
			break;
	}

	/* Plot edges on image */
	fprintf (stderr, "Plotting edges...");
	k = 0;
	drawcol = (options.edgeremove) ? 64 : 32;
	for (i = 0; i < numedges; i++) {
		/* 
		   fprintf(stderr, "Edge %ld: vertex %d (%f, %f, %f) -> %d (%f, %f,
		   %f)\n", i, edgelist[i].vertex0, vertexlist[edgelist[i].vertex0].X,
		   vertexlist[edgelist[i].vertex0].Y,
		   vertexlist[edgelist[i].vertex0].Z, edgelist[i].vertex1,
		   vertexlist[edgelist[i].vertex1].X,
		   vertexlist[edgelist[i].vertex1].Y,
		   vertexlist[edgelist[i].vertex1].Z); */
		/* Do a check on this line ... see if we keep this line or not */
		/* 
		   fprintf(stderr,"edge %ld is referenced by %ld
		   faces\n",i,edge_extra[i].num_face_ref); */

		/* run through all referenced faces */

		/* ICK ... do I want to check area of all faces? */
		usearea = MAXINT;
		if (options.edgeremove) {
			// fprintf(stderr,"Edge %ld -
			// ref=%ld",i,edge_extra[i].num_face_ref);
			if (edge_extra[i].num_face_ref > 1) {
				tempf = 1.0;
				/* dot products of all referenced faces */
				for (j = 0; j < edge_extra[i].num_face_ref - 1; j = j + 2) {
					/* dot product */

					/* 
					   fprintf(stderr,". (%8.3f,%8.3f,%8.3f) .
					   (%8.3f,%8.3f,%8.3f)",
					   edge_extra[i].ref_faces_normal[j].X,
					   edge_extra[i].ref_faces_normal[j].Y,
					   edge_extra[i].ref_faces_normal[j].Z,
					   edge_extra[i].ref_faces_normal[j+1].X,
					   edge_extra[i].ref_faces_normal[j+1].Y,
					   edge_extra[i].ref_faces_normal[j+1].Z); */

					tempf =
						tempf * (edge_extra[i].ref_faces_normal[j].X *
								 edge_extra[i].ref_faces_normal[j + 1].X +
								 edge_extra[i].ref_faces_normal[j].Y *
								 edge_extra[i].ref_faces_normal[j + 1].Y +
								 edge_extra[i].ref_faces_normal[j].Z *
								 edge_extra[i].ref_faces_normal[j + 1].Z);

					/* What is the smallest area this edge references? */
					if (usearea > edge_extra[i].ref_faces_area[j])
						usearea = edge_extra[i].ref_faces_area[j];
					if (usearea > edge_extra[i].ref_faces_area[j + 1])
						usearea = edge_extra[i].ref_faces_area[j + 1];
				}
			} else {
				tempf = 0.0;
			}
			// fprintf(stderr," = %8.3f\n",tempf);
		} else {
			tempf = 0.0;
		}

		if ((abs (tempf) < options.flat_threshold) &&
			(usearea > options.area_threshold) &&
			(sqrt
			 ((vertexlist[edgelist[i].vertex0].X -
			   vertexlist[edgelist[i].vertex1].X) *
			  (vertexlist[edgelist[i].vertex0].X -
			   vertexlist[edgelist[i].vertex1].X) +
			  (vertexlist[edgelist[i].vertex0].Y -
			   vertexlist[edgelist[i].vertex1].Y) *
			  (vertexlist[edgelist[i].vertex0].Y -
			   vertexlist[edgelist[i].vertex1].Y) +
			  (vertexlist[edgelist[i].vertex0].Z -
			   vertexlist[edgelist[i].vertex1].Z) *
			  (vertexlist[edgelist[i].vertex0].Z -
			   vertexlist[edgelist[i].vertex1].Z)) >
			 options.linelen_threshold)) {
			Zoffset0 =
				(long) (options.z_pad *
						(vertexlist[edgelist[i].vertex0].Z - midZ) / (maxZ -
																	  minZ));
			Zoffset1 =
				(long) (options.z_pad *
						(vertexlist[edgelist[i].vertex1].Z - midZ) / (maxZ -
																	  minZ));

			bresline (image,
					  (long) ((vertexlist[edgelist[i].vertex0].X -
							   minX) / options.scaledown + options.image_pad +
							  options.z_pad + (float) (Zoffset0 * Z_Xdir)),
					  (long) ((vertexlist[edgelist[i].vertex0].Y -
							   minY) / options.scaledown + options.image_pad +
							  options.z_pad + (float) (Zoffset0 * Z_Ydir)),
					  (long) ((vertexlist[edgelist[i].vertex1].X -
							   minX) / options.scaledown + options.image_pad +
							  options.z_pad + (float) (Zoffset1 * Z_Xdir)),
					  (long) ((vertexlist[edgelist[i].vertex1].Y -
							   minY) / options.scaledown + options.image_pad +
							  options.z_pad + (float) (Zoffset1 * Z_Ydir)),
					  drawcol);
		} else {
			k++;
		}

	}
	printf ("%ld edges plotted", numedges);
	if (options.edgeremove) {
		printf (" (%ld edges removed)\n", k);
	} else {
		printf ("\n");
	}

	/* Little gradient */
	for (i = 0; i <= 255; i++) {
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
		for (i = 0; i < image->height; i++) {
			for (j = 0; j < image->width; j++) {
				image->image[i * image->width + j] =
					255 - image->image[i * image->width + j];
			}
		}
	}

	/* Write image */

	outfile = Qopen (options.outf_name, "w");
	if (outfile == NULL) {
		fprintf (stderr, "Error opening output file %s.\n", options.outf_name);

		return 1;
	} else {
		pcx_t      *pcx;
		int         pcx_len, i;
		byte        palette[768];

		// quick and dirty greyscale palette
		for (i = 0; i < 256; i++) {
			palette[i * 3 + 0] = i;
			palette[i * 3 + 1] = i;
			palette[i * 3 + 2] = i;
		}

		Cvar_Init_Hash ();
		Cmd_Init_Hash ();
		Cvar_Init ();
		Sys_Init_Cvars ();
		Cmd_Init ();

		Memory_Init (malloc (MEMSIZE), MEMSIZE);
		pcx = EncodePCX (image->image, image->width, image->height,
						 image->width, palette, false, &pcx_len);
		if (Qwrite (outfile, pcx, pcx_len) != pcx_len) {
			fprintf (stderr, "Error writing to %s\n", options.outf_name);
			return 1;
		}
	}

	printf ("File written to %s.\n", options.outf_name);
	Qclose (outfile);

	/* Close, done! */
	free (vertexlist);
	free (edgelist);
	free (ledges);
	free (facelist);
	free (image->image);
	free (image);
	if (options.edgeremove) {
		free (edge_extra);
	}

	if (options.write_raw) {
		printf ("\nIf you want to:\n"
				"  convert -verbose -colors 256 -size %ldx%ld gray:%s map.jpg\n"
				"(this is using ImageMagick's convert)\n",
				image->width, image->height, options.outf_name);
	} else {
		printf ("\nIf you want to:\n"
				"  convert -verbose -colors 256 pcx:%s map.jpg\n"
				"(this is using ImageMagick's convert)\n",
				options.outf_name);
	}

	return 0;
}
