/*
	Copyright (C) 1996-1997  Id Software, Inc.

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

// bsp5.c

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

#include "QF/quakefs.h"
#include "QF/sys.h"

#include "bsp5.h"

bsp_t      *bsp;

// command line flags
qboolean    drawflag;
qboolean    nofill;
qboolean    notjunc;
qboolean    noclip;
qboolean    onlyents;
qboolean    verbose = true;
qboolean    allverbose;
qboolean    usehulls;

int         subdivide_size = 240;

brushset_t *brushset;

int         valid;

char        bspfilename[1024];
char        pointfilename[1024];
char        portfilename[1024];
char        hullfilename[1024];

char       *argv0;						// changed after fork();

qboolean    worldmodel;

int         hullnum;


void
qprintf (char *fmt, ...)
{
	va_list     argptr;

	if (!verbose)
		return;

	va_start (argptr, fmt);
	vprintf (fmt, argptr);
	va_end (argptr);
}

winding_t  *
BaseWindingForPlane (plane_t *p)
{
	int         i, x;
	vec_t       max, v;
	vec3_t      org, vright, vup;
	winding_t  *w;

// find the major axis

	max = -BOGUS_RANGE;
	x = -1;
	for (i = 0; i < 3; i++) {
		v = fabs (p->normal[i]);
		if (v > max) {
			x = i;
			max = v;
		}
	}
	if (x == -1)
		Sys_Error ("BaseWindingForPlane: no axis found");

	VectorCopy (vec3_origin, vup);
	switch (x) {
		case 0:
		case 1:
			vup[2] = 1;
			break;
		case 2:
			vup[0] = 1;
			break;
	}

	v = DotProduct (vup, p->normal);
	VectorMA (vup, -v, p->normal, vup);
	VectorNormalize (vup);

	VectorScale (p->normal, p->dist, org);

	CrossProduct (vup, p->normal, vright);

	VectorScale (vup, 8192, vup);
	VectorScale (vright, 8192, vright);

// project a really big axis aligned box onto the plane
	w = NewWinding (4);

	VectorSubtract (org, vright, w->points[0]);
	VectorAdd (w->points[0], vup, w->points[0]);

	VectorAdd (org, vright, w->points[1]);
	VectorAdd (w->points[1], vup, w->points[1]);

	VectorAdd (org, vright, w->points[2]);
	VectorSubtract (w->points[2], vup, w->points[2]);

	VectorSubtract (org, vright, w->points[3]);
	VectorSubtract (w->points[3], vup, w->points[3]);

	w->numpoints = 4;

	return w;
}

winding_t  *
CopyWinding (winding_t *w)
{
	int         size;
	winding_t  *c;

	size = (int) ((winding_t *) 0)->points[w->numpoints];
	c = malloc (size);
	memcpy (c, w, size);
	return c;
}

/*
==================
CheckWinding

Check for possible errors
==================
*/
void
CheckWinding (winding_t * w)
{
}

/*
==================
ClipWinding

Clips the winding to the plane, returning the new winding on the positive side
Frees the input winding.
If keepon is true, an exactly on-plane winding will be saved, otherwise
it will be clipped away.
==================
*/
winding_t  *
ClipWinding (winding_t *in, plane_t *split, qboolean keepon)
{
	int         maxpts, i, j;
	int         sides[MAX_POINTS_ON_WINDING];
	int         counts[3];
	vec_t       dot;
	vec_t       dists[MAX_POINTS_ON_WINDING];
	vec_t      *p1, *p2;
	vec3_t      mid;
	winding_t  *neww;

	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i = 0; i < in->numpoints; i++) {
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (keepon && !counts[0] && !counts[1])
		return in;

	if (!counts[0]) {
		FreeWinding (in);
		return NULL;
	}
	if (!counts[1])
		return in;

	maxpts = in->numpoints + 4;			// can't use counts[0]+2 because of fp
										// grouping errors
	neww = NewWinding (maxpts);

	for (i = 0; i < in->numpoints; i++) {
		p1 = in->points[i];

		if (sides[i] == SIDE_ON) {
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		// generate a split point
		p2 = in->points[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) {		// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy (mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}

	if (neww->numpoints > maxpts)
		Sys_Error ("ClipWinding: points exceeded estimate");

// free the original winding
	FreeWinding (in);

	return neww;
}

/*
==================
DivideWinding

Divides a winding by a plane, producing one or two windings.  The
original winding is not damaged or freed.  If only on one side, the
returned winding will be the input winding.  If on both sides, two
new windings will be created.
==================
*/
void
DivideWinding (winding_t *in, plane_t *split, winding_t **front,
			   winding_t **back)
{
	int         maxpts, i, j;
	int         sides[MAX_POINTS_ON_WINDING];
	int         counts[3];
	vec_t       dot;
	vec_t       dists[MAX_POINTS_ON_WINDING];
	vec_t      *p1, *p2;
	vec3_t      mid;
	winding_t  *f, *b;

	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i = 0; i < in->numpoints; i++) {
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*front = *back = NULL;

	if (!counts[0]) {
		*back = in;
		return;
	}
	if (!counts[1]) {
		*front = in;
		return;
	}

	maxpts = in->numpoints + 4;			// can't use counts[0]+2 because
	// of fp grouping errors

	*front = f = NewWinding (maxpts);
	*back = b = NewWinding (maxpts);

	for (i = 0; i < in->numpoints; i++) {
		p1 = in->points[i];

		if (sides[i] == SIDE_ON) {
			VectorCopy (p1, f->points[f->numpoints]);
			f->numpoints++;
			VectorCopy (p1, b->points[b->numpoints]);
			b->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy (p1, f->points[f->numpoints]);
			f->numpoints++;
		}
		if (sides[i] == SIDE_BACK) {
			VectorCopy (p1, b->points[b->numpoints]);
			b->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		// generate a split point
		p2 = in->points[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) {		// avoid round off error when
										// possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy (mid, f->points[f->numpoints]);
		f->numpoints++;
		VectorCopy (mid, b->points[b->numpoints]);
		b->numpoints++;
	}

	if (f->numpoints > maxpts || b->numpoints > maxpts)
		Sys_Error ("ClipWinding: points exceeded estimate");
}


//===========================================================================

int         c_activefaces, c_peakfaces;
int         c_activesurfaces, c_peaksurfaces;
int         c_activewindings, c_peakwindings;
int         c_activeportals, c_peakportals;

void
PrintMemory (void)
{
	printf ("faces   : %6i (%6i)\n", c_activefaces, c_peakfaces);
	printf ("surfaces: %6i (%6i)\n", c_activesurfaces, c_peaksurfaces);
	printf ("windings: %6i (%6i)\n", c_activewindings, c_peakwindings);
	printf ("portals : %6i (%6i)\n", c_activeportals, c_peakportals);
}

winding_t  *
NewWinding (int points)
{
	int         size;
	winding_t  *w;

	if (points > MAX_POINTS_ON_WINDING)
		Sys_Error ("NewWinding: %i points", points);

	c_activewindings++;
	if (c_activewindings > c_peakwindings)
		c_peakwindings = c_activewindings;

	size = (int) ((winding_t *) 0)->points[points];
	w = malloc (size);
	memset (w, 0, size);

	return w;
}

void
FreeWinding (winding_t *w)
{
	c_activewindings--;
	free (w);
}

face_t     *
AllocFace (void)
{
	face_t     *f;

	c_activefaces++;
	if (c_activefaces > c_peakfaces)
		c_peakfaces = c_activefaces;

	f = malloc (sizeof (face_t));
	memset (f, 0, sizeof (face_t));
	f->planenum = -1;

	return f;
}

void
FreeFace (face_t *f)
{
	c_activefaces--;
//	memset (f, 0xff, sizeof (face_t));
	free (f);
}

surface_t  *
AllocSurface (void)
{
	surface_t  *s;

	s = malloc (sizeof (surface_t));
	memset (s, 0, sizeof (surface_t));

	c_activesurfaces++;
	if (c_activesurfaces > c_peaksurfaces)
		c_peaksurfaces = c_activesurfaces;

	return s;
}

void
FreeSurface (surface_t *s)
{
	c_activesurfaces--;
	free (s);
}

portal_t   *
AllocPortal (void)
{
	portal_t   *p;

	c_activeportals++;
	if (c_activeportals > c_peakportals)
		c_peakportals = c_activeportals;

	p = malloc (sizeof (portal_t));
	memset (p, 0, sizeof (portal_t));

	return p;
}

void
FreePortal (portal_t *p)
{
	c_activeportals--;
	free (p);
}

node_t     *
AllocNode (void)
{
	node_t     *n;

	n = malloc (sizeof (node_t));
	memset (n, 0, sizeof (node_t));

	return n;
}

brush_t    *
AllocBrush (void)
{
	brush_t    *b;

	b = malloc (sizeof (brush_t));
	memset (b, 0, sizeof (brush_t));

	return b;
}

//===========================================================================

void
ProcessEntity (int entnum)
{
	brushset_t *bs;
	char        mod[80];
	entity_t   *ent;
	surface_t  *surfs;
	node_t     *nodes;

	ent = &entities[entnum];
	if (!ent->brushes)
		return;							// non-bmodel entity

	if (entnum > 0) {
		worldmodel = false;
		if (entnum == 1)
			qprintf ("--- Internal Entities ---\n");
		sprintf (mod, "*%i", bsp->nummodels);
		if (verbose)
			PrintEntity (ent);

		if (hullnum == 0)
			printf ("MODEL: %s\n", mod);
		SetKeyValue (ent, "model", mod);
	} else
		worldmodel = true;

// take the brush_ts and clip off all overlapping and contained faces,
// leaving a perfect skin of the model with no hidden faces
	bs = Brush_LoadEntity (ent, hullnum);

	if (!bs->brushes) {
		PrintEntity (ent);
		Sys_Error ("Entity with no valid brushes");
	}

	brushset = bs;
	surfs = CSGFaces (bs);

	if (hullnum != 0) {
		nodes = SolidBSP (surfs, true);
		if (entnum == 0 && !nofill) {	// assume non-world bmodels are simple
			PortalizeWorld (nodes);
			if (FillOutside (nodes)) {
				surfs = GatherNodeFaces (nodes);
				nodes = SolidBSP (surfs, false);	// make a really good tree
			}
			FreeAllPortals (nodes);
		}
		WriteNodePlanes (nodes);
		WriteClipNodes (nodes);
		BumpModel (hullnum);
	} else {
		// SolidBSP generates a node tree
		// 
		// if not the world, make a good tree first
		// the world is just going to make a bad tree
		// because the outside filling will force a regeneration later
		nodes = SolidBSP (surfs, entnum == 0);

		// build all the portals in the bsp tree
		// some portals are solid polygons, and some are paths to other leafs
		if (entnum == 0 && !nofill) {	// assume non-world bmodels are simple
			PortalizeWorld (nodes);

			if (FillOutside (nodes)) {
				FreeAllPortals (nodes);

				// get the remaining faces together into surfaces again
				surfs = GatherNodeFaces (nodes);

				// merge polygons
				MergeAll (surfs);

				// make a really good tree
				nodes = SolidBSP (surfs, false);

				// make the real portals for vis tracing
				PortalizeWorld (nodes);

				// save portal file for vis tracing
				WritePortalfile (nodes);

				// fix tjunctions
				tjunc (nodes);
			}
			FreeAllPortals (nodes);
		}

		WriteNodePlanes (nodes);
		MakeFaceEdges (nodes);
		WriteDrawNodes (nodes);
	}
}

void
UpdateEntLump (void)
{
	int         m, entnum;
	char        mod[80];
	QFile      *f;

	m = 1;
	for (entnum = 1; entnum < num_entities; entnum++) {
		if (!entities[entnum].brushes)
			continue;
		sprintf (mod, "*%i", m);
		SetKeyValue (&entities[entnum], "model", mod);
		m++;
	}

	printf ("Updating entities lump...\n");
	f = Qopen (bspfilename, "rb");
	bsp = LoadBSPFile (f, Qfilesize (f));
	Qclose (f);
	WriteEntitiesToString ();
	f = Qopen (bspfilename, "wb");
	WriteBSPFile (bsp, f);
	Qclose (f);
}

/*
=================
WriteClipHull

Write the clipping hull out to a text file so the parent process can get it
=================
*/
void
WriteClipHull (void)
{
	FILE        *f;
	dclipnode_t *d;
	dplane_t    *p;
	int          i;

	hullfilename[strlen (hullfilename) - 1] = '0' + hullnum;

	qprintf ("---- WriteClipHull ----\n");
	qprintf ("Writing %s\n", hullfilename);

	f = fopen (hullfilename, "w");
	if (!f)
		Sys_Error ("Couldn't open %s", hullfilename);

	fprintf (f, "%i\n", bsp->nummodels);

	for (i = 0; i < bsp->nummodels; i++)
		fprintf (f, "%i\n", bsp->models[i].headnode[hullnum]);

	fprintf (f, "\n%i\n", bsp->numclipnodes);

	for (i = 0; i < bsp->numclipnodes; i++) {
		d = &bsp->clipnodes[i];
		p = &bsp->planes[d->planenum];
		// the node number is only written out for human readability
		fprintf (f, "%5i : %f %f %f %f : %5i %5i\n", i, p->normal[0],
				 p->normal[1], p->normal[2], p->dist, d->children[0],
				 d->children[1]);
	}

	fclose (f);
}

/*
=================
ReadClipHull

Read the files written out by the child processes
=================
*/
void
ReadClipHull (int hullnum)
{
	FILE        *f;
	dclipnode_t  d;
	dplane_t     p;
	float        f1, f2, f3, f4;
	int          firstclipnode, junk, c1, c2, i, j, n;
	vec3_t       norm;

	hullfilename[strlen (hullfilename) - 1] = '0' + hullnum;

	f = fopen (hullfilename, "r");
	if (!f)
		Sys_Error ("Couldn't open %s", hullfilename);

	if (fscanf (f, "%i\n", &n) != 1)
		Sys_Error ("Error parsing %s", hullfilename);

	if (n != bsp->nummodels)
		Sys_Error ("ReadClipHull: hull had %i models, base had %i", n,
				   bsp->nummodels);

	for (i = 0; i < n; i++) {
		fscanf (f, "%i\n", &j);
		bsp->models[i].headnode[hullnum] = bsp->numclipnodes + j;
	}


	fscanf (f, "\n%i\n", &n);
	firstclipnode = bsp->numclipnodes;

	for (i = 0; i < n; i++) {
		if (bsp->numclipnodes == MAX_MAP_CLIPNODES)
			Sys_Error ("ReadClipHull: MAX_MAP_CLIPNODES");
		if (fscanf (f, "%i : %f %f %f %f : %i %i\n", &junk, &f1, &f2, &f3, &f4,
					&c1, &c2) != 7)
			Sys_Error ("Error parsing %s", hullfilename);

		p.normal[0] = f1;
		p.normal[1] = f2;
		p.normal[2] = f3;
		p.dist = f4;

		norm[0] = f1;
		norm[1] = f2;
		norm[2] = f3;					// vec_t precision
		p.type = PlaneTypeForNormal (norm);

		d.children[0] = c1 >= 0 ? c1 + firstclipnode : c1;
		d.children[1] = c2 >= 0 ? c2 + firstclipnode : c2;
		d.planenum = FindFinalPlane (&p);
		BSP_AddClipnode (bsp, &d);
	}

}

void
CreateSingleHull (void)
{
	int         entnum;

// for each entity in the map file that has geometry
	for (entnum = 0; entnum < num_entities; entnum++) {
		ProcessEntity (entnum);
		if (!allverbose)
			verbose = false;			// don't print rest of entities
	}

	if (hullnum)
		WriteClipHull ();
}

void
CreateHulls (void)
{
// commanded to create a single hull only
	if (hullnum) {
		CreateSingleHull ();
		exit (0);
	}
// commanded to use the allready existing hulls 1 and 2
	if (usehulls) {
		CreateSingleHull ();
		return;
	}
// commanded to ignore the hulls altogether
	if (noclip) {
		CreateSingleHull ();
		return;
	}

// create all the hulls

#ifdef __alpha
	printf ("forking hull processes...\n");
// fork a process for each clipping hull
	fflush (stdout);
	if (!fork ()) {
		hullnum = 1;
		verbose = false;
		drawflag = false;
		sprintf (argv0, "HUL%i", hullnum);
	} else if (!fork ()) {
		hullnum = 2;
		verbose = false;
		drawflag = false;
		sprintf (argv0, "HUL%i", hullnum);
	}
	CreateSingleHull ();

	if (hullnum)
		exit (0);

	wait (NULL);						// wait for clip hull process to finish
	wait (NULL);						// wait for clip hull process to finish

#else
// create the hulls sequentially
	printf ("building hulls sequentially...\n");

	hullnum = 1;
	CreateSingleHull ();

	bsp->nummodels = 0;
	bsp->numplanes = 0;
	bsp->numclipnodes = 0;
	hullnum = 2;
	CreateSingleHull ();

	bsp->nummodels = 0;
	bsp->numplanes = 0;
	bsp->numclipnodes = 0;
	hullnum = 0;
	CreateSingleHull ();
#endif

}

void
ProcessFile (char *sourcebase, char *bspfilename1)
{
// create filenames
	strcpy (bspfilename, bspfilename1);
	COM_StripExtension (bspfilename, bspfilename);
	strcat (bspfilename, ".bsp");

	strcpy (hullfilename, bspfilename1);
	COM_StripExtension (hullfilename, hullfilename);
	strcat (hullfilename, ".h0");

	strcpy (portfilename, bspfilename1);
	COM_StripExtension (portfilename, portfilename);
	strcat (portfilename, ".prt");

	strcpy (pointfilename, bspfilename1);
	COM_StripExtension (pointfilename, pointfilename);
	strcat (pointfilename, ".pts");

	if (!onlyents) {
		remove (bspfilename);
		if (!usehulls) {
			hullfilename[strlen (hullfilename) - 1] = '1';
			remove (hullfilename);
			hullfilename[strlen (hullfilename) - 1] = '2';
			remove (hullfilename);
		}
		remove (portfilename);
		remove (pointfilename);
	}
	bsp = BSP_New ();
// load brushes and entities
	LoadMapFile (sourcebase);
	if (onlyents) {
		UpdateEntLump ();
		return;
	}
// init the tables to be shared by all models
	BeginBSPFile ();

// the clipping hulls will be written out to text files by forked processes
	CreateHulls ();

	ReadClipHull (1);
	ReadClipHull (2);

	WriteEntitiesToString ();
	FinishBSPFile ();
}

int
main (int argc, char **argv)
{
	char        sourcename[1024];
	char        destname[1024];
	double      start, end;
	int         i;

//	malloc_debug (15);

// check command line flags
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			break;
		else if (!strcmp (argv[i], "-draw"))
			drawflag = true;
		else if (!strcmp (argv[i], "-notjunc"))
			notjunc = true;
		else if (!strcmp (argv[i], "-nofill"))
			nofill = true;
		else if (!strcmp (argv[i], "-noclip"))
			noclip = true;
		else if (!strcmp (argv[i], "-onlyents"))
			onlyents = true;
		else if (!strcmp (argv[i], "-verbose"))
			allverbose = true;
		else if (!strcmp (argv[i], "-usehulls"))
			usehulls = true;			// don't fork -- use the existing files
		else if (!strcmp (argv[i], "-hullnum")) {
			hullnum = atoi (argv[i + 1]);
			i++;
		} else if (!strcmp (argv[i], "-subdivide")) {
			subdivide_size = atoi (argv[i + 1]);
			i++;
		} else
			Sys_Error ("qbsp: Unknown option '%s'", argv[i]);
	}

	if (i != argc - 2 && i != argc - 1)
		Sys_Error
			("usage: qbsp [options] sourcefile [destfile]\noptions: -nojunc -nofill -threads[124] -draw -onlyents -verbose -proj <projectpath>");

// XXX	SetQdirFromPath (argv[i]);

// let forked processes change name for ps status
	argv0 = argv[0];

// create destination name if not specified
	strcpy (sourcename, argv[i]);
	COM_DefaultExtension (sourcename, ".map");

	if (i != argc - 2) {
		strcpy (destname, argv[i]);
		COM_StripExtension (destname, destname);
		strcat (destname, ".bsp");
		printf ("outputfile: %s\n", destname);
	} else
		strcpy (destname, argv[i + 1]);

// do it!
	start = Sys_DoubleTime ();
	ProcessFile (sourcename, destname);
	end = Sys_DoubleTime ();
	printf ("%5.1f seconds elapsed\n", end - start);

	return 0;
}
