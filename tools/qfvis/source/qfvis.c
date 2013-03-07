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
#include <ctype.h>
#ifdef HAVE_PTHREAD_H
# include <pthread.h>
#endif

#include "QF/bspfile.h"
#include "QF/cmd.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "vis.h"
#include "options.h"

#define	MAX_THREADS		4

#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)
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

int         portal_count;
int         numportals;
int         portalclusters;
int         numrealleafs;
size_t      originalvismapsize;
int         totalvis;
int         count_sep;
int         bitbytes;	// (portalleafs + 63)>>3
int         bitlongs;
int         bitbytes_l;	// (numrealleafs + 63)>>3

portal_t   *portals;
cluster_t  *clusters;
dstring_t  *visdata;
byte       *uncompressed;		// [bitbytes * portalleafs]
int        *leafcluster;	// leaf to cluster mappings as read from .prt file

int        *working;		// per thread current portal #


static void
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
	size_t      size;

	if (points > MAX_POINTS_ON_WINDING)
		Sys_Error ("NewWinding: %i points", points);

	size = (size_t)(uintptr_t) ((winding_t *) 0)->points[points];
	winding = calloc (1, size);

	return winding;
}

void
FreeWinding (winding_t *w)
{
	if (!w->original)
		free (w);
}

winding_t *
CopyWinding (winding_t *w)
{
	size_t      size;
	winding_t  *copy;

	size = (size_t) (uintptr_t) ((winding_t *) 0)->points[w->numpoints];
	copy = malloc (size);
	memcpy (copy, w, size);
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
	int         counts[3], sides[MAX_POINTS_ON_WINDING];
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
static portal_t *
GetNextPortal (void)
{
	int         min, j;
	portal_t   *p, *tp;

	LOCK;

	min = 99999;
	p = NULL;

	for (j = 0, tp = portals; j < numportals * 2; j++, tp++) {
		if (tp->nummightsee < min && tp->status == stat_none) {
			min = tp->nummightsee;
			p = tp;
		}
	}

	if (p) {
		portal_count++;
		p->status = stat_selected;
	}

	UNLOCK;

	return p;
}

static void *
LeafThread (void *_thread)
{
	portal_t   *portal;
	int         thread = (int) (intptr_t) _thread;

	do {
		portal = GetNextPortal ();
		if (!portal)
			break;

		if (working)
			working[thread] = (int) (portal - portals);
		PortalFlow (portal);

		if (options.verbosity > 0)
			printf ("portal:%4i  mightsee:%4i  cansee:%4i\n",
						(int) (portal - portals),
						portal->nummightsee,
						portal->numcansee);
	} while (1);

	printf ("thread %d done\n", thread);
	if (working)
		working[thread] = -1;
	return NULL;
}

#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)
static void *
WatchThread (void *_thread)
{
	int         thread = (intptr_t) _thread;
	int        *local_work = malloc (thread * sizeof (int));
	int         i;
	const char *spinner = "|/-\\";
	int         spinner_ind = 0;
	int         count = 0;

	while (1) {
		usleep (1000);
		for (i = 0; i < thread; i ++)
			if (working[i] >= 0)
				break;
		if (i == thread)
			break;
		if (count++ == 1000) {
			count = 0;

			for (i = 0; i < thread; i ++)
				local_work[i] = working[i];
			for (i = 0; i < thread; i++)
				printf ("%6d", local_work[i]);
			printf ("  %5d / %5d", portal_count, numportals * 2);
			fflush (stdout);
			printf (" %c\r", spinner[(spinner_ind++) % 4]);
			fflush (stdout);
		}
	}
	printf ("watch thread done\n");
	free (local_work);

	return NULL;
}
#endif

static int
CompressRow (byte *vis, byte *dest)
{
	int         rep, visrow, j;
	byte       *dest_p;

	dest_p = dest;
	visrow = (numrealleafs + 7) >> 3;

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

static void
ClusterFlowExpand (const set_t *src, byte *dest)
{
	int         i, j;

	for (j = 1, i = 0; i < numrealleafs; i++) {
		if (set_is_member (src, leafcluster[i]))
			*dest |= j;
		j <<= 1;
		if (j == 256) {
			j = 1;
			dest++;
		}
	}
}

/*
	ClusterFlow

	Builds the entire visibility list for a cluster
*/
void
ClusterFlow (int clusternum)
{
	set_t      *visclusters;
	byte        compressed[MAX_MAP_LEAFS / 8];
	byte       *outbuffer;
	int         numvis, i;
	cluster_t  *cluster;
	portal_t   *portal;

	outbuffer = uncompressed + clusternum * bitbytes_l;
	cluster = &clusters[clusternum];

	// flow through all portals, collecting visible bits

	memset (compressed, 0, sizeof (compressed));
	visclusters = set_new ();
	for (i = 0; i < cluster->numportals; i++) {
		portal = cluster->portals[i];
		if (portal->status != stat_done)
			Sys_Error ("portal not done");
		set_union (visclusters, portal->visbits);
	}

	if (set_is_member (visclusters, clusternum))
		Sys_Error ("Cluster portals saw into cluster");

	set_add (visclusters, clusternum);

	numvis = set_size (visclusters);

	// expand to cluster->leaf PVS
	ClusterFlowExpand (visclusters, outbuffer);

	set_delete (visclusters);

	// compress the bit string
	if (options.verbosity > 0)
		printf ("cluster %4i : %4i visible\n", clusternum, numvis);
	totalvis += numvis;

	i = CompressRow (outbuffer, compressed);
	cluster->visofs = visdata->size;
	dstring_append (visdata, (char *) compressed, i);
}

static void
CalcPortalVis (void)
{
	long        i;

	// fastvis just uses mightsee for a very loose bound
	if (options.minimal) {
		for (i = 0; i < numportals * 2; i++) {
			portals[i].visbits = portals[i].mightsee;
			portals[i].status = stat_done;
		}
		return;
	}

#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)
	{
		pthread_t   work_threads[MAX_THREADS + 1];
		void *status;
		pthread_attr_t attrib;

		if (options.threads > 1) {
			working = calloc (options.threads, sizeof (int));
			my_mutex = malloc (sizeof (*my_mutex));
			if (pthread_mutex_init (my_mutex, 0) == -1)
				Sys_Error ("pthread_mutex_init failed");
			if (pthread_attr_init (&attrib) == -1)
				Sys_Error ("pthread_attr_create failed");
			if (pthread_attr_setstacksize (&attrib, 0x100000) == -1)
				Sys_Error ("pthread_attr_setstacksize failed");
			for (i = 0; i < options.threads; i++) {
				if (pthread_create (&work_threads[i], &attrib, LeafThread,
									(void *) (intptr_t) i) == -1)
					Sys_Error ("pthread_create failed");
			}
			if (pthread_create (&work_threads[i], &attrib, WatchThread,
								(void *) (intptr_t) i) == -1)
				Sys_Error ("pthread_create failed");

			for (i = 0; i < options.threads; i++) {
				if (pthread_join (work_threads[i], &status) == -1)
					Sys_Error ("pthread_join failed");
			}
			if (pthread_join (work_threads[i], &status) == -1)
				Sys_Error ("pthread_join failed");

			if (pthread_mutex_destroy (my_mutex) == -1)
				Sys_Error ("pthread_mutex_destroy failed");
			free (working);
			free (my_mutex);
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

static void
CalcVis (void)
{
	int         i;

	BasePortalVis ();
	CalcPortalVis ();

	// assemble the leaf vis lists by oring and compressing the portal lists
	for (i = 0; i < portalclusters; i++)
		ClusterFlow (i);

	for (i = 0; i < numrealleafs; i++) {
		bsp->leafs[i + 1].visofs = clusters[leafcluster[i]].visofs;
	}
	if (options.verbosity >= 0)
		printf ("average clusters visible: %i\n", totalvis / portalclusters);
}
#if 0
static qboolean
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

static sep_t *
FindPassages (winding_t *source, winding_t *pass)
{
	double      length;
	float       d;
	int         i, j, k, l;
	int         counts[3];
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
				VectorNegate (plane.normal, plane.normal);
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

static void
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
#endif
static void
LoadPortals (char *name)
{
	const char *line;
	char       *err;
	int         numpoints, i, j, k;
	int         read_leafs = 0;
	int         clusternums[2];
	cluster_t  *cluster;
	plane_t     plane;
	portal_t   *portal;
	winding_t  *winding;
	QFile	   *f;

	if (!strcmp (name, "-"))
		f = Qdopen (0, "rt"); // create a QFile of stdin
	else {
		f = Qopen (name, "r");
		if (!f) {
			printf ("LoadPortals: couldn't read %s\n", name);
			printf ("No vising performed.\n");
			exit (1);
		}
	}

	line = Qgetline (f);

	if (line && (!strcmp (line, PORTALFILE "\n")
				 || !strcmp (line, PORTALFILE "\r\n"))) {
		line = Qgetline (f);
		if (!line || sscanf (line, "%i\n", &portalclusters) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		line = Qgetline (f);
		if (!line || sscanf (line, "%i\n", &numportals) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		numrealleafs = portalclusters;
	} else if (line && (!strcmp (line, PORTALFILE_AM "\n")
						|| !strcmp (line, PORTALFILE_AM "\r\n"))) {
		line = Qgetline (f);
		if (!line || sscanf (line, "%i\n", &portalclusters) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		line = Qgetline (f);
		if (!line || sscanf (line, "%i\n", &numportals) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		line = Qgetline (f);
		if (!line || sscanf (line, "%i\n", &numrealleafs) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		read_leafs = 1;
	} else {
		Sys_Error ("LoadPortals: not a portal file");
	}

	if (options.verbosity >= 0) {
		printf ("%4i portalclusters\n", portalclusters);
		printf ("%4i numportals\n", numportals);
		printf ("%4i numrealleafs\n", numrealleafs);
	}

	bitbytes = ((portalclusters + 63) & ~63) >> 3;
	bitlongs = bitbytes / sizeof (long);

	bitbytes_l = ((numrealleafs + 63) & ~63) >> 3;

	// each file portal is split into two memory portals, one for each
	// direction
	portals = calloc (2 * numportals, sizeof (portal_t));

	clusters = calloc (portalclusters, sizeof (cluster_t));

	originalvismapsize = numrealleafs * ((numrealleafs + 7) / 8);

	for (i = 0, portal = portals; i < numportals; i++) {
		line = Qgetline (f);
		if (!line)
			Sys_Error ("LoadPortals: reading portal %i", i);

		numpoints = strtol (line, &err, 10);
		if (err == line)
			Sys_Error ("LoadPortals: reading portal %i", i);
		line = err;
		for (j = 0; j < 2; j++) {
			clusternums[j] = strtol (line, &err, 10);
			if (err == line)
				Sys_Error ("LoadPortals: reading portal %i", i);
			line = err;
		}

		if (numpoints > MAX_POINTS_ON_WINDING)
			Sys_Error ("LoadPortals: portal %i has too many points", i);
		if ((unsigned) clusternums[0] > (unsigned) portalclusters
			|| (unsigned) clusternums[1] > (unsigned) portalclusters)
			Sys_Error ("LoadPortals: reading portal %i", i);

		winding = portal->winding = NewWinding (numpoints);
		winding->original = true;
		winding->numpoints = numpoints;

		for (j = 0; j < numpoints; j++) {
			// (%ld %ld %ld)
			while (isspace ((byte) *line))
				line++;
			if (*line++ != '(')
				Sys_Error ("LoadPortals: reading portal %i", i);

			for (k = 0; k < 3; k++) {
				winding->points[j][k] = strtod (line, &err);
				if (err == line)
					Sys_Error ("LoadPortals: reading portal %i", i);
				line = err;
			}

			while (isspace ((byte) *line))
				line++;
			if (*line++ != ')')
				Sys_Error ("LoadPortals: reading portal %i", i);
		}

		// calc plane
		PlaneFromWinding (winding, &plane);

		// create forward portal
		cluster = &clusters[clusternums[0]];
		if (cluster->numportals == MAX_PORTALS_ON_CLUSTER)
			Sys_Error ("Cluster with too many portals");
		cluster->portals[cluster->numportals] = portal;
		cluster->numportals++;

		portal->winding = winding;
		VectorNegate (plane.normal, portal->plane.normal);
		portal->plane.dist = -plane.dist;
		portal->cluster = clusternums[1];
		portal++;

		// create backwards portal
		cluster = &clusters[clusternums[1]];
		if (cluster->numportals == MAX_PORTALS_ON_CLUSTER)
			Sys_Error ("Cluster with too many portals");
		cluster->portals[cluster->numportals] = portal;
		cluster->numportals++;

		portal->winding = winding;
		portal->plane = plane;
		portal->cluster = clusternums[0];
		portal++;
	}

	leafcluster = calloc (numrealleafs, sizeof (int));
	if (read_leafs) {
		for (i = 0; i < numrealleafs; i++) {
			line = Qgetline (f);
			if (sscanf (line, "%i\n", &leafcluster[i]) != 1)
				Sys_Error ("LoadPortals: parse error in leaf->cluster "
						   "mappings");
		}
	} else {
		for (i = 0; i < numrealleafs; i++)
			leafcluster[i] = i;
	}
	Qclose (f);
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
		usage (1);
		Sys_Error ("%s: no bsp file specified.", this_program);
	}

	QFS_SetExtension (options.bspfile, ".bsp");

	f = Qopen (options.bspfile->str, "rb");
	if (!f)
		Sys_Error ("couldn't open %s for reading.", options.bspfile->str);
	bsp = LoadBSPFile (f, Qfilesize (f));
	Qclose (f);

	visdata = dstring_new ();

	dstring_copystr (portalfile, options.bspfile->str);
	QFS_SetExtension (portalfile, ".prt");
	LoadPortals (portalfile->str);

	uncompressed = calloc (bitbytes_l, portalclusters);

	CalcVis ();

	if (options.verbosity >= 0)
		printf ("c_chains: %i%s\n", c_chains,
				options.threads > 1 ? " (not reliable)" :"");

	BSP_AddVisibility (bsp, (byte *) visdata->str, visdata->size);
	if (options.verbosity >= 0)
		printf ("visdatasize:%ld  compressed from %ld\n",
				(long) bsp->visdatasize, (long) originalvismapsize);

	CalcAmbientSounds ();

	f = Qopen (options.bspfile->str, "wb");
	if (!f)
		Sys_Error ("couldn't open %s for writing.", options.bspfile->str);
	WriteBSPFile (bsp, f);
	Qclose (f);

	stop = Sys_DoubleTime ();

	if (options.verbosity >= 0)
		printf ("%5.1f seconds elapsed\n", stop - start);

	dstring_delete (portalfile);
	dstring_delete (visdata);
	dstring_delete (options.bspfile);
	BSP_Free (bsp);
	free (leafcluster);
	free (uncompressed);
	free (portals);
	free (clusters);

	return 0;
}
