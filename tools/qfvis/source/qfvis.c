/*
	vis.c

	PVS/PHS generation tool

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
#ifdef HAVE_PTHREAD_H
# include <pthread.h>
#endif

#include "QF/cmd.h"
#include "QF/bspfile.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "vis.h"
#include "options.h"

#define	MAX_THREADS		4

#ifdef HAVE_PTHREAD_H
pthread_mutex_t *my_mutex;
#endif

bsp_t *bsp;

options_t   options;

int         c_chains;
int         c_mighttest;
int         c_portaltest;
int         c_portalpass;
int         c_portalcheck;
int         c_vistest;

int         numportals;
int         portalleafs;
int         originalvismapsize;
int         totalvis;
int         count_sep;
int         bitbytes;	// (portalleafs + 63)>>3
int         bitlongs;

portal_t   *portals;
leaf_t     *leafs;
dstring_t  *visdata;
byte       *uncompressed;		// [bitbytes * portalleafs]


void
PlaneFromWinding (winding_t *winding, plane_t *plane)
{
	vec3_t      v1, v2;

	// calc plane
	VectorSubtract (winding->points[2], winding->points[1], v1);
	VectorSubtract (winding->points[0], winding->points[1], v2);
	CrossProduct (v2, v1, plane->normal);
	_VectorNormalize (plane->normal);
	plane->dist = DotProduct (winding->points[0], plane->normal);
}

winding_t *
NewWinding (int points)
{
	winding_t  *winding;
	int         size;

	if (points > MAX_POINTS_ON_WINDING)
		Sys_Error ("NewWinding: %i points", points);

	size = (int) ((winding_t *) 0)->points[points];
	winding = calloc (1, size);

	return winding;
}

void
FreeWinding (winding_t *winding)
{
	if (!winding->original)
		free (winding);
}

winding_t *
CopyWinding (winding_t *winding)
{
	int         size;
	winding_t  *copy;

	size = (int) ((winding_t *) 0)->points[winding->numpoints];
	copy = malloc (size);
	memcpy (copy, winding, size);
	copy->original = false;
	return copy;
}

/*
	ClipWinding

	Clips the winding to the plane, returning the new winding on the positive
	side

	Frees the input winding.

	If keepon is true, an exactly on-plane winding will be saved, otherwise
	it will be clipped away.
*/
winding_t *
ClipWinding (winding_t *in, plane_t *split, qboolean keepon)
{
	int         maxpts, i, j;
	int         sides[MAX_POINTS_ON_WINDING];
	int         counts[3];
	vec_t       dists[MAX_POINTS_ON_WINDING];
	vec_t       dot;
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

	maxpts = in->numpoints + 4;	// can't use counts[0] + 2 because
								// of fp grouping errors
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
		for (j = 0; j < 3; j++) {	
			// avoid round off error when possible
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
	GetNextPortal

	Returns the next portal for a thread to work on
	Returns the portals from the least complex, so the later ones can reuse
	the earlier information.
*/
portal_t *
GetNextPortal (void)
{
	int         j;
	portal_t   *p, *tp;
	int         min;

	LOCK;

	min = 99999;
	p = NULL;

	for (j = 0, tp = portals; j < numportals * 2; j++, tp++) {
		if (tp->nummightsee < min && tp->status == stat_none) {
			min = tp->nummightsee;
			p = tp;
		}
	}

	if (p)
		p->status = stat_selected;

	UNLOCK;

	return p;
}

void *
LeafThread (void *thread)
{
	portal_t   *portal;

	do {
		portal = GetNextPortal ();
		if (!portal)
			break;

		PortalFlow (portal);

		if (options.verbosity > 0)
			printf ("portal:%4i  mightsee:%4i  cansee:%4i\n", 
						(int) (portal - portals), 
						portal->nummightsee, 
						portal->numcansee);
	} while (1);

	return NULL;
}

int
CompressRow (byte *vis, byte *dest)
{
	int         j;
	int         rep;
	int         visrow;
	byte       *dest_p;

	dest_p = dest;
	visrow = (portalleafs + 7) >> 3;

	for (j = 0; j < visrow; j++) {
		*dest_p++ = vis[j];
		if (vis[j])
			continue;

		rep = 1;
		for (j++; j < visrow; j++)
			if (vis[j] || rep == 255)
				break;
			else
				rep++;
		*dest_p++ = rep;
		j--;
	}

	return dest_p - dest;
}

/*
	LeafFlow

	Builds the entire visibility list for a leaf
*/
void
LeafFlow (int leafnum)
{
	byte       *outbuffer;
	byte        compressed[MAX_MAP_LEAFS / 8];
	int         numvis, i, j;
	leaf_t     *leaf;
	portal_t   *portal;

	// flow through all portals, collecting visible bits
	outbuffer = uncompressed + leafnum * bitbytes;
	leaf = &leafs[leafnum];
	for (i = 0; i < leaf->numportals; i++) {
		portal = leaf->portals[i];
		if (portal->status != stat_done)
			Sys_Error ("portal not done");
		for (j = 0; j < bitbytes; j++)
			outbuffer[j] |= portal->visbits[j];
	}

	if (outbuffer[leafnum >> 3] & (1 << (leafnum & 7)))
		Sys_Error ("Leaf portals saw into leaf");

	outbuffer[leafnum >> 3] |= (1 << (leafnum & 7));

	numvis = 0;
	for (i = 0; i < portalleafs; i++)
		if (outbuffer[i >> 3] & (1 << (i & 3)))
			numvis++;

	// compress the bit string
	if (options.verbosity > 0)
		printf ("leaf %4i : %4i visible\n", leafnum, numvis);
	totalvis += numvis;

	i = CompressRow (outbuffer, compressed);

	bsp->leafs[leafnum + 1].visofs = visdata->size;
	dstring_append (visdata, compressed, i);
}

void
CalcPortalVis (void)
{
	int         i;

	// fastvis just uses mightsee for a very loose bound
	if (options.minimal) {
		for (i = 0; i < numportals * 2; i++) {
			portals[i].visbits = portals[i].mightsee;
			portals[i].status = stat_done;
		}
		return;
	}

#ifdef HAVE_PTHREAD_H
	{
		pthread_t   work_threads[MAX_THREADS];
		void *status;
		pthread_attr_t attrib;

		if (options.threads > 1) {
			my_mutex = malloc (sizeof (*my_mutex));
			if (pthread_mutex_init (my_mutex, 0) == -1)
				Sys_Error ("pthread_mutex_init failed");
			if (pthread_attr_init (&attrib) == -1)
				Sys_Error ("pthread_attr_create failed");
			if (pthread_attr_setstacksize (&attrib, 0x100000) == -1)
				Sys_Error ("pthread_attr_setstacksize failed");
			for (i = 0; i < options.threads; i++) {
				if (pthread_create (&work_threads[i], &attrib, LeafThread,
									(void *) i) == -1)
					Sys_Error ("pthread_create failed");
			}

			for (i = 0; i < options.threads; i++) {
				if (pthread_join (work_threads[i], &status) == -1)
					Sys_Error ("pthread_join failed");
			}

			if (pthread_mutex_destroy (my_mutex) == -1)
				Sys_Error ("pthread_mutex_destroy failed");
		} else {
			LeafThread (0);
		}
	}
#else
	LeafThread (0);
#endif

	if (options.verbosity > 0) {
		printf ("portalcheck: %i  portaltest: %i  portalpass: %i\n",
				c_portalcheck, c_portaltest, c_portalpass);
		printf ("c_vistest: %i  c_mighttest: %i\n", c_vistest, c_mighttest);
	}
}

void
CalcVis (void)
{
	int         i;

	BasePortalVis ();
	CalcPortalVis ();

	// assemble the leaf vis lists by oring and compressing the portal lists
	for (i = 0; i < portalleafs; i++)
		LeafFlow (i);

	if (options.verbosity >= 0)
		printf ("average leafs visible: %i\n", totalvis / portalleafs);
}

qboolean
PlaneCompare (plane_t *p1, plane_t *p2)
{
	int         i;

	if (fabs (p1->dist - p2->dist) > 0.01)
		return false;

	for (i = 0; i < 3; i++)
		if (fabs (p1->normal[i] - p2->normal[i]) > 0.001)
			return false;

	return true;
}

sep_t *
FindPassages (winding_t *source, winding_t *pass)
{
	double      length;
	float       d;
	int         counts[3];
	int         i, j, k, l;
	plane_t     plane;
	qboolean    fliptest;
	sep_t      *sep, *list;
	vec3_t      v1, v2;

	list = NULL;

	// check all combinations
	for (i = 0; i < source->numpoints; i++) {
		l = (i + 1) % source->numpoints;
		VectorSubtract (source->points[l], source->points[i], v1);

		// find a vertex of pass that makes a plane that puts all of the
		// vertexes of pass on the front side and all of the vertexes of
		// source on the back side
		for (j = 0; j < pass->numpoints; j++) {
			VectorSubtract (pass->points[j], source->points[i], v2);

			plane.normal[0] = v1[1] * v2[2] - v1[2] * v2[1];
			plane.normal[1] = v1[2] * v2[0] - v1[0] * v2[2];
			plane.normal[2] = v1[0] * v2[1] - v1[1] * v2[0];

			// if points don't make a valid plane, skip it
			length = plane.normal[0] * plane.normal[0] + 
					 plane.normal[1] * plane.normal[1] + 
					 plane.normal[2] * plane.normal[2];

			if (length < ON_EPSILON)
				continue;

			length = 1 / sqrt(length);

			plane.normal[0] *= length;
			plane.normal[1] *= length;
			plane.normal[2] *= length;

			plane.dist = DotProduct (pass->points[j], plane.normal);

			// find out which side of the generated seperating plane has the
			// source portal
			fliptest = false;
			for (k = 0; k < source->numpoints; k++) {
				if (k == i || k == l)
					continue;
				d = DotProduct (source->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON) {	
					// source is on the negative side, so we want all
					// pass and target on the positive side
					fliptest = false;
					break;
				} else if (d > ON_EPSILON) {
					// source is on the positive side, so we want all
					// pass and target on the negative side
					fliptest = true;
					break;
				}
			}
			if (k == source->numpoints)
				continue;	// planar with source portal

			// flip the normal if the source portal is backwards
			if (fliptest) {
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}

			// if all of the pass portal points are now on the positive side,
			// this is the seperating plane
			counts[0] = counts[1] = counts[2] = 0;
			for (k = 0; k < pass->numpoints; k++) {
				if (k == j)
					continue;
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
					break;
				else if (d > ON_EPSILON)
					counts[0]++;
				else
					counts[2]++;
			}
			if (k != pass->numpoints)
				continue;	// points on negative side, not a seperating plane

			if (!counts[0])
				continue;	// planar with pass portal

			// save this out
			count_sep++;

			sep = malloc (sizeof (*sep));
			sep->next = list;
			list = sep;
			sep->plane = plane;
		}
	}
	return list;
}

void
CalcPassages (void)
{
	int         count, count2, i, j, k;
	leaf_t     *leaf;
	portal_t   *p1, *p2;
	sep_t      *sep;
	passage_t  *passages;

	if (options.verbosity >= 0)
		printf ("building passages...\n");

	count = count2 = 0;
	for (i = 0; i < portalleafs; i++) {
		leaf = &leafs[i];

		for (j = 0; j < leaf->numportals; j++) {
			p1 = leaf->portals[j];
			for (k = 0; k < leaf->numportals; k++) {
				if (k == j)
					continue;

				count++;
				p2 = leaf->portals[k];

				// definately can't see into a coplanar portal
				if (PlaneCompare (&p1->plane, &p2->plane))
					continue;

				count2++;

				sep = FindPassages (p1->winding, p2->winding);
				if (!sep) {
					count_sep++;
					sep = malloc (sizeof (*sep));
					sep->next = NULL;
					sep->plane = p1->plane;
				}
				passages = malloc (sizeof (*passages));
				passages->planes = sep;
				passages->from = p1->leaf;
				passages->to = p2->leaf;
				passages->next = leaf->passages;
				leaf->passages = passages;
			}
		}
	}

	if (options.verbosity >= 0) {
		printf ("numpassages: %i (%i)\n", count2, count);
		printf ("total passages: %i\n", count_sep);
	}
}

void
LoadPortals (char *name)
{
	char        magic[80];
	FILE       *f;
	int         leafnums[2];
	int         numpoints, i, j;
	leaf_t     *leaf;
	plane_t     plane;
	portal_t   *portal;
	winding_t  *winding;

	if (!strcmp (name, "-"))
		f = stdin;
	else {
		f = fopen (name, "r");
		if (!f) {
			printf ("LoadPortals: couldn't read %s\n", name);
			printf ("No vising performed.\n");
			exit (1);
		}
	}

	if (fscanf (f, "%79s\n%i\n%i\n", magic, &portalleafs, &numportals) != 3)
		Sys_Error ("LoadPortals: failed to read header");
	if (strcmp (magic, PORTALFILE))
		Sys_Error ("LoadPortals: not a portal file");

	if (options.verbosity >= 0) {
		printf ("%4i portalleafs\n", portalleafs);
		printf ("%4i numportals\n", numportals);
	}
	
	bitbytes = ((portalleafs + 63) & ~63) >> 3;
	bitlongs = bitbytes / sizeof (long);

	// each file portal is split into two memory portals, one for each
	// direction
	portals = calloc (2 * numportals, sizeof (portal_t));

	leafs = calloc (portalleafs, sizeof (leaf_t));

	originalvismapsize = portalleafs * ((portalleafs + 7) / 8);

	for (i = 0, portal = portals; i < numportals; i++) {
		if (fscanf (f, "%i %i %i ", &numpoints, &leafnums[0],
					&leafnums[1]) != 3)
			Sys_Error ("LoadPortals: reading portal %i", i);
		if (numpoints > MAX_POINTS_ON_WINDING)
			Sys_Error ("LoadPortals: portal %i has too many points", i);
		if ((unsigned) leafnums[0] > portalleafs
			|| (unsigned) leafnums[1] > portalleafs)
			Sys_Error ("LoadPortals: reading portal %i", i);

		winding = portal->winding = NewWinding (numpoints);
		winding->original = true;
		winding->numpoints = numpoints;

		for (j = 0; j < numpoints; j++) {
			double v[3];
			int k;

			// scanf into double, then assign to vec_t
			if (fscanf (f, "(%lf %lf %lf ) ", &v[0], &v[1], &v[2]) != 3)
				Sys_Error ("LoadPortals: reading portal %i", i);

			for (k = 0; k < 3; k++)		
				winding->points[j][k] = v[k];
		}
	
		fscanf (f, "\n");

		// calc plane
		PlaneFromWinding (winding, &plane);

		// create forward portal
		leaf = &leafs[leafnums[0]];
		if (leaf->numportals == MAX_PORTALS_ON_LEAF)
			Sys_Error ("Leaf with too many portals");
		leaf->portals[leaf->numportals] = portal;
		leaf->numportals++;

		portal->winding = winding;
		VectorSubtract (vec3_origin, plane.normal, portal->plane.normal);
		portal->plane.dist = -plane.dist;
		portal->leaf = leafnums[1];
		portal++;

		// create backwards portal
		leaf = &leafs[leafnums[1]];
		if (leaf->numportals == MAX_PORTALS_ON_LEAF)
			Sys_Error ("Leaf with too many portals");
		leaf->portals[leaf->numportals] = portal;
		leaf->numportals++;

		portal->winding = winding;
		portal->plane = plane;
		portal->leaf = leafnums[0];
		portal++;
	}
	fclose (f);
}

int
main (int argc, char **argv)
{
	double      start, stop;
	dstring_t  *portalfile = dstring_new ();
	QFile      *f;
	
	start = Sys_DoubleTime ();

	this_program = argv[0];

	DecodeArgs (argc, argv);

	if (!options.bspfile) {
		Sys_Error ("%s: no bsp file specified.", this_program);
		usage (1);
	}

	COM_StripExtension (options.bspfile, options.bspfile);
	COM_DefaultExtension (options.bspfile, ".bsp");
	
	f = Qopen (options.bspfile, "rb");
	bsp = LoadBSPFile (f, Qfilesize (f));
	Qclose (f);

	visdata = dstring_new ();

	portalfile->size = strlen (options.bspfile) + 1;
	dstring_adjust (portalfile);
	COM_StripExtension (options.bspfile, portalfile->str);
	dstring_appendstr (portalfile, ".prt");
	LoadPortals (portalfile->str);

	uncompressed = calloc (bitbytes, portalleafs);

	CalcVis ();

	if (options.verbosity >= 0)
		printf ("c_chains: %i%s\n", c_chains,
				options.threads > 1 ? " (not reliable)" :"");

	BSP_AddVisibility (bsp, visdata->str, visdata->size);
	if (options.verbosity >= 0)
		printf ("visdatasize:%i  compressed from %i\n", bsp->visdatasize,
				originalvismapsize);

	CalcAmbientSounds ();

	f = Qopen (options.bspfile, "wb");
	WriteBSPFile (bsp, f);
	Qclose (f);

	stop = Sys_DoubleTime ();
	
	if (options.verbosity >= 0)
		printf ("%5.1f seconds elapsed\n", stop - start);

	return 0;
}
