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
#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif
#include <stdlib.h>
#include <errno.h>

#include "QF/quakefs.h"
#include "QF/sys.h"

#include "tools/qfbsp/include/csg4.h"
#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/merge.h"
#include "tools/qfbsp/include/options.h"
#include "tools/qfbsp/include/outside.h"
#include "tools/qfbsp/include/portals.h"
#include "tools/qfbsp/include/readbsp.h"
#include "tools/qfbsp/include/solidbsp.h"
#include "tools/qfbsp/include/surfaces.h"
#include "tools/qfbsp/include/writebsp.h"
#include "tools/qfbsp/include/tjunc.h"

/**	\addtogroup qfbsp
*/
//@{

options_t   options;

bsp_t      *bsp;

brushset_t *brushset;

int         valid;

char       *argv0;						// changed after fork();

qboolean    worldmodel;

int         hullnum;


node_t *
AllocNode (void)
{
	node_t     *n;

	n = malloc (sizeof (node_t));
	memset (n, 0, sizeof (node_t));

	return n;
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
	BSP_Free (bsp);
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
		// the node number is written out only for human readability
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
	plane_t      p;
	dplane_t     dp;
	float        f1, f2, f3, f4;
	int          firstclipnode, junk, c1, c2, i, j, n;
	int          flip;

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
		if (fscanf (f, "%d\n", &j) != 1)
			Sys_Error ("Error parsing %s", options.hullfile);
		bsp->models[i].headnode[hullnum] = bsp->numclipnodes + j;
	}


	if (fscanf (f, "\n%d\n", &n) != 1)
		Sys_Error ("Error parsing %s", options.hullfile);
	firstclipnode = bsp->numclipnodes;

	for (i = 0; i < n; i++) {
		if (bsp->numclipnodes == MAX_MAP_CLIPNODES)
			Sys_Error ("ReadClipHull: MAX_MAP_CLIPNODES");
		if (fscanf (f, "%d : %f %f %f %f : %d %d\n", &junk, &f1, &f2, &f3, &f4,
					&c1, &c2) != 7)
			Sys_Error ("Error parsing %s", options.hullfile);

		VectorSet (f1, f2, f3, p.normal);
		p.dist = f4;
		flip = NormalizePlane (&p);

		VectorCopy (p.normal, dp.normal);
		dp.dist = p.dist;
		dp.type = p.type;

		d.children[flip] = c1 >= 0 ? c1 + firstclipnode : c1;
		d.children[!flip] = c2 >= 0 ? c2 + firstclipnode : c2;

		d.planenum = FindFinalPlane (&dp);
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
	// commanded to create only a single hull
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
		if (options.extract_hull)
			extract_hull ();
		BSP_Free (bsp);
		return;
	}

	// load brushes and entities
	LoadMapFile (options.mapfile);

	if (options.onlyents) {
		UpdateEntLump ();
		BSP_Free (bsp);
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
	BSP_Free (bsp);
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

//@}
