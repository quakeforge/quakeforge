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
#include "QF/cmem.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/msg.h"
#include "QF/quakefs.h"
#include "QF/sizebuf.h"
#include "QF/sys.h"

#include "tools/qfvis/include/vis.h"
#include "tools/qfvis/include/options.h"

#ifdef USE_PTHREADS
pthread_attr_t threads_attrib;
pthread_rwlock_t *global_lock;
pthread_rwlock_t *portal_locks;
pthread_rwlock_t *stats_lock;
#endif

bsp_t *bsp;

options_t   options;

static threaddata_t main_thread;
static visstat_t global_stats;
int         base_mightsee;
unsigned    base_spherecull;
unsigned    base_windingcull;

static unsigned portal_count;
unsigned    numportals;
unsigned    portalclusters;
unsigned    numrealleafs;
unsigned    originalvismapsize;
int         totalvis;
int         count_sep;
int         bitbytes;	// (portalleafs + 63)>>3
int         bitlongs;
int         bitbytes_l;	// (numrealleafs + 63)>>3

portal_t  **portal_queue;

portal_t   *portals;
cluster_t  *clusters;
dstring_t  *visdata;
byte       *uncompressed;		// [bitbytes * portalleafs]
int        *leafcluster;	// leaf to cluster mappings as read from .prt file

int        *working;		// per thread current portal #

static void
InitThreads (void)
{
#ifdef USE_PTHREADS
	if (pthread_attr_init (&threads_attrib) == -1)
		Sys_Error ("pthread_attr_create failed");
	if (pthread_attr_setstacksize (&threads_attrib, 0x100000) == -1)
		Sys_Error ("pthread_attr_setstacksize failed");

	global_lock = malloc (sizeof (pthread_rwlock_t));
	if (pthread_rwlock_init (global_lock, 0))
		Sys_Error ("pthread_rwlock_init failed");

	stats_lock = malloc (sizeof (pthread_rwlock_t));
	if (pthread_rwlock_init (stats_lock, 0))
		Sys_Error ("pthread_rwlock_init failed");
#else
	// Unable to run multi-threaded, so force threadcount to 1
	options.threads = 1;
#endif
}

static void
EndThreads (void)
{
#ifdef USE_PTHREADS
	if (pthread_rwlock_destroy (global_lock) == -1)
		Sys_Error ("pthread_rwlock_destroy failed");
	free (global_lock);
	if (pthread_rwlock_destroy (stats_lock) == -1)
		Sys_Error ("pthread_rwlock_destroy failed");
	free (stats_lock);
#endif
}

static vec4f_t
PlaneFromWinding (winding_t *winding)
{
	vec4f_t     plane;
	vec4f_t     v1, v2;

	// calc plane using CW winding
	v1 = winding->points[2] - winding->points[1];
	v2 = winding->points[0] - winding->points[1];
	plane = normalf (crossf (v2, v1));
	// negative so dot(point, plane) includes -dist (point[3] = 1)
	plane[3] = -dotf (winding->points[0], plane)[0];
	return plane;
}

winding_t *
NewWinding (threaddata_t *thread, int points)
{
	winding_t  *winding;
	unsigned    size;

	size = field_offset (winding_t, points[points]);
	winding = Hunk_RawAlloc (thread->hunk, size);
	memset (winding, 0, size);
	winding->id = thread->winding_id++;
	winding->thread = thread->id;

	return winding;
}

winding_t *
CopyWinding (threaddata_t *thread, const winding_t *w)
{
	unsigned    size;
	winding_t  *copy;

	size = field_offset (winding_t, points[w->numpoints]);
	copy = Hunk_RawAlloc (thread->hunk, size);
	memcpy (copy, w, size);
	copy->original = false;
	copy->id = thread->winding_id++;
	copy->thread = thread->id;
	return copy;
}

static winding_t *
NewFlippedWinding (threaddata_t *thread, const winding_t *w)
{
	winding_t  *flipped;
	unsigned    i;

	flipped = NewWinding (thread, w->numpoints);
	for (i = 0; i < w->numpoints; i++) {
		flipped->points[w->numpoints - 1 - i] = w->points[i];
	}
	flipped->numpoints = w->numpoints;
	return flipped;
}

static vec4i_t
signeps (vec4f_t dist)
{
#ifdef __SSE3__
	const vec4f_t zero = {};
	const vec4f_t eps = { ON_EPSILON, ON_EPSILON, ON_EPSILON, ON_EPSILON };
	vec4f_t     d = _mm_addsub_ps (zero, dist);
	vec4i_t     c = (d - eps) > 0;
	c = (vec4i_t) _mm_hsub_epi32 ((__m128i) c, (__m128i) c);
	return c;
#else
	float       d = dist[0];
	int         front = (d >= ON_EPSILON);
	int         back = (d <= -ON_EPSILON);
	int         i = front - back;
	return (vec4i_t) { i, i, i, i };
#endif
}

static vec4f_t
split_edge (const vec4f_t *points, const vec4f_t *dists,
			int ind1, int ind2, vec4f_t split)
{
	vec4f_t     p1 = points[ind1];
	vec4f_t     p2 = points[ind2];
	vec4f_t     d1 = dists[ind1];
	vec4f_t     d2 = dists[ind2];
	// avoid nan/inf in w: d1's w is never 0 (would not be here if it was)
	// so the multiply ensures d1.w - d2.w cannot be 0 and thus d1.w/diff
	// will not result in division by 0
	static const vec4f_t one = { 1, 1, 1, 0 };
	vec4f_t     d = d1 / (d1 - d2 * one);
	vec4f_t     mid = p1 + d * (p2 - p1);

	// avoid roundoff error when possible by forcing the appropriate
	// component to the split-plane's distance when the split-plane's
	// normal is signed-canonical.
	// "nan" because 0x7fffffff is nan when viewed as a float
	static const vec4f_t onenan = { 1, 1, 1, ~0u >> 1 };
	static const vec4i_t nan = { ~0u >> 1, ~0u >> 1, ~0u >> 1, ~0u >> 1};
	vec4i_t     x = _mm_and_ps (split, (__m128) nan) == onenan;
	// plane vector has -dist in w
	vec4f_t     y = _mm_and_ps (split, (__m128) x) * -split[3];
#ifdef __SSE3__
	mid = _mm_blendv_ps (mid, y, (__m128) x);
#else
	mid = (vec4f_t) ((vec4i_t) _mm_and_ps (y, (__m128) x) |
					 (vec4i_t) _mm_and_ps (mid, (__m128) ~x));
#endif
	if (isnan (mid[0])) *(int *) 0 = 0;
	return mid;
}

static inline unsigned __attribute__((const))
is_not_on (unsigned x)
{
	return x & 1;
}

static inline unsigned __attribute__((const))
is_back (unsigned x)
{
	return x & 2;
}

static inline unsigned __attribute__((const))
is_not_back (unsigned x)
{
	return ~x & 2;
}

static inline unsigned __attribute__((const))
is_front (unsigned x)
{
	return is_not_on (x) & (is_not_back (x) >> 1);
}

static inline unsigned __attribute__((const))
is_back_front (unsigned x, unsigned y)
{
	return (is_back (x) >> 1) & is_front (y);
}

static inline unsigned __attribute__((const))
is_front_back (unsigned x, unsigned y)
{
	return is_front (x) & (is_back (y) >> 1);
}

static inline unsigned __attribute__((const))
is_transition (unsigned x, unsigned y)
{
	return is_back_front (x, y) | is_front_back (x, y);
}

static inline void
test_point (vec4f_t split, const vec4f_t *points, int index, vec4f_t *dists,
			int *sides, unsigned *counts)
{
	dists[index] = dotf (points[index], split);
	sides[index] = signeps (dists[index])[0];
	counts[sides[index]]++;
}

#undef SIDE_FRONT
#undef SIDE_BACK
#undef SIDE_ON
#define SIDE_FRONT 1
#define SIDE_BACK -1
#define SIDE_ON 0

/*
	ClipWinding

	Clips the winding to the plane, returning the new winding on the positive
	side

	Frees the input winding.

	If keepon is true, an exactly on-plane winding will be saved, otherwise
	it will be clipped away.
*/
winding_t *
ClipWinding (threaddata_t *thread, winding_t *in, vec4f_t split,
			 qboolean keepon)
{
	unsigned    maxpts = 0;
	unsigned    i;
	unsigned    _counts[3];
	unsigned   *const counts = _counts + 1;
	int        *const sides = alloca ((in->numpoints + 1) * sizeof (int));
	vec4f_t    *const dists = alloca ((in->numpoints + 1) * sizeof (vec4f_t));
	winding_t  *neww;

	counts[SIDE_FRONT] = counts[SIDE_ON] = counts[SIDE_BACK] = 0;

	// determine sides for each point
	test_point (split, in->points, 0, dists, sides, counts);
	for (i = 1; i < in->numpoints; i++) {
		test_point (split, in->points, i, dists, sides, counts);
		maxpts += is_transition (sides[i - 1], sides[i]);
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	maxpts += is_transition (sides[i - 1], sides[i]);

	if (keepon && counts[SIDE_ON] == in->numpoints) {
		return in;
	}
	if (!counts[SIDE_FRONT]) {
		return NULL;
	}
	if (!counts[SIDE_BACK]) {
		return in;
	}

	maxpts += in->numpoints - counts[SIDE_BACK];
	neww = NewWinding (thread, maxpts);

	for (i = 0; i < in->numpoints; i++) {
		if (sides[i] == SIDE_ON) {
			neww->points[neww->numpoints++] = in->points[i];
			continue;
		}
		if (sides[i] == SIDE_FRONT) {
			neww->points[neww->numpoints++] = in->points[i];
		}
		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i]) {
			continue;
		}
		vec4f_t     mid = split_edge (in->points, dists, i,
									  (i + 1) % in->numpoints, split);
		neww->points[neww->numpoints++] = mid;
	}

	if (neww->numpoints < maxpts) {
		Sys_Error ("ClipWinding: not all points copied: n:%u m:%u i:%u %u %u %u",
				   neww->numpoints, maxpts, in->numpoints,
				   counts[SIDE_BACK], counts[SIDE_ON], counts[SIDE_FRONT]);
	}
	if (neww->numpoints > maxpts) {
		Sys_Error ("ClipWinding: points exceeded estimate: n:%u m:%u",
				   neww->numpoints, maxpts);
	}

	return neww;
}

static portal_t *
GetNextPortal (int limit)
{
	portal_t *p = 0;

	if (!(limit
		  && options.portal_limit > 0
		  && portal_count >= options.portal_limit)) {
		WRLOCK (global_lock);
		if (portal_count < 2 * numportals) {
			p = portal_queue[portal_count++];
			p->status = stat_selected;
		}
		UNLOCK (global_lock);
	}
	return p;
}

static void
UpdateMightsee (threaddata_t *thread, cluster_t *source, cluster_t *dest)
{
	int         i, clusternum;
	portal_t   *portal;

	clusternum = dest - clusters;
	for (i = 0; i < source->numportals; i++) {
		portal = source->portals[i];
		WRLOCK_PORTAL (portal);
		if (portal->status == stat_none) {
			if (set_is_member (portal->mightsee, clusternum)) {
				set_remove (portal->mightsee, clusternum);
				portal->nummightsee--;
				thread->stats.mightseeupdate++;
			}
		}
		UNLOCK_PORTAL (portal);
	}
}

static void
UpdateStats (threaddata_t *thread)
{
	WRLOCK (stats_lock);
	global_stats.portaltest += thread->stats.portaltest;
	global_stats.portalpass += thread->stats.portalpass;
	global_stats.portalcheck += thread->stats.portalcheck;
	global_stats.targettested += thread->stats.targettested;
	global_stats.targettrimmed += thread->stats.targettrimmed;
	global_stats.targetclipped += thread->stats.targetclipped;
	global_stats.sourcetested += thread->stats.sourcetested;
	global_stats.sourcetrimmed += thread->stats.sourcetrimmed;
	global_stats.sourceclipped += thread->stats.sourceclipped;
	global_stats.chains += thread->stats.chains;
	global_stats.mighttest += thread->stats.mighttest;
	global_stats.vistest += thread->stats.vistest;
	global_stats.mightseeupdate += thread->stats.mightseeupdate;
	global_stats.sep_alloc += thread->stats.sep_alloc;
	global_stats.sep_free += thread->stats.sep_free;
	global_stats.winding_mark = max (global_stats.winding_mark,
									 thread->stats.winding_mark);
	global_stats.stack_alloc += thread->stats.stack_alloc;
	global_stats.stack_free += thread->stats.stack_free;
	UNLOCK (stats_lock);
	memset (&thread->stats, 0, sizeof (thread->stats));
}

static void
PortalCompleted (threaddata_t *thread, portal_t *completed)
{
	portal_t   *portal;
	cluster_t  *cluster;
	set_t      *changed;
	set_iter_t *ci;
	int         i, j;

	completed->status = stat_done;

	changed = set_new_size_r (&thread->set_pool, portalclusters);
	cluster = &clusters[completed->cluster];
	for (i = 0; i < cluster->numportals; i++) {
		portal = cluster->portals[i];
		if (portal->status != stat_done)
			continue;
		set_assign (changed, portal->mightsee);
		set_difference (changed, portal->visbits);
#if 0
		dstring_copystr (thread->str, "mightsee: ");
		set_as_string_r (thread->str, portal->mightsee);
		dstring_appendstr (thread->str, "\nvisbits: ");
		set_as_string_r (thread->str, portal->visbits);
		dstring_appendstr (thread->str, "\nchanged: ");
		set_as_string_r (thread->str, changed);
		dstring_appendstr (thread->str, "\n");
		write (0, thread->str->str, thread->str->size - 1);
#endif
		for (j = 0; j < cluster->numportals; j++) {
			if (j == i)
				continue;
			if (cluster->portals[j]->status == stat_done)
				set_difference (changed, cluster->portals[j]->visbits);
			else
				set_difference (changed, cluster->portals[j]->mightsee);
		}
		for (ci = set_first_r (&thread->set_pool, changed); ci;
			 ci = set_next_r (&thread->set_pool, ci)) {
			UpdateMightsee (thread, &clusters[ci->element], cluster);
		}
	}
	set_delete_r (&thread->set_pool, changed);

	UpdateStats (thread);
}

static void
dump_super_stats (int id, memsuper_t *super)
{
	size_t      total_size = 0;
	size_t      total_allocated = 0;
	size_t      total_post_size = 0;
	size_t      total_post_allocated = 0;
	size_t      num_blocks = 0;
	size_t      free_counts[MAX_CACHE_LINES];

	for (memblock_t *block = super->memblocks; block; block = block->next) {
		num_blocks++;
		total_size += block->size;
		total_allocated += block->allocated;
		total_post_size += block->post_size;
		// post_free is a flag
		total_post_allocated += !block->post_free * block->post_size;
	}
	for (int i = 0; i < MAX_CACHE_LINES; i++) {
		free_counts[i] = 0;
		for (memline_t *line = super->free_lines[i]; line;
			 line = line->free_next) {
			free_counts[i]++;
		}
	}

	WRLOCK (global_lock);
	printf ("cmem stats for thread %d\n", id);
	printf ("    blocks: %zd\n", num_blocks);
	printf ("          : s:%-8zd a:%-8zd f:%-8zd\n", total_size,
			total_allocated, total_size - total_allocated);
	printf ("      post: s:%-8zd a:%-8zd f:%-8zd\n", total_post_size,
			total_post_allocated, total_post_size - total_post_allocated);
	printf ("   ");
	for (int i = 0; i < MAX_CACHE_LINES; i++) {
		printf (" %5d", 64 << i);
	}
	printf ("\n");
	printf ("   ");
	for (int i = 0; i < MAX_CACHE_LINES; i++) {
		printf (" %5zd", free_counts[i]);
	}
	printf ("\n");
	UNLOCK (global_lock);
}

static void *
LeafThread (void *_thread)
{
	portal_t   *portal;
	int         thread = (int) (intptr_t) _thread;
	threaddata_t data;
	int         count = 0;
	size_t      thread_memsize = 1024 * 1024;

	memset (&data, 0, sizeof (data));
	data.hunk = Hunk_Init (Sys_Alloc (thread_memsize), thread_memsize);
	set_pool_init (&data.set_pool);
	data.id = thread;
	data.memsuper = new_memsuper ();
	data.str = dstring_new ();
	do {
		portal = GetNextPortal (1);
		if (!portal)
			break;

		if (options.verbosity >= 3 && (!count++ % 16)) {
			dump_super_stats (thread, data.memsuper);
		}
		if (working)
			working[thread] = (int) (portal - portals);

		PortalFlow (&data, portal);

		int whm = data.stats.winding_mark;
		int shw = data.stats.sep_highwater;
		int smb = data.stats.sep_maxbulk;
		PortalCompleted (&data, portal);
		data.stats.sep_highwater = shw;
		data.stats.sep_maxbulk = smb;
		data.stats.winding_mark = whm;

		if (options.verbosity >= 4)
			printf ("portal:%5i  mightsee:%5i  cansee:%5i %5u/%u\n",
						(int) (portal - portals),
						portal->nummightsee,
						portal->numcansee,
						portal_count, numportals * 2);
	} while (1);

	if (options.verbosity >= 2) {
		printf ("thread %d winding mark: %zd\n", thread,
				data.stats.winding_mark);
		printf ("thread %d separator highwater: %d\n", thread,
				data.stats.sep_highwater);
		printf ("thread %d separator maxbulk: %d\n", thread,
				data.stats.sep_maxbulk);
	}

	if (options.verbosity >= 4)
		printf ("thread %d done\n", thread);
	if (working)
		working[thread] = -1;
	delete_memsuper (data.memsuper);
	dstring_delete (data.str);

	Sys_Free (data.hunk, thread_memsize);
	return NULL;
}

static void *
BaseVisThread (void *_thread)
{
	portal_t   *portal;
	int         thread = (int) (intptr_t) _thread;
	basethread_t data;
	set_pool_t  set_pool;
	int         num_mightsee = 0;

	memset (&data, 0, sizeof (data));
	data.id = thread;
	set_pool_init (&set_pool);
	data.portalsee = set_new_size_r (&set_pool, numportals * 2);
	do {
		portal = GetNextPortal (0);
		if (!portal)
			break;

		if (working)
			working[thread] = (int) (portal - portals);

		portal->mightsee = set_new_size_r (&set_pool, portalclusters);
		set_empty (data.portalsee);

		PortalBase (&data, portal);
		num_mightsee += data.clustersee;
		data.clustersee = 0;
	} while (1);

	WRLOCK (stats_lock);
	base_spherecull += data.spherecull;
	base_windingcull += data.windingcull;
	base_mightsee += num_mightsee;
	UNLOCK (stats_lock);

	if (options.verbosity >= 4)
		printf ("thread %d done\n", thread);
	if (working)
		working[thread] = -1;
	return NULL;
}

#ifdef USE_PTHREADS
const char spinner[] = "|/-\\";
const char progress[] = "0....1....2....3....4....5....6....7....8....9....";

static void
print_thread_stats (const int *local_work, int thread, int spinner_ind)
{
	int         i;

	for (i = 0; i < thread; i++)
		printf ("%6d", local_work[i]);
	printf ("  %5u / %5u", portal_count, numportals * 2);
	fflush (stdout);
	printf (" %c\r", spinner[spinner_ind % 4]);
	fflush (stdout);
}

static int
print_progress (int prev_prog, int spinner_ind)
{
	int         prog;

	prog = portal_count * 50 / (numportals * 2) + 1;
	if (prog > prev_prog)
		printf ("%.*s", prog - prev_prog, progress + prev_prog);
	printf (" %c\b\b", spinner[spinner_ind % 4]);
	fflush (stdout);
	return prog;
}

typedef struct {
	int         thread;
	int       (*progress)(int, int);
} watch_data_t;

static void *
WatchThread (void *_wd)
{
	watch_data_t *wd = _wd;
	int         thread = wd->thread;
	int        *local_work = malloc (thread * sizeof (int));
	int         i;
	int         spinner_ind = 0;
	int         count = 0;
	int         prev_prog = 0;
	unsigned    prev_port = 0;
	int         stalled = 0;

	while (1) {
		usleep (1000);
		for (i = 0; i < thread; i ++)
			if (working[i] >= 0)
				break;
		if (i == thread)
			break;
		if (count++ == 100) {
			count = 0;

			for (i = 0; i < thread; i ++)
				local_work[i] = working[i];
			if (options.verbosity >= 4)
				print_thread_stats (local_work, thread, spinner_ind);
			else if (options.verbosity >= 0)
				prev_prog = wd->progress (prev_prog, spinner_ind);
			if (prev_port != portal_count || stalled++ == 10) {
				prev_port = portal_count;
				stalled = 0;
				spinner_ind++;
			}
		}
	}
	if (options.verbosity >= 4) {
		printf ("watch thread done\n");
	} else if (options.verbosity >= 0) {
		prev_prog = wd->progress (prev_prog, spinner_ind);
		printf ("\n");
	}
	free (local_work);

	return NULL;
}
#endif

void
RunThreads (void *(*thread_func) (void *), int (*progress)(int, int))
{
#ifdef USE_PTHREADS
	pthread_t  *work_threads;
	void       *status;
	int         i;

	if (options.threads > 1) {
		work_threads = alloca ((options.threads + 1) * sizeof (pthread_t *));
		working = calloc (options.threads, sizeof (int));
		for (i = 0; i < options.threads; i++) {
			if (pthread_create (&work_threads[i], &threads_attrib,
								thread_func, (void *) (intptr_t) i) == -1)
				Sys_Error ("pthread_create failed");
		}
		watch_data_t wd = { i, progress };
		if (pthread_create (&work_threads[i], &threads_attrib,
							WatchThread, &wd) == -1)
			Sys_Error ("pthread_create failed");

		for (i = 0; i < options.threads; i++) {
			if (pthread_join (work_threads[i], &status) == -1)
				Sys_Error ("pthread_join failed");
		}
		if (pthread_join (work_threads[i], &status) == -1)
			Sys_Error ("pthread_join failed");

		free (working);
	} else {
		thread_func (0);
	}
#else
	thread_func (0);
#endif
}

int
CompressRow (sizebuf_t *dest, const byte *vis, unsigned num_leafs, int utf8)
{
	int         rep, visrow, j;
	byte       *dest_p;
	// if we ever need 2GB (32Gb) of vis data for a single leaf...
	int         maxrep = utf8 ? 0x7fffffff : 255;

	dest_p = dest->data;
	visrow = (num_leafs + 7) >> 3;

	for (j = 0; j < visrow; j++) {
		*dest_p++ = vis[j];
		if (vis[j])
			continue;

		rep = 1;
		for (j++; j < visrow; j++)
			if (vis[j] || rep == maxrep)
				break;
			else
				rep++;
		if (utf8) {
			dest->cursize = dest_p - dest->data;
			MSG_WriteUTF8 (dest, rep);
			dest_p = dest->data + dest->cursize;
		} else {
			*dest_p++ = rep;
		}
		j--;
	}
	dest->cursize = dest_p - dest->data;
	return dest->cursize;
}

static void
ClusterFlowExpand (const set_t *src, byte *dest)
{
	unsigned    i, j;

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
	sizebuf_t   compressed = {
		.maxsize = (bitbytes_l * 3) / 2,
		.data = alloca ((bitbytes_l * 3) / 2)
	};

	byte       *outbuffer;
	int         numvis, i;
	cluster_t  *cluster;
	portal_t   *portal;

	outbuffer = uncompressed + clusternum * bitbytes_l;
	cluster = &clusters[clusternum];

	// flow through all portals, collecting visible bits

	memset (compressed.data, 0, compressed.maxsize);
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

	numvis = set_count (visclusters);

	// expand to cluster->leaf PVS
	ClusterFlowExpand (visclusters, outbuffer);

	set_delete (visclusters);

	// compress the bit string
	if (options.verbosity >= 4)
		printf ("cluster %4i : %4i visible\n", clusternum, numvis);
	totalvis += numvis;

	i = CompressRow (&compressed, outbuffer, numrealleafs, 0);
	cluster->visofs = visdata->size;
	dstring_append (visdata, (char *) compressed.data, i);
}

static int
portalcmp (const void *_a, const void *_b)
{
	portal_t   *a = *(portal_t **) _a;
	portal_t   *b = *(portal_t **) _b;
	return a->nummightsee - b->nummightsee;
}

static void
BasePortalVis (void)
{
	double      start, end;

	if (options.verbosity >= 0)
		printf ("Base vis: ");
	if (options.verbosity >= 4)
		printf ("\n");

	start = Sys_DoubleTime ();
	RunThreads (BaseVisThread, print_progress);
	end = Sys_DoubleTime ();

	if (options.verbosity >= 1) {
		printf ("base_mightsee: %d %gs\n", base_mightsee, end - start);
		printf ("sphere cull: %u winding cull %u\n",
				base_spherecull, base_windingcull);
	}
}

static void
CalcPortalVis (void)
{
	unsigned    i;
	double      start, end;

	// fastvis just uses mightsee for a very loose bound
	if (options.minimal) {
		for (i = 0; i < numportals * 2; i++) {
			portals[i].visbits = portals[i].mightsee;
			portals[i].status = stat_done;
		}
		return;
	}
	start = Sys_DoubleTime ();
	qsort (portal_queue, numportals * 2, sizeof (portal_t *), portalcmp);
	end = Sys_DoubleTime ();
	if (options.verbosity >= 1)
		printf ("qsort: %gs\n", end - start);

	if (options.verbosity >= 0)
		printf ("Full vis: ");
	if (options.verbosity >= 4)
		printf ("\n");

	portal_count = 0;

	RunThreads (LeafThread, print_progress);

	if (options.verbosity >= 1) {
		printf ("portalcheck: %i  portaltest: %i  portalpass: %i\n",
				global_stats.portalcheck, global_stats.portaltest,
				global_stats.portalpass);
		printf ("target trimmed: %d clipped: %d tested: %d\n",
				global_stats.targettrimmed, global_stats.targetclipped,
				global_stats.targettested);
		printf ("source trimmed: %d clipped: %d tested: %d\n",
				global_stats.sourcetrimmed, global_stats.sourceclipped,
				global_stats.sourcetested);
		printf ("vistest: %i  mighttest: %i mightseeupdate: %i\n",
				global_stats.vistest, global_stats.mighttest,
				global_stats.mightseeupdate);
		if (options.verbosity >= 2) {
			printf ("separators allocated: %u freed: %u %u\n",
					global_stats.sep_alloc, global_stats.sep_free,
					global_stats.sep_alloc - global_stats.sep_free);
			printf ("max windings mark: %zd\n", global_stats.winding_mark);
			printf ("stack blocks allocated: %u freed: %u %u\n",
					global_stats.stack_alloc, global_stats.stack_free,
					global_stats.stack_alloc - global_stats.stack_free);
		}
	}
}

static void
CalcVis (void)
{
	unsigned    i;

	printf ("Thread count: %d\n", options.threads);
	BasePortalVis ();
	for (i = 0; i < 2 * numportals; i++) {
		portals[i].status = stat_none;
	}
	CalcPortalVis ();

	// assemble the leaf vis lists by oring and compressing the portal lists
	for (i = 0; i < portalclusters; i++)
		ClusterFlow (i);

	for (i = 0; i < numrealleafs; i++) {
		bsp->leafs[i + 1].visofs = clusters[leafcluster[i]].visofs;
	}
	if (options.verbosity >= 0)
		printf ("average clusters visible: %u\n", totalvis / portalclusters);
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
	unsigned    numpoints, i, j, k;
	int         read_leafs = 0;
	int         clusternums[2];
	cluster_t  *cluster;
	vec4f_t     plane;
	portal_t   *portal;
	winding_t  *winding;
	vspheref_t  sphere;
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
		if (!line || sscanf (line, "%u\n", &portalclusters) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		line = Qgetline (f);
		if (!line || sscanf (line, "%u\n", &numportals) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		numrealleafs = portalclusters;
	} else if (line && (!strcmp (line, PORTALFILE_AM "\n")
						|| !strcmp (line, PORTALFILE_AM "\r\n"))) {
		line = Qgetline (f);
		if (!line || sscanf (line, "%u\n", &portalclusters) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		line = Qgetline (f);
		if (!line || sscanf (line, "%u\n", &numportals) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		line = Qgetline (f);
		if (!line || sscanf (line, "%u\n", &numrealleafs) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		read_leafs = 1;
	} else if (line && (!strcmp (line, PORTALFILE2 "\n")
						|| !strcmp (line, PORTALFILE2 "\r\n"))) {
		line = Qgetline (f);
		if (!line || sscanf (line, "%u\n", &numrealleafs) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		line = Qgetline (f);
		if (!line || sscanf (line, "%u\n", &portalclusters) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		line = Qgetline (f);
		if (!line || sscanf (line, "%u\n", &numportals) != 1)
			Sys_Error ("LoadPortals: failed to read header");
		read_leafs = 1;
	} else {
		Sys_Error ("LoadPortals: not a portal file");
	}

	if (options.verbosity >= 0) {
		printf ("%4u portalclusters\n", portalclusters);
		printf ("%4u numportals\n", numportals);
		printf ("%4u numrealleafs\n", numrealleafs);
	}

	bitbytes = ((portalclusters + 63) & ~63) >> 3;
	bitlongs = bitbytes / sizeof (long);

	bitbytes_l = ((numrealleafs + 63) & ~63) >> 3;

	// each file portal is split into two memory portals, one for each
	// direction
	portals = calloc (2 * numportals, sizeof (portal_t));
	portal_queue = malloc (2 * numportals * sizeof (portal_t *));
	for (i = 0; i < 2 * numportals; i++) {
		portal_queue[i] = &portals[i];
	}
#ifdef USE_PTHREADS
	portal_locks = calloc (2 * numportals, sizeof (pthread_rwlock_t));
	for (i = 0; i < 2 * numportals; i++) {
		if (pthread_rwlock_init (&portal_locks[i], 0))
			Sys_Error ("pthread_rwlock_init failed");
	}
#endif

	clusters = calloc (portalclusters, sizeof (cluster_t));

	originalvismapsize = numrealleafs * ((numrealleafs + 7) / 8);

	for (i = 0, portal = portals; i < numportals; i++) {
		line = Qgetline (f);
		if (!line)
			Sys_Error ("LoadPortals: reading portal %u", i);

		numpoints = strtol (line, &err, 10);
		if (err == line)
			Sys_Error ("LoadPortals: reading portal %u", i);
		line = err;
		for (j = 0; j < 2; j++) {
			clusternums[j] = strtol (line, &err, 10);
			if (err == line)
				Sys_Error ("LoadPortals: reading portal %u", i);
			line = err;
		}

		if ((unsigned) clusternums[0] > (unsigned) portalclusters
			|| (unsigned) clusternums[1] > (unsigned) portalclusters)
			Sys_Error ("LoadPortals: reading portal %u", i);

		winding = portal->winding = NewWinding (&main_thread, numpoints);
		winding->original = true;
		winding->numpoints = numpoints;

		for (j = 0; j < numpoints; j++) {
			// (%ld %ld %ld)
			while (isspace ((byte) *line))
				line++;
			if (*line++ != '(')
				Sys_Error ("LoadPortals: reading portal %u", i);

			for (k = 0; k < 3; k++) {
				winding->points[j][k] = strtod (line, &err);
				if (err == line)
					Sys_Error ("LoadPortals: reading portal %u", i);
				line = err;
			}
			winding->points[j][3] = 1;

			while (isspace ((byte) *line))
				line++;
			if (*line++ != ')')
				Sys_Error ("LoadPortals: reading portal %u", i);
		}

		sphere = SmallestEnclosingBall_vf(winding->points, winding->numpoints);
		//printf (VEC4F_FMT" %.9g\n", VEC4_EXP (sphere.center), sphere.radius);

		// calc plane
		plane = PlaneFromWinding (winding);

		// create forward portal
		cluster = &clusters[clusternums[0]];
		if (cluster->numportals == MAX_PORTALS_ON_CLUSTER)
			Sys_Error ("Cluster with too many portals");
		cluster->portals[cluster->numportals] = portal;
		cluster->numportals++;

		portal->winding = winding;
		portal->plane = -plane;	// plane is for CW, portal is CCW
		portal->cluster = clusternums[1];
		portal->sphere = sphere;
		portal++;

		// create backwards portal
		cluster = &clusters[clusternums[1]];
		if (cluster->numportals == MAX_PORTALS_ON_CLUSTER)
			Sys_Error ("Cluster with too many portals");
		cluster->portals[cluster->numportals] = portal;
		cluster->numportals++;

		// Use a flipped winding for the reverse portal so the winding
		// direction and plane normal match.
		portal->winding = NewFlippedWinding (&main_thread, winding);
		portal->winding->original = true;
		portal->plane = plane;
		portal->cluster = clusternums[0];
		portal->sphere = sphere;
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

static void
generate_pvs (void)
{
	QFile      *f;

	dstring_t  *portalfile = dstring_new ();

	visdata = dstring_new ();

	dstring_copystr (portalfile, options.bspfile->str);
	QFS_SetExtension (portalfile, ".prt");
	LoadPortals (portalfile->str);

	uncompressed = calloc (bitbytes_l, portalclusters);

	CalcVis ();

	if (options.verbosity >= 1)
		printf ("chains: %i%s\n", global_stats.chains,
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

	dstring_delete (portalfile);
	dstring_delete (visdata);
	dstring_delete (options.bspfile);
	free (leafcluster);
	free (uncompressed);
	free (portals);
	free (clusters);
}

int
main (int argc, char **argv)
{
	double      start, stop;
	QFile      *f;
	size_t      main_memsize = 1024 * 1024 * 1024;

	main_thread.memsuper = new_memsuper ();
	main_thread.hunk = Hunk_Init (Sys_Alloc (main_memsize), main_memsize);

	start = Sys_DoubleTime ();

	this_program = argv[0];

	DecodeArgs (argc, argv);

	InitThreads ();

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

	if (!options.no_auto_pvs) {
		generate_pvs ();
	}
	if (options.fat_pvs) {
		CalcFatPVS ();
	}

	BSP_Free (bsp);

	EndThreads ();

	stop = Sys_DoubleTime ();

	if (options.verbosity >= -1)
		printf ("%5.1f seconds elapsed\n", stop - start);

	return 0;
}
