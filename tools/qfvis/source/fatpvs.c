/*
	fatpvs.c

	PVS PHS generator tool

	Copyright (C) 2021 Bil Currie

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
		Boston, MA	02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/heapsort.h"
#include "QF/pvsfile.h"
#include "QF/quakefs.h"
#include "QF/set.h"
#include "QF/sizebuf.h"
#include "QF/sys.h"

#include "tools/qfvis/include/options.h"
#include "tools/qfvis/include/vis.h"

typedef struct {
	uint32_t    visoffs;
	uint32_t    leafnum;
} leafvis_t;

typedef struct {
	uint32_t    first_leaf;
	uint32_t    num_leafs;
} leafmap_t;

static set_pool_t *set_pool;
static set_t     *base_pvs;
static set_t     *fat_pvs;
static leafvis_t *leafvis;
static leafmap_t *leafmap;
static sizebuf_t *cmp_pvs;

static uint32_t num_leafs;
static uint32_t num_clusters;
static uint32_t work_cluster;

typedef struct {
	long        pvs_visible;
	long        fat_visible;
	long        fat_bytes;
} fatstats_t;

fatstats_t  fatstats;

static void
update_stats (fatstats_t *stats)
{
	WRLOCK (stats_lock);
	fatstats.pvs_visible += stats->pvs_visible;
	fatstats.fat_visible += stats->fat_visible;
	fatstats.fat_bytes += stats->fat_bytes;
	UNLOCK (stats_lock);
}

static int
cluster_progress (void)
{
	return work_cluster * 100 / num_clusters;
}

static unsigned
next_cluster (void)
{
	unsigned    leaf = ~0;
	WRLOCK (global_lock);
	if (work_cluster < num_clusters) {
		leaf = work_cluster++;
	}
	UNLOCK (global_lock);
	return leaf;
}

static inline void
decompress_vis (const byte *in, unsigned numleafs, set_t *pvs)
{
	byte       *out = (byte *) pvs->map;
	byte       *start = out;
	int			row, c;

	row = (numleafs + 7) >> 3;

	if (!in) {							// no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
	} else {
		do {
			if (*in) {
				*out++ = *in++;
				continue;
			}
			c = in[1];
			in += 2;
			while (c) {
				*out++ = 0;
				c--;
			}
		} while (out - start < row);
	}

	while ((out - start) & (sizeof (set_bits_t) - 1)) {
		*out++ = 0;
	}
}

static void *
decompress_thread (void *d)
{
	fatstats_t  stats = { };
	int         thread = (intptr_t) d;

	while (1) {
		unsigned    cluster_num = next_cluster ();

		if (working)
			working[thread] = cluster_num;
		if (cluster_num == ~0u) {
			break;
		}
		byte       *visdata = 0;
		dleaf_t    *leaf = &bsp->leafs[leafmap[cluster_num].first_leaf + 1];
		if (leaf->visofs >= 0) {
			visdata = bsp->visdata + leaf->visofs;
		}
		decompress_vis (visdata, num_leafs, &base_pvs[cluster_num]);
		if (cluster_num == 0) {
			continue;
		}
		stats.pvs_visible += set_count (&base_pvs[cluster_num]);
	}
	update_stats (&stats);
	return 0;
}

static void *
fatten_thread (void *d)
{
	fatstats_t  stats = { };
	int         thread = (intptr_t) d;

	while (1) {
		unsigned    cluster_num = next_cluster ();

		if (working)
			working[thread] = cluster_num;
		if (cluster_num == ~0u) {
			break;
		}

		set_assign (&fat_pvs[cluster_num], &base_pvs[cluster_num]);
		for (set_iter_t *iter = set_first_r (&set_pool[thread],
											 &base_pvs[cluster_num]);
			 iter;
			 iter = set_next_r (&set_pool[thread], iter)) {

			set_union (&fat_pvs[cluster_num], &base_pvs[leafcluster[iter->element]]);
		}
		stats.fat_visible += set_count (&fat_pvs[cluster_num]);
		set_difference (&fat_pvs[cluster_num], &base_pvs[cluster_num]);
	}
	update_stats (&stats);
	return 0;
}
#if 0
static void *
fatten_thread2 (void *d)
{
	for (unsigned visword = 0; visword < SET_WORDS (&sv.pvs[0]); visword++) {
		Sys_Printf ("%d / %ld\n", visword, SET_WORDS (&sv.pvs[0]));
		for (i = 0; i < num; i++) {
			for (unsigned j = 0; j < SET_BITS; j++) {
				unsigned    visbit = visword * SET_BITS + j;
				if (visbit >= (unsigned) num) {
					break;
				}
				if (SET_TEST_MEMBER (&sv.pvs[i], visbit)) {
					// or this pvs row into the phs
					// +1 because pvs is 1 based
					set_union (&sv.phs[i], &sv.pvs[visbit + 1]);
				}
			}
		}
	}
	for (i = 1; i < num; i++) {
		for (set_iter_t *iter = set_first (&sv.phs[i]); iter;
			 iter = set_next (iter)) {
			count++;
		}
	}
}
#endif

static void *
compress_thread (void *d)
{
	fatstats_t  stats = { };
	int         thread = (intptr_t) d;
	qboolean    rle = options.utf8;

	while (1) {
		unsigned    cluster_num = next_cluster ();

		if (working)
			working[thread] = cluster_num;
		if (cluster_num == ~0u) {
			break;
		}
		sizebuf_t  *compressed = &cmp_pvs[cluster_num];
		const byte *fat_bytes = (const byte *) fat_pvs[cluster_num].map;
		int         bytes = CompressRow (compressed, fat_bytes, num_leafs, rle);
		stats.fat_bytes += bytes;
	}
	update_stats (&stats);
	return 0;
}

static int
leaf_compare (const void *_la, const void *_lb)
{
	const leafvis_t *la = _la;
	const leafvis_t *lb = _lb;
	if (la->visoffs == lb->visoffs) {
		return la->leafnum - lb->leafnum;
	}
	return la->visoffs - lb->visoffs;
}

static void
reconstruct_clusters (void)
{
	leafvis = malloc (num_leafs * sizeof (leafvis_t));
	for (unsigned i = 0; i < num_leafs; i++) {
		leafvis[i].visoffs = bsp->leafs[i + 1].visofs;
		leafvis[i].leafnum = i;
	}
	heapsort (leafvis, num_leafs, sizeof (leafvis_t), leaf_compare);

	num_clusters = 1;
	for (unsigned i = 1; i < num_leafs; i++) {
		num_clusters += leafvis[i].visoffs != leafvis[i - 1].visoffs;
	}

	printf ("num_clusters: %u\n", num_clusters);

	leafcluster = malloc (num_leafs * sizeof (uint32_t));
	leafmap = calloc (num_clusters, sizeof (leafmap_t));
	leafmap_t  *lm = leafmap;
	uint32_t    offs = leafvis[0].visoffs;
	for (unsigned i = 0; i < num_leafs; i++) {
		if (leafvis[i].visoffs != offs) {
			lm++;
			lm->first_leaf = i;
			offs = leafvis[i].visoffs;
		}
		leafcluster[leafvis[i].leafnum] = lm - leafmap;
		lm->num_leafs++;
	}
	//for (unsigned i = 0; i < num_clusters; i++) {
	//	printf ("%d %d %d\n", i, leafmap[i].first_leaf, leafmap[i].num_leafs);
	//}
	printf ("%d\n", num_leafs);
}

static void
allocate_data (void)
{
	set_pool = calloc (options.threads, sizeof (set_pool_t));

	base_pvs = malloc (num_clusters * sizeof (set_t));
	fat_pvs = malloc (num_clusters * sizeof (set_t));
	cmp_pvs = malloc (num_clusters * sizeof (sizebuf_t));

	uint32_t visbytes = (num_leafs + 7) / 8;
	// Worst case, RLE can add 50% to the bytes required (alternating zero
	// and non-zero bytes: 0 x 0 y -> 0 1 x 0 1 y ...). Also, if the map is
	// very tiny (8 leafs or fewer), there will be only one byte for vis, but
	// if that byte is 0, an extra byte for the count is required.
	visbytes = (visbytes * 3) / 2 + 1;
	for (unsigned i = 0; i < num_clusters; i++) {
		base_pvs[i] = (set_t) SET_STATIC_INIT (num_leafs - 1, malloc);
		fat_pvs[i] = (set_t) SET_STATIC_INIT (num_leafs - 1, malloc);
		cmp_pvs[i] = (sizebuf_t) {
			.data = malloc (visbytes),
			.maxsize = visbytes,
		};
	}
}

static void
write_pvs_file (void)
{
	uint32_t    offset = sizeof (pvsfile_t) + num_leafs * sizeof (uint32_t);
	pvsfile_t  *pvsfile = malloc (offset + fatstats.fat_bytes);

	strncpy (pvsfile->magic, PVS_MAGIC, sizeof (pvsfile->magic));
	pvsfile->version = PVS_VERSION;
	pvsfile->md4_offset = 0;	//FIXME add
	pvsfile->flags = PVS_IS_FATPVS;
	if (options.utf8) {
		pvsfile->flags |= PVS_UTF8_RLE;
	}
	pvsfile->visleafs = num_leafs;
	for (uint32_t i = 0; i < num_clusters; i++) {
		unsigned    size = cmp_pvs[i].cursize;
		for (uint32_t j = 0; j < leafmap[i].num_leafs; j++) {
			uint32_t    l = leafmap[i].first_leaf + j;
			pvsfile->visoffsets[leafvis[l].leafnum] = offset;
		}
		memcpy ((byte *) pvsfile + offset, cmp_pvs[i].data, size);
		offset += size;
	}

	dstring_t  *pvsname = dstring_new ();
	dstring_copystr (pvsname, options.bspfile->str);
	QFS_SetExtension (pvsname, ".pvs");

	QFile      *f = Qopen (pvsname->str, "wb");
	if (!f) {
		Sys_Error ("couldn't open %s for writing.", pvsname->str);
	}
	Qwrite (f, pvsfile, offset);
	Qclose (f);
}

void
CalcFatPVS (void)
{
	num_leafs = bsp->models[0].visleafs;

	reconstruct_clusters ();
	allocate_data ();

	work_cluster = 0;
	RunThreads (decompress_thread, cluster_progress);

	work_cluster = 0;
	RunThreads (fatten_thread, cluster_progress);

	work_cluster = 0;
	RunThreads (compress_thread, cluster_progress);

	printf ("Average leafs visible / fat visible / total: %d / %d / %d\n",
			(int) (fatstats.pvs_visible / num_clusters),
			(int) (fatstats.fat_visible / num_clusters), num_leafs);
	printf ("Compressed fat vis size: %ld\n", fatstats.fat_bytes);

	write_pvs_file ();
}
