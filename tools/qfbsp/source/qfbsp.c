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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <errno.h>

#include "QF/quakefs.h"
#include "QF/sys.h"

#include "bsp5.h"
#include "options.h"

options_t   options;

bsp_t      *bsp;

brushset_t *brushset;

int         c_activefaces, c_peakfaces;
int         c_activesurfaces, c_peaksurfaces;
int         c_activewindings, c_peakwindings;
int         c_activeportals, c_peakportals;
int         valid;

char       *argv0;						// changed after fork();

qboolean    worldmodel;

int         hullnum;


winding_t *
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

	VectorZero (vup);
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
	VectorMultSub (vup, v, p->normal, vup);
	_VectorNormalize (vup);

	VectorScale (p->normal, p->dist, org);

	CrossProduct (vup, p->normal, vright);

	VectorScale (vup, BOGUS_RANGE, vup);
	VectorScale (vright, BOGUS_RANGE, vright);

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

winding_t *
CopyWinding (winding_t *w)
{
	int         size;
	winding_t  *c;

	size = (long) ((winding_t *) 0)->points[w->numpoints];
	c = malloc (size);
	memcpy (c, w, size);
	return c;
}

winding_t *
CopyWindingReverse (winding_t *w)
{
	int         i, size;
	winding_t  *c;

	size = (long) ((winding_t *) 0)->points[w->numpoints];
	c = malloc (size);
	c->numpoints = w->numpoints;
	for (i = 0; i < w->numpoints; i++) {
		// add points backwards
		VectorCopy (w->points[w->numpoints - 1 - i], c->points[i]);
	}
	return c;
}

void
CheckWinding (winding_t * w)
{
}

/*
	ClipWinding

	Clips the winding to the plane, returning the new winding on the positive
	side.

	Frees the input winding.

	If keepon is true, an exactly on-plane winding will be saved, otherwise
	it will be clipped away.
*/
winding_t *
ClipWinding (winding_t *in, plane_t *split, qboolean keepon)
{
	int         maxpts, i, j;
	int        *sides;
	int         counts[3];
	vec_t       dot;
	vec_t      *dists;
	vec_t      *p1, *p2;
	vec3_t      mid;
	winding_t  *neww;

	counts[0] = counts[1] = counts[2] = 0;

	sides = alloca ((in->numpoints + 1) * sizeof (int));
	dists = alloca ((in->numpoints + 1) * sizeof (vec_t));

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
#if 0
	maxpts = in->numpoints + 4;			// can't use counts[0]+2 because of fp
										// grouping errors
#else
	maxpts = counts[0] + 2;
#endif
	neww = NewWinding (maxpts);

	for (i = 0; i < in->numpoints; i++) {
		p1 = in->points[i];

		if (sides[i] == SIDE_ON) {
			if (neww->numpoints == maxpts)
				Sys_Error ("ClipWinding: points exceeded estimate");
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			if (neww->numpoints == maxpts)
				Sys_Error ("ClipWinding: points exceeded estimate");
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		if (neww->numpoints == maxpts)
			Sys_Error ("ClipWinding: points exceeded estimate");

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

	// free the original winding
	FreeWinding (in);

	return neww;
}

/*
	DivideWinding

	Divides a winding by a plane, producing one or two windings.  The
	original winding is not damaged or freed.  If only on one side, the
	returned winding will be the input winding.  If on both sides, two
	new windings will be created.
*/
void
DivideWinding (winding_t *in, plane_t *split, winding_t **front,
			   winding_t **back)
{
	int         maxpts, i;
	int         counts[3];
	plane_t     plane;
	vec_t       dot;
	winding_t  *tmp;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i = 0; i < in->numpoints; i++) {
		dot = DotProduct (in->points[i], split->normal) - split->dist;
		if (dot > ON_EPSILON)
			counts[SIDE_FRONT]++;
		else if (dot < -ON_EPSILON)
			counts[SIDE_BACK]++;
	}

	*front = *back = NULL;

	if (!counts[SIDE_FRONT]) {
		*back = in;
		return;
	}
	if (!counts[SIDE_BACK]) {
		*front = in;
		return;
	}

	maxpts = in->numpoints + 4;			// can't use counts[0]+2 because
										// of fp grouping errors

	tmp = CopyWinding (in);
	*front = ClipWinding (tmp, split, 0);

	plane.dist = -split->dist;
	VectorNegate (split->normal, plane.normal);

	tmp = CopyWinding (in);
	*back = ClipWinding (tmp, &plane, 0);
}

winding_t *
NewWinding (int points)
{
	int         size;
	winding_t  *w;

	if (points < 3)
		Sys_Error ("NewWinding: %i points", points);

	c_activewindings++;
	if (c_activewindings > c_peakwindings)
		c_peakwindings = c_activewindings;

	size = (long) ((winding_t *) 0)->points[points];
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

face_t *
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
	free (f);
}

surface_t *
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

portal_t *
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

node_t *
AllocNode (void)
{
	node_t     *n;

	n = malloc (sizeof (node_t));
	memset (n, 0, sizeof (node_t));

	return n;
}

brush_t *
AllocBrush (void)
{
	brush_t    *b;

	b = malloc (sizeof (brush_t));
	memset (b, 0, sizeof (brush_t));

	return b;
}

static void
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
		if (options.verbosity)
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
		if (entnum == 0 && !options.nofill) {
			// assume non-world bmodels are simple
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
		if (entnum == 0 && !options.nofill) {
			// assume non-world bmodels are simple
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
				PortalizeWorldDetail (nodes);

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

static void
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
	f = Qopen (options.bspfile, "rb");
	if (!f)
		Sys_Error ("couldn't open %s. %s", options.bspfile, strerror(errno));
	bsp = LoadBSPFile (f, Qfilesize (f));
	Qclose (f);
	WriteEntitiesToString ();
	f = Qopen (options.bspfile, "wb");
	if (!f)
		Sys_Error ("couldn't open %s. %s", options.bspfile, strerror(errno));
	WriteBSPFile (bsp, f);
	Qclose (f);
}

/*
	WriteClipHull

	Write the clipping hull out to a text file so the parent process can get it
*/
static void
WriteClipHull (void)
{
	FILE        *f;
	dclipnode_t *d;
	dplane_t    *p;
	int          i;

	options.hullfile[strlen (options.hullfile) - 1] = '0' + hullnum;

	qprintf ("---- WriteClipHull ----\n");
	qprintf ("Writing %s\n", options.hullfile);

	f = fopen (options.hullfile, "w");
	if (!f)
		Sys_Error ("Couldn't open %s", options.hullfile);

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
	ReadClipHull

	Read the files written out by the child processes
*/
static void
ReadClipHull (int hullnum)
{
	FILE        *f;
	dclipnode_t  d;
	dplane_t     p;
	float        f1, f2, f3, f4;
	int          firstclipnode, junk, c1, c2, i, j, n;
	vec3_t       norm;

	options.hullfile[strlen (options.hullfile) - 1] = '0' + hullnum;

	f = fopen (options.hullfile, "r");
	if (!f)
		Sys_Error ("Couldn't open %s", options.hullfile);

	if (fscanf (f, "%d\n", &n) != 1)
		Sys_Error ("Error parsing %s", options.hullfile);

	if (n != bsp->nummodels)
		Sys_Error ("ReadClipHull: hull had %i models, base had %i", n,
				   bsp->nummodels);

	for (i = 0; i < n; i++) {
		fscanf (f, "%d\n", &j);
		bsp->models[i].headnode[hullnum] = bsp->numclipnodes + j;
	}


	fscanf (f, "\n%d\n", &n);
	firstclipnode = bsp->numclipnodes;

	for (i = 0; i < n; i++) {
		if (bsp->numclipnodes == MAX_MAP_CLIPNODES)
			Sys_Error ("ReadClipHull: MAX_MAP_CLIPNODES");
		if (fscanf (f, "%d : %f %f %f %f : %d %d\n", &junk, &f1, &f2, &f3, &f4,
					&c1, &c2) != 7)
			Sys_Error ("Error parsing %s", options.hullfile);

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

static void
CreateSingleHull (void)
{
	int         entnum;

	// for each entity in the map file that has geometry
	for (entnum = 0; entnum < num_entities; entnum++) {
		ProcessEntity (entnum);
		if (options.verbosity < 2)
			options.verbosity = 0;	// don't print rest of entities
	}

	if (hullnum)
		WriteClipHull ();
}

static void
CreateHulls (void)
{
	// commanded to create a single hull only
	if (hullnum) {
		CreateSingleHull ();
		exit (0);
	}
	// commanded to use the allready existing hulls 1 and 2
	if (options.usehulls) {
		CreateSingleHull ();
		return;
	}
	// commanded to ignore the hulls altogether
	if (options.noclip) {
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
		options.verbosity = 0;
		options.drawflag = false;
		sprintf (argv0, "HUL%i", hullnum);
	} else if (!fork ()) {
		hullnum = 2;
		options.verbosity = 0;
		options.drawflag = false;
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

static void
ProcessFile (void)
{
	bsp = BSP_New ();

	if (options.extract) {
		LoadBSP ();
		if (options.portal)
			bsp2prt ();
		if (options.extract_textures)
			extract_textures ();
		if (options.extract_entities)
			extract_entities ();
		return;
	}

	// load brushes and entities
	LoadMapFile (options.mapfile);

	if (options.onlyents) {
		UpdateEntLump ();
		return;
	}

	remove (options.bspfile);
	if (!options.usehulls) {
		options.hullfile[strlen (options.hullfile) - 1] = '1';
		remove (options.hullfile);
		options.hullfile[strlen (options.hullfile) - 1] = '2';
		remove (options.hullfile);
	}
	remove (options.portfile);
	remove (options.pointfile);

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
	double      start, end;

	// let forked processes change name for ps status
	this_program = argv0 = argv[0];

	// check command line flags
	DecodeArgs (argc, argv);

// XXX	SetQdirFromPath (argv[i]);

	// do it!
	start = Sys_DoubleTime ();
	ProcessFile ();
	end = Sys_DoubleTime ();
	printf ("%5.1f seconds elapsed\n", end - start);

	return 0;
}

void
qprintf (const char *fmt, ...)
{
	va_list     argptr;

	if (!options.verbosity)
		return;

	va_start (argptr, fmt);
	vprintf (fmt, argptr);
	va_end (argptr);
}
